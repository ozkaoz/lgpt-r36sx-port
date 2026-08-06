#include "Adapters/TREEFROG/Libretro/libretro.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "Adapters/TREEFROG/Audio/TreeFrogAudioDriver.h"
#include "Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.h"
#include "Adapters/TREEFROG/GUI/TreeFrogEventManager.h"
#include "Adapters/TREEFROG/GUI/TreeFrogGUIWindowImp.h"
#include "Adapters/TREEFROG/Main/TreeFrogSamplerInput.h"
#include "Adapters/TREEFROG/System/TreeFrogSystem.h"
#include "Adapters/TREEFROG/Timer/TreeFrogTimer.h"
#include "Application/Application.h"
#include "System/System/System.h"

extern "C" void TreeFrogAppWindow_SynchronizeInputMask(
    unsigned short mask);
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

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
#define TREEFROG_PORT_VERSION_BADGE_COLOR 0x9dbf
#endif

#ifndef TREEFROG_RETRO_LIFECYCLE_DEBUG
#define TREEFROG_RETRO_LIFECYCLE_DEBUG 0
#endif

static retro_environment_t environ_cb = 0;
static retro_video_refresh_t video_cb = 0;
static retro_audio_sample_batch_t audio_batch_cb = 0;
static retro_input_poll_t input_poll_cb = 0;
static retro_input_state_t input_state_cb = 0;
static bool g_inputBitmaskSupported = false;
static uint16_t g_currentJoypadBits = 0;
static uint32_t g_currentPhysicalSnapshot = 0;
static unsigned long g_inputSnapshotSequence = 0;
static unsigned long g_sequentialSnapshotMismatchCount = 0;

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
static unsigned long audio_budget_last_ms = 0;
static uint32_t last_phys_for_taps = 0;
static uint32_t last_phys_for_combo = 0;

static unsigned long treefrog_v11_frame_counter = 0;

/*
 * TREEFROG_BOOT_DIAG (Bacon 1.1.1 V17-diagnostico): boot-stage marker log
 * persisted to the SD so a device hang at startup leaves evidence.  The
 * frontend discards /tmp on power-off, so the core writes one line per boot
 * stage to /mnt/sdcard/LGPT_OTG_LOGS/boot_debug.log.  Pure diagnostics: it
 * does not change the audio driver, the input path or any view logic.
 */
static void boot_diag_log(const char *stage) {
    FILE *f = fopen("/mnt/sdcard/LGPT_OTG_LOGS/boot_debug.log", "a");
    if (!f) return;
    fprintf(f, "%lu BOOTDIAG %s\n", treefrog_v11_frame_counter, stage);
    fclose(f);
}

/*
 * U2.45.0 SAMPLER_EXCLUSIVE_SOURCE_NO_DEBOUNCE
 *
 * Normal LGPT views keep the original merged Cubevol/libretro path. The
 * sampler temporarily selects Cubevol as the only input source when shared
 * memory is available, with libretro as an immediate fallback.
 */
static bool g_samplerExclusiveInput = false;

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
static uint16_t read_libretro_sequential_bits_once(void) {
    if (!input_state_cb) return 0;
    uint16_t bits = 0;
    for (unsigned id = 0; id < 16; ++id) {
        if (input_state_cb(
                0,
                RETRO_DEVICE_JOYPAD,
                0,
                id)) {
            bits |= (uint16_t)(1u << id);
        }
    }
    return bits;
}

static uint16_t read_libretro_coherent_bits(void) {
    if (!input_state_cb) return 0;

    if (g_inputBitmaskSupported) {
        return (uint16_t)input_state_cb(
            0,
            RETRO_DEVICE_JOYPAD,
            0,
            RETRO_DEVICE_ID_JOYPAD_MASK);
    }

    const uint16_t first =
        read_libretro_sequential_bits_once();
    const uint16_t second =
        read_libretro_sequential_bits_once();

    if (first == second)
        return second;

    const uint16_t third =
        read_libretro_sequential_bits_once();

    ++g_sequentialSnapshotMismatchCount;

    if (second == third)
        return third;
    if (first == third)
        return third;

    return g_currentJoypadBits;
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

/*
 * U2.51.2 U2506_INPUT_STABILITY_BASELINE
 *
 * There is one canonical physical source and one complete logical mask.
 * The old per-button output-state table was removed because it could dispatch
 * a new press before dispatching an old release, creating impossible chords
 * such as A+X or B+R that were never physically held together.
 */
/*
 * g_lastLogicalMask is the latest physical logical snapshot.
 * g_deliveredLogicalMask is the state already emitted to the UI queue.
 */
static unsigned short g_lastLogicalMask = 0;
static unsigned short g_deliveredLogicalMask = 0;
static unsigned short g_pendingDualRoleTap = 0;
static unsigned short g_consumedDualRoleMask = 0;
static uint32_t g_lastCanonicalPhysicalMask = 0;
static unsigned long g_inputPollSequence = 0;

static void reset_input_latches(bool send_releases) {
    TreeFrogEventManager *eventManager =
        TreeFrogEventManager::GetInstance();

    if (send_releases &&
        g_deliveredLogicalMask != 0 &&
        eventManager) {
        eventManager->PushMask(0, false);
        eventManager->Flush();
    }

    if (eventManager)
        eventManager->ClearQueue();

    g_lastLogicalMask = 0;
    g_deliveredLogicalMask = 0;
    g_pendingDualRoleTap = 0;
    g_consumedDualRoleMask = 0;
    g_lastCanonicalPhysicalMask = 0;
    g_currentJoypadBits = 0;
    g_currentPhysicalSnapshot = 0;
    TreeFrogAppWindow_SynchronizeInputMask(0);
    last_phys_for_taps = 0;
    last_phys_for_combo = 0;
}

static uint32_t read_cubevol_physical_mask(uint32_t cv) {
    uint32_t mask = 0;
    for (unsigned i = 0;
         i < sizeof(physical_sources) / sizeof(physical_sources[0]);
         ++i) {
        if (cv_btn(cv, physical_sources[i].cv))
            mask |= PHY_BIT(physical_sources[i].phy);
    }
    return mask;
}

static uint32_t physical_mask_from_joypad_bits(
    uint16_t joypadBits) {
    uint32_t mask = 0;
    for (unsigned i = 0;
         i < sizeof(physical_sources) / sizeof(physical_sources[0]);
         ++i) {
        const unsigned id = physical_sources[i].lr;
        if (id < 16u &&
            (joypadBits & (uint16_t)(1u << id))) {
            mask |= PHY_BIT(physical_sources[i].phy);
        }
    }
    return mask;
}

static uint32_t read_physical_mask(
    uint32_t cv,
    uint16_t joypadBits,
    uint32_t *lr_mask_out) {
    const uint32_t cubevolMask =
        cv_keys ? read_cubevol_physical_mask(cv) : 0u;
    const uint32_t libretroMask =
        physical_mask_from_joypad_bits(joypadBits);

    if (lr_mask_out)
        *lr_mask_out = libretroMask;

    return input_state_cb ? libretroMask : cubevolMask;
}

extern "C" void TreeFrogSamplerInput_SetExclusiveMode(int enable) {
    g_samplerExclusiveInput = enable != 0;
}

extern "C" int TreeFrogSamplerInput_GetExclusiveMode(void) {
    return g_samplerExclusiveInput ? 1 : 0;
}

extern "C" void TreeFrogSamplerInput_Read(
    TreeFrogSamplerInputSnapshot *snapshot) {
    if (!snapshot) return;

    memset(snapshot, 0, sizeof(*snapshot));
    cv_init();

    const uint32_t cv =
        cv_keys ? (*cv_keys & 0xffffu) : 0u;

    snapshot->cubevolRaw = cv;
    snapshot->cubevolAvailable = cv_keys ? 1 : 0;
    snapshot->libretroAvailable = input_state_cb ? 1 : 0;
    snapshot->cubevolPhysical =
        snapshot->cubevolAvailable
            ? read_cubevol_physical_mask(cv)
            : 0u;
    snapshot->libretroPhysical =
        g_currentPhysicalSnapshot;
    snapshot->joypadBits =
        g_currentJoypadBits;
    snapshot->snapshotSequence =
        g_inputSnapshotSequence;
    snapshot->bitmaskSupported =
        g_inputBitmaskSupported ? 1 : 0;
    snapshot->sequentialMismatchCount =
        g_sequentialSnapshotMismatchCount;
    snapshot->exclusiveMode =
        g_samplerExclusiveInput ? 1 : 0;
    snapshot->selectedSource =
        snapshot->libretroAvailable
            ? TFSS_LIBRETRO
            : TFSS_CUBEVOL;
    snapshot->selectedPhysical =
        snapshot->libretroAvailable
            ? snapshot->libretroPhysical
            : snapshot->cubevolPhysical;
}

extern "C" const char *TreeFrogSamplerInput_BuildMarker(void) {
    return
        "U2510_CHORD_AWARE_INPUT_GLOBAL_CHOP_STEREO "
        "U2510_SELECT_R2_BRANCH_COMPILES";
}

#if TREEFROG_ENABLE_SELECT
extern "C"
__attribute__((used, visibility("default")))
const char *TreeFrogU2510GlobalChopStereoBuildMarker(void) {
    return "U2510_SELECT_R2_BRANCH_COMPILES U2510_CHOPPER_XY_L2_RESTORED U2510_JOYPAD_BITMASK_OR_COHERENT_FALLBACK U2510_CHORD_AWARE_DUAL_ROLE_TAPS U2510_ADAPTER_OWNED_VIEW_TRACE U2510_GLOBAL_CHOP_HISTORY U2510_GLOBAL_CHOPPER_HISTORY_24_OVERLAY_SAFE U2510_STEREO_48K_DYNAMIC_PROFILE U2510_RATE_CORRECT_FILE_STREAMER";
}
#endif

static unsigned short map_physical_to_lgpt(
    uint32_t physicalMask) {
    unsigned short logicalMask = 0;

    if (physicalMask & PHY_BIT(PHY_LEFT))
        logicalMask |= (1u << EPBT_LEFT);
    if (physicalMask & PHY_BIT(PHY_RIGHT))
        logicalMask |= (1u << EPBT_RIGHT);
    if (physicalMask & PHY_BIT(PHY_UP))
        logicalMask |= (1u << EPBT_UP);
    if (physicalMask & PHY_BIT(PHY_DOWN))
        logicalMask |= (1u << EPBT_DOWN);
    if (physicalMask & PHY_BIT(PHY_A))
        logicalMask |= (1u << EPBT_A);
    if (physicalMask & PHY_BIT(PHY_B))
        logicalMask |= (1u << EPBT_B);
    if (physicalMask & PHY_BIT(PHY_L1))
        logicalMask |= (1u << EPBT_L);
    if (physicalMask & PHY_BIT(PHY_R1))
        logicalMask |= (1u << EPBT_R);
    if (physicalMask & PHY_BIT(PHY_X))
        logicalMask |= (1u << EPBT_X);
    if (physicalMask & PHY_BIT(PHY_Y))
        logicalMask |= (1u << EPBT_Y);
    if (physicalMask & PHY_BIT(PHY_L2))
        logicalMask |= (1u << EPBT_L2);

#if TREEFROG_ENABLE_START
    if (physicalMask & PHY_BIT(PHY_START))
        logicalMask |= (1u << EPBT_START);
#endif

#if TREEFROG_ENABLE_SELECT
    if (physicalMask & PHY_BIT(PHY_SELECT))
        logicalMask |= (1u << EPBT_SELECT);
    if (physicalMask & PHY_BIT(PHY_R2))
        logicalMask |= (1u << EPBT_R2);
#endif

    /*
     * X, Y and L2 are part of the existing LGPT/Chopper contract and must be
     * delivered. FN remains diagnostic-only because there is no EPBT_FN.
     */
    return logicalMask;
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
        "/tmp/r36sx_lgpt_logs/input_debug.log",
        "/tmp/r36sx_lgpt_logs/input_semantics.log",
        "/tmp/r36sx_lgpt_logs/input_view.log",
        "/tmp/r36sx_lgpt_logs/event_debug.log",
        "/tmp/r36sx_lgpt_logs/audio_debug.log"
    };
    for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        FILE *f = fopen(paths[i], "w");
        if (f) fclose(f);
    }
    last_logged_phys = 0xffffffffu;
    last_logged_cv = 0xffffffffu;
    last_logged_lr = 0xffffffffu;
}

static void log_input_change(
    uint32_t cv,
    uint16_t joypadBits,
    uint32_t lr_mask,
    uint32_t phys,
    const char *selectedSource,
    const char *snapshotMode,
    unsigned long sequence) {
    if (phys == last_logged_phys && cv == last_logged_cv && lr_mask == last_logged_lr) return;
    last_logged_phys = phys;
    last_logged_cv = cv;
    last_logged_lr = lr_mask;

    if (!input_log) input_log = fopen("/tmp/r36sx_lgpt_logs/input_debug.log", "a");
    if (!input_log) return;

    fprintf(
        input_log,
        "seq=%lu clock=%lu source=%s snapshot=%s joy=0x%04x "
        "cv=0x%04x lr=0x%04x mismatches=%lu phys=",
        sequence,
        (unsigned long)System::GetInstance()->GetClock(),
        selectedSource ? selectedSource : "unknown",
        snapshotMode ? snapshotMode : "unknown",
        (unsigned int)joypadBits,
        (unsigned)(cv & 0xffff),
        (unsigned)lr_mask,
        g_sequentialSnapshotMismatchCount);
    append_button_names(input_log, phys);
    fprintf(input_log, " profile=%d\n", TREEFROG_INPUT_PROFILE);
    fflush(input_log);
}

static void log_semantic_state(
    const char *phase,
    unsigned short physicalMask,
    unsigned short emittedMask,
    int pressed) {
    FILE *file =
        fopen("/tmp/r36sx_lgpt_logs/input_semantics.log", "a");
    if (!file) return;
    fprintf(
        file,
        "%lu phase=%s physical=0x%04x delivered=0x%04x "
        "pending=0x%04x consumed=0x%04x emitted=0x%04x pressed=%d\n",
        (unsigned long)System::GetInstance()->GetClock(),
        phase ? phase : "state",
        (unsigned int)physicalMask,
        (unsigned int)g_deliveredLogicalMask,
        (unsigned int)g_pendingDualRoleTap,
        (unsigned int)g_consumedDualRoleMask,
        (unsigned int)emittedMask,
        pressed);
    fclose(file);
}

extern "C" void TreeFrogInputTrace_LogView(
    const char *phase,
    int viewType,
    int hasModal,
    unsigned short incomingMask,
    unsigned short activeMask,
    int pressed,
    int audioLatched) {
    FILE *file =
        fopen("/tmp/r36sx_lgpt_logs/input_view.log", "a");
    if (!file) return;
    fprintf(
        file,
        "%lu phase=%s view=%d modal=%d incoming=0x%04x "
        "active=0x%04x pressed=%d audioLatch=%d\n",
        (unsigned long)System::GetInstance()->GetClock(),
        phase ? phase : "view",
        viewType,
        hasModal,
        (unsigned int)incomingMask,
        (unsigned int)activeMask,
        pressed,
        audioLatched);
    fclose(file);
}
#else
static void close_input_log(void) {}
static void truncate_debug_logs(void) {}
static void log_input_change(
    uint32_t cv,
    uint16_t joypadBits,
    uint32_t lr_mask,
    uint32_t phys,
    const char *selectedSource,
    const char *snapshotMode,
    unsigned long sequence) {
    (void)cv;
    (void)joypadBits;
    (void)lr_mask;
    (void)phys;
    (void)selectedSource;
    (void)snapshotMode;
    (void)sequence;
}

static void log_semantic_state(
    const char *phase,
    unsigned short physicalMask,
    unsigned short emittedMask,
    int pressed) {
    (void)phase;
    (void)physicalMask;
    (void)emittedMask;
    (void)pressed;
}

extern "C" void TreeFrogInputTrace_LogView(
    const char *phase,
    int viewType,
    int hasModal,
    unsigned short incomingMask,
    unsigned short activeMask,
    int pressed,
    int audioLatched) {
    (void)phase;
    (void)viewType;
    (void)hasModal;
    (void)incomingMask;
    (void)activeMask;
    (void)pressed;
    (void)audioLatched;
}
#endif

static bool is_single_bit(unsigned short mask) {
    return mask != 0 &&
        (mask & (unsigned short)(mask - 1)) == 0;
}

static bool is_single_dual_role(unsigned short mask) {
    const unsigned short dualRoleMask =
        (1u << EPBT_A) | (1u << EPBT_B) | (1u << EPBT_X) | (1u << EPBT_Y);
    return is_single_bit(mask) &&
        (mask & dualRoleMask) != 0;
}

static void emit_logical_transition(
    TreeFrogEventManager *eventManager,
    unsigned short targetMask,
    long when) {
    if (!eventManager ||
        targetMask == g_deliveredLogicalMask)
        return;

    const unsigned short commonMask =
        (unsigned short)(
            g_deliveredLogicalMask & targetMask);

    if (commonMask != g_deliveredLogicalMask) {
        eventManager->PushMask(
            commonMask,
            false,
            when);
        log_semantic_state(
            "emit.release",
            g_lastLogicalMask,
            commonMask,
            0);
    }

    if (targetMask != commonMask) {
        eventManager->PushMask(
            targetMask,
            true,
            when);
        log_semantic_state(
            "emit.press",
            g_lastLogicalMask,
            targetMask,
            1);
    }

    g_deliveredLogicalMask = targetMask;
}

static void dispatch_atomic_logical_mask(
    unsigned short nextMask) {
    if (nextMask == g_lastLogicalMask)
        return;

    TreeFrogEventManager *eventManager =
        TreeFrogEventManager::GetInstance();

    const unsigned short previousPhysical =
        g_lastLogicalMask;
    g_lastLogicalMask = nextMask;

    if (!eventManager)
        return;

    const long when =
        System::GetInstance()->GetClock();

    const unsigned short dualRoleMask =
        (1u << EPBT_A) | (1u << EPBT_B) | (1u << EPBT_X) | (1u << EPBT_Y);

    /*
     * U2.51.2 U2506_CHORD_AWARE_DUAL_ROLE_INPUT
     *
     * A, B, X and Y have both standalone and modifier/chord meanings.
     * A standalone action is committed on physical release.  If another
     * button arrives while the dual-role button is held, only the complete
     * chord is emitted.  No timer, debounce or lockout is involved.
     */
    if (g_pendingDualRoleTap != 0) {
        const unsigned short pending =
            g_pendingDualRoleTap;

        if (nextMask == 0) {
            log_semantic_state(
                "tap.commit",
                nextMask,
                pending,
                1);
            emit_logical_transition(
                eventManager,
                pending,
                when);
            emit_logical_transition(
                eventManager,
                0,
                when);
            g_pendingDualRoleTap = 0;
            g_consumedDualRoleMask = 0;
            return;
        }

        if ((nextMask & pending) != 0 &&
            nextMask != pending) {
            g_pendingDualRoleTap = 0;
            g_consumedDualRoleMask |=
                (unsigned short)(
                    nextMask & dualRoleMask);
            log_semantic_state(
                "chord.commit",
                nextMask,
                nextMask,
                1);
            emit_logical_transition(
                eventManager,
                nextMask,
                when);
            return;
        }

        /*
         * The pending key was released while another independent control
         * became active. Commit the completed tap, then process the new state.
         */
        emit_logical_transition(
            eventManager,
            pending,
            when);
        emit_logical_transition(
            eventManager,
            0,
            when);
        g_pendingDualRoleTap = 0;
        g_consumedDualRoleMask = 0;
    }

    if (nextMask == 0) {
        emit_logical_transition(
            eventManager,
            0,
            when);
        g_consumedDualRoleMask = 0;
        return;
    }

    /*
     * A dual-role button that already participated in a chord must not become
     * a new standalone tap when the other chord buttons are released first.
     */
    if (is_single_dual_role(nextMask) &&
        (g_consumedDualRoleMask & nextMask) != 0) {
        log_semantic_state(
            "chord.remainder",
            nextMask,
            nextMask,
            0);
        emit_logical_transition(
            eventManager,
            nextMask,
            when);
        return;
    }

    /*
     * Any fresh standalone A/B/X/Y becomes a pending release-confirmed tap.
     * If another previously delivered control is being released in the same
     * transition, release it first without firing the new dual-role action.
     */
    if (is_single_dual_role(nextMask)) {
        if (g_deliveredLogicalMask != 0) {
            emit_logical_transition(
                eventManager,
                0,
                when);
        }

        g_pendingDualRoleTap = nextMask;
        log_semantic_state(
            "tap.pending",
            nextMask,
            0,
            0);
        return;
    }

    if ((nextMask & (unsigned short)(nextMask - 1)) != 0) {
        g_consumedDualRoleMask |=
            (unsigned short)(
                nextMask & dualRoleMask);
    }

    emit_logical_transition(
        eventManager,
        nextMask,
        when);

    (void)previousPhysical;
}

static void poll_input(void) {
    if (!app_ready)
        return;

    if (input_poll_cb)
        input_poll_cb();

    cv_init();

    const uint32_t cubevolRaw =
        cv_keys ? (*cv_keys & 0xffffu) : 0u;

    const uint16_t joypadBits =
        read_libretro_coherent_bits();

    uint32_t libretroPhysical = 0;
    const uint32_t canonicalPhysical =
        read_physical_mask(
            cubevolRaw,
            joypadBits,
            &libretroPhysical);

    g_currentJoypadBits =
        joypadBits;
    g_currentPhysicalSnapshot =
        libretroPhysical;
    ++g_inputSnapshotSequence;
    ++g_inputPollSequence;

    log_input_change(
        cubevolRaw,
        joypadBits,
        libretroPhysical,
        canonicalPhysical,
        input_state_cb ? "libretro" : "cubevol-fallback",
        g_inputBitmaskSupported
            ? "joypad-bitmask"
            : "coherent-sequential-fallback",
        g_inputPollSequence);

    const unsigned short logicalMask =
        map_physical_to_lgpt(
            canonicalPhysical);

    TreeFrogAppWindow_SynchronizeInputMask(
        logicalMask);

    dispatch_atomic_logical_mask(
        logicalMask);

    g_lastCanonicalPhysicalMask =
        canonicalPhysical;
}

static void stop_audio_and_clear(void) {
    TreeFrogAudioDriver *drv = TreeFrogGetAudioDriver();
    if (drv) drv->ResetPlaybackState();
    memset(audio_buffer, 0, sizeof(audio_buffer));
    audio_accum = 0.0;
    audio_budget_last_ms = 0;
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


#if TREEFROG_INPUT_DEBUG
#define TREEFROG_V133_RETRO_TRACE 1
static void v11_log_retro(const char *msg) {
    FILE *f = fopen("/tmp/r36sx_lgpt_logs/retro_debug.log", "a");
    if (!f) return;
    fprintf(f, "%lu %s\n", treefrog_v11_frame_counter, msg ? msg : "retro");
    fclose(f);
}
#else
static void v11_log_retro(const char *msg) { (void)msg; }
#endif


static const char *treefrog_v40_source_rooted_marker = "TREEFROG_R36SX_U2_36_1_PRE_OTG_BUGFIX";
extern "C" {

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;
    enum { RETRO_PIXEL_FORMAT_RGB565_LOCAL = RETRO_PIXEL_FORMAT_RGB565 };
    unsigned fmt = RETRO_PIXEL_FORMAT_RGB565_LOCAL;
    if (environ_cb) environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
    bool no_game = true;
    if (environ_cb) environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);

    bool supportsInputBitmasks = false;
    if (environ_cb &&
        environ_cb(
            RETRO_ENVIRONMENT_GET_INPUT_BITMASKS,
            &supportsInputBitmasks)) {
        g_inputBitmaskSupported =
            supportsInputBitmasks;
    } else {
        g_inputBitmaskSupported = false;
    }
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
    info->library_version = "R36SX U2.41 WINDOWS CODE10 GLOBAL AUDIO FIX";
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
    info->timing.sample_rate = 48000.0;
}



// Optional lifecycle diagnostics for launcher/re-entry investigations.
// Keep disabled in stable builds to avoid repeated SD-card writes during normal use.
static char g_treefrog_v40_game_path[1024];
static unsigned g_treefrog_v40_run_count = 0;

#if TREEFROG_RETRO_LIFECYCLE_DEBUG
static void TreeFrogV51Log(const char *where, const char *detail) {
    FILE *fp = fopen("/tmp/r36sx_lgpt_logs/reentry_debug.log", "a");
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
    FILE *fp = fopen("/tmp/r36sx_lgpt_logs/reentry_debug.log", "a");
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
    boot_diag_log("retro_init.enter");
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
    boot_diag_log("retro_load_game.before-boot");
    TreeFrogSystem::Boot(path);
    boot_diag_log("retro_load_game.after-boot");

    TreeFrogCreateWindowParams params;
    params.framebuffer_ = framebuffer;
    boot_diag_log("retro_load_game.before-init");
    app_ready = Application::GetInstance()->Init(params);
    boot_diag_log(app_ready ? "retro_load_game.init-ok"
                            : "retro_load_game.init-failed");
    return app_ready;
}

void retro_unload_game(void) {
    TreeFrogV51Log("retro_unload_game.enter", g_treefrog_v40_game_path);
    TreeFrogV51LogProjectRoot("retro_unload_game.enter");

    reset_runtime_state(true);
    app_ready = false;
    close_input_log();
    cv_detach();
    /* H33: stop only after audio/input/video cleanup has completed. */
    TreeFrogUac2Bridge_MarkCoreUnloaded();
}

void retro_deinit(void) {
    TreeFrogV51Log("retro_deinit.enter", g_treefrog_v40_game_path);
    TreeFrogV51LogProjectRoot("retro_deinit.enter");

    reset_runtime_state(false);
    app_ready = false;
    close_input_log();
    cv_detach();
    /* H33: stop only after audio/input/video cleanup has completed. */
    TreeFrogUac2Bridge_MarkCoreUnloaded();
}

void retro_run(void) {
    ++treefrog_v11_frame_counter;
    if (g_treefrog_v40_run_count < 32) {
        char msg[128];
        snprintf(msg, sizeof(msg), "frame=%u path=%.96s", g_treefrog_v40_run_count, g_treefrog_v40_game_path);
        TreeFrogV51Log("retro_run.enter", msg);
    }
    ++g_treefrog_v40_run_count;

    if (!app_ready) {
        if (g_treefrog_v40_run_count < 4) {
            char m[72];
            snprintf(m, sizeof(m), "retro_run.not-ready n=%u", g_treefrog_v40_run_count);
            boot_diag_log(m);
        }
        memset(audio_buffer, 0, 800 * 2 * sizeof(int16_t));
        if (audio_batch_cb) audio_batch_cb(audio_buffer, 800);
        if (video_cb) {
            unsigned vw, vh; size_t vp;
            uint16_t *vf = make_video_output(framebuffer, &vw, &vh, &vp);
            video_cb(vf, vw, vh, vp);
        }
        audio_budget_last_ms = System::GetInstance()->GetClock();
        return;
    }

    poll_input();

    TreeFrogEventManager::GetInstance()->Flush();

    TreeFrogTimerService *ts = TreeFrogGetTimerService();
    if (ts) ts->Tick();

    GUIWindow *w = Application::GetInstance()->GetWindow();
    if (w) w->Update();

    if (g_treefrog_v40_run_count < 6) {
        char m[72];
        snprintf(m, sizeof(m), "retro_run.ready-update n=%u", g_treefrog_v40_run_count);
        boot_diag_log(m);
    }

    /*
     * U2.51.11 WALL_CLOCK_AUDIO_BUDGET:
     *
     * The audio budget is derived from the wall clock instead of a fixed
     * 48000/60 = 800 frames per retro_run. Some frontend states call
     * retro_run at ~120 Hz (2x the reported 60 Hz), which with the fixed
     * budget advanced the engine at 2x realtime: the FIFO/ring producer then
     * ran at ~96k samples/s and the SP-404MKII played 1.25x (aceleraciones).
     * With a wall-clock budget the engine always advances in real time
     * regardless of the frontend cadence, and the producer stays at 48k/s.
     */
    {
        unsigned long now_ms = System::GetInstance()->GetClock();
        if (audio_budget_last_ms == 0)
            audio_budget_last_ms = now_ms;
        unsigned long elapsed_ms = now_ms - audio_budget_last_ms;
        audio_budget_last_ms = now_ms;
        audio_accum += 48000.0 * (double)elapsed_ms / 1000.0;
    }
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

    if (TreeFrogUac2Bridge_ShouldRequestManagedRestartShutdown()) {
        TreeFrogSetQuitRequested(true);
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
