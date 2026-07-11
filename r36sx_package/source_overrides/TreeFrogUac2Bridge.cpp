#include "Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef TREEFROG_UAC2_BRIDGE
#define TREEFROG_UAC2_BRIDGE 0
#endif

#if TREEFROG_UAC2_BRIDGE

extern "C" const char *TreeFrogAu11UsbSamplerCleanBuildMarker(void) {
    return "U2_38AU11M_REVERT_INSTRUMENT_RECORDING_FIX FULL_SOURCE AU11M_THREE_AUDIO_DRIVER_MODES AU11M_MODE_CONSOLE_AUDIO AU11M_MODE_USB_INPUT_OUTPUT AU11M_MODE_EXTERNAL_RECORDING SCPI_R_R1_RIGHT USB_C_RECORD AU11M_DUPLEX_STABLE_ALWAYS_OPEN_ENDPOINTS AU11M_WINDOWS_MONITORING_FIX AU11M_USB_RECORD_VISUAL_REC_INDICATOR AU11M_USB_RECORD_COUNTDOWN_120 AU11M_CAPTURE_STAGING_COPY AU11M_REVERTED_INSTRUMENTVIEW_TO_FINAL_MIXER AU11M_USB_REC_SHORTCUT_STABLE_INSTRUMENT AU11M_HOST_HELPER_PENDING AU11M_ANDROID_OTG_TEST_PENDING AU11M_IDEMPOTENT_USB_SETUP AU11M_DAEMON_KEEPALIVE";
}
enum {
    /* AU11M_THREE_AUDIO_DRIVER_MODES
       UI names are intentionally short for the LGPT field width.
       CONSOLE_AUDIO: local console speaker only.
       USB_IN_OUT: duplex USB interface; project reaches Windows/host capture, host playback feeds USB-C RECORD.
       EXTERNAL_RECORDING: block LGPT->host USB output; keep host->console input path for external-source sampling. */
    AU11M_CONSOLE_AUDIO = 0,
    AU11M_USB_INPUT_OUTPUT = 1,
    AU11M_EXTERNAL_RECORDING = 2,
    AU10Y_LOCAL_ONLY = AU11M_CONSOLE_AUDIO,
    AU10Y_USB_OUT_AUTO_MUTE = AU11M_USB_INPUT_OUTPUT,
    AU10Y_FULL_DUPLEX = AU11M_USB_INPUT_OUTPUT,
    AU10Y_USB_IN_CAPTURE = AU11M_EXTERNAL_RECORDING
};

static int g_fifo_fd = -1;
static int g_setup_started = 0;
static unsigned long g_submit_count = 0;
static int g_driver_mode = AU10Y_USB_OUT_AUTO_MUTE;
static time_t g_mode_mtime = 0;
static int g_usb_raw = 0;
static int g_usb_out_allowed = 0;
static int g_was_muted = 0;
static unsigned long g_raw_configured_since = 0;
static unsigned long g_last_usb_check_submit = 0;
static double g_resample_phase = 0.0;
static int16_t g_last_mono = 0;
static int g_mixer_volume_percent = 100;
static int g_project_master_volume_percent = 100;
static char g_last_capture_name[96] = "";
static char g_last_capture_path[256] = "";
static char g_capture_status[96] = "USB capture idle";
static int g_capture_level_percent = 0;
static int g_capture_level_left_percent = 0;
static int g_capture_level_right_percent = 0;
static int g_capture_elapsed_seconds = 0;
static int g_usb_monitor_enabled = 0;
static int g_au11i2_build_marker_logged = 0;
static int g_monitor_fifo_fd = -1;
static double g_monitor_phase = 0.0;
#define AU10Y_MONITOR_RING_SAMPLES 32768
static int16_t g_monitor_ring[AU10Y_MONITOR_RING_SAMPLES];
static unsigned g_monitor_rpos = 0, g_monitor_wpos = 0, g_monitor_fill = 0;

static const char *kEnable = "/mnt/sdcard/lgpt/otg/enable_lgpt_uac2_bridge";
static const char *kMode = "/mnt/sdcard/lgpt/otg/audio_driver_mode";
static const char *kNoMute = "/mnt/sdcard/lgpt/otg/disable_mute_local";
static const char *kFifo = "/tmp/r36sx_uac2_bridge_fifo";
static const char *kLog = "/mnt/sdcard/lgpt/uac2_bridge_lgpt.log";
static const char *kActiveMarker = "/tmp/r36sx_uac2_usb_active";
static const char *kRuntimeDir = "/tmp/r36sx_lgpt_usb";
static const char *kRuntimeMirrorDir = "/mnt/sdcard/lgpt/otg/logs/runtime_state";
static const char *kCaptureCmd = "/tmp/r36sx_lgpt_usb/usb_capture_cmd";
static const char *kCaptureStatus = "/tmp/r36sx_lgpt_usb/usb_capture_status";
static const char *kCaptureLastName = "/tmp/r36sx_lgpt_usb/usb_capture_last_name";
static const char *kCaptureLastPath = "/tmp/r36sx_lgpt_usb/usb_capture_last_path";
static const char *kCaptureLevel = "/tmp/r36sx_lgpt_usb/usb_capture_level";
static const char *kCaptureLevelL = "/tmp/r36sx_lgpt_usb/usb_capture_level_l";
static const char *kCaptureLevelR = "/tmp/r36sx_lgpt_usb/usb_capture_level_r";
static const char *kCaptureElapsed = "/tmp/r36sx_lgpt_usb/usb_capture_elapsed";
static const char *kCaptureMonitor = "/tmp/r36sx_lgpt_usb/usb_capture_monitor";
static const char *kCaptureMonitorFifo = "/tmp/r36sx_usb_capture_monitor_fifo";
static const char *kBranchRoot = "/mnt/sdcard/lgpt/otg/branches";
static const char *kActiveBranch = "/mnt/sdcard/lgpt/otg/active_audio_branch";
static const char *kPhysicalVolumeTmp = "/tmp/r36sx_physical_volume_percent";
static const char *kPhysicalVolumePersist = "/mnt/sdcard/lgpt/otg/physical_master_volume_percent";
static time_t g_physical_volume_mtime = 0;
static int g_physical_volume_percent = -1;

static int exists_file(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0;
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
        case AU11M_CONSOLE_AUDIO: return "CONSOLE_AUDIO";
        case AU11M_EXTERNAL_RECORDING: return "EXTERNAL_RECORDING";
        case AU11M_USB_INPUT_OUTPUT:
        default: return "USB_IN_OUT";
    }
}



static const char *branch_name_for_mode(int mode) {
    switch (mode) {
        case AU11M_CONSOLE_AUDIO: return "audio_driver_console_audio";
        case AU11M_EXTERNAL_RECORDING: return "audio_driver_external_recording";
        case AU11M_USB_INPUT_OUTPUT:
        default: return "audio_driver_usb_in_out";
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
        const char *mn = mode_name(mode);
        write(fd, mn, strlen(mn));
        write(fd, "\n", 1);
        close(fd);
    }
}

static const char *mode_desc(int mode) {
    switch (mode) {
        case AU11M_CONSOLE_AUDIO: return "Console speaker only";
        case AU11M_USB_INPUT_OUTPUT: return "USB input/output interface";
        case AU11M_EXTERNAL_RECORDING: return "External USB source recording";
        default: return "Unknown";
    }
}


static int selectable_mode(int mode) {
    /* AU11M_THREE_AUDIO_DRIVER_MODES */
    return mode == AU11M_CONSOLE_AUDIO || mode == AU11M_USB_INPUT_OUTPUT || mode == AU11M_EXTERNAL_RECORDING;
}


static int mode_from_text(const char *s) {
    if (!s) return AU11M_USB_INPUT_OUTPUT;
    if (strstr(s, "CONSOLE_AUDIO") || strstr(s, "LOCAL_ONLY")) return AU11M_CONSOLE_AUDIO;
    if (strstr(s, "EXTERNAL_RECORDING") || strstr(s, "EXTERNAL_RECORD") || strstr(s, "USB_IN_CAPTURE")) return AU11M_EXTERNAL_RECORDING;
    if (strstr(s, "USB_IN_OUT") || strstr(s, "USB_INPUT_OUTPUT") || strstr(s, "USB_OUT_AUTO_MUTE") || strstr(s, "USB_OUT_PLUS_LOCAL") || strstr(s, "FULL_DUPLEX")) return AU11M_USB_INPUT_OUTPUT;
    return AU11M_USB_INPUT_OUTPUT;
}


static void log_msg(const char *msg) {
    FILE *f = fopen(kLog, "a");
    if (!f) { f = fopen("/tmp/r36sx_uac2_bridge_lgpt.log", "a"); }
    if (!f) return;
    fprintf(f,
            "AU11 %s errno=%d (%s) submit=%lu mode=%s raw=%d out=%d mute=%d mixer=%d master=%d phase=%.6f fifo=%d marker=%d capture=%s\n",
            msg ? msg : "log", errno, strerror(errno), g_submit_count,
            TreeFrogUac2Bridge_GetDriverModeName(), g_usb_raw, g_usb_out_allowed,
            g_was_muted, g_mixer_volume_percent, g_project_master_volume_percent,
            g_resample_phase, g_fifo_fd, marker_fresh(kActiveMarker, 2), g_capture_status);
    fclose(f);
}


static void au10z_write_text_file(const char *path, const char *text) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) { if (text) write(fd, text, strlen(text)); close(fd); }
}

static void au10z_mirror_runtime_file(const char *leaf, const char *text) {
    if (!leaf || !leaf[0]) return;
    mkdir("/mnt/sdcard/lgpt/otg", 0777);
    mkdir("/mnt/sdcard/lgpt/otg/logs", 0777);
    mkdir(kRuntimeMirrorDir, 0777);
    char p[256];
    snprintf(p, sizeof(p), "%s/%s", kRuntimeMirrorDir, leaf);
    au10z_write_text_file(p, text ? text : "");
}

static void launch_apply_profile_label(const char *label) {
    /* AU11M_SPLIT_USB_PROFILE_SAFE
       Do not keep a single always-duplex Windows endpoint open.  Windows can
       keep the host playback side alive with silence; on this MUSB/UAC2 stack
       that interferes with the LGPT->Windows path.  Switch profile by sampler
       context: USB_OUT_AUTO_MUTE outside record, USB_C_RECORD inside record. */
    const char *m = label ? label : "USB_OUT_AUTO_MUTE";
    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "/mnt/sdcard/lgpt/otg/bin/otg_38au11_apply_profile_once.sh", m, (char *)0);
        _exit(127);
    }
}

static void launch_apply_profile_once(int mode) {
    launch_apply_profile_label(mode_name(mode));
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

static void write_mode_file(int mode) {
    mkdir(kRuntimeDir, 0777);
    int fd = open(kMode, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        const char *m = mode_name(mode);
        write(fd, m, strlen(m));
        write(fd, "\n", 1);
        fsync(fd);
        close(fd);
    }
    write_active_branch_file(mode);
    struct stat st;
    if (stat(kMode, &st) == 0) g_mode_mtime = st.st_mtime;
}

static void refresh_mode_from_file(int force) {
    struct stat st;
    if (stat(kMode, &st) != 0) {
        if (force) write_mode_file(g_driver_mode);
        return;
    }
    if (!force && st.st_mtime == g_mode_mtime) return;
    g_mode_mtime = st.st_mtime;
    char buf[96];
    int fd = open(kMode, O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n <= 0) return;
    buf[n] = 0;
    int new_mode = mode_from_text(buf);
    if (!selectable_mode(new_mode)) new_mode = AU10Y_USB_OUT_AUTO_MUTE;
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
    if (g_submit_count != 0 && (g_submit_count - g_last_usb_check_submit) < 2) return;
    g_last_usb_check_submit = g_submit_count;
    int raw = udc_configured_raw();
    if (raw != g_usb_raw) {
        g_usb_raw = raw;
        g_raw_configured_since = raw ? (g_submit_count ? g_submit_count : 1) : 0;
        log_msg(raw ? "udc configured raw" : "udc not configured raw");
    }
    int marker = marker_fresh(kActiveMarker, 2);
    int out_now = raw || marker;
    if (out_now != g_usb_out_allowed) {
        g_usb_out_allowed = out_now;
        log_msg(out_now ? "usb out allowed" : "usb out stopped");
    }
}

static int should_mute_now(void) {
    /* AU10Y: USB_OUT_AUTO_MUTE must mute the local console as soon as the
       USB gadget is configured. AU10B depended on the playback-active marker,
       so Windows could be configured while local audio was still audible. */
    return (g_driver_mode == AU11M_USB_INPUT_OUTPUT) && !g_usb_monitor_enabled && !capture_recording_active() && !exists_file(kNoMute) && g_usb_raw;
}

static void ensure_setup_started(void) {
    if (g_setup_started) return;
    if (!exists_file(kEnable)) return;
    g_setup_started = 1;
    refresh_mode_from_file(1);
    log_msg("starting setup script nonblocking");
#if defined(PLATFORM_TREEFROG) || 1
    pid_t pid = fork();
    if (pid == 0) {
        int fd = open("/mnt/sdcard/lgpt/otg/logs/u2_38au11_setup_from_lgpt.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) fd = open("/tmp/u2_38au11_setup_from_lgpt.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); if (fd > 2) close(fd); }
        execl("/bin/sh", "sh", "/mnt/sdcard/lgpt/otg/bin/otg_38au11_lgpt_sync_setup_once.sh", (char *)0);
        _exit(127);
    }
#else
    system("sh /mnt/sdcard/lgpt/otg/bin/otg_38au11_lgpt_sync_setup_once.sh >/mnt/sdcard/lgpt/otg/logs/u2_38au11_setup_from_lgpt.log 2>&1 &");
#endif
}

static void close_fifo_if_open(const char *why) {
    if (g_fifo_fd >= 0) {
        close(g_fifo_fd);
        g_fifo_fd = -1;
        log_msg(why ? why : "fifo closed");
    }
}

static void ensure_fifo_open_nonblocking(void) {
    if (g_fifo_fd >= 0) return;
    if (!exists_file(kEnable)) return;
    ensure_setup_started();
    g_fifo_fd = open(kFifo, O_WRONLY | O_NONBLOCK);
    if (g_fifo_fd < 0) {
        if ((g_submit_count % 240) == 0) log_msg("fifo open pending");
        return;
    }
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
}

static unsigned monitor_ring_push(const int16_t *src, unsigned n) {
    unsigned pushed = 0;
    while (pushed < n && g_monitor_fill < AU10Y_MONITOR_RING_SAMPLES) {
        g_monitor_ring[g_monitor_wpos] = src[pushed++];
        g_monitor_wpos = (g_monitor_wpos + 1) % AU10Y_MONITOR_RING_SAMPLES;
        g_monitor_fill++;
    }
    return pushed;
}

static int monitor_ring_peek(unsigned idx, int16_t *out) {
    if (idx >= g_monitor_fill) return 0;
    unsigned pos = (g_monitor_rpos + idx) % AU10Y_MONITOR_RING_SAMPLES;
    if (out) *out = g_monitor_ring[pos];
    return 1;
}

static unsigned monitor_ring_drop(unsigned n) {
    unsigned dropped = 0;
    while (dropped < n && g_monitor_fill > 0) {
        g_monitor_rpos = (g_monitor_rpos + 1) % AU10Y_MONITOR_RING_SAMPLES;
        g_monitor_fill--;
        dropped++;
    }
    return dropped;
}

static void ensure_monitor_fifo_open(void) {
    if (g_monitor_fifo_fd >= 0) return;
    if (!g_usb_monitor_enabled) return;
    g_monitor_fifo_fd = open(kCaptureMonitorFifo, O_RDONLY | O_NONBLOCK);
    if (g_monitor_fifo_fd >= 0) log_msg("capture monitor fifo opened");
}

static void close_monitor_fifo(void) {
    if (g_monitor_fifo_fd >= 0) {
        close(g_monitor_fifo_fd);
        g_monitor_fifo_fd = -1;
        monitor_ring_reset();
        log_msg("capture monitor fifo closed");
    }
}

static void read_monitor_fifo(void) {
    if (!g_usb_monitor_enabled) { close_monitor_fifo(); return; }
    ensure_monitor_fifo_open();
    if (g_monitor_fifo_fd < 0) return;
    int16_t buf[1024];
    for (;;) {
        ssize_t r = read(g_monitor_fifo_fd, buf, sizeof(buf));
        if (r > 0) { monitor_ring_push(buf, (unsigned)(r / 2)); continue; }
        if (r == 0) { close_monitor_fifo(); break; }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
        close_monitor_fifo(); break;
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

static void refresh_capture_status(void) {
    int fd = open(kCaptureStatus, O_RDONLY);
    if (fd >= 0) {
        char buf[96];
        ssize_t n = read(fd, buf, sizeof(buf)-1);
        close(fd);
        if (n > 0) {
            buf[n] = 0;
            char *nl = strchr(buf, '\n');
            if (nl) *nl = 0;
            snprintf(g_capture_status, sizeof(g_capture_status), "%s", buf);
        }
    }
    g_capture_level_percent = read_int_file_clamped(kCaptureLevel, 0, 100, g_capture_level_percent);
    g_capture_level_left_percent = read_int_file_clamped(kCaptureLevelL, 0, 100, g_capture_level_percent);
    g_capture_level_right_percent = read_int_file_clamped(kCaptureLevelR, 0, 100, g_capture_level_percent);
    g_capture_elapsed_seconds = read_int_file_clamped(kCaptureElapsed, 0, 120, g_capture_elapsed_seconds);
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
    if (!exists_file(kEnable)) return;
    refresh_mode_from_file(1);
    ensure_setup_started();
    refresh_usb_state();
    refresh_passive_physical_volume_file();
    refresh_capture_status();
#endif
}

void TreeFrogUac2Bridge_ResetTransport(void) {
#if TREEFROG_UAC2_BRIDGE
    g_resample_phase = 0.0;
    g_last_mono = 0;
    /* AU10Y: avoid synchronous SD-card logging on every playback start.
       Repeated transport reset logs were visible in AU10J logs and can add
       perceived latency on first START after loading a project. */
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

void TreeFrogUac2Bridge_SubmitStereo44100(const int16_t *stereo, int frames) {
#if TREEFROG_UAC2_BRIDGE
    ++g_submit_count;
    if (!g_au11i2_build_marker_logged) {
        g_au11i2_build_marker_logged = 1;
        log_msg("U2_38AU11M_REVERT_INSTRUMENT_RECORDING_FIX AU11M_DUPLEX_STABLE_ALWAYS_OPEN AU11M_WINDOWS_COMPANION_READY");
    }
    if (!stereo || frames <= 0) return;
    if (!exists_file(kEnable)) return;
    if ((g_submit_count % 30) == 0) refresh_mode_from_file(0);
    ensure_setup_started();
    refresh_usb_state();
    if ((g_submit_count % 12) == 0) refresh_passive_physical_volume_file();
    if ((g_submit_count % 60) == 0) refresh_capture_status();
    if (g_driver_mode == AU11M_CONSOLE_AUDIO || g_driver_mode == AU11M_EXTERNAL_RECORDING || capture_recording_active() || g_usb_monitor_enabled || !g_usb_out_allowed) {
        close_fifo_if_open("fifo closed console-external-record-monitor-or-usb-inactive");
        return;
    }
    ensure_fifo_open_nonblocking();
    if (g_fifo_fd < 0) return;
    enum { MAX_OUT = 4096 };
    int16_t out[MAX_OUT];
    int out_frames = 0;
    const double step = 44100.0 / 48000.0;
    while (g_resample_phase < (double)frames && out_frames < MAX_OUT) {
        int idx = (int)g_resample_phase;
        double frac = g_resample_phase - (double)idx;
        int16_t a = (idx <= 0) ? ((idx == 0) ? mono_from_stereo(stereo, 0) : g_last_mono)
                                : ((idx < frames) ? mono_from_stereo(stereo, idx) : mono_from_stereo(stereo, frames - 1));
        int16_t b = (idx + 1 < frames) ? mono_from_stereo(stereo, idx + 1) : a;
        int v = (int)((double)a + ((double)b - (double)a) * frac);
        int usb_master = usb_effective_master_percent(g_project_master_volume_percent);
        int gain = g_mixer_volume_percent * usb_master;
        v = (v * gain + 5000) / 10000;
        out[out_frames++] = clamp16(v);
        g_resample_phase += step;
    }
    g_resample_phase -= (double)frames;
    if (g_resample_phase < 0.0) g_resample_phase = 0.0;
    g_last_mono = mono_from_stereo(stereo, frames - 1);
    if (out_frames <= 0) return;
    ssize_t w = write(g_fifo_fd, out, (size_t)out_frames * sizeof(int16_t));
    if (w < 0) {
        if (errno == EPIPE || errno == ENXIO || errno == EBADF) close_fifo_if_open("fifo closed after hard write error");
        else if ((g_submit_count % 240) == 0) log_msg("fifo write nonfatal error");
    }
#else
    (void)stereo;
    (void)frames;
#endif
}


void TreeFrogUac2Bridge_MixUsbCaptureMonitorStereo44100(int16_t *stereo, int frames) {
#if TREEFROG_UAC2_BRIDGE
    if (!stereo || frames <= 0) return;
    if (!g_usb_monitor_enabled) { close_monitor_fifo(); return; }
    read_monitor_fifo();
    if (g_monitor_fill < 4) return;
    const double step = 48000.0 / 44100.0;
    for (int i = 0; i < frames; ++i) {
        unsigned idx = (unsigned)g_monitor_phase;
        int16_t a = 0, b = 0;
        if (!monitor_ring_peek(idx, &a)) break;
        if (!monitor_ring_peek(idx + 1, &b)) b = a;
        double frac = g_monitor_phase - (double)idx;
        int v = (int)((double)a + ((double)b - (double)a) * frac);
        /* Conservative 75% monitor gain to avoid clipping with local output. */
        v = (v * 75) / 100;
        int l = stereo[i * 2] + v;
        int r = stereo[i * 2 + 1] + v;
        stereo[i * 2] = clamp16(l);
        stereo[i * 2 + 1] = clamp16(r);
        g_monitor_phase += step;
        unsigned drop = (unsigned)g_monitor_phase;
        if (drop > 0) { monitor_ring_drop(drop); g_monitor_phase -= (double)drop; }
    }
#else
    (void)stereo; (void)frames;
#endif
}

int TreeFrogUac2Bridge_ShouldMuteLocal(void) {
#if TREEFROG_UAC2_BRIDGE
    if (!exists_file(kEnable)) return 0;
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
    if (g_driver_mode != mode) {
        g_driver_mode = mode;
        log_msg("driver mode changed");
    }
    /* AU10Y: always rewrite + direct-apply, even if the same item is selected.
       This recovers when Windows still holds the previous USB OUT descriptor. */
    write_mode_file(g_driver_mode);
    launch_apply_profile_once(g_driver_mode);
    log_msg("driver mode direct apply requested");
    return mode_name(g_driver_mode);
#else
    (void)mode;
    return "DISABLED";
#endif
}

const char *TreeFrogUac2Bridge_CycleDriverMode(void) {
#if TREEFROG_UAC2_BRIDGE
    int next = g_driver_mode + 1;
    if (next > AU11M_EXTERNAL_RECORDING) next = AU11M_CONSOLE_AUDIO;
    return TreeFrogUac2Bridge_SetDriverMode(next);
#else
    return "DISABLED";
#endif
}

int TreeFrogUac2Bridge_GetDriverModeCount(void) { return 3; }

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

const char *TreeFrogUac2Bridge_GetUsbStateText(void) {
#if TREEFROG_UAC2_BRIDGE
    refresh_usb_state();
    refresh_capture_status();
    if (g_usb_raw && marker_fresh(kActiveMarker, 2)) return "USB active";
    if (g_usb_raw) return "USB warming";
    if (marker_fresh(kActiveMarker, 2)) return "USB marker";
    return "USB inactive";
#else
    return "disabled";
#endif
}

int TreeFrogUac2Bridge_StartUsbCapture(const char *wav_path, int seconds) {
#if TREEFROG_UAC2_BRIDGE
    if (!wav_path || !wav_path[0]) return 0;
    if (seconds <= 0) seconds = 15;
    if (seconds > 120) seconds = 120;
    ensure_setup_started();
    mkdir(kRuntimeDir, 0777);
    int fd = open(kCaptureCmd, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        log_msg("capture cmd open failed");
        return 0;
    }
    char name[96];
    basename_only(wav_path, name, sizeof(name));
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "START\nPATH=%s\nSECONDS=%d\nNAME=%s\n", wav_path, seconds, name);
    write(fd, buf, n);
    close(fd);
    au10z_mirror_runtime_file("usb_capture_cmd", buf);
    snprintf(g_last_capture_name, sizeof(g_last_capture_name), "%s", name);
    snprintf(g_last_capture_path, sizeof(g_last_capture_path), "%s", wav_path);
    snprintf(g_capture_status, sizeof(g_capture_status), "USB rec start %ds", seconds);
    log_msg("capture start command");
    return 1;
#else
    (void)wav_path;
    (void)seconds;
    return 0;
#endif
}

int TreeFrogUac2Bridge_StopUsbCapture(void) {
#if TREEFROG_UAC2_BRIDGE
    mkdir(kRuntimeDir, 0777);
    int fd = open(kCaptureCmd, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return 0;
    write(fd, "STOP\n", 5);
    close(fd);
    au10z_mirror_runtime_file("usb_capture_cmd", "STOP\n");
    snprintf(g_capture_status, sizeof(g_capture_status), "USB rec stopping");
    log_msg("capture stop command");
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

int TreeFrogUac2Bridge_SetUsbMonitor(int enable) {
#if TREEFROG_UAC2_BRIDGE
    g_usb_monitor_enabled = enable ? 1 : 0;
    mkdir(kRuntimeDir, 0777);
    int fd = open(kCaptureMonitor, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        const char *mon = g_usb_monitor_enabled ? "1\n" : "0\n";
        write(fd, mon, strlen(mon));
        close(fd);
        /* AU11M_NO_SYNC_SD_MIRROR_ON_MONITOR_TOGGLE AU11M_FIRST_SAMPLE_A_ARM_ONLY AU11M_NO_REOPEN_ON_RECORD_EXIT: keep menu entry responsive.
           The daemon log records MONITOR_ON/OFF; runtime mirror is not worth
           synchronous SD I/O from the audio/UI thread. */
    }
    /* AU11M_DUPLEX_STABLE_ALWAYS_OPEN_ENDPOINTS: keep Windows playback and recording endpoints present all the time.
       Do not recreate the USB gadget when entering/exiting USB-C RECORD. Direction is gated in the core/daemon:
       monitor ON blocks LGPT->USB and opens PC->console capture monitor; monitor OFF closes capture monitor and recovers LGPT->USB. */
    int cfd = open(kCaptureCmd, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (cfd >= 0) {
        const char *cmd = g_usb_monitor_enabled ? "MONITOR=1\n" : "MONITOR=0\nRECOVER_OUT=1\n";
        write(cfd, cmd, strlen(cmd));
        close(cfd);
    }
    if (g_usb_monitor_enabled) {
        close_fifo_if_open("fifo closed usb-record-monitor-enter");
        monitor_ring_reset();
    } else {
        close_monitor_fifo();
        monitor_ring_reset();
        close_fifo_if_open("fifo closed usb-record-monitor-exit-force-reopen");
        g_resample_phase = 0.0;
        g_last_mono = 0;
    }
    /* AU11M_NO_REOPEN_ON_RECORD_EXIT: no synchronous SD mirror or FIFO open from UI/audio thread. */
    log_msg(g_usb_monitor_enabled ? "capture monitor requested on" : "capture monitor requested off");
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
    mkdir(kRuntimeDir, 0777);
    int fd = open(kCaptureCmd, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return 0;
    write(fd, "DISCARD\n", 8);
    close(fd);
    au10z_mirror_runtime_file("usb_capture_cmd", "DISCARD\n");
    g_last_capture_name[0] = 0;
    snprintf(g_capture_status, sizeof(g_capture_status), "USB rec discarded");
    log_msg("capture discard command");
    return 1;
#else
    return 0;
#endif
}
