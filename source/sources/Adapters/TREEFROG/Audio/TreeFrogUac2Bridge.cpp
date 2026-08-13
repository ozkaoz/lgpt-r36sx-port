#include "Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef TREEFROG_UAC2_BRIDGE
#define TREEFROG_UAC2_BRIDGE 0
#endif

#if TREEFROG_UAC2_BRIDGE

extern "C" const char *TreeFrogU241OtgBuildMarker(void) {
    return "R36SX U2.51.7 MONITOR FIFO HANDSHAKE FILENAME EDITOR";
}

extern "C" const char *TreeFrogH25DualRateRuntimeMarker(void) {
    return "H36_SINGLE_RATE_48K";
}

extern "C" const char *TreeFrogH26ThreeDriverMarker(void) {
    return "H26_AUDIO_DRIVERS_LOCAL_WINDOWS_ANDROID";
}

extern "C" const char *TreeFrogH27BackendSafetyMarker(void) {
    return "H27_ANDROID_READY_MODE_AWARE_WINDOWS_RESTART_SAFE";
}

extern "C" const char *TreeFrogH28ManagedRestartMarker(void) {
    return "H28_MANAGED_IN_PORT_AUDIO_BACKEND_RESTART_FAILSAFE";
}

extern "C" const char *TreeFrogH29SingleOwnerRestartMarker(void) {
    return "H29_SINGLE_OWNER_PID_SCOPED_FRAMEBUFFER_SAFE_RESTART";
}

extern "C" const char *TreeFrogH30AudioQualityMarker(void) {
    return "H30_ADAPTIVE_MONITOR_ASRC_ANTIJITTER_FAST_RELAUNCH";
}

extern "C" const char *TreeFrogH32RecordStabilityMarker(void) {
    return "H35_WINDOWS_FRESH_ENUM_MONO48_INLINE_EXPORT_ABI5";
}

extern "C" const char *TreeFrogH35LifecycleDuplexMarker(void) {
    return "H35_WINDOWS_FRESH_ENUM_INLINE_EXPORT_STABLE_BASE";
}

extern "C" const char *TreeFrogH371WindowsIsolationMarker(void) {
    return "H38_6_ABI7_THREE_MODE_LOCAL_WINDOWS_ANDROID_SAFE_FRONTEND";
}

extern "C" const char *TreeFrogU2510CaptureProtocolMarker(void) {
    return "U2510_CAPTURE_ABI2_STEREO_TOKEN_META_PROTOCOL";
}

extern "C" const char *TreeFrogU2517RecordRuntimeMarker(void) {
    return
        "U2514_FIXED_RATIO_160_147 "
        "U2514_CONTINUOUS_FIXED_RATIO_160_147 "
        "U2514_ALSA_CLOCKED_PLAYBACK_ENGINE "
        "U2515_RECORD_SESSION_STATE_MACHINE U2517_MONITOR_FIFO_HANDSHAKE_CONTROL "
        "U2515_CAPTURE_SNAPSHOT_CACHE U2517_FILENAME_EDITOR_FAST_CASE_DUPLICATE_GUARD "
        "U2517_RUNTIME_ABI7_DAEMON_ONLY_RECOVERY "
        "H38_6_THREE_MODE_ABI7_LOCAL_WINDOWS_ANDROID "
        "U2517_AUDIO_DIRECTION_SP404_OUT_IN";
}
enum {
    U241_LOCAL_CONSOLE = 0,
    U241_WINDOWS = 1,
    U241_ANDROID = 2,
    U241_USB_OUT = 3,
    U241_MIDI = 4,
    /* U2.51.10: SP404 simplex IN direction (SP -> console, recording only).
     * Kept as a distinct mode so U241_ANDROID ("Android" phone gadget) is
     * untouched. */
    U241_SP404_IN = 5,
    /* Backward-compatible aliases retained for older call sites and logs. */
    U241_USB_DUPLEX = U241_WINDOWS,
    U241_USB_IN = U241_ANDROID
};

enum {
    U241_DEVICE_NONE = 0,
    U241_DEVICE_WINDOWS = 1,
    U241_DEVICE_ANDROID = 2,
    U241_DEVICE_SP404 = 3,
    U241_DEVICE_MIDI = 4
};

static int g_fifo_fd = -1;
static int g_setup_started = 0;
static int g_setup_attempts = 0;
static long long g_last_setup_attempt_ms = -1000000;
static pid_t g_setup_child_pid = -1;
static int g_setup_child_status = 0;
static int g_last_runtime_contract_code = -1;
static unsigned long g_submit_count = 0;
static long long g_android_fifo_miss_start_ms = 0;
static long long g_android_last_heal_ms = 0;
static int g_android_heal_attempts = 0;
static int g_driver_mode = U241_LOCAL_CONSOLE;
static int g_sampler_direction_in = 0;
static time_t g_mode_mtime = 0;
static int g_usb_raw = 0;
static int g_usb_out_allowed = 0;
static int g_was_muted = 0;
static unsigned long g_raw_configured_since = 0;
static unsigned long g_last_usb_check_submit = 0;
static long long g_last_usb_refresh_ms = -1000000;
static unsigned g_resample_phase_160 = 0;
enum { U2514_RESAMPLE_INPUT_CAPACITY_FRAMES = 8192 };
static int16_t g_resample_input[U2514_RESAMPLE_INPUT_CAPACITY_FRAMES * 2];
static unsigned g_resample_input_fill_frames = 0;
/* H40: bounded staging for partial fifo writes. The fifo is O_NONBLOCK;
 * when the daemon is momentarily behind, write() can accept a fraction of
 * the block (or nothing). Previously the remainder was silently dropped,
 * which clipped ~20-40 ms of audio on every backpressure event. The staged
 * buffer keeps the remaining samples and retries from the front on the
 * next submit; the cap bounds worst-case added latency (~341 ms at
 * 48 kHz stereo) and overflow drops the OLDEST samples, never the newest. */
enum { H40_FIFO_PENDING_CAP_SAMPLES = 16384 };
static int16_t g_fifo_pending[H40_FIFO_PENDING_CAP_SAMPLES];
static unsigned g_fifo_pending_samples = 0;
static unsigned long g_fifo_pending_drop_frames = 0;
static unsigned long g_fifo_pending_stage_events = 0;
static int g_usb_channels = 1;
static int g_usb_rate = 48000;
static int g_engine_rate = 48000;
static int g_mixer_volume_percent = 100;
static int g_project_master_volume_percent = 100;
static char g_last_capture_name[96] = "";
static char g_last_capture_path[256] = "";
static char g_capture_status[96] = "USB capture idle";
static int g_capture_level_percent = 0;
static int g_capture_level_left_percent = 0;
static int g_capture_level_right_percent = 0;
static int g_capture_elapsed_seconds = 0;
static int g_capture_state = TREEFROG_USB_CAPTURE_IDLE;
static long g_capture_frames = 0;
static long g_capture_bytes = 0;
static char g_capture_error[128] = "";
static char g_capture_token[96] = "";
static unsigned long g_capture_command_counter = 0;
static int g_usb_monitor_enabled = 0;
static long long g_last_capture_refresh_ms = -1000000;
static int g_au11i2_build_marker_logged = 0;
static long long g_last_mode_change_ms = -1000000;
static int g_monitor_fifo_fd = -1;
static double g_monitor_phase = 0.0;
#define U241_MONITOR_RING_SAMPLES 32768
static int16_t g_monitor_ring[U241_MONITOR_RING_SAMPLES];
static unsigned g_monitor_rpos = 0, g_monitor_wpos = 0, g_monitor_fill = 0;
static int g_monitor_primed = 0;
enum { U2415_MONITOR_PREBUFFER_SAMPLES = 960 };

static const char *kEnable = "/mnt/sdcard/lgpt/otg/enable_lgpt_uac2_bridge";
static const char *kMode = "/mnt/sdcard/lgpt/otg/audio_driver_mode";
/* v14.1: in-session source of truth. /tmp is always writable even when the
 * SD FAT is mounted read-only (dirty bit from a bad unplug), so the driver
 * mode can change and never flips back to Local Console on refresh. */
static const char *kRuntimeMode = "/tmp/r36sx_lgpt_usb/audio_driver_mode";
static const char *kNoMute = "/mnt/sdcard/lgpt/otg/disable_mute_local";
static const char *kU2430BuildMarker =
    "R36SX U2.51.7 MONITOR FIFO HANDSHAKE FILENAME EDITOR CORE";
static const char *kFifo = "/tmp/r36sx_uac2_bridge_fifo";
static const char *kLog = "/tmp/r36sx_lgpt_logs/uac2_bridge_lgpt.log";
static const char *kActiveMarker = "/tmp/r36sx_uac2_usb_active";
static const char *kRuntimeDir = "/tmp/r36sx_lgpt_usb";
static const char *kDaemonPid = "/tmp/r36sx_lgpt_usb/daemon_pid";
static const char *kRuntimeMirrorDir = "/tmp/r36sx_lgpt_logs/runtime_state";
static const char *kCaptureCmd = "/tmp/r36sx_lgpt_usb/usb_capture_cmd";
static const char *kCaptureStatus = "/tmp/r36sx_lgpt_usb/usb_capture_status";
static const char *kCaptureLastName = "/tmp/r36sx_lgpt_usb/usb_capture_last_name";
static const char *kCaptureLastPath = "/tmp/r36sx_lgpt_usb/usb_capture_last_path";
static const char *kCaptureLevel = "/tmp/r36sx_lgpt_usb/usb_capture_level";
static const char *kCaptureLevelL = "/tmp/r36sx_lgpt_usb/usb_capture_level_l";
static const char *kCaptureLevelR = "/tmp/r36sx_lgpt_usb/usb_capture_level_r";
static const char *kCaptureElapsed = "/tmp/r36sx_lgpt_usb/usb_capture_elapsed";
static const char *kCaptureMeta = "/tmp/r36sx_lgpt_usb/usb_capture_meta";
static const char *kDaemonVersion = "/tmp/r36sx_lgpt_usb/daemon_version";
static const char *kCaptureAbi = "/tmp/r36sx_lgpt_usb/capture_abi";
static char g_daemon_version[96] = "";
static char g_capture_abi[96] = "";
static const char *kCaptureMonitor = "/tmp/r36sx_lgpt_usb/usb_capture_monitor";
static const char *kCaptureMonitorFifo = "/tmp/r36sx_usb_capture_monitor_fifo";
static const char *kAudioProfile = "/tmp/r36sx_lgpt_usb/audio_profile";
static const char *kAudioChannels = "/tmp/r36sx_lgpt_usb/audio_channels";
static const char *kAudioRate = "/tmp/r36sx_lgpt_usb/audio_rate";
static const char *kRequestedAudioProfile = "/mnt/sdcard/lgpt/otg/audio_usb_profile";
static const char *kPlaybackPcmStatus = "/tmp/r36sx_lgpt_usb/playback_pcm_status";
static const char *kGadgetUdc = "/sys/kernel/config/usb_gadget/r36sx_lgpt_u2414/UDC";
static const char *kSetupLock = "/tmp/r36sx_u2414_audio_driver_lock";
static const char *kBranchRoot = "/mnt/sdcard/lgpt/otg/branches";
static const char *kActiveBranch = "/mnt/sdcard/lgpt/otg/active_audio_branch";
static const char *kPhysicalVolumeTmp = "/tmp/r36sx_physical_volume_percent";
static const char *kPhysicalVolumePersist = "/mnt/sdcard/lgpt/otg/physical_master_volume_percent";
static const char *kPolicy = "/mnt/sdcard/lgpt/otg/audio_driver_policy";
static const char *kAoaState = "/tmp/r36sx_lgpt_usb/aoa_state";
static const char *kAoaResult = "/tmp/r36sx_lgpt_usb/aoa_result";
static const char *kAoaAccessory = "/tmp/r36sx_lgpt_usb/aoa_bulk_accessory_present";
static const char *kAoaStream = "/tmp/r36sx_lgpt_usb/aoa_bulk_stream_ready";
static const char *kAoaPcmFifo = "/tmp/r36sx_aoa_bulk_pcm_fifo";
static const char *kSp404Card = "/tmp/r36sx_lgpt_usb/sp404_card";
static const char *kSp404Fifo = "/tmp/r36sx_sp404_pcm_fifo";
static const char *kMidiRawmidi = "/tmp/r36sx_lgpt_usb/midi_rawmidi";
static const char *kMidiFifo = "/tmp/r36sx_midi_pcm_fifo";
static const char *kH32TransitionStatus =
    "/tmp/r36sx_lgpt_usb/h35_transition_status";
static time_t g_physical_volume_mtime = 0;
static int g_physical_volume_percent = -1;
static int g_pending_driver_mode = -1;
static int g_android_managed_logged = 0;

static void log_msg(const char *msg);
static const char *runtime_contract_reason(int code);
static void au10z_mirror_runtime_file(
    const char *leaf,
    const char *text);

static int exists_file(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0;
}

static long long monotonic_milliseconds(void);

/* H38.7 OPT_PERF: the USB bridge enable/mute switches live on the SD card.
 * Every audio callback (60/s) used to stat() them from the realtime thread,
 * adding ~180 SD-card syscalls per second and jitter under heavy write load.
 * Cache the result for 100 ms (same period as refresh_usb_state) so the hot
 * path touches the SD at most ~10 times per second instead. */
static long long g_enable_cache_ms = -1000000;
static int g_enable_cache_value = 0;

static int enable_file_present(void) {
    const long long now_ms = monotonic_milliseconds();
    if (now_ms > 0 && (now_ms - g_enable_cache_ms) < 100) {
        return g_enable_cache_value;
    }
    g_enable_cache_ms = now_ms;
    g_enable_cache_value = exists_file(kEnable);
    return g_enable_cache_value;
}

static long long g_nomute_cache_ms = -1000000;
static int g_nomute_cache_value = 0;

static int nomute_file_present(void) {
    const long long now_ms = monotonic_milliseconds();
    if (now_ms > 0 && (now_ms - g_nomute_cache_ms) < 100) {
        return g_nomute_cache_value;
    }
    g_nomute_cache_ms = now_ms;
    g_nomute_cache_value = exists_file(kNoMute);
    return g_nomute_cache_value;
}

static long long monotonic_milliseconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return ((long long)ts.tv_sec * 1000LL) +
           ((long long)ts.tv_nsec / 1000000LL);
}

/*
 * U2.51.6 ATOMIC_RUNTIME_CONTROL:
 * The daemon polls files in tmpfs. O_TRUNC followed by write exposed empty or
 * partial states and could overwrite a pending START/STOP command when the
 * monitor was toggled. Write a complete sibling file and rename it atomically.
 */
static int write_runtime_file_atomic(const char *path, const char *text) {
    static unsigned long serial = 0;
    if (!path || !path[0] || !text) return 0;

    char temporary[512];
    snprintf(
        temporary,
        sizeof(temporary),
        "%s.tmp.%ld.%lu",
        path,
        (long)getpid(),
        ++serial);

    int fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return 0;

    const size_t length = strlen(text);
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = write(fd, text + offset, length - offset);
        if (count > 0) {
            offset += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        close(fd);
        unlink(temporary);
        return 0;
    }

    if (close(fd) != 0) {
        unlink(temporary);
        return 0;
    }
    if (rename(temporary, path) != 0) {
        unlink(temporary);
        return 0;
    }
    return 1;
}


static void make_capture_token(char *dst, int len) {
    struct timespec ts;
    unsigned long long stamp = 0;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        stamp = ((unsigned long long)ts.tv_sec * 1000000000ULL) +
                (unsigned long long)ts.tv_nsec;
    }
    ++g_capture_command_counter;
    snprintf(
        dst,
        len,
        "%ld-%llu-%lu",
        (long)getpid(),
        stamp,
        g_capture_command_counter);
}

static int write_capture_command(
    const char *verb,
    const char *path,
    int seconds,
    const char *name) {
    if (!verb || !verb[0]) return 0;

    mkdir(kRuntimeDir, 0777);

    char token[96];
    make_capture_token(token, sizeof(token));

    char command[768];
    int length = snprintf(
        command,
        sizeof(command),
        "%s\nTOKEN=%s\n",
        verb,
        token);

    if (path && path[0]) {
        length += snprintf(
            command + length,
            sizeof(command) - (size_t)length,
            "PATH=%s\n",
            path);
    }

    if (seconds > 0) {
        length += snprintf(
            command + length,
            sizeof(command) - (size_t)length,
            "SECONDS=%d\n",
            seconds);
    }

    if (name && name[0]) {
        length += snprintf(
            command + length,
            sizeof(command) - (size_t)length,
            "NAME=%s\n",
            name);
    }

    if (length <= 0 || length >= (int)sizeof(command)) return 0;

    if (!write_runtime_file_atomic(kCaptureCmd, command)) {
        log_msg("capture command atomic publish failed");
        return 0;
    }

    snprintf(g_capture_token, sizeof(g_capture_token), "%s", token);
    au10z_mirror_runtime_file("usb_capture_cmd", command);
    return 1;
}

static const char *capture_meta_value(
    const char *buffer,
    const char *key,
    char *dst,
    int dst_len) {
    if (!buffer || !key || !dst || dst_len <= 0) return 0;
    dst[0] = 0;

    const char *start = strstr(buffer, key);
    if (!start) return 0;
    start += strlen(key);

    const char *end = strchr(start, '\n');
    int count = end ? (int)(end - start) : (int)strlen(start);
    if (count >= dst_len) count = dst_len - 1;
    if (count < 0) count = 0;

    memcpy(dst, start, (size_t)count);
    dst[count] = 0;
    return dst;
}

static int capture_state_from_text(const char *text) {
    if (!text) return TREEFROG_USB_CAPTURE_IDLE;
    if (strcmp(text, "STARTING") == 0)
        return TREEFROG_USB_CAPTURE_STARTING;
    if (strcmp(text, "RECORDING") == 0)
        return TREEFROG_USB_CAPTURE_RECORDING;
    if (strcmp(text, "STOPPING") == 0)
        return TREEFROG_USB_CAPTURE_STOPPING;
    if (strcmp(text, "READY") == 0)
        return TREEFROG_USB_CAPTURE_READY;
    if (strcmp(text, "ERROR") == 0)
        return TREEFROG_USB_CAPTURE_ERROR;
    return TREEFROG_USB_CAPTURE_IDLE;
}

static int marker_fresh(const char *p, int max_age_sec) {
    struct stat st;
    if (!p || stat(p, &st) != 0) return 0;
    time_t now = time(NULL);
    if (now < st.st_mtime) return 1;
    return (now - st.st_mtime) <= max_age_sec;
}

static const char *mode_name(int mode) {
    switch (mode) {
    case U241_WINDOWS: return "Windows";
    case U241_ANDROID: return "Android";
    case U241_USB_OUT: return "Sampler";
    case U241_SP404_IN: return "Sampler";
    case U241_MIDI: return "MIDI";
    case U241_LOCAL_CONSOLE:
    default: return "Local Console";
    }
}

static const char *mode_token(int mode) {
    switch (mode) {
    case U241_WINDOWS: return "USB_DUPLEX";
    case U241_ANDROID: return "USB_IN";
    case U241_USB_OUT: return "USB_OUT";
    case U241_SP404_IN: return "SP404_IN";
    case U241_MIDI: return "MIDI";
    case U241_LOCAL_CONSOLE:
    default: return "LOCAL_CONSOLE";
    }
}

static const char *policy_token(int mode) {
    switch (mode) {
    case U241_WINDOWS: return "USB_DUPLEX_OTG";
    case U241_ANDROID: return "USB_IN_OTG";
    case U241_USB_OUT: return "USB_OUT_OTG";
    case U241_SP404_IN: return "USB_OUT_OTG";
    case U241_MIDI: return "MIDI_OTG";
    case U241_LOCAL_CONSOLE:
    default: return "LOCAL_CONSOLE";
    }
}

static const char *branch_name_for_mode(int mode) {
    switch (mode) {
    case U241_WINDOWS: return "audio_driver_usb_duplex";
    case U241_ANDROID: return "audio_driver_usb_in";
    case U241_USB_OUT: return "audio_driver_usb_out";
    case U241_SP404_IN: return "audio_driver_sp404_in";
    case U241_MIDI: return "audio_driver_midi";
    case U241_LOCAL_CONSOLE:
    default: return "audio_driver_local_console";
    }
}

static void write_active_branch_file(int mode) {
    mkdir(kBranchRoot, 0777);
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/%s", kBranchRoot, branch_name_for_mode(mode));
    mkdir(dir, 0777);
    int fd = open(kActiveBranch, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        const char *bn = branch_name_for_mode(mode);
        write(fd, bn, strlen(bn));
        write(fd, "\n", 1);
        close(fd);
    }
    char branch_mode[320];
    snprintf(branch_mode, sizeof(branch_mode), "%s/MODE", dir);
    fd = open(branch_mode, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        const char *mn = mode_token(mode);
        write(fd, mn, strlen(mn));
        write(fd, "\n", 1);
        close(fd);
    }
}

static const char *mode_desc(int mode) {
    switch (mode) {
    case U241_WINDOWS:
        return "Duplex UAC2 gadget (PC host)";
    case U241_ANDROID:
        return "Duplex UAC2 gadget (phone host)";
    case U241_USB_OUT:
        return "SP404: console sound to sampler (EXT SOURCE)";
    case U241_SP404_IN:
        return "SP404 IN: sampler->console, recording only";
    case U241_MIDI:
        return "MIDI: USB piano/controller";
    case U241_LOCAL_CONSOLE:
    default:
        return "Console sound, OTG may stay connected";
    }
}

static int selectable_mode(int mode) {
    return mode == U241_LOCAL_CONSOLE ||
        mode == U241_WINDOWS ||
        mode == U241_ANDROID ||
        mode == U241_USB_OUT ||
        mode == U241_MIDI;
}

static int mode_from_text(const char *s) {
    if (!s) return U241_LOCAL_CONSOLE;
    if (strstr(s, "SP404_IN") || strstr(s, "SP404IN"))
        return U241_SP404_IN;
    if (strstr(s, "ANDROID") || strstr(s, "AOA") ||
        strstr(s, "USB_IN"))
        return U241_ANDROID;
    if (strstr(s, "USB_OUT"))
        return U241_USB_OUT;
    if (strstr(s, "MIDI"))
        return U241_MIDI;
    if (strstr(s, "WINDOWS") || strstr(s, "USB_DUPLEX") ||
        strstr(s, "USB_IN_OUT") || strstr(s, "USB_INPUT_OUTPUT") ||
        strstr(s, "FULL_DUPLEX") || strstr(s, "USB_OUT_AUTO_MUTE") ||
        strstr(s, "USB_IN_CAPTURE") || strstr(s, "EXTERNAL_RECORD"))
        return U241_WINDOWS;
    return U241_LOCAL_CONSOLE;
}


static void log_msg(const char *msg) {
    FILE *f = fopen(kLog, "a");
    if (!f) { f = fopen("/tmp/r36sx_uac2_bridge_lgpt.log", "a"); }
    if (!f) return;
    char playback_status[96] = "unavailable";
    {
        int status_fd = open(kPlaybackPcmStatus, O_RDONLY);
        if (status_fd >= 0) {
            ssize_t n = read(
                status_fd,
                playback_status,
                sizeof(playback_status) - 1);
            close(status_fd);
            if (n > 0) {
                playback_status[n] = 0;
                char *newline = strchr(playback_status, '\n');
                if (newline) *newline = 0;
            }
        }
    }
    fprintf(f,
            "U2517 %s errno=%d (%s) submit=%lu mode=%s raw=%d out=%d mute=%d mixer=%d master=%d phase160=%u fifo=%d marker=%d setup_attempts=%d contract=%s playback=%s capture=%s\n",
            msg ? msg : "log", errno, strerror(errno), g_submit_count,
            TreeFrogUac2Bridge_GetDriverModeName(), g_usb_raw, g_usb_out_allowed,
            g_was_muted, g_mixer_volume_percent, g_project_master_volume_percent,
            g_resample_phase_160, g_fifo_fd, marker_fresh(kActiveMarker, 2),
            g_setup_attempts,
            runtime_contract_reason(g_last_runtime_contract_code),
            playback_status,
            g_capture_status);
    fclose(f);
}


static void au10z_write_text_file(const char *path, const char *text) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) { if (text) write(fd, text, strlen(text)); close(fd); }
}

static void au10z_mirror_runtime_file(const char *leaf, const char *text) {
    if (!leaf || !leaf[0]) return;
    /* H42: the mirror target is tmpfs (kRuntimeMirrorDir). The SD mkdirs
     * below used to create FAT directory entries on every capture command;
     * removed for the zero-runtime-SD-writes mandate. */
    mkdir(kRuntimeMirrorDir, 0777);
    char p[256];
    snprintf(p, sizeof(p), "%s/%s", kRuntimeMirrorDir, leaf);
    au10z_write_text_file(p, text ? text : "");
}

static void launch_apply_profile_label(const char *label) {
    /* AU11U_SPLIT_USB_PROFILE_SAFE
       Do not keep a single always-duplex Windows endpoint open.  Windows can
       keep the host playback side alive with silence; on this MUSB/UAC2 stack
       that interferes with the LGPT->Windows path.  Switch profile by sampler
       context: USB_OUT_AUTO_MUTE outside record, USB_C_RECORD inside record. */
    const char *m = label ? label : "USB_OUT_AUTO_MUTE";
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "/mnt/sdcard/lgpt/otg/bin/otg_u241_apply_profile_once.sh", m, (char *)0);
        _exit(127);
    }
}

static void launch_apply_profile_once(int mode) {
    launch_apply_profile_label(mode_token(mode));
}

static int file_contains(const char *p, const char *needle) {
    char buf[128];
    int fd = open(p, O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = 0;
    return strstr(buf, needle) != 0;
}

static int udc_configured_raw(void) {
    if (file_contains("/sys/class/udc/musb-hdrc.0.auto/state", "configured")) return 1;
    if (file_contains("/sys/class/udc/musb-hdrc.1.auto/state", "configured")) return 1;
    return 0;
}

static int daemon_pid_alive(void) {
    static const char *candidates[] = {
        kDaemonPid,
        "/tmp/r36sx_lgpt_usb/sp404_daemon_pid",
        "/tmp/r36sx_lgpt_usb/midi_daemon_pid"
    };
    for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        char buf[32];
        int fd = open(candidates[i], O_RDONLY);
        if (fd < 0) continue;
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n <= 0) continue;
        buf[n] = 0;
        long pid = strtol(buf, 0, 10);
        if (pid <= 1) continue;
        if (kill((pid_t)pid, 0) == 0 || errno == EPERM) return 1;
    }
    return 0;
}

static int requested_audio_channels(void) {
    /*
     * Absence, truncation or an unknown profile must converge on the same
     * recovery baseline used by the setup script.  Only an explicit stereo
     * request may select two channels; otherwise the core and script could
     * disagree forever during first boot and trigger an endless rebuild loop.
     */
    return file_contains(
               kRequestedAudioProfile,
               "STEREO_48K")
        ? 2
        : 1;
}

static int android_stream_ready_raw(void) {
    if (!exists_file(kAoaStream)) return 0;
    if (file_contains(kAoaResult, "FAILED")) return 0;
    return 1;
}

static int sp404_card_present_raw(void) {
    if (!exists_file(kSp404Card)) return 0;
    if (file_contains(kSp404Card, "none")) return 0;
    if (file_contains(kSp404Card, "FAILED")) return 0;
    return 1;
}

static int midi_device_present_raw(void) {
    if (!exists_file(kMidiRawmidi)) return 0;
    if (file_contains(kMidiRawmidi, "none")) return 0;
    if (file_contains(kMidiRawmidi, "FAILED")) return 0;
    return 1;
}

/* Unified device detection. Windows is the only peripheral (gadget) role;
 * Android, SP404MKII and MIDI instruments are all host-role devices. */
static int detected_device(void) {
    if (udc_configured_raw()) return U241_DEVICE_WINDOWS;
    if (android_stream_ready_raw()) return U241_DEVICE_ANDROID;
    if (sp404_card_present_raw()) return U241_DEVICE_SP404;
    if (midi_device_present_raw()) return U241_DEVICE_MIDI;
    return U241_DEVICE_NONE;
}

/*
 * U2.52.5 SAMPLER OUT_ONLY + Android input-only:
 *   USB_OUT  (Sampler): console -> SP playback only (direction toggle removed).
 *   Android AOA: input-only (receiver/daemon have no playback path), so it has
 *   no OUT and never mutes the console.
 * Windows gadget mode keeps its existing out+in semantics.
 */
static int mode_has_out(int mode) {
    if (mode == U241_USB_OUT) return !g_sampler_direction_in;
    return mode == U241_WINDOWS;
}

static int mode_has_in(int mode) {
    if (mode == U241_USB_OUT) return g_sampler_direction_in;
    return mode == U241_WINDOWS ||
        mode == U241_ANDROID ||
        mode == U241_SP404_IN;
}

static const char *device_out_fifo(int device) {
    switch (device) {
    case U241_DEVICE_SP404: return kSp404Fifo;
    case U241_DEVICE_MIDI: return kMidiFifo;
    default: return kFifo;
    }
}

static int sp404_runtime_contract_code(void) {
    if (!sp404_card_present_raw()) return 30;
    if (!exists_file(kSp404Fifo)) return 31;
    if (!daemon_pid_alive()) return 32;
    if (!file_contains(kDaemonVersion, "R36SX_SP404_AUDIO_DAEMON_ABI=1"))
        return 33;
    if (!file_contains(kCaptureAbi, "R36SX_SP404_CAPTURE_ABI=1"))
        return 34;
    if (!file_contains(kAudioRate, "48000")) return 35;
    return 0;
}

static int midi_runtime_contract_code(void) {
    if (!midi_device_present_raw()) return 40;
    if (!exists_file(kMidiFifo)) return 41;
    if (!daemon_pid_alive()) return 42;
    if (!file_contains(kDaemonVersion, "R36SX_MIDI_DAEMON_ABI=1"))
        return 43;
    return 0;
}

static int android_runtime_contract_code(void) {
    if (!exists_file(kAoaPcmFifo)) return 21;
    if (!daemon_pid_alive()) return 22;
    if (!file_contains(kDaemonVersion, "R36SX_AOA_BULK_AUDIO_DAEMON_ABI=4"))
        return 23;
    if (!file_contains(kCaptureAbi, "R36SX_CAPTURE_ABI=4"))
        return 24;
    if (!file_contains(kAudioChannels, "2")) return 25;
    if (!file_contains(kAudioRate, "48000")) return 26;
    if (!exists_file(kAoaState) && !exists_file(kAoaResult)) return 27;
    return 0;
}

static int runtime_contract_code(void) {
    if (g_driver_mode == U241_LOCAL_CONSOLE) return 0;
    if (g_driver_mode == U241_MIDI)
        return midi_runtime_contract_code();
    if (g_driver_mode == U241_ANDROID)
        return android_runtime_contract_code();
    if (detected_device() == U241_DEVICE_SP404)
        return sp404_runtime_contract_code();
    /*
     * U2.51.4 WAIT_HOST_READY CONTRACT
     *
     * A configured UDC is a host state, not an installation/runtime state.
     * The daemon, FIFO and matching ABI can be completely ready while the
     * cable is disconnected or while Windows is still enumerating. Treat that
     * state as ready and let refresh_usb_state() gate actual audio submission.
     * Rebuilding the gadget every three seconds while waiting for the host was
     * the source of the connection lag and input stalls seen in U2.51.3.
     */
    if (!file_contains(kGadgetUdc, "musb-hdrc.0.auto")) return 8;
    if (!exists_file(kFifo)) return 1;
    if (!daemon_pid_alive()) return 2;
    if (!file_contains(
            kDaemonVersion,
            "R36SX_USB_AUDIO_DAEMON_ABI=7"))
        return 3;
    if (!file_contains(
            kCaptureAbi,
            "R36SX_CAPTURE_ABI=2"))
        return 4;

    const int expected_channels =
        requested_audio_channels();
    if (expected_channels == 1) {
        if (!file_contains(kAudioChannels, "1"))
            return 5;
    } else {
        if (!file_contains(kAudioChannels, "2"))
            return 6;
    }

    if (!file_contains(kAudioRate, "48000"))
        return 7;

    return 0;
}

static const char *runtime_contract_reason(int code) {
    switch (code) {
    case 0:
        if (g_driver_mode == U241_ANDROID)
            return android_stream_ready_raw() ?
                "android-ready-stream" : "android-ready-wait-app";
        return udc_configured_raw() ? "ready-configured" : "ready-wait-host";
    case 1: return "fifo-missing";
    case 2: return "daemon-not-alive";
    case 3: return "daemon-version-mismatch";
    case 4: return "capture-abi-mismatch";
    case 5: return "runtime-not-mono";
    case 6: return "runtime-not-stereo";
    case 7: return "runtime-rate-mismatch";
    case 8: return "gadget-not-bound";
    case 21: return "android-pcm-fifo-missing";
    case 22: return "android-daemon-not-alive";
    case 23: return "android-daemon-version-mismatch";
    case 24: return "android-capture-abi-mismatch";
    case 25: return "android-runtime-not-stereo";
    case 26: return "android-runtime-rate-mismatch";
    case 27: return "android-receiver-state-missing";
    case 30: return "sp404-card-missing";
    case 31: return "sp404-pcm-fifo-missing";
    case 32: return "sp404-daemon-not-alive";
    case 33: return "sp404-daemon-version-mismatch";
    case 34: return "sp404-capture-abi-mismatch";
    case 35: return "sp404-runtime-rate-mismatch";
    case 40: return "midi-device-missing";
    case 41: return "midi-pcm-fifo-missing";
    case 42: return "midi-daemon-not-alive";
    case 43: return "midi-daemon-version-mismatch";
    default: return "unknown";
    }
}

static int runtime_ready_fast(void) {
    const int code = runtime_contract_code();
    if (code != g_last_runtime_contract_code) {
        g_last_runtime_contract_code = code;
        char message[128];
        snprintf(
            message,
            sizeof(message),
            "runtime contract code=%d reason=%s expected_channels=%d",
            code,
            runtime_contract_reason(code),
            requested_audio_channels());
        log_msg(message);
    }
    return code == 0;
}

static void write_mode_file(int mode) {
    mkdir(kRuntimeDir, 0777);
    const char *m = mode_token(mode);
    /* v14.1: write the runtime copy first (always writable); the SD copy is
     * a best-effort persistence for the next boot. */
    int rt = open(kRuntimeMode, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (rt >= 0) {
        write(rt, m, strlen(m));
        write(rt, "\n", 1);
        close(rt);
    }
    int fd = open(kMode, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        write(fd, m, strlen(m));
        write(fd, "\n", 1);
        /*
         * U2.41.5.2 FAST_MODE_FILE:
         * close() is sufficient here.  A synchronous fsync on the SD card
         * delayed every audio-device change even though routing is already
         * updated in memory.
         */
        close(fd);
    }
    write_runtime_file_atomic(kPolicy, policy_token(mode));
    write_active_branch_file(mode);
    struct stat st;
    if (stat(kRuntimeMode, &st) == 0) g_mode_mtime = st.st_mtime;
}

static void refresh_mode_from_file(int force) {
    struct stat st;
    const char *src = kMode;
    if (stat(kRuntimeMode, &st) == 0) {
        src = kRuntimeMode;
    } else if (stat(kMode, &st) != 0) {
        if (force) write_mode_file(g_driver_mode);
        return;
    }
    if (!force && st.st_mtime == g_mode_mtime) return;
    g_mode_mtime = st.st_mtime;
    char buf[96];
    int fd = open(src, O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) return;
    buf[n] = 0;
    int new_mode = mode_from_text(buf);
    if (new_mode == U241_SP404_IN) {
        /* U2.52.5 SAMPLER_OUT_ONLY: a persisted SP404_IN token behaves as
         * Sampler OUT; the port no longer captures. */
        new_mode = U241_USB_OUT;
        g_sampler_direction_in = 0;
    } else if (new_mode == U241_USB_OUT) {
        g_sampler_direction_in = 0;
    }
    if (!selectable_mode(new_mode)) new_mode = U241_LOCAL_CONSOLE;
    if (new_mode != g_driver_mode) {
        g_driver_mode = new_mode;
        log_msg("driver mode loaded from file");
    }
}

static int read_percent_file(const char *p, time_t *mtime_out) {
    struct stat st;
    if (!p || stat(p, &st) != 0) return -1;
    if (mtime_out && *mtime_out == st.st_mtime) return g_physical_volume_percent;
    char buf[32];
    int fd = open(p, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = 0;
    int v = atoi(buf);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    if (mtime_out) *mtime_out = st.st_mtime;
    return v;
}

static void refresh_passive_physical_volume_file(void) {
    int v = read_percent_file(kPhysicalVolumeTmp, &g_physical_volume_mtime);
    if (v < 0) v = read_percent_file(kPhysicalVolumePersist, &g_physical_volume_mtime);
    if (v >= 0 && v != g_physical_volume_percent) {
        g_physical_volume_percent = v;
        TreeFrogUac2Bridge_SetMixerVolumePercent(v);
        char msg[80];
        snprintf(msg, sizeof(msg), "passive physical volume file=%d", v);
        log_msg(msg);
    }
}

static int capture_recording_active(void); /* AU11 forward */

static void refresh_usb_state(void) {
    /* Record stops the tracker transport, so submit_count may remain zero.
     * A submit-count-only governor therefore polled sysfs on every Record
     * frame and competed with input.  U2.51.5 uses an absolute 100 ms cache
     * in addition to the realtime submit-count governor. */
    const long long now_ms = monotonic_milliseconds();
    if (now_ms > 0 && (now_ms - g_last_usb_refresh_ms) < 100) return;
    if (g_submit_count != 0 &&
        (g_submit_count - g_last_usb_check_submit) < 2 &&
        g_last_usb_refresh_ms > 0) return;
    g_last_usb_refresh_ms = now_ms;
    g_last_usb_check_submit = g_submit_count;
    /*
     * Unified device detection. Android AOA, SP404MKII and MIDI are host-role
     * topologies: R36SX is USB host and readiness comes from the stream/card/
     * rawmidi markers, never from /sys/class/udc. Windows is the peripheral
     * (gadget) role and uses the configured UDC.
     */
    const int device = detected_device();
    const int raw =
        (device == U241_DEVICE_WINDOWS) ? udc_configured_raw() :
        (device == U241_DEVICE_ANDROID) ? android_stream_ready_raw() :
        (device == U241_DEVICE_SP404) ? sp404_card_present_raw() :
        (device == U241_DEVICE_MIDI) ? midi_device_present_raw() : 0;
    if (raw != g_usb_raw) {
        g_usb_raw = raw;
        g_raw_configured_since = raw ? (g_submit_count ? g_submit_count : 1) : 0;
        log_msg(raw ?
            (device == U241_DEVICE_ANDROID ?
                "android AOA stream ready raw" :
                device == U241_DEVICE_SP404 ?
                    "sp404 host card ready raw" :
                    device == U241_DEVICE_MIDI ?
                        "midi rawmidi ready raw" :
                        "udc configured raw") :
            (device == U241_DEVICE_ANDROID ?
                "android AOA stream stopped raw" :
                device == U241_DEVICE_SP404 ?
                    "sp404 host card stopped raw" :
                    device == U241_DEVICE_MIDI ?
                        "midi rawmidi stopped raw" :
                        "udc not configured raw"));
    }
    /*
     * OUT is only routed when the selected mode requests playback. Windows and
     * SP404 both expose an OUT path; Android AOA and MIDI are input-only.
     */
    const int marker = marker_fresh(kActiveMarker, 2);
    int out_now = mode_has_out(g_driver_mode) ? (raw || marker) : 0;
    if (out_now != g_usb_out_allowed) {
        g_usb_out_allowed = out_now;
        log_msg(out_now ? "usb out allowed" : "usb out stopped");
    }
}

static int should_mute_now(void) {
    /*
     * Mode 0: local console remains audible even with OTG attached.
     * Mode 1: once UAC2 is configured, project audio goes to USB and the local
     * project mix is muted. During capture/monitor, only PC input preview is
     * mixed locally to avoid a feedback loop.
     */
    /*
     * U2.41.5.1 LOCAL_MIX_ISOLATION:
     * The project is always sent to Windows in USB_DUPLEX, including while
     * prelisten is active.  Mute only the local project mix; the PC monitor is
     * added afterwards by MixUsbCaptureMonitorStereo48000().
     */
    /*
     * U2.51.4 USB_EXCLUSIVE_RECORD_MONITOR:
     * When the OTG gadget is configured, the tracker project is routed to the
     * USB host and removed from the local mix.  The Record modal may still add
     * PC->console monitor audio afterwards through
     * MixUsbCaptureMonitorStereo48000().  Therefore normal OTG playback is
     * heard only in Windows, while prelisten exists only inside Record.
     *
     * disable_mute_local is retained only as an explicit diagnostic override.
     */
    /*
     * U2.52.5 ANDROID_NO_MUTE:
     * Android AOA is input-only (there is no console->phone playback path),
     * so routing audio to USB must not silence the console: the user hears
     * the project locally while the phone capture feeds the Record modal.
     */
    if (g_driver_mode == U241_ANDROID) return 0;
    return mode_has_out(g_driver_mode) &&
           !nomute_file_present() &&
           g_usb_raw;
}

static void reap_setup_child_nonblocking(void) {
    if (g_setup_child_pid <= 0) return;

    int status = 0;
    const pid_t result = waitpid(g_setup_child_pid, &status, WNOHANG);
    if (result == 0) return;

    if (result == g_setup_child_pid) {
        g_setup_child_status = status;
        char message[128];
        if (WIFEXITED(status)) {
            snprintf(
                message,
                sizeof(message),
                "setup child exited pid=%ld code=%d",
                (long)g_setup_child_pid,
                WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            snprintf(
                message,
                sizeof(message),
                "setup child signaled pid=%ld signal=%d",
                (long)g_setup_child_pid,
                WTERMSIG(status));
        } else {
            snprintf(
                message,
                sizeof(message),
                "setup child finished pid=%ld status=%d",
                (long)g_setup_child_pid,
                status);
        }
        log_msg(message);
        g_setup_child_pid = -1;
        return;
    }

    if (result < 0 && errno == ECHILD) {
        g_setup_child_pid = -1;
    }
}

static long long g_last_mode_refresh_ms = -1000000;

static void ensure_setup_started(void) {
    if (!enable_file_present()) return;

    const long long now_ms = monotonic_milliseconds();
    if (now_ms > 0 && (now_ms - g_last_mode_refresh_ms) < 100) return;
    g_last_mode_refresh_ms = now_ms;

    refresh_mode_from_file(1);
    reap_setup_child_nonblocking();

    if (g_driver_mode == U241_ANDROID ||
        g_driver_mode == U241_USB_OUT ||
        g_driver_mode == U241_MIDI) {
        /* The Android AOA, SP404MKII and MIDI runtimes are owned by the SD
         * apply script and their supervisors. The core never forks the Windows
         * gadget setup for host-role modes, otherwise it would fight the
         * host-role runtime. */
        if (!g_android_managed_logged) {
            g_android_managed_logged = 1;
            log_msg("host-role runtime managed by SD apply script");
        }
        g_setup_started = 1;
        return;
    }

    /*
     * U2.51.5 single-owner recovery contract.
     *
     * Runtime readiness and Windows enumeration are different states.  A
     * valid ABI/FIFO/daemon runtime is reused even while the cable is absent
     * or Windows is still enumerating.  Only one setup child may exist, and
     * a completed failed attempt is followed by a conservative retry rather
     * than repeated forks from the realtime callback.
     */
    if (runtime_ready_fast()) {
        if (!g_setup_started)
            log_msg("fast runtime attach contract-valid ABI7");
        g_setup_started = 1;
        return;
    }

    if (g_setup_child_pid > 0) return;
    if (exists_file(kSetupLock)) return;

    const long long now_ms2 = monotonic_milliseconds();
    if (now_ms2 > 0 &&
        (now_ms2 - g_last_setup_attempt_ms) < 10000)
        return;

    g_setup_started = 1;
    g_last_setup_attempt_ms = now_ms2;
    ++g_setup_attempts;

    char message[160];
    snprintf(
        message,
        sizeof(message),
        "starting single setup attempt=%d contract=%s previous_status=%d",
        g_setup_attempts,
        runtime_contract_reason(runtime_contract_code()),
        g_setup_child_status);
    log_msg(message);

    const pid_t pid = fork();
    if (pid == 0) {
        int fd = open(
            "/mnt/sdcard/lgpt/otg/logs/u241_setup_from_lgpt.log",
            O_WRONLY | O_CREAT | O_TRUNC,
            0666);
        if (fd < 0)
            fd = open(
                "/tmp/u241_setup_from_lgpt.log",
                O_WRONLY | O_CREAT | O_TRUNC,
                0666);
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            if (fd > 2) close(fd);
        }
        execl(
            "/bin/sh",
            "sh",
            "/mnt/sdcard/lgpt/otg/bin/otg_u241_setup_once.sh",
            (char *)0);
        _exit(127);
    }

    if (pid < 0) {
        log_msg("setup fork failed");
        return;
    }

    g_setup_child_pid = pid;
}

static void close_fifo_if_open(const char *why) {
    if (g_fifo_fd >= 0) {
        close(g_fifo_fd);
        g_fifo_fd = -1;
        g_resample_phase_160 = 0;
        g_resample_input_fill_frames = 0;
        g_fifo_pending_samples = 0;
        log_msg(why ? why : "fifo closed");
    }
}

static void ensure_fifo_open_nonblocking(void) {
    if (g_fifo_fd >= 0) return;
    if (!enable_file_present()) return;
    ensure_setup_started();
    const char *out_fifo = device_out_fifo(detected_device());
    g_fifo_fd = open(out_fifo, O_WRONLY | O_NONBLOCK);
    if (g_fifo_fd < 0) {
        if ((g_submit_count % 240) == 0) log_msg("fifo open pending");
        /* v12 self-heal: in Android mode a persistently missing AOA PCM fifo
         * means the AOA runtime died; re-request the host-role apply (which
         * reuses a healthy runtime or restarts the supervisor watchdog) at
         * most once per 30 s, up to 5 attempts. */
        if (detected_device() == U241_DEVICE_ANDROID) {
            const long long now_ms = monotonic_milliseconds();
            if (g_android_fifo_miss_start_ms == 0)
                g_android_fifo_miss_start_ms = now_ms;
            if (g_android_heal_attempts < 5 &&
                now_ms > 0 &&
                now_ms - g_android_fifo_miss_start_ms >= 30000 &&
                now_ms - g_android_last_heal_ms >= 30000) {
                g_android_last_heal_ms = now_ms;
                g_android_fifo_miss_start_ms = now_ms;
                ++g_android_heal_attempts;
                log_msg("android fifo missing - re-applying host-role runtime");
                launch_apply_profile_once(U241_ANDROID);
            }
        }
        return;
    }
    g_android_fifo_miss_start_ms = 0;
    log_msg("fifo opened");
}

static int16_t clamp16(int v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static int16_t mono_from_stereo(const int16_t *stereo, int idx) {
    return clamp16(((int)stereo[idx * 2] + (int)stereo[idx * 2 + 1]) / 2);
}


static void monitor_ring_reset(void) {
    g_monitor_rpos = g_monitor_wpos = g_monitor_fill = 0;
    g_monitor_phase = 0.0;
    g_monitor_primed = 0;
}

static unsigned monitor_ring_push(const int16_t *src, unsigned n) {
    unsigned pushed = 0;
    while (pushed < n) {
        if (g_monitor_fill >= U241_MONITOR_RING_SAMPLES) {
            /* U2.41.5: keep the newest PC audio instead of accumulating
               unbounded monitor latency. */
            g_monitor_rpos =
                (g_monitor_rpos + 1) % U241_MONITOR_RING_SAMPLES;
            g_monitor_fill--;
        }
        g_monitor_ring[g_monitor_wpos] = src[pushed++];
        g_monitor_wpos =
            (g_monitor_wpos + 1) % U241_MONITOR_RING_SAMPLES;
        g_monitor_fill++;
    }
    return pushed;
}

static int monitor_ring_peek(unsigned idx, int16_t *out) {
    if (idx >= g_monitor_fill) return 0;
    unsigned pos = (g_monitor_rpos + idx) % U241_MONITOR_RING_SAMPLES;
    if (out) *out = g_monitor_ring[pos];
    return 1;
}

static unsigned monitor_ring_drop(unsigned n) {
    unsigned dropped = 0;
    while (dropped < n && g_monitor_fill > 0) {
        g_monitor_rpos = (g_monitor_rpos + 1) % U241_MONITOR_RING_SAMPLES;
        g_monitor_fill--;
        dropped++;
    }
    return dropped;
}

static int ensure_monitor_fifo_node(void) {
    struct stat info;
    if (lstat(kCaptureMonitorFifo, &info) == 0) {
        if (S_ISFIFO(info.st_mode)) {
            chmod(kCaptureMonitorFifo, 0666);
            return 1;
        }
        if (unlink(kCaptureMonitorFifo) != 0) {
            log_msg("capture monitor fifo invalid node unlink failed");
            return 0;
        }
    } else if (errno != ENOENT) {
        log_msg("capture monitor fifo lstat failed");
        return 0;
    }

    if (mkfifo(kCaptureMonitorFifo, 0666) != 0 && errno != EEXIST) {
        log_msg("capture monitor fifo create failed");
        return 0;
    }
    chmod(kCaptureMonitorFifo, 0666);
    log_msg("capture monitor fifo node ready");
    return 1;
}

static void ensure_monitor_fifo_open(void) {
    if (g_monitor_fifo_fd >= 0) return;
    /*
     * U2.51.7 MONITOR FIFO HANDSHAKE:
     * The previous transaction had a circular dependency: the core required
     * the monitor FIFO before publishing ON, while the daemon created the FIFO
     * only after observing ON.  Create/validate the FIFO node first, then hold
     * a persistent RDWR descriptor, and only afterwards publish the state.
     */
    if (!ensure_monitor_fifo_node()) return;
    g_monitor_fifo_fd =
        open(kCaptureMonitorFifo, O_RDWR | O_NONBLOCK);
    if (g_monitor_fifo_fd >= 0)
        log_msg("capture monitor fifo opened persistent-rdwr ABI7");
    else
        log_msg("capture monitor fifo open failed");
}

static void close_monitor_fifo(void) {
    if (g_monitor_fifo_fd >= 0) {
        close(g_monitor_fifo_fd);
        g_monitor_fifo_fd = -1;
        monitor_ring_reset();
        log_msg("capture monitor fifo closed");
    }
}

static void drain_monitor_fifo_discard(void) {
    if (g_monitor_fifo_fd < 0) return;
    int16_t discard[1024];
    for (;;) {
        ssize_t count = read(g_monitor_fifo_fd, discard, sizeof(discard));
        if (count > 0) continue;
        if (count == 0) break;
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
        close_monitor_fifo();
        break;
    }
}

static void read_monitor_fifo(void) {
    /*
     * U2.51.6 PERSISTENT_MONITOR_READER:
     * Keep the reader descriptor open after the first activation. Closing it
     * immediately on OFF raced the daemon writer and delivered SIGPIPE. While
     * disabled, discard any residual samples and keep the monitor ring empty.
     */
    if (!g_usb_monitor_enabled) {
        drain_monitor_fifo_discard();
        monitor_ring_reset();
        return;
    }
    ensure_monitor_fifo_open();
    if (g_monitor_fifo_fd < 0) return;
    int16_t buf[1024];
    for (;;) {
        ssize_t r = read(g_monitor_fifo_fd, buf, sizeof(buf));
        if (r > 0) {
            monitor_ring_push(buf, (unsigned)(r / 2));
            continue;
        }
        /* Persistent RDWR FIFO: zero bytes are transient, not a disconnect. */
        if (r == 0) break;
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
        close_monitor_fifo();
        break;
    }
}

static int usb_effective_master_percent(int master) {
    if (master <= 0) return 0;
    if (master > 100) master = 100;
    return master;
}

static int read_int_file_clamped(const char *path, int minv, int maxv, int fallback) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return fallback;
    char lb[32];
    ssize_t n = read(fd, lb, sizeof(lb)-1);
    close(fd);
    if (n <= 0) return fallback;
    lb[n] = 0;
    int v = atoi(lb);
    if (v < minv) v = minv;
    if (v > maxv) v = maxv;
    return v;
}

static void refresh_runtime_audio_profile(void) {
    int channels = read_int_file_clamped(
        kAudioChannels,
        1,
        2,
        g_usb_channels);
    int rate = read_int_file_clamped(
        kAudioRate,
        48000,
        48000,
        48000);

    if (channels != g_usb_channels || rate != g_usb_rate) {
        g_usb_channels = channels;
        g_usb_rate = rate;
        g_resample_phase_160 = 0;
        g_resample_input_fill_frames = 0;
        monitor_ring_reset();
        char message[96];
        snprintf(
            message,
            sizeof(message),
            "USB audio profile channels=%d rate=%d",
            g_usb_channels,
            g_usb_rate);
        log_msg(message);
    }
}

static void refresh_capture_status_internal(int force) {
    const long long now_ms = monotonic_milliseconds();
    if (!force && now_ms > 0 &&
        (now_ms - g_last_capture_refresh_ms) < 50)
        return;
    g_last_capture_refresh_ms = now_ms;
    int fd = open(kCaptureStatus, O_RDONLY);
    if (fd >= 0) {
        char buf[192];
        ssize_t n = read(fd, buf, sizeof(buf)-1);
        close(fd);
        if (n > 0) {
            buf[n] = 0;
            char *nl = strchr(buf, '\n');
            if (nl) *nl = 0;
            snprintf(g_capture_status, sizeof(g_capture_status), "%s", buf);
        }
    }

    fd = open(kCaptureMeta, O_RDONLY);
    if (fd >= 0) {
        char meta[1024];
        ssize_t n = read(fd, meta, sizeof(meta)-1);
        close(fd);
        if (n > 0) {
            meta[n] = 0;

            char value[320];

            if (capture_meta_value(meta, "STATE=", value, sizeof(value)))
                g_capture_state = capture_state_from_text(value);

            if (capture_meta_value(meta, "TOKEN=", value, sizeof(value)) &&
                value[0]) {
                snprintf(
                    g_capture_token,
                    sizeof(g_capture_token),
                    "%s",
                    value);
            }

            if (capture_meta_value(meta, "PATH=", value, sizeof(value)) &&
                value[0]) {
                snprintf(
                    g_last_capture_path,
                    sizeof(g_last_capture_path),
                    "%s",
                    value);
            }

            if (capture_meta_value(meta, "NAME=", value, sizeof(value)) &&
                value[0]) {
                snprintf(
                    g_last_capture_name,
                    sizeof(g_last_capture_name),
                    "%s",
                    value);
            }

            if (capture_meta_value(meta, "FRAMES=", value, sizeof(value)))
                g_capture_frames = strtol(value, 0, 10);

            if (capture_meta_value(meta, "BYTES=", value, sizeof(value)))
                g_capture_bytes = strtol(value, 0, 10);

            if (capture_meta_value(meta, "ELAPSED=", value, sizeof(value)))
                g_capture_elapsed_seconds = atoi(value);

            if (capture_meta_value(meta, "ERROR=", value, sizeof(value))) {
                snprintf(
                    g_capture_error,
                    sizeof(g_capture_error),
                    "%s",
                    value);
            }
        }
    }

    g_capture_level_percent =
        read_int_file_clamped(
            kCaptureLevel,
            0,
            100,
            g_capture_level_percent);

    g_capture_level_left_percent =
        read_int_file_clamped(
            kCaptureLevelL,
            0,
            100,
            g_capture_level_percent);

    g_capture_level_right_percent =
        read_int_file_clamped(
            kCaptureLevelR,
            0,
            100,
            g_capture_level_percent);

    g_capture_elapsed_seconds =
        read_int_file_clamped(
            kCaptureElapsed,
            0,
            120,
            g_capture_elapsed_seconds);
}

static void refresh_capture_status(void) {
    refresh_capture_status_internal(0);
}

static void basename_only(const char *path, char *dst, int len) {
    const char *b = path ? strrchr(path, '/') : 0;
    b = b ? b + 1 : path;
    if (!b) b = "";
    snprintf(dst, len, "%s", b);
}
#endif

void TreeFrogUac2Bridge_Prime(void) {
#if TREEFROG_UAC2_BRIDGE
    if (!enable_file_present()) return;
    refresh_mode_from_file(1);
    ensure_setup_started();
    refresh_usb_state();
    refresh_runtime_audio_profile();
    refresh_passive_physical_volume_file();
    refresh_capture_status();
#endif
}

void TreeFrogUac2Bridge_ResetTransport(void) {
#if TREEFROG_UAC2_BRIDGE
    g_resample_phase_160 = 0;
    g_resample_input_fill_frames = 0;
    /* AU10Y: avoid synchronous SD-card logging on every playback start.
       Repeated transport reset logs were visible in AU10J logs and can add
       perceived latency on first START after loading a project. */
#endif
}

void TreeFrogUac2Bridge_SetEngineSampleRate(int rate) {
#if TREEFROG_UAC2_BRIDGE
    if (rate <= 0) return;
    if (g_engine_rate != rate) {
        g_engine_rate = rate;
        g_resample_phase_160 = 0;
        g_resample_input_fill_frames = 0;
        g_monitor_phase = 0.0;
        log_msg("engine sample rate set");
    }
#else
    (void)rate;
#endif
}

void TreeFrogUac2Bridge_SetMixerVolumePercent(int volume) {
#if TREEFROG_UAC2_BRIDGE
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (g_mixer_volume_percent != volume) {
        g_mixer_volume_percent = volume;
        char msg[64];
        snprintf(msg, sizeof(msg), "mixer volume changed=%d", g_mixer_volume_percent);
        log_msg(msg);
    }
#else
    (void)volume;
#endif
}

void TreeFrogUac2Bridge_SetProjectMasterVolumePercent(int volume) {
#if TREEFROG_UAC2_BRIDGE
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (g_project_master_volume_percent != volume) {
        g_project_master_volume_percent = volume;
        char msg[80];
        snprintf(msg, sizeof(msg), "project master changed=%d", g_project_master_volume_percent);
        log_msg(msg);
    }
#else
    (void)volume;
#endif
}

int TreeFrogUac2Bridge_GetMixerVolumePercent(void) {
#if TREEFROG_UAC2_BRIDGE
    return g_mixer_volume_percent;
#else
    return 100;
#endif
}

static int capture_recording_active(void) {
    refresh_capture_status();
    return strstr(g_capture_status, "recording") != 0 || strstr(g_capture_status, "rec start") != 0;
}

void TreeFrogUac2Bridge_SubmitStereo48000(const int16_t *stereo, int frames) {
#if TREEFROG_UAC2_BRIDGE
    ++g_submit_count;
    if (!g_au11i2_build_marker_logged) {
        g_au11i2_build_marker_logged = 1;
        log_msg("R36SX U2.51.4 CLEAN CLOCKED USB AUDIO INPUT RESPONSIVENESS");
    }
    if (!stereo || frames <= 0) return;
    if (!enable_file_present()) return;
    if ((g_submit_count % 30) == 0) refresh_mode_from_file(0);
    ensure_setup_started();
    refresh_usb_state();
    /* RC9.6: refresh the audio profile on EVERY submit instead of every 30.
     * The SP404 daemon writes audio_channels=2 at start; a lazy refresh left
     * the first ~0.5 s of playback streaming MONO into the stereo daemon,
     * which drained the ring at 2x and buzzed until the switch. Applying the
     * profile at the first playback callback keeps the fifo layout in lockstep
     * with the daemon from sample one. */
    refresh_runtime_audio_profile();
    if ((g_submit_count % 12) == 0) refresh_passive_physical_volume_file();
    if ((g_submit_count % 60) == 0) refresh_capture_status();

    if (!mode_has_out(g_driver_mode) || !g_usb_out_allowed) {
        close_fifo_if_open("fifo closed local-or-usb-inactive");
        return;
    }
    ensure_fifo_open_nonblocking();
    if (g_fifo_fd < 0) return;

    enum {
        MAX_OUT_FRAMES = 4096,
        RESAMPLE_DENOMINATOR = 160
    };
    int16_t out[MAX_OUT_FRAMES * 2];
    int out_frames = 0;
    const int usb_master =
        usb_effective_master_percent(
            g_project_master_volume_percent);
    const int gain =
        g_mixer_volume_percent * usb_master;

    /*
     * U2.51.4 CONTINUOUS_FIXED_RATIO_160_147:
     *
     * Preserve one or more source frames across callbacks. The previous
     * block-local interpolator clamped idx+1 at the end of every 735-frame
     * callback, repeating the final sample about 60 times per second. This
     * streaming buffer produces only when a real next source frame exists,
     * so interpolation remains continuous across callback boundaries.
     *
     * H36_SINGLE_RATE_48K: the engine now renders at 48000 Hz end-to-end.
     * When the USB profile is also 48000 the increment equals the denominator
     * and the resampler degenerates to a sample-exact copy (identity).
     */
    const int resample_increment =
        (g_usb_rate <= 0 || g_engine_rate <= 0)
            ? RESAMPLE_DENOMINATOR
            : (int)((long long)g_engine_rate * RESAMPLE_DENOMINATOR /
                    g_usb_rate);
    if (resample_increment <= 0) {
        close_fifo_if_open("fifo closed invalid resample increment");
        return;
    }
    if ((unsigned)frames >= U2514_RESAMPLE_INPUT_CAPACITY_FRAMES) {
        const int keep_frames = U2514_RESAMPLE_INPUT_CAPACITY_FRAMES - 1;
        stereo += (frames - keep_frames) * 2;
        frames = keep_frames;
        g_resample_input_fill_frames = 0;
        g_resample_phase_160 = 0;
    } else if (g_resample_input_fill_frames + (unsigned)frames >
               U2514_RESAMPLE_INPUT_CAPACITY_FRAMES) {
        /* This should not occur at the normal 800-frame callback cadence.
         * Reset rather than accumulate stale audio or block the audio thread. */
        g_resample_input_fill_frames = 0;
        g_resample_phase_160 = 0;
    }

    memcpy(
        g_resample_input + (g_resample_input_fill_frames * 2U),
        stereo,
        (size_t)frames * 2U * sizeof(int16_t));
    g_resample_input_fill_frames += (unsigned)frames;

    while (out_frames < MAX_OUT_FRAMES) {
        const unsigned idx =
            g_resample_phase_160 / RESAMPLE_DENOMINATOR;
        const unsigned frac =
            g_resample_phase_160 % RESAMPLE_DENOMINATOR;
        if (idx + 1U >= g_resample_input_fill_frames)
            break;

        const int left_a = g_resample_input[idx * 2U];
        const int right_a = g_resample_input[idx * 2U + 1U];
        const int left_b = g_resample_input[(idx + 1U) * 2U];
        const int right_b = g_resample_input[(idx + 1U) * 2U + 1U];

        int left =
            (left_a * (RESAMPLE_DENOMINATOR - (int)frac) +
             left_b * (int)frac +
             (RESAMPLE_DENOMINATOR / 2)) /
            RESAMPLE_DENOMINATOR;
        int right =
            (right_a * (RESAMPLE_DENOMINATOR - (int)frac) +
             right_b * (int)frac +
             (RESAMPLE_DENOMINATOR / 2)) /
            RESAMPLE_DENOMINATOR;

        left = (left * gain + 5000) / 10000;
        right = (right * gain + 5000) / 10000;

        if (g_usb_channels == 2) {
            out[out_frames * 2] = clamp16(left);
            out[out_frames * 2 + 1] = clamp16(right);
        } else {
            out[out_frames] =
                clamp16((left + right) / 2);
        }

        ++out_frames;
        g_resample_phase_160 += (unsigned)resample_increment;
    }

    {
        unsigned consumed_frames =
            g_resample_phase_160 / RESAMPLE_DENOMINATOR;
        if (consumed_frames > 0) {
            if (consumed_frames >= g_resample_input_fill_frames) {
                g_resample_input_fill_frames = 0;
                g_resample_phase_160 = 0;
            } else {
                const unsigned remaining =
                    g_resample_input_fill_frames - consumed_frames;
                memmove(
                    g_resample_input,
                    g_resample_input + consumed_frames * 2U,
                    (size_t)remaining * 2U * sizeof(int16_t));
                g_resample_input_fill_frames = remaining;
                g_resample_phase_160 -=
                    consumed_frames * RESAMPLE_DENOMINATOR;
            }
        }
    }

    if (out_frames <= 0) return;

    /* H40: first drain any remainder of a previous partial write, so older
     * audio is never overtaken by newer blocks. */
    if (g_fifo_pending_samples > 0) {
        const size_t pend_bytes =
            (size_t)g_fifo_pending_samples * sizeof(int16_t);
        const ssize_t n = write(g_fifo_fd, g_fifo_pending, pend_bytes);
        if (n < 0) {
            if (errno == EPIPE || errno == ENXIO || errno == EBADF) {
                close_fifo_if_open(
                    "fifo closed after hard write error (pending)");
                return;
            }
            /* EAGAIN (or other nonfatal): keep staged, retry next submit. */
        } else if (n > 0) {
            const size_t consumed = (size_t)n / sizeof(int16_t);
            if (consumed >= g_fifo_pending_samples) {
                g_fifo_pending_samples = 0;
            } else {
                memmove(
                    g_fifo_pending,
                    g_fifo_pending + consumed,
                    (size_t)(g_fifo_pending_samples - consumed) *
                        sizeof(int16_t));
                g_fifo_pending_samples -=
                    (unsigned)(consumed);
            }
        }
    }

    {
        const size_t sample_count =
            (size_t)out_frames * (size_t)g_usb_channels;
        ssize_t written = write(
            g_fifo_fd,
            out,
            sample_count * sizeof(int16_t));
        if (written < 0) {
            if (errno == EPIPE || errno == ENXIO || errno == EBADF) {
                close_fifo_if_open(
                    "fifo closed after hard write error");
                return;
            }
            written = 0; /* EAGAIN: stage the whole block below. */
        }
        const size_t w_samples =
            (size_t)written / sizeof(int16_t);
        if (w_samples < sample_count) {
            size_t rem = sample_count - w_samples;
            const int16_t *src = out + w_samples;
            ++g_fifo_pending_stage_events;
            if (rem > H40_FIFO_PENDING_CAP_SAMPLES) {
                /* Pathological: even the newest block does not fit. Keep the
                 * most recent tail, drop the rest (counted). */
                src += rem - H40_FIFO_PENDING_CAP_SAMPLES;
                g_fifo_pending_drop_frames +=
                    (unsigned long)((rem - H40_FIFO_PENDING_CAP_SAMPLES) /
                                    2u);
                rem = H40_FIFO_PENDING_CAP_SAMPLES;
            }
            if ((size_t)g_fifo_pending_samples + rem >
                H40_FIFO_PENDING_CAP_SAMPLES) {
                const size_t excess =
                    (size_t)g_fifo_pending_samples + rem -
                    H40_FIFO_PENDING_CAP_SAMPLES;
                memmove(
                    g_fifo_pending,
                    g_fifo_pending + excess,
                    (size_t)(g_fifo_pending_samples - excess) *
                        sizeof(int16_t));
                g_fifo_pending_samples -= (unsigned)(excess);
                g_fifo_pending_drop_frames +=
                    (unsigned long)(excess / 2u);
            }
            memcpy(
                g_fifo_pending + g_fifo_pending_samples,
                src,
                rem * sizeof(int16_t));
            g_fifo_pending_samples += (unsigned)(rem);
            if ((g_submit_count % 240) == 0)
                log_msg("fifo backpressure: staged frames pending");
        }
    }
#else
    (void)stereo;
    (void)frames;
#endif
}

void TreeFrogUac2Bridge_MixUsbCaptureMonitorStereo48000(
    int16_t *stereo,
    int frames) {
#if TREEFROG_UAC2_BRIDGE
    if (!stereo || frames <= 0) return;
    if (!g_usb_monitor_enabled) {
        close_monitor_fifo();
        return;
    }

    refresh_runtime_audio_profile();
    read_monitor_fifo();

    const unsigned channels =
        (unsigned)(g_usb_channels == 2 ? 2 : 1);
    const unsigned prebuffer_samples =
        (unsigned)U2415_MONITOR_PREBUFFER_SAMPLES * channels;

    if (!g_monitor_primed) {
        if (g_monitor_fill < prebuffer_samples) return;
        g_monitor_primed = 1;
        log_msg("capture monitor prebuffer ready");
    }
    if (g_monitor_fill < channels * 4u) {
        g_monitor_primed = 0;
        return;
    }

    const double step =
        (g_engine_rate > 0)
            ? (double)g_usb_rate / (double)g_engine_rate
            : 1.0;

    for (int i = 0; i < frames; ++i) {
        const unsigned frame_index =
            (unsigned)g_monitor_phase;
        const double frac =
            g_monitor_phase - (double)frame_index;

        int16_t l0 = 0, l1 = 0, r0 = 0, r1 = 0;
        if (!monitor_ring_peek(
                frame_index * channels,
                &l0)) break;
        if (!monitor_ring_peek(
                (frame_index + 1u) * channels,
                &l1)) l1 = l0;

        if (channels == 2u) {
            if (!monitor_ring_peek(
                    frame_index * channels + 1u,
                    &r0)) r0 = l0;
            if (!monitor_ring_peek(
                    (frame_index + 1u) * channels + 1u,
                    &r1)) r1 = r0;
        } else {
            r0 = l0;
            r1 = l1;
        }

        int left = (int)(
            (double)l0 +
            ((double)l1 - (double)l0) * frac);
        int right = (int)(
            (double)r0 +
            ((double)r1 - (double)r0) * frac);

        /* Conservative monitor gain leaves headroom for local mixing. */
        left = (left * 75) / 100;
        right = (right * 75) / 100;

        stereo[i * 2] = clamp16(
            (int)stereo[i * 2] + left);
        stereo[i * 2 + 1] = clamp16(
            (int)stereo[i * 2 + 1] + right);

        g_monitor_phase += step;
        unsigned drop_frames =
            (unsigned)g_monitor_phase;
        if (drop_frames > 0) {
            monitor_ring_drop(
                drop_frames * channels);
            g_monitor_phase -=
                (double)drop_frames;
        }
    }
#else
    (void)stereo;
    (void)frames;
#endif
}

int TreeFrogUac2Bridge_ShouldMuteLocal(void) {
#if TREEFROG_UAC2_BRIDGE
    if (!enable_file_present()) return 0;
    if ((g_submit_count % 30) == 0) refresh_mode_from_file(0);
    refresh_usb_state();
    if ((g_submit_count % 30) == 0) refresh_passive_physical_volume_file();
    int should = should_mute_now();
    if (should != g_was_muted) {
        g_was_muted = should;
        log_msg(should ? "local output muted" : "local output restored");
    }
    return should;
#else
    return 0;
#endif
}

void TreeFrogUac2Bridge_Close(void) {
#if TREEFROG_UAC2_BRIDGE
    close_fifo_if_open("fifo closed by core");
#endif
}

int TreeFrogUac2Bridge_ShouldRequestManagedRestartShutdown(void) {
    /* This build never shuts down the frontend; mode changes apply in-app
     * through the SD apply script. */
    return 0;
}

void TreeFrogUac2Bridge_MarkCoreStarted(void) {
#if TREEFROG_UAC2_BRIDGE
    mkdir(kRuntimeDir, 0777);
    char text[160];
    snprintf(
        text,
        sizeof(text),
        "CORE_STARTED mode=%s child_pid=%ld\n",
        mode_token(g_driver_mode),
        (long)getpid());
    write_runtime_file_atomic(kH32TransitionStatus, text);
#endif
}

void TreeFrogUac2Bridge_MarkCoreUnloaded(void) {
#if TREEFROG_UAC2_BRIDGE
    close_fifo_if_open("fifo closed on core unload");
    mkdir(kRuntimeDir, 0777);
    char text[192];
    snprintf(text, sizeof(text),
        "CORE_UNLOADED reason=NORMAL_EXIT active=%s pending=%s child_pid=%ld\n",
        mode_token(g_driver_mode),
        g_pending_driver_mode >= 0 ? mode_token(g_pending_driver_mode) : "NONE",
        (long)getpid());
    write_runtime_file_atomic(kH32TransitionStatus, text);
    log_msg("core unloaded normally; frontend untouched");
#endif
}

int TreeFrogUac2Bridge_GetDriverMode(void) {
#if TREEFROG_UAC2_BRIDGE
    return g_driver_mode;
#else
    return 0;
#endif
}

const char *TreeFrogUac2Bridge_GetDriverModeName(void) {
#if TREEFROG_UAC2_BRIDGE
    return mode_name(g_driver_mode);
#else
    return "DISABLED";
#endif
}

const char *TreeFrogUac2Bridge_SetDriverMode(int mode) {
#if TREEFROG_UAC2_BRIDGE
    if (!selectable_mode(mode)) return mode_name(mode);

    const int effective =
        (mode == U241_USB_OUT && g_sampler_direction_in)
            ? U241_SP404_IN
            : mode;

    if (g_driver_mode == mode) {
        if (effective != mode) write_mode_file(effective);
        return mode_name(g_driver_mode);
    }

    const long long now_ms = monotonic_milliseconds();
    if (now_ms > 0 && (now_ms - g_last_mode_change_ms) < 180) {
        log_msg("driver mode debounce ignored");
        return mode_name(g_driver_mode);
    }
    g_last_mode_change_ms = now_ms;

    g_driver_mode = mode;
    g_pending_driver_mode = mode;
    log_msg("driver mode changed");
    write_mode_file(effective);

    /*
     * U2.41.5.2 FAST_DEVICE_SWITCH:
     * If the gadget, daemon and FIFO are already present, changing LOCAL/USB
     * is a core routing decision.  Do not fork the profile script.
     */
    if (g_driver_mode == U241_ANDROID ||
        g_driver_mode == U241_USB_OUT ||
        g_driver_mode == U241_SP404_IN ||
        g_driver_mode == U241_MIDI) {
        /*
         * U2.52 HOST_ROLE_MODE_ALWAYS_APPLY:
         * Host-role modes (Android AOA, SP404 sampler OUT/IN, MIDI) load ALSA
         * host modules, switch the musb controller to host role and start the
         * host supervisor.  A live Windows gadget/daemon contract says nothing
         * about the host-role runtime, so routing the change as a fast in-core
         * switch silently leaves snd-usb-audio unloaded and the device never
         * enumerates (SP404_CARD=none).  Always fork the profile + supervisor
         * for host-role modes.
         */
        close_fifo_if_open("fifo closed host-role apply");
        launch_apply_profile_once(effective);
        log_msg("driver mode host-role apply requested");
    } else if (runtime_ready_fast()) {
        if (g_driver_mode == U241_LOCAL_CONSOLE)
            close_fifo_if_open("fifo closed fast local-console switch");
        log_msg("driver mode fast apply runtime-ready");
    } else {
        launch_apply_profile_once(effective);
        log_msg("driver mode setup apply requested");
    }

    return mode_name(g_driver_mode);
#else
    (void)mode;
    return "DISABLED";
#endif
}

const char *TreeFrogUac2Bridge_CycleDriverMode(void) {
#if TREEFROG_UAC2_BRIDGE
    int next = (g_driver_mode + 1) %
        TreeFrogUac2Bridge_GetDriverModeCount();
    return TreeFrogUac2Bridge_SetDriverMode(next);
#else
    return "DISABLED";
#endif
}

int TreeFrogUac2Bridge_GetDriverModeCount(void) { return 5; }

int TreeFrogUac2Bridge_GetSamplerDirectionIn(void) {
#if TREEFROG_UAC2_BRIDGE
    return g_sampler_direction_in;
#else
    return 0;
#endif
}

void TreeFrogUac2Bridge_SetSamplerDirectionIn(int in) {
#if TREEFROG_UAC2_BRIDGE
    /* U2.52.5 SAMPLER_OUT_ONLY: the OUT/IN direction toggle is removed;
     * Sampler always plays console -> SP404. */
    (void)in;
#else
    (void)in;
#endif
}

const char *TreeFrogUac2Bridge_GetDriverModeNameByIndex(int mode) {
#if TREEFROG_UAC2_BRIDGE
    return mode_name(mode);
#else
    (void)mode;
    return "DISABLED";
#endif
}

const char *TreeFrogUac2Bridge_GetDriverModeDescriptionByIndex(int mode) {
#if TREEFROG_UAC2_BRIDGE
    return mode_desc(mode);
#else
    (void)mode;
    return "disabled";
#endif
}

int TreeFrogUac2Bridge_IsDriverModeSelectable(int mode) {
#if TREEFROG_UAC2_BRIDGE
    return selectable_mode(mode);
#else
    (void)mode;
    return 0;
#endif
}

const char *TreeFrogUac2Bridge_GetUsbDeviceText(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_usb_state();
    switch (detected_device()) {
    case U241_DEVICE_WINDOWS: return "Windows";
    case U241_DEVICE_ANDROID: return "Android";
    case U241_DEVICE_SP404: return "SP404MKII";
    case U241_DEVICE_MIDI: return "MIDI";
    default: return "None";
    }
#else
    return "disabled";
#endif
}

int TreeFrogUac2Bridge_ModeHasOut(int mode) {
#if TREEFROG_UAC2_BRIDGE
    return mode_has_out(mode);
#else
    (void)mode;
    return 0;
#endif
}

int TreeFrogUac2Bridge_ModeHasIn(int mode) {
#if TREEFROG_UAC2_BRIDGE
    return mode_has_in(mode);
#else
    (void)mode;
    return 0;
#endif
}

const char *TreeFrogUac2Bridge_GetUsbStateText(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_usb_state();
    const int device = detected_device();
    if (g_driver_mode == U241_LOCAL_CONSOLE) return "Local active";
    if (g_driver_mode == U241_MIDI) {
        if (midi_device_present_raw() && marker_fresh(kActiveMarker, 2))
            return "MIDI active";
        if (midi_device_present_raw()) return "MIDI device ready";
        return "MIDI waiting";
    }
    if (g_driver_mode == U241_USB_OUT) {
        if (g_usb_raw && marker_fresh(kActiveMarker, 2))
            return "USB out active";
        if (g_usb_raw) return "USB out ready";
        if (device == U241_DEVICE_SP404) return "SP404 warming";
        return "USB out inactive";
    }
    if (g_driver_mode == U241_ANDROID) {
        if (android_stream_ready_raw() && marker_fresh(kActiveMarker, 2))
            return "Android active";
        if (android_stream_ready_raw()) return "Android stream ready";
        if (exists_file(kAoaAccessory)) return "Android ready";
        if (file_contains(kAoaState, "WAIT")) return "Android waiting";
        if (file_contains(kAoaResult, "FAILED")) return "Android warning";
        return "Android starting";
    }
    if (device == U241_DEVICE_SP404) {
        if (g_usb_raw && marker_fresh(kActiveMarker, 2))
            return "SP404 active";
        if (g_usb_raw) return "SP404 ready";
        return "SP404 warming";
    }
    if (g_usb_raw && marker_fresh(kActiveMarker, 2)) return "USB active";
    if (g_usb_raw) return "USB warming";
    if (marker_fresh(kActiveMarker, 2)) return "USB marker";
    return "USB inactive";
#else
    return "disabled";
#endif
}

int TreeFrogUac2Bridge_GetUsbCaptureSnapshot(
    TreeFrogUsbCaptureSnapshot *snapshot,
    int force) {
#if TREEFROG_UAC2_BRIDGE
    if (!snapshot) return 0;

    refresh_capture_status_internal(force ? 1 : 0);
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->state = g_capture_state;
    snapshot->levelPercent = g_capture_level_percent;
    snapshot->levelLeftPercent = g_capture_level_left_percent;
    snapshot->levelRightPercent = g_capture_level_right_percent;
    snapshot->elapsedSeconds = g_capture_elapsed_seconds;
    snapshot->monitorEnabled = g_usb_monitor_enabled;
    snapshot->frames = g_capture_frames;
    snapshot->bytes = g_capture_bytes;
    snprintf(snapshot->status, sizeof(snapshot->status), "%s", g_capture_status);
    snprintf(snapshot->error, sizeof(snapshot->error), "%s", g_capture_error);
    snprintf(snapshot->path, sizeof(snapshot->path), "%s", g_last_capture_path);
    snprintf(snapshot->name, sizeof(snapshot->name), "%s", g_last_capture_name);
    return 1;
#else
    if (snapshot) memset(snapshot, 0, sizeof(*snapshot));
    (void)force;
    return 0;
#endif
}

int TreeFrogUac2Bridge_StartUsbCapture(
    const char *wav_path,
    int seconds) {
#if TREEFROG_UAC2_BRIDGE
    if (!wav_path || !wav_path[0]) return 0;

    if (g_driver_mode == U241_LOCAL_CONSOLE ||
        g_driver_mode == U241_MIDI) {
        snprintf(
            g_capture_status,
            sizeof(g_capture_status),
            "Select USB duplex/in in Audio Driver");
        g_capture_state = TREEFROG_USB_CAPTURE_ERROR;
        snprintf(
            g_capture_error,
            sizeof(g_capture_error),
            "select external audio driver");
        return 0;
    }

    /*
     * H38 SP404 RECORD ENABLE:
     * The SP404MKII is a UAC2 audio interface with a real capture endpoint
     * (SP main out -> console). Its daemon prepares and streams that capture
     * through the same START/STOP command mailbox. The generic USB_OUT mode
     * (plain gadget sampler) still has no host-side capture PCM, so it keeps
     * the block; only the SP404 host-role device is allowed.
     */
    if ((g_driver_mode == U241_USB_OUT ||
         g_driver_mode == U241_SP404_IN) &&
        !g_sampler_direction_in) {
        snprintf(
            g_capture_status,
            sizeof(g_capture_status),
            "Sampler is OUT; switch to IN (L/R in Audio Driver)");
        g_capture_state = TREEFROG_USB_CAPTURE_ERROR;
        snprintf(
            g_capture_error,
            sizeof(g_capture_error),
            "sampler direction is OUT");
        return 0;
    }

    if (g_driver_mode == U241_USB_OUT &&
        detected_device() != U241_DEVICE_SP404) {
        snprintf(
            g_capture_status,
            sizeof(g_capture_status),
            "USB OUT without SP404 has no capture path");
        g_capture_state = TREEFROG_USB_CAPTURE_ERROR;
        snprintf(
            g_capture_error,
            sizeof(g_capture_error),
            "SP404 capture unavailable");
        return 0;
    }

    if (seconds <= 0) seconds = 10;
    if (seconds > 120) seconds = 120;

    ensure_setup_started();

    char name[96];
    basename_only(wav_path, name, sizeof(name));

    if (!write_capture_command(
            "START",
            wav_path,
            seconds,
            name)) {
        snprintf(
            g_capture_status,
            sizeof(g_capture_status),
            "USB recording command failed");
        g_capture_state = TREEFROG_USB_CAPTURE_ERROR;
        snprintf(
            g_capture_error,
            sizeof(g_capture_error),
            "command write failed");
        return 0;
    }

    snprintf(g_last_capture_name, sizeof(g_last_capture_name), "%s", name);
    snprintf(g_last_capture_path, sizeof(g_last_capture_path), "%s", wav_path);
    snprintf(g_capture_status, sizeof(g_capture_status), "USB recording starting");
    g_capture_state = TREEFROG_USB_CAPTURE_STARTING;
    g_capture_frames = 0;
    g_capture_bytes = 0;
    g_capture_error[0] = 0;
    log_msg("capture start token command");
    return 1;
#else
    (void)wav_path;
    (void)seconds;
    return 0;
#endif
}

int TreeFrogUac2Bridge_StopUsbCapture(void) {
#if TREEFROG_UAC2_BRIDGE
    if (!write_capture_command("STOP", 0, 0, 0)) return 0;
    snprintf(g_capture_status, sizeof(g_capture_status), "USB recording stopping");
    g_capture_state = TREEFROG_USB_CAPTURE_STOPPING;
    log_msg("capture stop token command");
    return 1;
#else
    return 0;
#endif
}

const char *TreeFrogUac2Bridge_GetUsbCaptureStatusText(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_capture_status();
    return g_capture_status;
#else
    return "disabled";
#endif
}

int TreeFrogUac2Bridge_GetLastCaptureName(char *dst, int dst_len) {
#if TREEFROG_UAC2_BRIDGE
    if (!dst || dst_len <= 0) return 0;
    dst[0] = 0;
    int fd = open(kCaptureLastName, O_RDONLY);
    if (fd >= 0) {
        char buf[128];
        ssize_t n = read(fd, buf, sizeof(buf)-1);
        close(fd);
        if (n > 0) {
            buf[n] = 0;
            char *nl = strchr(buf, '\n');
            if (nl) *nl = 0;
            snprintf(g_last_capture_name, sizeof(g_last_capture_name), "%s", buf);
        }
    }
    if (!g_last_capture_name[0]) return 0;
    snprintf(dst, dst_len, "%s", g_last_capture_name);
    return 1;
#else
    (void)dst;
    (void)dst_len;
    return 0;
#endif
}


int TreeFrogUac2Bridge_GetLastCapturePath(char *dst, int dst_len) {
#if TREEFROG_UAC2_BRIDGE
    if (!dst || dst_len <= 0) return 0;
    dst[0] = 0;
    int fd = open(kCaptureLastPath, O_RDONLY);
    if (fd >= 0) {
        char buf[300];
        ssize_t n = read(fd, buf, sizeof(buf)-1);
        close(fd);
        if (n > 0) {
            buf[n] = 0;
            char *nl = strchr(buf, '\n');
            if (nl) *nl = 0;
            snprintf(g_last_capture_path, sizeof(g_last_capture_path), "%s", buf);
        }
    }
    if (!g_last_capture_path[0]) return 0;
    snprintf(dst, dst_len, "%s", g_last_capture_path);
    return 1;
#else
    (void)dst;
    (void)dst_len;
    return 0;
#endif
}

int TreeFrogUac2Bridge_GetUsbCaptureLevelPercent(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_capture_status();
    return g_capture_level_percent;
#else
    return 0;
#endif
}

int TreeFrogUac2Bridge_GetUsbCaptureLevelLeftPercent(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_capture_status();
    return g_capture_level_left_percent;
#else
    return 0;
#endif
}

int TreeFrogUac2Bridge_GetUsbCaptureLevelRightPercent(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_capture_status();
    return g_capture_level_right_percent;
#else
    return 0;
#endif
}

int TreeFrogUac2Bridge_GetUsbCaptureElapsedSeconds(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_capture_status();
    return g_capture_elapsed_seconds;
#else
    return 0;
#endif
}


int TreeFrogUac2Bridge_GetUsbCaptureState(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_capture_status();
    return g_capture_state;
#else
    return TREEFROG_USB_CAPTURE_IDLE;
#endif
}

long TreeFrogUac2Bridge_GetUsbCaptureFrames(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_capture_status();
    return g_capture_frames;
#else
    return 0;
#endif
}

long TreeFrogUac2Bridge_GetUsbCaptureBytes(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_capture_status();
    return g_capture_bytes;
#else
    return 0;
#endif
}

const char *TreeFrogUac2Bridge_GetUsbCaptureErrorText(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_capture_status();
    return g_capture_error;
#else
    return "disabled";
#endif
}

int TreeFrogUac2Bridge_IsUsbReady(void) {
#if TREEFROG_UAC2_BRIDGE
    if (g_driver_mode == U241_LOCAL_CONSOLE) return 0;
    refresh_usb_state();
    if (g_driver_mode == U241_MIDI)
        return midi_device_present_raw() && runtime_ready_fast();
    if (g_driver_mode == U241_ANDROID)
        return android_stream_ready_raw() && runtime_ready_fast();
    return g_usb_raw && runtime_ready_fast();
#else
    return 0;
#endif
}


int TreeFrogUac2Bridge_IsRecordingDaemonReady(void) {
#if TREEFROG_UAC2_BRIDGE
    int fd = open(kCaptureAbi, O_RDONLY);
    if (fd < 0) {
        g_capture_abi[0] = 0;
        return 0;
    }

    ssize_t count = read(
        fd,
        g_capture_abi,
        sizeof(g_capture_abi) - 1);
    close(fd);

    if (count <= 0) {
        g_capture_abi[0] = 0;
        return 0;
    }

    g_capture_abi[count] = 0;
    char *newline = strchr(g_capture_abi, '\n');
    if (newline) *newline = 0;

    if (!daemon_pid_alive()) return 0;
    if (g_driver_mode == U241_MIDI) {
        if (strcmp(g_capture_abi, "R36SX_MIDI_CAPTURE_ABI=1") != 0)
            return 0;
        if (!file_contains(
                kDaemonVersion,
                "R36SX_MIDI_DAEMON_ABI=1"))
            return 0;
        return exists_file(kMidiFifo) && midi_device_present_raw();
    }
    if ((g_driver_mode == U241_USB_OUT ||
         g_driver_mode == U241_SP404_IN) &&
        g_sampler_direction_in &&
        detected_device() == U241_DEVICE_SP404) {
        if (strcmp(g_capture_abi, "R36SX_SP404_CAPTURE_ABI=1") != 0)
            return 0;
        if (!file_contains(
                kDaemonVersion,
                "R36SX_SP404_AUDIO_DAEMON_ABI=1"))
            return 0;
        return exists_file(kSp404Fifo) && sp404_card_present_raw();
    }
    if (g_driver_mode == U241_ANDROID) {
        if (strcmp(g_capture_abi, "R36SX_CAPTURE_ABI=4") != 0)
            return 0;
        if (!file_contains(
                kDaemonVersion,
                "R36SX_AOA_BULK_AUDIO_DAEMON_ABI=4"))
            return 0;
        return exists_file(kAoaPcmFifo) && android_stream_ready_raw();
    }
    if (g_driver_mode == U241_WINDOWS) {
        if (detected_device() == U241_DEVICE_SP404) {
            if (strcmp(g_capture_abi, "R36SX_SP404_CAPTURE_ABI=1") != 0)
                return 0;
            if (!file_contains(
                    kDaemonVersion,
                    "R36SX_SP404_AUDIO_DAEMON_ABI=1"))
                return 0;
            return exists_file(kSp404Fifo) && sp404_card_present_raw();
        }
        if (strcmp(g_capture_abi, "R36SX_CAPTURE_ABI=2") != 0)
            return 0;
        if (!file_contains(
                kDaemonVersion,
                "R36SX_USB_AUDIO_DAEMON_ABI=7"))
            return 0;
        return exists_file(kFifo);
    }
    return 0;
#else
    return 0;
#endif
}

const char *TreeFrogUac2Bridge_GetRecordingDaemonVersionText(void) {
#if TREEFROG_UAC2_BRIDGE
    int fd = open(kDaemonVersion, O_RDONLY);
    if (fd < 0) {
        g_daemon_version[0] = 0;
        return "recording daemon not detected";
    }

    ssize_t count = read(
        fd,
        g_daemon_version,
        sizeof(g_daemon_version) - 1);
    close(fd);

    if (count <= 0) {
        g_daemon_version[0] = 0;
        return "recording daemon not detected";
    }

    g_daemon_version[count] = 0;
    char *newline = strchr(g_daemon_version, '\n');
    if (newline) *newline = 0;
    return g_daemon_version;
#else
    return "disabled";
#endif
}

const char *TreeFrogUac2Bridge_GetRecordingDaemonAbiText(void) {
#if TREEFROG_UAC2_BRIDGE
    TreeFrogUac2Bridge_IsRecordingDaemonReady();
    return g_capture_abi[0] ?
        g_capture_abi :
        "capture ABI file not detected";
#else
    return "disabled";
#endif
}

int TreeFrogUac2Bridge_SetUsbMonitor(int enable) {
#if TREEFROG_UAC2_BRIDGE
    const int requested = enable ? 1 : 0;

    mkdir(kRuntimeDir, 0777);

    /*
     * U2.51.6 MONITOR TRANSACTION ORDER
     *
     * When enabling, open the persistent RDWR reader before publishing ON.
     * This removes the final no-reader window rather than merely surviving it
     * with SIGPIPE ignored in the daemon.  The state is republished even when
     * the requested value equals the cached value, so a daemon-only recovery
     * can resynchronize without forcing the user to toggle twice.
     */
    if (requested) {
        ensure_monitor_fifo_open();
        if (g_monitor_fifo_fd < 0) {
            log_msg("capture monitor reader unavailable");
            return 0;
        }
        drain_monitor_fifo_discard();
        monitor_ring_reset();
    }

    const char *state = requested ? "1\n" : "0\n";
    if (!write_runtime_file_atomic(kCaptureMonitor, state)) {
        log_msg("capture monitor atomic publish failed");
        return 0;
    }

    /*
     * Monitor state is independent from the record command mailbox. Never
     * publish monitor changes through the START/STOP command channel. The
     * daemon observes kCaptureMonitor directly.
     */
    g_usb_monitor_enabled = requested;
    if (!requested) {
        /* Keep the RDWR reader alive to prevent a writer-side SIGPIPE race. */
        drain_monitor_fifo_discard();
        monitor_ring_reset();
    }

    log_msg(g_usb_monitor_enabled ?
        "capture monitor requested on atomic" :
        "capture monitor requested off atomic");
    return 1;
#else
    (void)enable;
    return 0;
#endif
}

int TreeFrogUac2Bridge_GetUsbMonitor(void) {
#if TREEFROG_UAC2_BRIDGE
    return g_usb_monitor_enabled;
#else
    return 0;
#endif
}

int TreeFrogUac2Bridge_DiscardUsbCapture(void) {
#if TREEFROG_UAC2_BRIDGE
    if (!write_capture_command("DISCARD", 0, 0, 0)) return 0;
    g_last_capture_name[0] = 0;
    g_last_capture_path[0] = 0;
    g_capture_frames = 0;
    g_capture_bytes = 0;
    g_capture_error[0] = 0;
    g_capture_state = TREEFROG_USB_CAPTURE_IDLE;
    snprintf(g_capture_status, sizeof(g_capture_status), "USB recording discarded");
    log_msg("capture discard token command");
    return 1;
#else
    return 0;
#endif
}

int TreeFrogUac2Bridge_CommitUsbCapture(void) {
#if TREEFROG_UAC2_BRIDGE
    /*
     * COMMIT clears runtime capture state while preserving the source WAV in
     * /mnt/sdcard/lgpt/samples/records.
     */
    if (!write_capture_command("COMMIT", 0, 0, 0))
        return 0;

    g_last_capture_name[0] = 0;
    g_last_capture_path[0] = 0;
    g_capture_frames = 0;
    g_capture_bytes = 0;
    g_capture_error[0] = 0;
    g_capture_state = TREEFROG_USB_CAPTURE_IDLE;
    snprintf(
        g_capture_status,
        sizeof(g_capture_status),
        "USB recording committed");
    log_msg("capture commit token command");
    return 1;
#else
    return 0;
#endif
}

