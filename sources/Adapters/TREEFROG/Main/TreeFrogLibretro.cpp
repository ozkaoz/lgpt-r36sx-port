#include "Adapters/TREEFROG/Libretro/libretro.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "Adapters/TREEFROG/Audio/TreeFrogAudioDriver.h"
#include "Adapters/TREEFROG/GUI/TreeFrogEventManager.h"
#include "Adapters/TREEFROG/GUI/TreeFrogGUIWindowImp.h"
#include "Adapters/TREEFROG/System/TreeFrogSystem.h"
#include "Adapters/TREEFROG/Timer/TreeFrogTimer.h"
#include "Application/Application.h"
#include "Application/AppWindow.h"
#include "System/System/System.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#ifndef TREEFROG_INPUT_PROFILE
#define TREEFROG_INPUT_PROFILE 0
#endif

#ifndef TREEFROG_INPUT_DEBUG
#define TREEFROG_INPUT_DEBUG 0
#endif

#ifndef TREEFROG_FACE_TAP_MODE
#define TREEFROG_FACE_TAP_MODE 0
#endif

#ifndef TREEFROG_MODIFIER_RETRIGGER
#define TREEFROG_MODIFIER_RETRIGGER 0
#endif

#ifndef TREEFROG_VIDEO_MODE
#define TREEFROG_VIDEO_MODE 0
#endif

#ifndef TREEFROG_VIDEO_PROBE
#define TREEFROG_VIDEO_PROBE 0
#endif

#ifndef TREEFROG_ENABLE_START
#define TREEFROG_ENABLE_START 1
#endif

#ifndef TREEFROG_ENABLE_SELECT
#define TREEFROG_ENABLE_SELECT 0
#endif

#ifndef TREEFROG_PORT_VERSION_BADGE
#define TREEFROG_PORT_VERSION_BADGE 1
#endif

#ifndef TREEFROG_PORT_VERSION_BADGE_COLOR
#define TREEFROG_PORT_VERSION_BADGE_COLOR 0xd99b
#endif

#ifndef TREEFROG_RETRO_LIFECYCLE_DEBUG
#define TREEFROG_RETRO_LIFECYCLE_DEBUG 0
#endif

#ifndef TREEFROG_PHYSICAL_VOLUME_EVDEV
#define TREEFROG_PHYSICAL_VOLUME_EVDEV 1
#endif

#ifndef TREEFROG_PHYSICAL_VOLUME_JOYKEY_PROBE
#define TREEFROG_PHYSICAL_VOLUME_JOYKEY_PROBE 1
#endif

#ifndef TREEFROG_PHYSICAL_VOLUME_SYSTEM_PROBE
#define TREEFROG_PHYSICAL_VOLUME_SYSTEM_PROBE 1
#endif

#ifndef TREEFROG_PHYSICAL_VOLUME_STEP
#define TREEFROG_PHYSICAL_VOLUME_STEP 5
#endif

static retro_environment_t environ_cb = 0;
static retro_video_refresh_t video_cb = 0;
static retro_audio_sample_batch_t audio_batch_cb = 0;
static retro_input_poll_t input_poll_cb = 0;
static retro_input_state_t input_state_cb = 0;

static uint16_t framebuffer[TREEFROG_LGPT_WIDTH * TREEFROG_LGPT_HEIGHT];
#if TREEFROG_VIDEO_MODE == 1
/* Squeeze 320x240 into 240x240. */
static uint16_t video_squeeze_buffer[240 * 240];
#elif TREEFROG_VIDEO_MODE == 2
/* Right-edge diagnostic: copy the right 240px of the logical 320px framebuffer
 * to a 240x240 output, so missing right-side HUD pixels should appear on the
 * device even if the frontend crops the original left/right edge. */
static uint16_t video_right_probe_buffer[240 * 240];
#endif
static int16_t audio_buffer[2048 * 2];
static bool app_ready = false;
static double audio_accum = 0.0;
static uint32_t last_phys_for_taps = 0;
static uint32_t last_phys_for_combo = 0;

/* Cubevol shared-memory fallback used by FrogUI/TreeFrogUI on SF3000/GB300/R36SX.
 * Known bits match FrogUI. The L2/R2/FN bits are best-effort for R36SX
 * revisions and can be verified with TREEFROG_INPUT_DEBUG=1. */
#define CV_SEL   0
#define CV_FN    2
#define CV_START 3
#define CV_UP    4
#define CV_RIGHT 5
#define CV_DOWN  6
#define CV_LEFT  7
#define CV_L2    8
#define CV_R2    9
#define CV_L1    10
#define CV_R1    11
#define CV_X     12
#define CV_A     13
#define CV_B     14
#define CV_Y     15

static volatile uint32_t *cv_keys = 0;
static int cv_shmid = -1;

static void cv_detach(void) {
    if (cv_keys) {
        shmdt((const void *)cv_keys);
        cv_keys = 0;
    }
    cv_shmid = -1;
}

static void cv_init(void) {
    if (cv_keys) return;
    key_t key = ftok("/tmp/joy_key", 'a');
    if (key == (key_t)-1) return;
    cv_shmid = shmget(key, 4, 0666);
    if (cv_shmid < 0) return;
    cv_keys = (volatile uint32_t *)shmat(cv_shmid, 0, 0);
    if (cv_keys == (void *)-1) {
        cv_keys = 0;
        cv_shmid = -1;
    }
}

static bool cv_btn(uint32_t state, int bit) { return ((state >> bit) & 1) != 0; }
static bool lr_btn(unsigned id) {
    return input_state_cb && input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, id);
}

enum PhysicalButton {
    PHY_LEFT = 0,
    PHY_RIGHT,
    PHY_UP,
    PHY_DOWN,
    PHY_A,
    PHY_B,
    PHY_X,
    PHY_Y,
    PHY_L1,
    PHY_R1,
    PHY_L2,
    PHY_R2,
    PHY_START,
    PHY_SELECT,
    PHY_FN,
    PHY_COUNT
};

#define PHY_BIT(p) (1u << (unsigned)(p))

typedef struct PhysicalSource {
    PhysicalButton phy;
    const char *name;
    unsigned lr;
    int cv;
} PhysicalSource;

static const PhysicalSource physical_sources[] = {
    { PHY_LEFT,   "LEFT",   RETRO_DEVICE_ID_JOYPAD_LEFT,   CV_LEFT   },
    { PHY_RIGHT,  "RIGHT",  RETRO_DEVICE_ID_JOYPAD_RIGHT,  CV_RIGHT  },
    { PHY_UP,     "UP",     RETRO_DEVICE_ID_JOYPAD_UP,     CV_UP     },
    { PHY_DOWN,   "DOWN",   RETRO_DEVICE_ID_JOYPAD_DOWN,   CV_DOWN   },
    { PHY_A,      "A",      RETRO_DEVICE_ID_JOYPAD_A,      CV_A      },
    { PHY_B,      "B",      RETRO_DEVICE_ID_JOYPAD_B,      CV_B      },
    { PHY_X,      "X",      RETRO_DEVICE_ID_JOYPAD_X,      CV_X      },
    { PHY_Y,      "Y",      RETRO_DEVICE_ID_JOYPAD_Y,      CV_Y      },
    { PHY_L1,     "L1",     RETRO_DEVICE_ID_JOYPAD_L,      CV_L1     },
    { PHY_R1,     "R1",     RETRO_DEVICE_ID_JOYPAD_R,      CV_R1     },
    { PHY_L2,     "L2",     RETRO_DEVICE_ID_JOYPAD_L2,     CV_L2     },
    { PHY_R2,     "R2",     RETRO_DEVICE_ID_JOYPAD_R2,     CV_R2     },
    { PHY_START,  "START",  RETRO_DEVICE_ID_JOYPAD_START,  CV_START  },
    { PHY_SELECT, "SELECT", RETRO_DEVICE_ID_JOYPAD_SELECT, CV_SEL    },
    { PHY_FN,     "FN",     RETRO_DEVICE_ID_JOYPAD_L3,     CV_FN     }
};

typedef struct OutputState {
    GUIEventPadButtonType lgpt;
    const char *name;
    bool pressed;
    bool last;
} OutputState;

static OutputState output_states[] = {
    { EPBT_LEFT,   "LGPT_LEFT",   false, false },
    { EPBT_RIGHT,  "LGPT_RIGHT",  false, false },
    { EPBT_UP,     "LGPT_UP",     false, false },
    { EPBT_DOWN,   "LGPT_DOWN",   false, false },
    { EPBT_A,      "LGPT_A",      false, false },
    { EPBT_B,      "LGPT_B",      false, false },
    { EPBT_X,      "LGPT_X",      false, false },
    { EPBT_Y,      "LGPT_Y",      false, false },
    { EPBT_L2,     "LGPT_L2",     false, false },
    { EPBT_R2,     "LGPT_R2",     false, false },
    { EPBT_L,      "LGPT_L",      false, false },
    { EPBT_R,      "LGPT_R",      false, false },
    { EPBT_START,  "LGPT_START",  false, false },
    { EPBT_SELECT, "LGPT_SELECT", false, false }
};

static void output_set(GUIEventPadButtonType lgpt) {
    for (unsigned i = 0; i < sizeof(output_states) / sizeof(output_states[0]); ++i) {
        if (output_states[i].lgpt == lgpt) {
            output_states[i].pressed = true;
            return;
        }
    }
}

static void clear_outputs(void) {
    for (unsigned i = 0; i < sizeof(output_states) / sizeof(output_states[0]); ++i) {
        output_states[i].pressed = false;
    }
}

static void reset_input_latches(bool send_releases) {
    TreeFrogEventManager *em = TreeFrogEventManager::GetInstance();
    for (unsigned i = 0; i < sizeof(output_states) / sizeof(output_states[0]); ++i) {
        if (send_releases && output_states[i].last && em) {
            em->PushPad(output_states[i].lgpt, false);
        }
        output_states[i].pressed = false;
        output_states[i].last = false;
    }
    if (send_releases && em) em->Flush();
    if (em) em->ClearQueue();
    last_phys_for_taps = 0;
}

static uint32_t read_physical_mask(uint32_t cv, uint32_t *lr_mask_out) {
    uint32_t mask = 0;
    uint32_t lr_mask = 0;
    for (unsigned i = 0; i < sizeof(physical_sources) / sizeof(physical_sources[0]); ++i) {
        bool lr = lr_btn(physical_sources[i].lr);
        bool cvp = cv_btn(cv, physical_sources[i].cv);
        if (lr) lr_mask |= PHY_BIT(physical_sources[i].phy);
        if (lr || cvp) mask |= PHY_BIT(physical_sources[i].phy);
    }
    if (lr_mask_out) *lr_mask_out = lr_mask;
    return mask;
}

static void map_physical_to_lgpt(uint32_t phys) {
    clear_outputs();

    if (phys & PHY_BIT(PHY_LEFT))  output_set(EPBT_LEFT);
    if (phys & PHY_BIT(PHY_RIGHT)) output_set(EPBT_RIGHT);
    if (phys & PHY_BIT(PHY_UP))    output_set(EPBT_UP);
    if (phys & PHY_BIT(PHY_DOWN))  output_set(EPBT_DOWN);

#if TREEFROG_FACE_TAP_MODE == 0
    if (phys & PHY_BIT(PHY_A))     output_set(EPBT_A);
    if (phys & PHY_BIT(PHY_B))     output_set(EPBT_B);

    /* R36SX-friendly safe duplicates.  On this hardware the right-stick lines
     * are digital face-button aliases on some revisions, so duplicates are
     * aggregated before dispatch and cannot create double down/up events. */
    // TREEFROG_INPUT_X_DEDICATED: X deja de duplicar A y queda reservado para funciones nuevas.
    if (phys & PHY_BIT(PHY_X))     output_set(EPBT_X);
    // TREEFROG_INPUT_XY_DUPLICATE_COMPAT: Y sigue duplicando B hasta activar Y dedicado.
    // TREEFROG_INPUT_Y_DEDICATED: Y deja de duplicar B y queda disponible para funciones nuevas.
    if (phys & PHY_BIT(PHY_Y))     output_set(EPBT_Y);
#else
    /* Diagnostic mode: face buttons are injected as rising-edge taps after
     * latched direction/shoulder state has been updated.  This avoids leaving
     * PhraseView in an A-held state and isolates A-down crashes. */
#endif
    if (phys & PHY_BIT(PHY_L1))    output_set(EPBT_L);
    if (phys & PHY_BIT(PHY_R1))    output_set(EPBT_R);
    // TREEFROG_INPUT_L2_R2_DEDICATED: L2 deja de duplicar L y queda reservado.
    if (phys & PHY_BIT(PHY_L2))     output_set(EPBT_L2);
    // TREEFROG_INPUT_L2_R2_DEDICATED: R2 deja de duplicar R y queda reservado.
    if (phys & PHY_BIT(PHY_R2))     output_set(EPBT_R2);

#if TREEFROG_ENABLE_START
    /* V1 stable: START is forwarded only after the Player empty-channel
     * playback guard is applied by patch_player_start_guard.py. */
    if (phys & PHY_BIT(PHY_START))  output_set(EPBT_START);
#endif

#if TREEFROG_ENABLE_SELECT
    if (phys & PHY_BIT(PHY_SELECT)) output_set(EPBT_SELECT);
#endif
}

#if TREEFROG_INPUT_DEBUG
static FILE *input_log = 0;
static uint32_t last_logged_phys = 0xffffffffu;
static uint32_t last_logged_cv = 0xffffffffu;
static uint32_t last_logged_lr = 0xffffffffu;

static void append_button_names(FILE *f, uint32_t phys) {
    bool any = false;
    for (unsigned i = 0; i < sizeof(physical_sources) / sizeof(physical_sources[0]); ++i) {
        if (phys & PHY_BIT(physical_sources[i].phy)) {
            if (any) fputc('+', f);
            fputs(physical_sources[i].name, f);
            any = true;
        }
    }
    if (!any) fputs("none", f);
}

static void close_input_log(void) {
    if (input_log) {
        fclose(input_log);
        input_log = 0;
    }
}

static void truncate_debug_logs(void) {
    const char *paths[] = {
        "/mnt/sdcard/lgpt/input_debug.log",
        "/mnt/sdcard/lgpt/event_debug.log",
        "/mnt/sdcard/lgpt/audio_debug.log"
    };
    for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        FILE *f = fopen(paths[i], "w");
        if (f) fclose(f);
    }
    last_logged_phys = 0xffffffffu;
    last_logged_cv = 0xffffffffu;
    last_logged_lr = 0xffffffffu;
}

static void log_input_change(uint32_t cv, uint32_t lr_mask, uint32_t phys) {
    if (phys == last_logged_phys && cv == last_logged_cv && lr_mask == last_logged_lr) return;
    last_logged_phys = phys;
    last_logged_cv = cv;
    last_logged_lr = lr_mask;

    if (!input_log) input_log = fopen("/mnt/sdcard/lgpt/input_debug.log", "a");
    if (!input_log) return;

    fprintf(input_log, "cv=0x%04x lr=0x%04x phys=", (unsigned)(cv & 0xffff), (unsigned)lr_mask);
    append_button_names(input_log, phys);
    fprintf(input_log, " profile=%d\n", TREEFROG_INPUT_PROFILE);
    fflush(input_log);
}
#else
static void close_input_log(void) {}
static void truncate_debug_logs(void) {}
static void log_input_change(uint32_t cv, uint32_t lr_mask, uint32_t phys) { (void)cv; (void)lr_mask; (void)phys; }
#endif

static void dispatch_outputs(void) {
    TreeFrogEventManager *em = TreeFrogEventManager::GetInstance();
    for (unsigned i = 0; i < sizeof(output_states) / sizeof(output_states[0]); ++i) {
        if (output_states[i].pressed != output_states[i].last) {
            em->PushPad(output_states[i].lgpt, output_states[i].pressed);
            output_states[i].last = output_states[i].pressed;
        }
    }
}

static void push_face_tap(GUIEventPadButtonType button) {
    TreeFrogEventManager *em = TreeFrogEventManager::GetInstance();
    if (!em) return;
    em->PushPad(button, true);
    em->PushPad(button, false);
}

static void dispatch_modifier_retrigger(uint32_t phys) {
#if TREEFROG_MODIFIER_RETRIGGER
    TreeFrogEventManager *em = TreeFrogEventManager::GetInstance();
    if (!em) {
        last_phys_for_combo = phys;
        return;
    }

    const uint32_t dirs =
        PHY_BIT(PHY_LEFT) | PHY_BIT(PHY_RIGHT) | PHY_BIT(PHY_UP) | PHY_BIT(PHY_DOWN);

    uint32_t rising_dirs = (phys & dirs) & ~(last_phys_for_combo & dirs);

    /* LGPT on this target edits reliably when it receives the modifier down
     * after the direction down. This preserves normal held A/B behavior and
     * fixes the user-facing order A-then-direction / B-then-direction.
     */
    if (rising_dirs) {
        if (phys & PHY_BIT(PHY_A)) {
            em->PushPad(EPBT_A, true);
        }
        if (phys & PHY_BIT(PHY_B)) {
            em->PushPad(EPBT_B, true);
        }
    }
#else
    (void)phys;
#endif
    last_phys_for_combo = phys;
}

static void dispatch_face_taps(uint32_t phys) {
#if TREEFROG_FACE_TAP_MODE == 1
    uint32_t rising = phys & ~last_phys_for_taps;
    if (rising & PHY_BIT(PHY_A)) push_face_tap(EPBT_A);
    // TREEFROG_INPUT_X_DEDICATED: face-tap X usa EPBT_X.
    if (rising & PHY_BIT(PHY_X)) push_face_tap(EPBT_X);
    if (rising & PHY_BIT(PHY_B)) push_face_tap(EPBT_B);
    // TREEFROG_INPUT_XY_DUPLICATE_COMPAT: face-tap Y conserva B.
    // TREEFROG_INPUT_Y_DEDICATED: face-tap Y usa EPBT_Y.
    if (rising & PHY_BIT(PHY_Y)) push_face_tap(EPBT_Y);
#else
    (void)phys;
#endif
    last_phys_for_taps = phys;
}



#if TREEFROG_PHYSICAL_VOLUME_EVDEV || TREEFROG_PHYSICAL_VOLUME_JOYKEY_PROBE || TREEFROG_PHYSICAL_VOLUME_SYSTEM_PROBE
/* AU9V: physical volume sync stays inside the libretro core. It does not
 * reinstall the removed external r36s_*_volume_watch daemon.
 * Sources tried in order:
 *   1) /dev/input/event* KEY_VOLUMEUP/DOWN when available.
 *   2) OSS mixer (/dev/mixer) because cubevol changes console volume there on many stock builds.
 *   3) raw stock volume files under /proc, /sys, and /tmp.
 *   4) /tmp/joy_key System V shared-memory bit-change probe, with configurable mapping.
 */
#define TF_EV_KEY 1
#define TF_KEY_VOLUMEDOWN 114
#define TF_KEY_VOLUMEUP 115
struct tf_volume_input_event { struct timeval time; unsigned short type; unsigned short code; int value; };
static int g_tf_volume_fds[32];
static int g_tf_volume_fds_init = 0;
static unsigned long g_tf_volume_event_counter = 0;
static unsigned long g_tf_volume_scan_counter = 0;
static int g_tf_oss_mixer_fd = -1;
static int g_tf_oss_mixer_dev = -1;
static int g_tf_oss_mixer_last = -9999;
static int g_tf_oss_mixer_open_logged = 0;
#ifndef TF_SOUND_MIXER_VOLUME
#define TF_SOUND_MIXER_VOLUME 0
#endif
#ifndef TF_SOUND_MIXER_PCM
#define TF_SOUND_MIXER_PCM 4
#endif
#ifndef TF_SOUND_MIXER_SPEAKER
#define TF_SOUND_MIXER_SPEAKER 5
#endif
#ifndef TF_MIXER_READ
#define TF_MIXER_READ(dev) _IOR('M', dev, int)
#endif
static int g_tf_system_volume_baseline = -9999;
static int g_tf_system_volume_last = -9999;
static uint32_t g_tf_joykey_last = 0xffffffffu;
static uint32_t g_tf_joykey_known_mask = 0x0000fffd; /* known LGPT/FrogUI bits except bit1; logs high/unknown bits */
static int g_tf_joykey_up_bit = -1;
static int g_tf_joykey_down_bit = -1;
static uint32_t g_tf_shm_last_words[16];
static int g_tf_shm_last_valid = 0;
static int g_tf_shm_words_logged = 0;
static int g_tf_auto_up_bit = -1;
static int g_tf_auto_down_bit = -1;
static int g_tf_auto_learning_enabled = 1;

static void tf_volume_log(const char *msg) {
    char line[384];
    int saved_errno = errno;
    int n = snprintf(line, sizeof(line), "AU9V %lu %s errno=%d (%s)\n",
                     g_tf_volume_event_counter, msg ? msg : "volume", saved_errno, strerror(saved_errno));
    if (n <= 0) return;
    if (n > (int)sizeof(line)) n = (int)sizeof(line);
    int fd = open("/mnt/sdcard/lgpt/physical_volume_debug.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) return;
    write(fd, line, (size_t)n);
    close(fd);
}

static int tf_read_int_file(const char *path, int *out) {
    char buf[64];
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = 0;
    int found = 0, neg = 0, val = 0;
    for (ssize_t i=0; i<n; ++i) {
        if (!found && buf[i]=='-') { neg=1; found=1; continue; }
        if (buf[i] >= '0' && buf[i] <= '9') { found=1; val = val*10 + (buf[i]-'0'); }
        else if (found) break;
    }
    if (!found) return 0;
    *out = neg ? -val : val;
    return 1;
}

static int tf_percent_from_raw_system_volume(int raw) {
    if (raw < 0) raw = 0;
    if (raw <= 15) return (raw * 100 + 7) / 15;
    if (raw <= 31) return (raw * 100 + 15) / 31;
    if (raw <= 100) return raw;
    return 100;
}

static void tf_volume_set_project_master(int percent, const char *source);

static int tf_volume_oss_average_percent(int packed) {
    int left = packed & 0xff;
    int right = (packed >> 8) & 0xff;
    if (left < 0) left = 0; if (left > 100) left = 100;
    if (right < 0) right = 0; if (right > 100) right = 100;
    return (left + right + 1) / 2;
}

static int tf_volume_open_oss_mixer(void) {
    if (g_tf_oss_mixer_fd >= 0) return 1;
    const char *paths[] = { "/dev/mixer", "/dev/sound/mixer", "/dev/mixer0", 0 };
    for (int i = 0; paths[i]; ++i) {
        int fd = open(paths[i], O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            g_tf_oss_mixer_fd = fd;
            char msg[128]; snprintf(msg, sizeof(msg), "oss mixer opened path=%s", paths[i]); tf_volume_log(msg);
            return 1;
        }
    }
    if (!g_tf_oss_mixer_open_logged) { g_tf_oss_mixer_open_logged = 1; tf_volume_log("oss mixer not available"); }
    return 0;
}

static int tf_volume_read_oss_dev(int dev, int *pct_out) {
    if (!tf_volume_open_oss_mixer()) return 0;
    int packed = 0;
    if (ioctl(g_tf_oss_mixer_fd, TF_MIXER_READ(dev), &packed) < 0) return 0;
    if (pct_out) *pct_out = tf_volume_oss_average_percent(packed);
    return 1;
}

static void tf_volume_poll_oss_mixer(void) {
    int pct = -1;
    int dev = -1;
    if (tf_volume_read_oss_dev(TF_SOUND_MIXER_VOLUME, &pct)) dev = TF_SOUND_MIXER_VOLUME;
    else if (tf_volume_read_oss_dev(TF_SOUND_MIXER_PCM, &pct)) dev = TF_SOUND_MIXER_PCM;
    else if (tf_volume_read_oss_dev(TF_SOUND_MIXER_SPEAKER, &pct)) dev = TF_SOUND_MIXER_SPEAKER;
    else return;

    if (g_tf_oss_mixer_last == -9999) {
        g_tf_oss_mixer_last = pct; g_tf_oss_mixer_dev = dev;
        char msg[128]; snprintf(msg, sizeof(msg), "oss mixer baseline dev=%d percent=%d", dev, pct); tf_volume_log(msg);
        return;
    }
    if (pct != g_tf_oss_mixer_last) {
        g_tf_oss_mixer_last = pct; g_tf_oss_mixer_dev = dev;
        ++g_tf_volume_event_counter;
        char msg[128]; snprintf(msg, sizeof(msg), "oss mixer changed dev=%d percent=%d", dev, pct); tf_volume_log(msg);
        tf_volume_set_project_master(pct, "oss_mixer");
    }
}

static void tf_volume_set_project_master(int percent, const char *source) {
    int out = -1;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    Application *app = Application::GetInstance();
    if (!app || !app->GetWindow()) { tf_volume_log("set master skipped: no app/window"); return; }
    AppWindow *aw = (AppWindow *)app->GetWindow();
    bool ok = aw->SetProjectMasterFromPhysicalVolume(percent, &out);
    char msg[160];
    snprintf(msg, sizeof(msg), "%s set project_master=%d requested=%d ok=%d", source ? source : "physical", out, percent, ok ? 1 : 0);
    tf_volume_log(msg);
}

static void tf_volume_adjust_project_master(int delta) {
    int out = -1;
    Application *app = Application::GetInstance();
    if (!app || !app->GetWindow()) { tf_volume_log("project master adjust skipped: no app/window"); return; }
    AppWindow *aw = (AppWindow *)app->GetWindow();
    bool ok = aw->AdjustProjectMasterFromPhysicalVolume(delta, &out);
    char msg[128];
    snprintf(msg, sizeof(msg), "physical key delta=%d project_master=%d ok=%d", delta, out, ok ? 1 : 0);
    tf_volume_log(msg);
}

static void tf_volume_init_fds(void) {
#if TREEFROG_PHYSICAL_VOLUME_EVDEV
    if (g_tf_volume_fds_init) return;
    g_tf_volume_fds_init = 1;
    for (int i = 0; i < 32; ++i) g_tf_volume_fds[i] = -1;
    int opened = 0;
    for (int i = 0; i < 32; ++i) {
        char path[64]; snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) { g_tf_volume_fds[i] = fd; opened++; }
    }
    char msg[96]; snprintf(msg, sizeof(msg), "evdev scan opened=%d", opened); tf_volume_log(msg);
#endif
}

static void tf_volume_poll_evdev(void) {
#if TREEFROG_PHYSICAL_VOLUME_EVDEV
    tf_volume_init_fds();
    if ((++g_tf_volume_scan_counter % 3600) == 0) {
        for (int i = 0; i < 32; ++i) { if (g_tf_volume_fds[i] >= 0) close(g_tf_volume_fds[i]); g_tf_volume_fds[i] = -1; }
        g_tf_volume_fds_init = 0; tf_volume_init_fds();
    }
    for (int i = 0; i < 32; ++i) {
        int fd = g_tf_volume_fds[i]; if (fd < 0) continue;
        for (int n = 0; n < 24; ++n) {
            struct tf_volume_input_event ev; ssize_t r = read(fd, &ev, sizeof(ev));
            if (r == 0) break;
            if (r < 0) { if (errno == EAGAIN || errno == EWOULDBLOCK) break; close(fd); g_tf_volume_fds[i] = -1; tf_volume_log("evdev fd closed after read error"); break; }
            if (r != (ssize_t)sizeof(ev) || ev.type != TF_EV_KEY || (ev.value != 1 && ev.value != 2)) continue;
            if (ev.code == TF_KEY_VOLUMEUP) { ++g_tf_volume_event_counter; tf_volume_adjust_project_master(+TREEFROG_PHYSICAL_VOLUME_STEP); }
            else if (ev.code == TF_KEY_VOLUMEDOWN) { ++g_tf_volume_event_counter; tf_volume_adjust_project_master(-TREEFROG_PHYSICAL_VOLUME_STEP); }
        }
    }
#endif
}

static void tf_volume_poll_system_raw(void) {
#if TREEFROG_PHYSICAL_VOLUME_SYSTEM_PROBE
    int raw = 0;
    if (!tf_read_int_file("/proc/device-tree/hcrtos/i2so/volume", &raw) &&
        !tf_read_int_file("/proc/device-tree/hcrtos/volume", &raw) &&
        !tf_read_int_file("/proc/hcrtos/volume", &raw) &&
        !tf_read_int_file("/proc/hc/audio/volume", &raw) &&
        !tf_read_int_file("/sys/class/hc/audio/volume", &raw) &&
        !tf_read_int_file("/tmp/cubevol_volume", &raw) &&
        !tf_read_int_file("/tmp/volume", &raw)) return;
    if (g_tf_system_volume_baseline == -9999) {
        g_tf_system_volume_baseline = raw;
        g_tf_system_volume_last = raw;
        char msg[96]; snprintf(msg, sizeof(msg), "system volume baseline raw=%d percent=%d", raw, tf_percent_from_raw_system_volume(raw)); tf_volume_log(msg);
        return;
    }
    if (raw != g_tf_system_volume_last) {
        g_tf_system_volume_last = raw;
        ++g_tf_volume_event_counter;
        int pct = tf_percent_from_raw_system_volume(raw);
        char msg[96]; snprintf(msg, sizeof(msg), "system volume changed raw=%d percent=%d", raw, pct); tf_volume_log(msg);
        tf_volume_set_project_master(pct, "system_volume");
    }
#endif
}

static void tf_volume_parse_joykey_map_line(const char *line) {
    int b = -1;
    if (!line) return;
    if (sscanf(line, "UP=%d", &b)==1 || sscanf(line, "VOLUP=%d", &b)==1) g_tf_joykey_up_bit = b;
    b = -1;
    if (sscanf(line, "DOWN=%d", &b)==1 || sscanf(line, "VOLDOWN=%d", &b)==1) g_tf_joykey_down_bit = b;
}

static void tf_volume_load_joykey_map_once(void) {
    static int loaded = 0; if (loaded) return; loaded = 1;
    char buf[512];
    int fd = open("/mnt/sdcard/lgpt/otg/physical_volume_joykey_map", O_RDONLY);
    if (fd < 0) { tf_volume_log("joykey map missing; probe-only mode"); return; }
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) { tf_volume_log("joykey map empty/unreadable; probe-only mode"); return; }
    buf[n] = 0;
    char *line = buf;
    while (line && *line) {
        char *end = strchr(line, '\n');
        if (end) *end = 0;
        tf_volume_parse_joykey_map_line(line);
        if (!end) break;
        line = end + 1;
    }
    char msg[96]; snprintf(msg, sizeof(msg), "joykey map loaded up=%d down=%d", g_tf_joykey_up_bit, g_tf_joykey_down_bit); tf_volume_log(msg);
}

static int tf_one_bit_index(uint32_t v) {
    if (!v || (v & (v - 1u))) return -1;
    for (int i = 0; i < 32; ++i) if (v & (1u << i)) return i;
    return -1;
}

static void tf_volume_apply_joykey_bit(int bit) {
    if (bit < 0 || bit >= 32) return;
    if (g_tf_joykey_up_bit >= 0 && bit == g_tf_joykey_up_bit) {
        ++g_tf_volume_event_counter;
        tf_volume_adjust_project_master(+TREEFROG_PHYSICAL_VOLUME_STEP);
        return;
    }
    if (g_tf_joykey_down_bit >= 0 && bit == g_tf_joykey_down_bit) {
        ++g_tf_volume_event_counter;
        tf_volume_adjust_project_master(-TREEFROG_PHYSICAL_VOLUME_STEP);
        return;
    }
    if (!g_tf_auto_learning_enabled) return;

    /* AU9V auto-learn mode. Test protocol requires pressing VOL+ first and VOL- second.
     * This is intentionally runtime-only; a permanent map can still be written to
     * /mnt/sdcard/lgpt/otg/physical_volume_joykey_map once bits are known.
     */
    if (g_tf_auto_up_bit < 0) {
        g_tf_auto_up_bit = bit;
        char msg[128]; snprintf(msg, sizeof(msg), "joykey auto-learn VOLUP bit=%d", bit); tf_volume_log(msg);
        ++g_tf_volume_event_counter;
        tf_volume_adjust_project_master(+TREEFROG_PHYSICAL_VOLUME_STEP);
        return;
    }
    if (bit == g_tf_auto_up_bit) {
        ++g_tf_volume_event_counter;
        tf_volume_adjust_project_master(+TREEFROG_PHYSICAL_VOLUME_STEP);
        return;
    }
    if (g_tf_auto_down_bit < 0) {
        g_tf_auto_down_bit = bit;
        char msg[128]; snprintf(msg, sizeof(msg), "joykey auto-learn VOLDOWN bit=%d", bit); tf_volume_log(msg);
        ++g_tf_volume_event_counter;
        tf_volume_adjust_project_master(-TREEFROG_PHYSICAL_VOLUME_STEP);
        return;
    }
    if (bit == g_tf_auto_down_bit) {
        ++g_tf_volume_event_counter;
        tf_volume_adjust_project_master(-TREEFROG_PHYSICAL_VOLUME_STEP);
        return;
    }
}

static void tf_volume_poll_joykey_raw(uint32_t cv_full) {
#if TREEFROG_PHYSICAL_VOLUME_JOYKEY_PROBE
    tf_volume_load_joykey_map_once();

    if (g_tf_joykey_last == 0xffffffffu) {
        g_tf_joykey_last = cv_full;
        char msg[128]; snprintf(msg, sizeof(msg), "joykey baseline word0=0x%08x", (unsigned)cv_full); tf_volume_log(msg);
    }

    if (cv_full != g_tf_joykey_last) {
        uint32_t changed = cv_full ^ g_tf_joykey_last;
        uint32_t rising = cv_full & changed;
        uint32_t unknown = changed & ~g_tf_joykey_known_mask;
        char msg[192]; snprintf(msg, sizeof(msg), "joykey word0 changed cv=0x%08x changed=0x%08x rising=0x%08x unknown=0x%08x", (unsigned)cv_full, (unsigned)changed, (unsigned)rising, (unsigned)unknown); tf_volume_log(msg);
        int bit = tf_one_bit_index(rising & ~g_tf_joykey_known_mask);
        if (bit >= 0) tf_volume_apply_joykey_bit(bit);
        if (g_tf_joykey_up_bit >= 0 && (rising & (1u << g_tf_joykey_up_bit))) { ++g_tf_volume_event_counter; tf_volume_adjust_project_master(+TREEFROG_PHYSICAL_VOLUME_STEP); }
        if (g_tf_joykey_down_bit >= 0 && (rising & (1u << g_tf_joykey_down_bit))) { ++g_tf_volume_event_counter; tf_volume_adjust_project_master(-TREEFROG_PHYSICAL_VOLUME_STEP); }
        g_tf_joykey_last = cv_full;
    }

    /* AU9V: /tmp/joy_key is System V shared memory, not a text file.
     * Probe more than the first word when cubevol allocates a larger segment.
     * Some stock builds keep volume/OSD state outside the 16-bit button word.
     */
    if (cv_keys && cv_shmid >= 0) {
        struct shmid_ds ds;
        int words = 1;
        if (shmctl(cv_shmid, IPC_STAT, &ds) == 0) {
            words = (int)(ds.shm_segsz / 4);
            if (words < 1) words = 1;
            if (words > 16) words = 16;
        }
        if (!g_tf_shm_words_logged) {
            g_tf_shm_words_logged = 1;
            char msg[128]; snprintf(msg, sizeof(msg), "joykey shm attached shmid=%d words=%d", cv_shmid, words); tf_volume_log(msg);
        }
        volatile uint32_t *base = (volatile uint32_t *)cv_keys;
        if (!g_tf_shm_last_valid) {
            for (int i = 0; i < words; ++i) g_tf_shm_last_words[i] = base[i];
            g_tf_shm_last_valid = 1;
            return;
        }
        for (int i = 0; i < words; ++i) {
            uint32_t now = base[i];
            uint32_t old = g_tf_shm_last_words[i];
            if (now == old) continue;
            uint32_t changed = now ^ old;
            uint32_t rising = now & changed;
            char msg[192]; snprintf(msg, sizeof(msg), "joykey shm word%d old=0x%08x new=0x%08x changed=0x%08x rising=0x%08x", i, (unsigned)old, (unsigned)now, (unsigned)changed, (unsigned)rising); tf_volume_log(msg);
            if (i == 0) {
                int bit = tf_one_bit_index(rising & ~g_tf_joykey_known_mask);
                if (bit >= 0) tf_volume_apply_joykey_bit(bit);
            } else {
                /* If a secondary word behaves like a small stock-volume scalar, mirror it to Project > Master. */
                if (now <= 100 && old <= 100 && changed != 0) {
                    int pct = tf_percent_from_raw_system_volume((int)now);
                    char msg2[128]; snprintf(msg2, sizeof(msg2), "joykey shm word%d scalar volume=%u pct=%d", i, (unsigned)now, pct); tf_volume_log(msg2);
                    ++g_tf_volume_event_counter;
                    tf_volume_set_project_master(pct, "joykey_shm_scalar");
                }
            }
            g_tf_shm_last_words[i] = now;
        }
    }
#endif
}

static void tf_volume_poll_all(uint32_t cv_full) {
    tf_volume_poll_evdev();
    if ((g_tf_volume_scan_counter % 6) == 0) tf_volume_poll_oss_mixer();
    if ((g_tf_volume_scan_counter % 6) == 0) tf_volume_poll_system_raw();
    tf_volume_poll_joykey_raw(cv_full);
}

static void tf_volume_close_evdev(void) {
    for (int i = 0; i < 32; ++i) { if (g_tf_volume_fds[i] >= 0) close(g_tf_volume_fds[i]); g_tf_volume_fds[i] = -1; }
    if (g_tf_oss_mixer_fd >= 0) { close(g_tf_oss_mixer_fd); g_tf_oss_mixer_fd = -1; }
    g_tf_volume_fds_init = 0;
}
#else
static void tf_volume_poll_all(uint32_t cv_full) { (void)cv_full; }
static void tf_volume_close_evdev(void) {}
#endif

static void poll_input(void) {
    if (!app_ready) return;
    if (input_poll_cb) input_poll_cb();
    cv_init();

    uint32_t cv_full = cv_keys ? *cv_keys : 0;
    tf_volume_poll_all(cv_full);
    uint32_t cv = cv_full & 0xffff;
    uint32_t lr_mask = 0;
    uint32_t phys = read_physical_mask(cv, &lr_mask);

    log_input_change(cv, lr_mask, phys);
    map_physical_to_lgpt(phys);
    dispatch_outputs();
    dispatch_modifier_retrigger(phys);
    dispatch_face_taps(phys);
}

static void stop_audio_and_clear(void) {
    TreeFrogAudioDriver *drv = TreeFrogGetAudioDriver();
    if (drv) drv->ResetPlaybackState();
    memset(audio_buffer, 0, sizeof(audio_buffer));
    audio_accum = 0.0;
}

static void reset_runtime_state(bool send_releases) {
    reset_input_latches(send_releases);
    stop_audio_and_clear();
    TreeFrogSetQuitRequested(false);
}

static void draw_probe_digit(uint16_t *dst, int ox, int oy, const unsigned char rows[7], uint16_t color) {
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 5; ++x) {
            if ((rows[y] >> (4 - x)) & 1) {
                int px = ox + x;
                int py = oy + y;
                if (px >= 0 && py >= 0 && px < TREEFROG_LGPT_WIDTH && py < TREEFROG_LGPT_HEIGHT) {
                    dst[py * TREEFROG_LGPT_WIDTH + px] = color;
                }
            }
        }
    }
}

static void draw_video_probe_overlay(uint16_t *dst) {
#if TREEFROG_VIDEO_PROBE
    const uint16_t white = 0xffff;
    const uint16_t red   = 0xf800;
    const uint16_t green = 0x07e0;
    const uint16_t mag   = 0xf81f;
    const uint16_t cyan  = 0x07ff;
    const uint16_t black = 0x0000;

    /* Full logical-frame border. If the right border is absent on device, the
     * frontend/display path is cropping the logical 320px image. */
    for (int x = 0; x < TREEFROG_LGPT_WIDTH; ++x) {
        dst[0 * TREEFROG_LGPT_WIDTH + x] = white;
        dst[(TREEFROG_LGPT_HEIGHT - 1) * TREEFROG_LGPT_WIDTH + x] = white;
    }
    for (int y = 0; y < TREEFROG_LGPT_HEIGHT; ++y) {
        dst[y * TREEFROG_LGPT_WIDTH + 0] = white;
        dst[y * TREEFROG_LGPT_WIDTH + (TREEFROG_LGPT_WIDTH - 1)] = red;
        dst[y * TREEFROG_LGPT_WIDTH + 239] = mag;
        dst[y * TREEFROG_LGPT_WIDTH + 279] = green;
    }

    /* Small unavoidable marker in top-left so the tester knows the probe build
     * is actually running. It draws block text V15. */
    for (int y = 2; y < 14; ++y) for (int x = 2; x < 34; ++x) dst[y * TREEFROG_LGPT_WIDTH + x] = black;
    static const unsigned char V[7] = {0x11,0x11,0x11,0x11,0x11,0x0a,0x04};
    static const unsigned char one[7] = {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e};
    static const unsigned char five[7] = {0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e};
    draw_probe_digit(dst, 4, 4, V, cyan);
    draw_probe_digit(dst, 12, 4, one, cyan);
    draw_probe_digit(dst, 20, 4, five, cyan);
#else
    (void)dst;
#endif
}

static void draw_badge_glyph(uint16_t *dst, int ox, int oy, const unsigned char rows[7], uint16_t color) {
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 5; ++x) {
            if ((rows[y] >> (4 - x)) & 1) {
                int px = ox + x;
                int py = oy + y;
                if (px >= 0 && py >= 0 && px < TREEFROG_LGPT_WIDTH && py < TREEFROG_LGPT_HEIGHT) {
                    dst[py * TREEFROG_LGPT_WIDTH + px] = color;
                }
            }
        }
    }
}

static void draw_port_version_badge(uint16_t *dst) {
    // Visible version badges are disabled in this development line to avoid
    // double build labels in the upper-right corner. Functional validation is
    // now done through Mixer behavior and build logs.
    (void)dst;
}

static uint16_t *make_video_output(uint16_t *src, unsigned *w, unsigned *h, size_t *pitch) {
    draw_video_probe_overlay(src);
    draw_port_version_badge(src);
#if TREEFROG_VIDEO_MODE == 1
    for (unsigned y = 0; y < 240; ++y) {
        for (unsigned x = 0; x < 240; ++x) {
            unsigned sx = (x * TREEFROG_LGPT_WIDTH) / 240;
            video_squeeze_buffer[y * 240 + x] = src[y * TREEFROG_LGPT_WIDTH + sx];
        }
    }
    *w = 240;
    *h = 240;
    *pitch = 240 * sizeof(uint16_t);
    return video_squeeze_buffer;
#elif TREEFROG_VIDEO_MODE == 2
    for (unsigned y = 0; y < 240; ++y) {
        for (unsigned x = 0; x < 240; ++x) {
            unsigned sx = 80 + x;
            video_right_probe_buffer[y * 240 + x] = src[y * TREEFROG_LGPT_WIDTH + sx];
        }
    }
    *w = 240;
    *h = 240;
    *pitch = 240 * sizeof(uint16_t);
    return video_right_probe_buffer;
#else
    *w = TREEFROG_LGPT_WIDTH;
    *h = TREEFROG_LGPT_HEIGHT;
    *pitch = TREEFROG_LGPT_WIDTH * sizeof(uint16_t);
    return src;
#endif
}


static unsigned long treefrog_v11_frame_counter = 0;
#if TREEFROG_INPUT_DEBUG
#define TREEFROG_V133_RETRO_TRACE 1
static void v11_log_retro(const char *msg) {
    FILE *f = fopen("/mnt/sdcard/lgpt/retro_debug.log", "a");
    if (!f) return;
    fprintf(f, "%lu %s\n", treefrog_v11_frame_counter, msg ? msg : "retro");
    fclose(f);
}
#else
static void v11_log_retro(const char *msg) { (void)msg; }
#endif


static const char *treefrog_v40_source_rooted_marker = "TREEFROG_PORT_LGPT_TRACKER_V4_0_SOURCE_ROOTED";
extern "C" {

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;
    enum { RETRO_PIXEL_FORMAT_RGB565_LOCAL = RETRO_PIXEL_FORMAT_RGB565 };
    unsigned fmt = RETRO_PIXEL_FORMAT_RGB565_LOCAL;
    if (environ_cb) environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
    bool no_game = true;
    if (environ_cb) environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }

void TreeFrogForceVideoRefresh(void) {
    if (!video_cb) return;
    unsigned vw, vh; size_t vp;
    uint16_t *vf = make_video_output(TreeFrogGetFramebuffer(), &vw, &vh, &vp);
    video_cb(vf, vw, vh, vp);
}
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name = "Little Piggy Tracker";
    info->library_version = "PORT LGPT TRACKER V4.0 SourceRooted";
    info->valid_extensions = "lgpt|xml|txt";
    info->need_fullpath = true;
    info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    memset(info, 0, sizeof(*info));
#if TREEFROG_VIDEO_MODE == 1 || TREEFROG_VIDEO_MODE == 2
    info->geometry.base_width = 240;
    info->geometry.base_height = 240;
    info->geometry.max_width = 240;
    info->geometry.max_height = 240;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
#else
    info->geometry.base_width = TREEFROG_LGPT_WIDTH;
    info->geometry.base_height = TREEFROG_LGPT_HEIGHT;
    info->geometry.max_width = TREEFROG_LGPT_WIDTH;
    info->geometry.max_height = TREEFROG_LGPT_HEIGHT;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
#endif
    info->timing.fps = 60.0;
    info->timing.sample_rate = 44100.0;
}



// Optional lifecycle diagnostics for launcher/re-entry investigations.
// Keep disabled in stable builds to avoid repeated SD-card writes during normal use.
static char g_treefrog_v40_game_path[1024];
static unsigned g_treefrog_v40_run_count = 0;

#if TREEFROG_RETRO_LIFECYCLE_DEBUG
static void TreeFrogV51Log(const char *where, const char *detail) {
    FILE *fp = fopen("/mnt/sdcard/lgpt/reentry_debug.log", "a");
    if (fp) {
        fprintf(fp, "TREEFROG_LIFECYCLE: %s %s\n", where ? where : "unknown", detail ? detail : "");
        fclose(fp);
    }
}

static int TreeFrogV51IsDir(const char *path) {
    struct stat st;
    return (path && stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static int TreeFrogV51IsRegularFile(const char *path) {
    struct stat st;
    return (path && stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static void TreeFrogV51LogProjectRoot(const char *where) {
    FILE *fp = fopen("/mnt/sdcard/lgpt/reentry_debug.log", "a");
    if (!fp) return;
    fprintf(fp, "TREEFROG_LIFECYCLE: project-root %s\n", where ? where : "unknown");

    FILE *lp = fopen("/mnt/sdcard/lgpt/last_project", "r");
    if (lp) {
        char buf[1024];
        if (fgets(buf, sizeof(buf), lp)) {
            buf[strcspn(buf, "\r\n")] = 0;
            fprintf(fp, "  last_project=%s exists=%d\n", buf, TreeFrogV51IsDir(buf));
        } else {
            fprintf(fp, "  last_project=<empty>\n");
        }
        fclose(lp);
    } else {
        fprintf(fp, "  last_project=<missing>\n");
    }

    DIR *d = opendir("/mnt/sdcard/lgpt/projects");
    if (!d) {
        fprintf(fp, "  projects: opendir failed\n");
        fclose(fp);
        return;
    }
    struct dirent *de;
    while ((de = readdir(d)) != 0) {
        if (!de->d_name || de->d_name[0] == '.') continue;
        char path[1536];
        char save[1536];
        snprintf(path, sizeof(path), "/mnt/sdcard/lgpt/projects/%s", de->d_name);
        snprintf(save, sizeof(save), "%s/lgptsav.dat", path);
        struct stat st;
        if (stat(path, &st) == 0) {
            fprintf(fp, "  entry=%s type=%s size=%ld has_lgptsav=%d\n", de->d_name, S_ISDIR(st.st_mode) ? "dir" : "file", (long)st.st_size, TreeFrogV51IsRegularFile(save));
        } else {
            fprintf(fp, "  entry=%s stat=failed\n", de->d_name);
        }
    }
    closedir(d);
    fclose(fp);
}
#else
static void TreeFrogV51Log(const char *, const char *) {}
static void TreeFrogV51LogProjectRoot(const char *) {}
#endif


void retro_init(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
    memset(audio_buffer, 0, sizeof(audio_buffer));
    reset_runtime_state(false);
}

bool retro_load_game(const struct retro_game_info *game) {
    g_treefrog_v40_game_path[0] = '\0';
    if (game && game->path) {
        snprintf(g_treefrog_v40_game_path, sizeof(g_treefrog_v40_game_path), "%s", game->path);
        g_treefrog_v40_game_path[sizeof(g_treefrog_v40_game_path) - 1] = '\0';
    }
    g_treefrog_v40_run_count = 0;
    TreeFrogV51Log("retro_load_game.enter", g_treefrog_v40_game_path);
    TreeFrogV51LogProjectRoot("retro_load_game.before-ui");

    const char *path = game ? game->path : 0;
    truncate_debug_logs();
    reset_runtime_state(false);
    TreeFrogSystem::Boot(path);

    TreeFrogCreateWindowParams params;
    params.framebuffer_ = framebuffer;
    app_ready = Application::GetInstance()->Init(params);
    return app_ready;
}

void retro_unload_game(void) {
    TreeFrogV51Log("retro_unload_game.enter", g_treefrog_v40_game_path);
    TreeFrogV51LogProjectRoot("retro_unload_game.enter");

    reset_runtime_state(true);
    app_ready = false;
    tf_volume_close_evdev();
    close_input_log();
    cv_detach();
}

void retro_deinit(void) {
    TreeFrogV51Log("retro_deinit.enter", g_treefrog_v40_game_path);
    TreeFrogV51LogProjectRoot("retro_deinit.enter");

    reset_runtime_state(false);
    app_ready = false;
    tf_volume_close_evdev();
    close_input_log();
    cv_detach();
}

void retro_run(void) {
    ++treefrog_v11_frame_counter;
    if (g_treefrog_v40_run_count < 32) {
        char msg[128];
        snprintf(msg, sizeof(msg), "frame=%u path=%s", g_treefrog_v40_run_count, g_treefrog_v40_game_path);
        TreeFrogV51Log("retro_run.enter", msg);
    }
    ++g_treefrog_v40_run_count;

    if (!app_ready) {
        memset(audio_buffer, 0, 735 * 2 * sizeof(int16_t));
        if (audio_batch_cb) audio_batch_cb(audio_buffer, 735);
        if (video_cb) {
            unsigned vw, vh; size_t vp;
            uint16_t *vf = make_video_output(framebuffer, &vw, &vh, &vp);
            video_cb(vf, vw, vh, vp);
        }
        return;
    }

    poll_input();

    TreeFrogEventManager::GetInstance()->Flush();

    TreeFrogTimerService *ts = TreeFrogGetTimerService();
    if (ts) ts->Tick();

    GUIWindow *w = Application::GetInstance()->GetWindow();
    if (w) w->Update();

    audio_accum += 44100.0 / 60.0;
    int frames = (int)audio_accum;
    audio_accum -= frames;
    if (frames < 0) frames = 0;
    if (frames > 2048) frames = 2048;

    if ((treefrog_v11_frame_counter % 60) == 0) v11_log_retro("retro.audio.render.enter");
    TreeFrogAudioDriver *drv = TreeFrogGetAudioDriver();
    if (drv) drv->Render(audio_buffer, frames);
    else memset(audio_buffer, 0, frames * 2 * sizeof(int16_t));

    if ((treefrog_v11_frame_counter % 60) == 0) v11_log_retro("retro.audio.render.leave");
    if (audio_batch_cb && frames > 0) audio_batch_cb(audio_buffer, frames);
    if (video_cb) {
        unsigned vw, vh; size_t vp;
        uint16_t *vf = make_video_output(TreeFrogGetFramebuffer(), &vw, &vh, &vp);
        video_cb(vf, vw, vh, vp);
    }

    if (TreeFrogQuitRequested() && environ_cb) {
        environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0);
    }
}

void retro_reset(void) {
    reset_runtime_state(true);
}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void *retro_get_memory_data(unsigned id) { (void)id; return 0; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) { (void)type; (void)info; (void)num; return false; }

} /* extern C */
