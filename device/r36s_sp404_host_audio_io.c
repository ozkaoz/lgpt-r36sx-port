/*
 * r36s_sp404_host_audio_io.c
 * R36SX LGPT unified driver host-side SP404MKII backend.
 *
 * The R36SX is USB host; the SP404MKII is a class-compliant UAC2 audio
 * interface. snd-usb-audio exposes it as an ALSA card (typically pcmC1D0).
 *
 * Playback/output path (LGPT -> SP404):
 *   LGPT core -> FIFO 48 kHz mono/stereo S16_LE -> /dev/snd/pcmC{N}D0p.
 *
 * Capture/input path (SP404 -> LGPT):
 *   SP404 line/audio -> UAC2 capture endpoint -> /dev/snd/pcmC{N}D0c -> WAV file.
 *
 * Capture is command driven through /tmp/r36sx_lgpt_usb/usb_capture_cmd.
 * Readiness is derived from the sp404_card marker written by the SD setup
 * script, never from the gadget UDC.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
/* ASRC FIR selector: 8-tap Lanczos-4 (default, deployed behavior) or
 * 16-tap Lanczos-8 (maximum quality) via -DASRC_FIR_TAPS=16. */
#ifndef ASRC_FIR_TAPS
#define ASRC_FIR_TAPS 8
#endif
#if ASRC_FIR_TAPS == 16
#include "h36_14_fir16_q14.h"
#define kASRC_FIR_TABLE kTreeFrogFrontendFir16Q14
#define ASRC_FIR_HALF 7U
#define ASRC_FIR_LOOKAHEAD 8U
#define ASRC_FIR_PRIME 7U
#else
#include "h36_14_fir8_q14.h"
#define kASRC_FIR_TABLE kTreeFrogFrontendFir8Q14
#define ASRC_FIR_HALF 3U
#define ASRC_FIR_LOOKAHEAD 4U
#define ASRC_FIR_PRIME 3U
#endif

#include <time.h>
#include <unistd.h>
#include <sound/asound.h>

/* P1: FIFO dump disabled by default (production). Enable with -DSP404_ENABLE_FIFO_DUMP=1 */
#ifndef SP404_ENABLE_FIFO_DUMP
#define SP404_ENABLE_FIFO_DUMP 0
#endif

/* v14.2: USBDEVFS_RESET for the SP404 bus-reset self-heal. Defined here so the
 * build does not depend on linux/usbdevice_fs.h being present in the sysroot. */
#ifndef USBDEVFS_RESET
#define USBDEVFS_RESET _IO('U', 20)
#endif

static const char *ACTIVE_MARKER = "/tmp/r36sx_uac2_usb_active";
static const char *LOWLAT_SENTINEL = "/mnt/sdcard/lgpt/otg/lowlat_240";
static const char *RUNTIME_DIR = "/tmp/r36sx_lgpt_usb";
static const char *SP404_CARD = "/tmp/r36sx_lgpt_usb/sp404_card";
static const char *CAPTURE_CMD = "/tmp/r36sx_lgpt_usb/usb_capture_cmd";
static const char *CAPTURE_STATUS = "/tmp/r36sx_lgpt_usb/usb_capture_status";
static const char *CAPTURE_META = "/tmp/r36sx_lgpt_usb/usb_capture_meta";
static const char *DAEMON_VERSION = "/tmp/r36sx_lgpt_usb/daemon_version";
static const char *CAPTURE_ABI = "/tmp/r36sx_lgpt_usb/capture_abi";
static const char *CAPTURE_LAST_NAME = "/tmp/r36sx_lgpt_usb/usb_capture_last_name";
static const char *CAPTURE_LAST_PATH = "/tmp/r36sx_lgpt_usb/usb_capture_last_path";
static const char *CAPTURE_LEVEL = "/tmp/r36sx_lgpt_usb/usb_capture_level";
static const char *CAPTURE_LEVEL_L = "/tmp/r36sx_lgpt_usb/usb_capture_level_l";
static const char *CAPTURE_LEVEL_R = "/tmp/r36sx_lgpt_usb/usb_capture_level_r";
static const char *CAPTURE_ELAPSED = "/tmp/r36sx_lgpt_usb/usb_capture_elapsed";
static const char *CAPTURE_MONITOR = "/tmp/r36sx_lgpt_usb/usb_capture_monitor";
static const char *CAPTURE_MONITOR_FIFO = "/tmp/r36sx_usb_capture_monitor_fifo";
static const char *RUNTIME_MIRROR_DIR = "/mnt/sdcard/lgpt/otg/logs/runtime_state";
static const char *AUDIO_PROFILE = "/tmp/r36sx_lgpt_usb/audio_profile";
static const char *AUDIO_CHANNELS = "/tmp/r36sx_lgpt_usb/audio_channels";
static const char *AUDIO_RATE = "/tmp/r36sx_lgpt_usb/audio_rate";
static const char *PLAYBACK_PCM_STATUS = "/tmp/r36sx_lgpt_usb/playback_pcm_status";
static const char *AUDIO_MODE_FILE = "/mnt/sdcard/lgpt/otg/audio_driver_mode";
static int g_dir_out = 1;
static unsigned long long g_dir_check_ms = 0;
// H44 EVENT-DRIVEN USB STATE counters (20)
static unsigned long g_cnt_usb_marker_checks = 0;
static unsigned long g_cnt_usb_marker_reads = 0;
static unsigned long g_cnt_card_rechecks = 0;
static unsigned long g_cnt_audio_mode_reads = 0;
static unsigned long g_cnt_capture_cmd_polls = 0;
static unsigned long g_cnt_monitor_polls = 0;
static unsigned long g_cnt_slow_control_ticks = 0;
static unsigned long long g_last_slow_control_ms = 0;
static unsigned long long g_last_perf_stats_ms = 0;
enum { USB_UNKNOWN=0, USB_READY=1, USB_STREAMING=2, USB_RECOVERING=3 };
static int g_usb_state = USB_UNKNOWN;
static unsigned g_audio_channels = 1;
static unsigned g_audio_rate = 48000;
static int g_dev_play_format = SNDRV_PCM_FORMAT_S16_LE;
static unsigned g_dev_play_bytes = 2;
static unsigned g_dev_play_channels = 1;
static unsigned g_dev_play_rate = 48000;
static int g_dev_cap_format = SNDRV_PCM_FORMAT_S16_LE;
static unsigned g_dev_cap_bytes = 2;
static unsigned g_dev_cap_channels = 1;
static unsigned g_dev_cap_rate = 48000;
static unsigned g_dev_play_shift = 0;
static unsigned g_dev_cap_shift = 0;
/* v14.3: graceful daemon stop. Killing a live streaming process with a plain
 * SIGTERM while musb URB transfers are in flight wedges the controller on this
 * SoC (crash al volver a Sampler). SIGUSR1/SIGTERM/SIGINT all set this flag;
 * the main loop checks it at a safe point (between ALSA calls) and closes the
 * PCMs cleanly from its own context before exiting. */
static volatile sig_atomic_t g_sp404_stop_requested = 0;

static void sp404_stop_signal_handler(int sig) {
    (void)sig;
    g_sp404_stop_requested = 1;
}

static unsigned fmt_shift(int fmt) {
    switch (fmt) {
    case SNDRV_PCM_FORMAT_S16_LE:
    case SNDRV_PCM_FORMAT_S16_BE: return 0;
    case SNDRV_PCM_FORMAT_S24_LE:
    case SNDRV_PCM_FORMAT_S24_3LE: return 8;
    case SNDRV_PCM_FORMAT_S32_LE:
    case SNDRV_PCM_FORMAT_S32_BE: return 16;
    default: return 8;
    }
}

static void loge(const char *msg) { fprintf(stderr, "%s errno=%d (%s)\n", msg, errno, strerror(errno)); }
static void die_errno(const char *msg) { loge(msg); exit(2); }
static struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n) { return &p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]; }
static struct snd_interval *param_to_interval(struct snd_pcm_hw_params *p, int n) { return &p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]; }
static void mask_any(struct snd_mask *m) { size_t i; for (i = 0; i < sizeof(m->bits) / sizeof(m->bits[0]); ++i) m->bits[i] = 0xffffffffU; }
static void mask_none(struct snd_mask *m) { memset(m, 0, sizeof(*m)); }
static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned int bit) { struct snd_mask *m = param_to_mask(p, n); mask_none(m); m->bits[bit >> 5] |= (1U << (bit & 31)); }
static void interval_any(struct snd_interval *i) { memset(i, 0, sizeof(*i)); i->min = 0; i->max = 0xffffffffU; }
static void interval_set(struct snd_pcm_hw_params *p, int n, unsigned int val) { struct snd_interval *i = param_to_interval(p, n); memset(i, 0, sizeof(*i)); i->min = val; i->max = val; i->integer = 1; }
static void init_hw_params(struct snd_pcm_hw_params *p) { int n; memset(p, 0, sizeof(*p)); for (n = SNDRV_PCM_HW_PARAM_FIRST_MASK; n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++) mask_any(param_to_mask(p, n)); for (n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL; n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++) interval_any(param_to_interval(p, n)); p->rmask = ~0U; }
static int param_mask_supports(struct snd_pcm_hw_params *p, int n, unsigned int bit) { struct snd_mask *m = param_to_mask(p, n); return (m->bits[bit >> 5] & (1U << (bit & 31))) != 0; }
static void interval_minmax(struct snd_pcm_hw_params *p, int n, unsigned int *mn, unsigned int *mx) { struct snd_interval *i = param_to_interval(p, n); *mn = i->min; *mx = i->max; }
static int path_exists(const char *p) { struct stat st; return p && stat(p, &st) == 0; }
static void sleep_ms(int ms) { usleep((useconds_t)ms * 1000U); }
static int mask_first_bit(const struct snd_mask *m, int lo, int hi) { int b; for (b = lo; b <= hi; ++b) if (m->bits[b >> 5] & (1U << (b & 31))) return b; return -1; }

static int pcm_pick_device_config(
    int fd,
    unsigned want_rate,
    unsigned want_channels,
    unsigned want_period_frames,
    unsigned want_periods,
    int *out_format,
    unsigned *out_bytes,
    unsigned *out_channels,
    unsigned *out_rate,
    unsigned *out_period_frames,
    unsigned *out_periods) {
    struct snd_pcm_hw_params hw;
    init_hw_params(&hw);
    param_set_mask(&hw, SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    param_set_mask(&hw, SNDRV_PCM_HW_PARAM_SUBFORMAT, SNDRV_PCM_SUBFORMAT_STD);
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, &hw) < 0) {
        fprintf(stderr, "CONSTRAINT_REFINE_FAIL errno=%d (%s)\n",
                errno, strerror(errno));
        return -errno;
    }

    static const int fmt_pref[] = {
        SNDRV_PCM_FORMAT_S16_LE,
        SNDRV_PCM_FORMAT_S24_LE,
        SNDRV_PCM_FORMAT_S24_3LE,
        SNDRV_PCM_FORMAT_S32_LE,
        SNDRV_PCM_FORMAT_S16_BE,
        SNDRV_PCM_FORMAT_S32_BE
    };
    static const unsigned fmt_bytes[] = { 2, 4, 3, 4, 2, 4 };
    int fmt = -1, fi;
    unsigned bytes = 2;
    for (fi = 0; fi < (int)(sizeof(fmt_pref) / sizeof(fmt_pref[0])); ++fi) {
        if (param_mask_supports(&hw, SNDRV_PCM_HW_PARAM_FORMAT, fmt_pref[fi])) {
            fmt = fmt_pref[fi];
            bytes = fmt_bytes[fi];
            break;
        }
    }
    if (fmt < 0) {
        int raw = mask_first_bit(
            param_to_mask(&hw, SNDRV_PCM_HW_PARAM_FORMAT),
            0, 63);
        if (raw < 0) { fprintf(stderr, "CONSTRAINT_FORMAT_UNSUPPORTED\n"); return -EINVAL; }
        fmt = raw;
        bytes = (fmt == SNDRV_PCM_FORMAT_S16_LE ||
                 fmt == SNDRV_PCM_FORMAT_S16_BE) ? 2 : 4;
    }

    unsigned int ch_min, ch_max;
    interval_minmax(&hw, SNDRV_PCM_HW_PARAM_CHANNELS, &ch_min, &ch_max);
    if (ch_min < 1) ch_min = 1;
    unsigned int channels = want_channels;
    if (channels < ch_min) channels = ch_min;
    if (channels > ch_max) channels = ch_max;

    unsigned int rate_min, rate_max;
    interval_minmax(&hw, SNDRV_PCM_HW_PARAM_RATE, &rate_min, &rate_max);
    unsigned int rate = want_rate;
    if (rate < rate_min) rate = rate_min;
    if (rate > rate_max) rate = rate_max;

    unsigned int per_min, per_max;
    interval_minmax(&hw, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, &per_min, &per_max);
    unsigned int period_frames = want_period_frames;
    if (period_frames < per_min) period_frames = per_min;
    if (period_frames > per_max) period_frames = per_max;
    if (period_frames < 1) period_frames = 1;

    unsigned int pds_min, pds_max;
    interval_minmax(&hw, SNDRV_PCM_HW_PARAM_PERIODS, &pds_min, &pds_max);
    unsigned int periods = want_periods;
    if (periods < pds_min) periods = pds_min;
    if (periods > pds_max) periods = pds_max;
    if (periods < 1) periods = 1;

    unsigned int buf_min, buf_max;
    interval_minmax(&hw, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, &buf_min, &buf_max);
    unsigned int buffer_frames = period_frames * periods;
    if (buffer_frames < buf_min) buffer_frames = buf_min;
    if (buffer_frames > buf_max) buffer_frames = buf_max;

    fprintf(stderr,
            "CONSTRAINT_PICKED fmt=%d bytes=%u channels=%u rate=%u period=%u periods=%u buffer=%u (wanted rate=%u ch=%u)\n",
            fmt, bytes, channels, rate, period_frames, periods, buffer_frames,
            want_rate, want_channels);

    *out_format = fmt; *out_bytes = bytes; *out_channels = channels;
    *out_rate = rate; *out_period_frames = period_frames; *out_periods = periods;
    return 0;
}

static unsigned long long monotonic_milliseconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return ((unsigned long long)ts.tv_sec * 1000ULL) +
           ((unsigned long long)ts.tv_nsec / 1000000ULL);
}

/* H42: runtime_state mirroring to SD is compiled out by default: the daemon
 * must perform zero SD writes during normal operation (tmpfs-only runtime).
 * Re-enable with -DLGPT_SD_RUNTIME_MIRROR=1 for field debugging. */
#ifndef LGPT_SD_RUNTIME_MIRROR
#define LGPT_SD_RUNTIME_MIRROR 0
#endif
static void mirror_runtime_state(const char *path, const char *text) {
#if LGPT_SD_RUNTIME_MIRROR
    const char *base = path ? strrchr(path, '/') : 0;
    base = base ? base + 1 : path;
    if (!base || !base[0]) return;
    mkdir("/mnt/sdcard/lgpt/otg", 0777);
    mkdir("/mnt/sdcard/lgpt/otg/logs", 0777);
    mkdir(RUNTIME_MIRROR_DIR, 0777);
    char out[256];
    snprintf(out, sizeof(out), "%s/%s", RUNTIME_MIRROR_DIR, base);
    int fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) { if (text) write(fd, text, strlen(text)); close(fd); }
#else
    (void)path;
    (void)text;
    (void)RUNTIME_MIRROR_DIR;
#endif
}

static int file_contains(const char *p, const char *needle) {
    g_cnt_usb_marker_reads++;
    char buf[128]; int fd = open(p, O_RDONLY); if (fd < 0) return 0;
    ssize_t n = read(fd, buf, sizeof(buf)-1); close(fd); if (n <= 0) return 0;
    buf[n] = 0; return strstr(buf, needle) != 0;
}
static int usb_configured(void) {
    g_cnt_usb_marker_checks++;
    // path_exists also stat, count as check
    int a = file_contains(SP404_CARD, "FAILED");
    int b = file_contains(SP404_CARD, "none");
    int c = path_exists(SP404_CARD);
    g_cnt_usb_marker_reads++; // for path_exists
    return a == 0 && b == 0 && c;
}

static int usb_configured_cached(void) {
    static unsigned long long last_check_ms = 0;
    static int cached = 0;
    const unsigned long long now = monotonic_milliseconds();
    // H44: safety fallback only every 5000ms and never in streaming hot path
    if (last_check_ms == 0 || now < last_check_ms ||
        (now - last_check_ms) >= 5000ULL) {
        cached = usb_configured();
        last_check_ms = now;
    }
    return cached;
}
static int runtime_value_is_volatile(const char *path) {
    return path && (
        strcmp(path, CAPTURE_LEVEL) == 0 ||
        strcmp(path, CAPTURE_LEVEL_L) == 0 ||
        strcmp(path, CAPTURE_LEVEL_R) == 0 ||
        strcmp(path, CAPTURE_ELAPSED) == 0 ||
        strcmp(path, CAPTURE_STATUS) == 0);
}

static void write_text_file(const char *path, const char *text) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        if (text) write(fd, text, strlen(text));
        close(fd);
    }

    /*
     * U2.51.4 RUNTIME_IO_GOVERNOR:
     * Meter values are consumed live from tmpfs by the core. Mirroring L/R,
     * aggregate level and elapsed time to the SD on every 480-frame read caused
     * dozens of synchronous writes per second while Record was open. Keep those
     * values volatile; since H42 the SD mirror itself is compiled out (zero
     * SD writes at runtime) unless LGPT_SD_RUNTIME_MIRROR=1.
     */
    if (path &&
        strstr(path, RUNTIME_DIR) == path &&
        !runtime_value_is_volatile(path)) {
        mirror_runtime_state(path, text);
    }
}
static void mark_active(void) { char b[64]; snprintf(b, sizeof(b), "%ld\n", (long)time(NULL)); write_text_file(ACTIVE_MARKER, b); }
static void mark_inactive(void) { unlink(ACTIVE_MARKER); }

/* H42: the capture WAV is always written to the tmpfs path requested by the
 * core (/tmp/r36sx_lgpt_record/). The old fallback to the SD staging dir is
 * removed: a failed tmpfs open is a clean, logged error (the core surfaces
 * it in the Record modal) instead of a silent SD write. */
static int open_capture_wav_with_fallback(const char *requested, const char *name, char *actual, int actual_len) {
    if (requested && requested[0]) snprintf(actual, actual_len, "%s", requested);
    else snprintf(actual, actual_len, "%s/%s", "/tmp/r36sx_lgpt_record", name && name[0] ? name : "USBREC.wav");
    int fd = open(actual, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd >= 0) return fd;
    int first_errno = errno;
    if (first_errno == EEXIST) {
        fprintf(stderr, "CAPTURE_DUPLICATE_BLOCKED path=%s\n", actual);
        return -1;
    }
    fprintf(stderr, "CAPTURE_OPEN_REQUESTED_FAILED path=%s errno=%d (%s)\n", actual, first_errno, strerror(first_errno));
    return -1;
}

static int pcm_prepare_common(
    int fd,
    int is_capture,
    unsigned rate,
    unsigned channels,
    unsigned period_frames,
    unsigned periods) {
    const char *direction = is_capture ? "CAP" : "PLAY";
    int version = 0;
    struct snd_pcm_info info;
    struct snd_pcm_hw_params hw;
    struct snd_pcm_sw_params sw;

#define PCM_TRY_IOCTL(request, argument, stage)                              \
    do {                                                                    \
        if (ioctl(fd, (request), (argument)) < 0) {                         \
            const int saved_errno = errno;                                  \
            fprintf(                                                        \
                stderr,                                                     \
                "%s_PCM_CONFIG_FAIL stage=%s errno=%d (%s) "               \
                "rate=%u channels=%u period_frames=%u periods=%u\n",        \
                direction,                                                  \
                (stage),                                                    \
                saved_errno,                                                \
                strerror(saved_errno),                                      \
                rate,                                                       \
                channels,                                                   \
                period_frames,                                              \
                periods);                                                   \
            return -saved_errno;                                            \
        }                                                                   \
    } while (0)

    PCM_TRY_IOCTL(
        SNDRV_PCM_IOCTL_PVERSION,
        &version,
        "PVERSION");
    fprintf(
        stderr,
        "%s_PCM_VERSION=%d.%d.%d\n",
        direction,
        (version >> 16) & 0xff,
        (version >> 8) & 0xff,
        version & 0xff);

    memset(&info, 0, sizeof(info));
    PCM_TRY_IOCTL(
        SNDRV_PCM_IOCTL_INFO,
        &info,
        "INFO");
    fprintf(
        stderr,
        "%s_PCM_INFO card=%u device=%u subdevice=%u id=%s name=%s subname=%s\n",
        direction,
        info.card,
        info.device,
        info.subdevice,
        info.id,
        info.name,
        info.subname);

    int dev_fmt = SNDRV_PCM_FORMAT_S16_LE;
    unsigned dev_bytes = 2;
    unsigned dev_channels = channels;
    unsigned dev_rate = rate;
    unsigned dev_period_frames = period_frames;
    unsigned dev_periods = periods;
    if (pcm_pick_device_config(
            fd, rate, channels, period_frames, periods,
            &dev_fmt, &dev_bytes, &dev_channels, &dev_rate,
            &dev_period_frames, &dev_periods) == 0) {
        if (is_capture) {
            g_dev_cap_format = dev_fmt;
            g_dev_cap_bytes = dev_bytes;
            g_dev_cap_channels = dev_channels;
            g_dev_cap_rate = dev_rate;
            g_dev_cap_shift = fmt_shift(dev_fmt);
        } else {
            g_dev_play_format = dev_fmt;
            g_dev_play_bytes = dev_bytes;
            g_dev_play_channels = dev_channels;
            g_dev_play_rate = dev_rate;
            g_dev_play_shift = fmt_shift(dev_fmt);
        }
        rate = dev_rate;
        channels = dev_channels;
        period_frames = dev_period_frames;
        periods = dev_periods;
        fprintf(stderr,
                "%s_PCM_DEVICE_CONFIG fmt=%d bytes=%u channels=%u rate=%u period=%u periods=%u\n",
                direction, dev_fmt, dev_bytes, dev_channels, dev_rate,
                dev_period_frames, dev_periods);
    }

    init_hw_params(&hw);
    param_set_mask(
        &hw,
        SNDRV_PCM_HW_PARAM_ACCESS,
        SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    param_set_mask(
        &hw,
        SNDRV_PCM_HW_PARAM_FORMAT,
        dev_fmt);
    param_set_mask(
        &hw,
        SNDRV_PCM_HW_PARAM_SUBFORMAT,
        SNDRV_PCM_SUBFORMAT_STD);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
        dev_bytes * 8);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_FRAME_BITS,
        dev_bytes * 8 * channels);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_CHANNELS,
        channels);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_RATE,
        rate);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
        period_frames);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_PERIODS,
        periods);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
        period_frames * periods);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
        period_frames * channels * dev_bytes);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
        period_frames * periods * channels * dev_bytes);
#ifdef SNDRV_PCM_HW_PARAM_TICK_TIME
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_TICK_TIME,
        0);
#endif

    PCM_TRY_IOCTL(
        SNDRV_PCM_IOCTL_HW_REFINE,
        &hw,
        "HW_REFINE");
    PCM_TRY_IOCTL(
        SNDRV_PCM_IOCTL_HW_PARAMS,
        &hw,
        "HW_PARAMS");

    memset(&sw, 0, sizeof(sw));
    sw.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
    sw.period_step = 1;
    sw.avail_min = period_frames;
    sw.xfer_align = period_frames;
    sw.start_threshold =
        is_capture ? 1 : period_frames; /* U2.63: ASRC never stops writing */
    sw.stop_threshold =
        period_frames * periods;
    sw.boundary =
        period_frames * periods;
    while (sw.boundary < 0x40000000UL &&
           sw.boundary > 0)
        sw.boundary *= 2;

    PCM_TRY_IOCTL(
        SNDRV_PCM_IOCTL_SW_PARAMS,
        &sw,
        "SW_PARAMS");
    PCM_TRY_IOCTL(
        SNDRV_PCM_IOCTL_PREPARE,
        0,
        "PREPARE");

#undef PCM_TRY_IOCTL

    fprintf(
        stderr,
        "%s_PCM_CONFIG_OK rate=%u channels=%u period_frames=%u periods=%u\n",
        direction,
        rate,
        channels,
        period_frames,
        periods);
    return 0;
}

static int pcm_prepare_capture_safe(
    int fd,
    unsigned rate,
    unsigned channels,
    unsigned period_frames,
    unsigned periods) {
    int version = 0;
    if (ioctl(fd, SNDRV_PCM_IOCTL_PVERSION, &version) < 0) {
        loge("CAP_PVERSION_SAFE");
        return -errno;
    }

    struct snd_pcm_info info;
    memset(&info, 0, sizeof(info));
    if (ioctl(fd, SNDRV_PCM_IOCTL_INFO, &info) < 0) {
        loge("CAP_PCM_INFO_SAFE");
        return -errno;
    }

    int dev_fmt = SNDRV_PCM_FORMAT_S16_LE;
    unsigned dev_bytes = 2;
    unsigned dev_channels = channels;
    unsigned dev_rate = rate;
    unsigned dev_period_frames = period_frames;
    unsigned dev_periods = periods;
    if (pcm_pick_device_config(
            fd, rate, channels, period_frames, periods,
            &dev_fmt, &dev_bytes, &dev_channels, &dev_rate,
            &dev_period_frames, &dev_periods) == 0) {
        g_dev_cap_format = dev_fmt;
        g_dev_cap_bytes = dev_bytes;
        g_dev_cap_channels = dev_channels;
        g_dev_cap_rate = dev_rate;
        g_dev_cap_shift = fmt_shift(g_dev_cap_format);
        rate = dev_rate;
        channels = dev_channels;
        period_frames = dev_period_frames;
        periods = dev_periods;
        fprintf(stderr,
                "CAP_PCM_DEVICE_CONFIG fmt=%d bytes=%u channels=%u rate=%u period=%u periods=%u\n",
                dev_fmt, dev_bytes, dev_channels, dev_rate,
                dev_period_frames, dev_periods);
    }

    struct snd_pcm_hw_params hw;
    init_hw_params(&hw);
    param_set_mask(&hw, SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    param_set_mask(&hw, SNDRV_PCM_HW_PARAM_FORMAT, dev_fmt);
    param_set_mask(&hw, SNDRV_PCM_HW_PARAM_SUBFORMAT, SNDRV_PCM_SUBFORMAT_STD);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, dev_bytes * 8);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_FRAME_BITS, dev_bytes * 8 * channels);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_CHANNELS, channels);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_RATE, rate);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, period_frames);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_PERIODS, periods);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, period_frames * periods);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, period_frames * channels * dev_bytes);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, period_frames * periods * channels * dev_bytes);
#ifdef SNDRV_PCM_HW_PARAM_TICK_TIME
    interval_set(&hw, SNDRV_PCM_HW_PARAM_TICK_TIME, 0);
#endif

    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, &hw) < 0) {
        loge("CAP_HW_REFINE_SAFE");
        return -errno;
    }
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hw) < 0) {
        loge("CAP_HW_PARAMS_SAFE");
        return -errno;
    }

    struct snd_pcm_sw_params sw;
    memset(&sw, 0, sizeof(sw));
    sw.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
    sw.period_step = 1;
    sw.avail_min = period_frames;
    sw.xfer_align = period_frames;
    sw.start_threshold = 1;
    sw.stop_threshold = period_frames * periods;
    sw.boundary = period_frames * periods;
    while (sw.boundary < 0x40000000UL && sw.boundary > 0)
        sw.boundary *= 2;

    if (ioctl(fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sw) < 0) {
        loge("CAP_SW_PARAMS_SAFE");
        return -errno;
    }
    if (ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0) {
        loge("CAP_PREPARE_SAFE");
        return -errno;
    }

    fprintf(
        stderr,
        "CAP_PCM_PREPARED_SAFE card=%u device=%u id=%s name=%s\n",
        info.card,
        info.device,
        info.id,
        info.name);
    return 0;
}

static long play_xrun_recoveries = 0;
static long cap_xrun_recoveries = 0;

static int recover_xrun_in_place(int pcm, int is_capture, int e) {
    if (e != EPIPE && e != ESTRPIPE) return -e;
    int rc = ioctl(pcm, SNDRV_PCM_IOCTL_PREPARE);
    long *counter = is_capture ? &cap_xrun_recoveries : &play_xrun_recoveries;
    (*counter)++;
    if (*counter <= 4 || ((*counter) % 100) == 0) {
        fprintf(stderr,
                "%s_XRUN_RECOVER_IN_PLACE count=%ld prepare_rc=%d errno=%d (%s)\n",
                is_capture ? "CAP" : "PLAY", *counter, rc, e, strerror(e));
    }
    return rc == 0 ? 0 : -e;
}

static int wait_playback_writable(int pcm, int timeout_ms) {
    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = pcm;
    pfd.events = POLLOUT | POLLERR | POLLHUP;
    int rc = poll(&pfd, 1, timeout_ms);
    if (rc < 0) {
        if (errno == EINTR) return 0;
        return -errno;
    }
    if (rc == 0) return 0;
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) return -EIO;
    return (pfd.revents & POLLOUT) ? 1 : 0;
}

/* v12 EXT SOURCE backoff: when the SP-404MKII stalls (Ext Source pressed),
 * ALSA polls EIO and the device keeps returning EIO for every reopen. v11
 * reopened every ~100 ms, which made the ring refill, overflow, and beep
 * (the static burst). Escalate the retry delay up to 2 s so a stalled
 * device produces at most a few sparse clicks instead of a static storm;
 * reset the delay once the stream has written cleanly for 8 periods. */
static int g_eio_backoff_ms = 250;
static void eio_backoff_sleep(void) {
    sleep_ms(g_eio_backoff_ms);
    if (g_eio_backoff_ms < 2000) g_eio_backoff_ms *= 2;
}
/* v12.1 USB re-enumeration after a persistent EIO stall: the SP-404MKII
 * stops consuming its USB stream while EXT SOURCE is held and some firmware
 * revisions do not resume until the device re-enumerates. The daemon then
 * toggles the port's authorized flag (software unplug/replug) so the user no
 * longer has to pull the cable. */
static unsigned long long g_eio_storm_start_ms = 0;
static unsigned long long g_last_reenum_ms = 0;
static int g_reenum_count = 0;

static unsigned convert_s16_to_device(
    const int16_t *in,
    int frames,
    unsigned in_channels,
    unsigned char *out,
    unsigned out_channels,
    unsigned out_bytes,
    unsigned out_shift) {
    int f, c;
    if (out_channels < 1) out_channels = 1;
    for (f = 0; f < frames; ++f) {
        int32_t left = in[(size_t)f * in_channels];
        int32_t right = (in_channels == 2) ? in[(size_t)f * in_channels + 1] : left;
        for (c = 0; c < (int)out_channels; ++c) {
            /*
             * SP404 4CH CHANNEL MAPPING (RC9.6, EXT SOURCE gate):
             * Route L/R to ch1/2. The MK2 feeds ch1/2 through the EXT IN /
             * INPUT FX bus, which is only audible while [EXT SOURCE] is
             * engaged; ch3/4 stay silent so the console never sounds without
             * the pad. This restores the RC8.0-v11 behavior the user reports
             * as the stable one ("la SP solo suena con EXT SOURCE"). RC9.5
             * routed ch3/4, which is audible unconditionally, and is reverted
             * here. Works for both 2ch and 4ch devices.
             */
            int32_t v = (c == 0) ? left : ((c == 1) ? right : 0);
            v <<= (int)out_shift;
            unsigned char *dst = out + ((size_t)f * out_channels + (size_t)c) * out_bytes;
            unsigned b;
            for (b = 0; b < out_bytes; ++b) dst[b] = (unsigned char)((uint32_t)v >> (8 * b));
        }
    }
    return (unsigned)frames;
}

static unsigned convert_device_to_s16(
    const unsigned char *in,
    int frames,
    unsigned in_channels,
    unsigned in_bytes,
    unsigned in_shift,
    int16_t *out,
    unsigned out_channels) {
    int f, c;
    if (out_channels < 1) out_channels = 1;
    for (f = 0; f < frames; ++f) {
        int32_t ch[2];
        ch[0] = 0; ch[1] = 0;
        for (c = 0; c < (int)in_channels && c < 2; ++c) {
            uint32_t raw = 0;
            const unsigned char *src = in + ((size_t)f * in_channels + (size_t)c) * in_bytes;
            unsigned b;
            for (b = 0; b < in_bytes; ++b) raw |= ((uint32_t)src[b]) << (8 * b);
            ch[c] = (int32_t)raw >> (int)in_shift;
        }
        int32_t left = ch[0];
        int32_t right = (in_channels == 2) ? ch[1] : left;
        if (out_channels == 2) {
            out[(size_t)f * 2] = (int16_t)left;
            out[(size_t)f * 2 + 1] = (int16_t)right;
        } else {
            int32_t mono = (left + right) / 2;
            out[f] = (int16_t)mono;
        }
    }
    return (unsigned)frames;
}

static int write_frames_exact(
    int pcm,
    unsigned char *buf,
    int frames,
    unsigned dev_channels,
    unsigned dev_bytes) {
    const size_t frame_bytes = (size_t)dev_channels * dev_bytes;
    int completed = 0;
    int idle_retries = 0;
    while (completed < frames) {
        struct snd_xferi x;
        memset(&x, 0, sizeof(x));
        x.buf = buf + ((size_t)completed * frame_bytes);
        x.frames = frames - completed;
        if (ioctl(pcm, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x) < 0) {
            int e = errno;
            if (e == EINTR) continue;
            if (e == EAGAIN || e == EWOULDBLOCK) {
                if (++idle_retries <= 20) {
                    sleep_ms(1);
                    continue;
                }
                return completed;
            }
            if (e == EPIPE || e == ESTRPIPE) {
                recover_xrun_in_place(pcm, 0, e);
                return -e;
            }
            fprintf(stderr, "WRITE_ERR errno=%d (%s) frames=%d completed=%d\n",
                    e, strerror(e), frames, completed);
            return -e;
        }
        if (x.result <= 0) {
            if (++idle_retries <= 20) {
                sleep_ms(1);
                continue;
            }
            return completed;
        }
        completed += (int)x.result;
        idle_retries = 0;
    }
    return completed;
}

static void dump_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = 0;
        fprintf(stderr, "  %s\n", line);
    }
    fclose(f);
}

static void dump_asound_state(const char *tag) {
    static unsigned long long last_dump_ms = 0;
    const unsigned long long now = monotonic_milliseconds();
    if (last_dump_ms != 0 && (now - last_dump_ms) < 4000ULL) return;
    last_dump_ms = now;
    fprintf(stderr, "=== ASOUND_DUMP tag=%s ===\n", tag ? tag : "");
    dump_file("/proc/asound/cards");
    dump_file("/proc/asound/card0/stream0");
    dump_file("/proc/asound/card0/pcm0p/sub0/status");
    dump_file("/proc/asound/card0/pcm0p/sub0/hw_params");
    dump_file("/proc/asound/card0/pcm0c/sub0/status");
    dump_file("/proc/asound/card0/pcm0c/sub0/hw_params");
    dump_file("/proc/asound/card0/pcm0c/sub0/prealloc");
    dump_file("/proc/asound/card0/pcm1c/sub0/status");
    dump_file("/proc/asound/card0/pcm1c/sub0/hw_params");
    dump_file("/proc/asound/card0/pcm1p/sub0/status");
    dump_file("/proc/asound/card0/pcm1p/sub0/hw_params");
}

static void capture_start_diag(int pcm) {
    int rc = ioctl(pcm, SNDRV_PCM_IOCTL_START);
    fprintf(stderr, "CAPTURE_IOCTL_START rc=%d errno=%d (%s)\n",
            rc, rc < 0 ? errno : 0, rc < 0 ? strerror(errno) : "ok");
    dump_asound_state("capture-start");
}

static int read_frames(int pcm, unsigned char *buf, int frames, unsigned dev_channels, unsigned dev_bytes) {
    (void)dev_channels;
    (void)dev_bytes;
    struct snd_xferi x;
    memset(&x, 0, sizeof(x));
    x.buf = buf;
    x.frames = frames;
    if (ioctl(pcm, SNDRV_PCM_IOCTL_READI_FRAMES, &x) < 0) {
        int e = errno;
        if (e == EAGAIN || e == EWOULDBLOCK || e == EINTR) return 0;
        int recovered = recover_xrun_in_place(pcm, 1, e);
        if (recovered == 0) return 0;
        fprintf(stderr, "READ_ERR errno=%d (%s) frames=%d\n",
                e, strerror(e), frames);
        return -e;
    }
    return x.result > 0 ? (int)x.result : 0;
}

#define RING_SAMPLES 65536
static int16_t ring[RING_SAMPLES];
static unsigned rpos = 0, wpos = 0, rfill = 0;
static void ring_reset(void) { rpos = wpos = rfill = 0; }
/* P1 RING BULK: wrap-aware bulk memcpy, max 2 copies. Preserves rpos/wpos/rfill, stereo alignment, overflow. */
static unsigned ring_push_samples(const int16_t *s, unsigned n) {
    if (!s || n == 0) return 0;
    unsigned free = RING_SAMPLES - rfill;
    if (n > free) n = free;
    if (n == 0) return 0;
    unsigned first = RING_SAMPLES - wpos;
    if (first > n) first = n;
    memcpy(ring + wpos, s, (size_t)first * sizeof(int16_t));
    if (n > first) {
        memcpy(ring, s + first, (size_t)(n - first) * sizeof(int16_t));
    }
    wpos += n;
    if (wpos >= RING_SAMPLES) wpos -= RING_SAMPLES;
    rfill += n;
    return n;
}
static unsigned ring_pop_samples(int16_t *dst, unsigned n) {
    if (!dst || n == 0) return 0;
    if (n > rfill) n = rfill;
    if (n == 0) return 0;
    unsigned first = RING_SAMPLES - rpos;
    if (first > n) first = n;
    memcpy(dst, ring + rpos, (size_t)first * sizeof(int16_t));
    if (n > first) {
        memcpy(dst + first, ring, (size_t)(n - first) * sizeof(int16_t));
    }
    rpos += n;
    if (rpos >= RING_SAMPLES) rpos -= RING_SAMPLES;
    rfill -= n;
    return n;
}
static unsigned ring_pop_frames_bulk(int16_t *dst, unsigned frames) {
    unsigned samples = frames * 2u;
    unsigned got_samples = ring_pop_samples(dst, samples);
    return got_samples / 2u;
}
static long rs_fifo_total = 0;
/* v12.1: discard stale producer data so a freshly opened stream starts with
 * current audio. Without this, the core's project audio accumulated while
 * the SP-404MKII enumerated gets dumped into the ring and trimmed at once,
 * which is the initial static burst ("pitido de conexión"). */
static void flush_input_fifo(int in) {
    if (in < 0) return;
    int16_t junk[2048];
    long discarded = 0;
    for (;;) {
        ssize_t r = read(in, junk, sizeof(junk));
        if (r > 0) {
            discarded += (long)(r / 2);
            continue;
        }
        break;
    }
    if (discarded > 0)
        fprintf(stderr, "PCM_FIFO_FLUSH discarded_samples=%ld\n", discarded);
}
static void drain_fifo(int in, long *dropped) { int16_t inbuf[4096]; for (;;) { ssize_t r = read(in, inbuf, sizeof(inbuf)); if (r > 0) { unsigned samples = (unsigned)(r / 2); rs_fifo_total += (long)samples; unsigned pushed = ring_push_samples(inbuf, samples); if (pushed < samples && dropped) *dropped += (long)(samples - pushed); continue; } if (r == 0) break; if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break; loge("read fifo"); break; } }

/*
 * U2.53.0 PASSTHROUGH_REBUILD
 *
 * The fifo carries the core's final mix already resampled to 48 kHz stereo
 * S16LE interleaved (TreeFrogUac2Bridge::SubmitStereo44100 anchors the
 * 44100->48000 conversion). The SP-404MKII device clock is also 48 kHz, so
 * the daemon needs no resampling at all: this is a plain ring -> PCM
 * passthrough.
 *
 * The U2.51.8/U2.52.5 polyphase resampler plus ratio/feed-EMA control was
 * removed. Its state survived stream resets and, on a ring freeze (mode
 * switch without USB replug), the "rs_n < RS_TAPS" break left stale out[]
 * periods that were re-emitted forever at full scale: the permanent
 * "pitido". A passthrough can only emit what the fifo actually contains and
 * a frozen fifo degrades to silence, never to stale audio.
 *
 * PLAYBACK_GAIN keeps constant headroom so the SP-404MKII's USB input stage
 * (~+9 dB) does not saturate during USB-REC.
 * U2.56: the playback gain is runtime-tunable via RUNTIME_DIR/sp404_gain
 * (0.00-2.00, default 1.00) so console and SP404 output levels can be
 * matched without a rebuild. Refreshed on every primed stream start.
 */
#define PLAYBACK_GAIN 1.0f
#define CAPTURE_GAIN 0.85f

static float g_playback_gain = PLAYBACK_GAIN;

static void load_playback_gain(void) {
    char buf[32] = "";
    char path[96];
    snprintf(path, sizeof(path), "%s/sp404_gain", RUNTIME_DIR);
    FILE *f = fopen(path, "r");
    if (!f) {
        g_playback_gain = PLAYBACK_GAIN;
        return;
    }
    if (fgets(buf, sizeof(buf), f)) {
        float v = (float)atof(buf);
        if (v >= 0.0f && v <= 2.0f) g_playback_gain = v;
    }
    fclose(f);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int ring_pop_one_sample(int16_t *s) {
    if (rfill == 0 || !s) return -1;
    *s = ring[rpos];
    if (++rpos >= RING_SAMPLES) rpos = 0;
    --rfill;
    return 0;
}
static int ring_pop_frame(unsigned ch, int16_t *frm) {
    if (!frm || rfill < ch) return -1;
    unsigned c;
    for (c = 0; c < ch; ++c) {
        if (ring_pop_one_sample(&frm[c]) != 0) return -1;
    }
    return 0;
}
#pragma GCC diagnostic pop
/* Keep legacy helpers linked when P1 bulk is active */
static void *const _p1_ring_legacy_keepalive __attribute__((used)) = (void*)ring_pop_frame;

/* ---- U2.63 ABI7 ASRC engine (ported from H38.1 FIR8, 48 kHz stereo).
 * Same engine as the r36s_u2523_usb_audio_io ASRC build: PI backlog
 * controller with dead band, 8-tap 160-phase Lanczos-4 FIR, clock hold
 * (zero period, no phase advance, no ALSA prepare) on starvation.
 * Prime 2080 frames = 43 ms; target 1600 = 33 ms steady backlog. ---- */
#define ASRC_STAGE_FRAMES 8192U
#define ASRC_TARGET_BACKLOG_FRAMES 3600U
#define ASRC_PRIME_BACKLOG_FRAMES 3600U
/* H43: the audible Sampler pitch shift came from the control step swinging
 * up to +/-1% (30000 ppm clamped) while the producer bursts were shoveled
 * through the ring every ~2 s.  1200 ppm = 0.12% = ~2 cents, inaudible, and
 * still three times the observed steady-state ratio offset (~-311 ppm). */
#define ASRC_MAX_CORRECTION_PPM 1200
#define ASRC_INTEGRAL_LIMIT_PPM 30000
#define ASRC_PROPORTIONAL_GAIN 6000
#define ASRC_INTEGRAL_GAIN 120
/* H43: hold floor.  Never render the ring below this backlog; the pre-H43
 * loop drained source_frames to zero at the device's pace, so the SP404's
 * ADAPTIVE input underran every ~2 s and the device stalled after ~60 s
 * (SP404_REENUM_ATTEMPT treadmill).  With the floor the stream idles at
 * ~2400-3600 frames instead of starving to empty. */
#define ASRC_HOLD_FLOOR_FRAMES 2400U
static int16_t asrc_stage[ASRC_STAGE_FRAMES * 2U];
static unsigned asrc_stage_frames = 0U;
static uint64_t asrc_phase_q32 = 0ULL;
static int asrc_stage_primed = 0;
static int asrc_step_ppm = 0;
static int asrc_integral_ppm = 0;
/* H43: EMA of the measured backlog used by the PI controller.  The raw
 * backlog swings by thousands of frames every ~2 s because the core pushes
 * its H40 pending buffer in large bursts; feeding that into P kept the
 * control loop slamming between the +/-limits instead of locking the real
 * ratio (~-311 ppm).  15/16 blend = ~160 ms time constant. */
static int asrc_ctrl_ema = 0;
static unsigned asrc_backlog_min = 0xffffffffU;
static unsigned asrc_backlog_max = 0U;
static unsigned long long asrc_output_frames = 0ULL;
static unsigned long long asrc_clock_hold_periods = 0ULL;
static unsigned long long asrc_clock_hold_frames = 0ULL;
static unsigned long long asrc_periods_rendered = 0ULL;

static void asrc_source_reset(void) {
    asrc_stage_frames = 0U;
    asrc_phase_q32 = 0ULL;
    asrc_stage_primed = 0;
    asrc_step_ppm = 0;
    asrc_integral_ppm = 0;
    asrc_ctrl_ema = 0;
    asrc_backlog_min = 0xffffffffU;
    asrc_backlog_max = 0U;
}
static unsigned asrc_total_backlog(void) {
    unsigned stage_available;
    unsigned idx = (unsigned)(asrc_phase_q32 >> 32);
    unsigned source_frames = g_audio_channels ? rfill / g_audio_channels : 0U;
    if (asrc_stage_frames > idx) stage_available = asrc_stage_frames - idx;
    else stage_available = 0U;
    return source_frames + stage_available;
}
static unsigned asrc_stage_append(void) {
    unsigned room = ASRC_STAGE_FRAMES - asrc_stage_frames;
    unsigned want = room < 2048U ? room : 2048U;
    if (want == 0U || rfill < 2U) return 0U;
    unsigned frames_avail = rfill / 2u;
    unsigned got = want < frames_avail ? want : frames_avail;
    if (got == 0U) return 0U;
    int16_t tmp[2048U * 2U];
    unsigned got2 = ring_pop_frames_bulk(tmp, got);
    if (got2 == 0U) return 0U;
    got = got2;
    unsigned n;
    if (!asrc_stage_primed) {
        for (n = 0U; n < ASRC_FIR_PRIME; ++n) {
            asrc_stage[n * 2U] = tmp[0];
            asrc_stage[n * 2U + 1U] = tmp[1];
        }
        asrc_stage_frames = ASRC_FIR_PRIME;
        asrc_phase_q32 = (uint64_t)ASRC_FIR_PRIME << 32;
        asrc_stage_primed = 1;
    }
    memcpy(asrc_stage + asrc_stage_frames * 2U,
           tmp, (size_t)got * 2U * sizeof(int16_t));
    asrc_stage_frames += got;
    return got;
}
static uint64_t asrc_nominal_step_q32(void) {
    return ((uint64_t)g_audio_rate << 32) / 48000U;
}
static void asrc_update_step(void) {
    const unsigned target = ASRC_TARGET_BACKLOG_FRAMES;
    const unsigned backlog = asrc_total_backlog();
    /* H43: feed the smoothed backlog to the controller.  The plain backlog
     * also keeps the min/max diagnostics honest. */
    if (asrc_ctrl_ema == 0) asrc_ctrl_ema = (int)target;
    asrc_ctrl_ema = (asrc_ctrl_ema * 15 + (int)backlog) / 16;
    const int error = asrc_ctrl_ema - (int)target;
    const int dead = (int)(target / 8U);
    int proportional;
    int desired;
    proportional = (error * ASRC_PROPORTIONAL_GAIN) / (int)target;
    if (error > dead || error < -dead) {
        asrc_integral_ppm += (error * ASRC_INTEGRAL_GAIN) / (int)target;
        if (asrc_integral_ppm > ASRC_INTEGRAL_LIMIT_PPM) asrc_integral_ppm = ASRC_INTEGRAL_LIMIT_PPM;
        if (asrc_integral_ppm < -ASRC_INTEGRAL_LIMIT_PPM) asrc_integral_ppm = -ASRC_INTEGRAL_LIMIT_PPM;
    }
    desired = proportional + asrc_integral_ppm;
    if (desired > ASRC_MAX_CORRECTION_PPM) desired = ASRC_MAX_CORRECTION_PPM;
    if (desired < -ASRC_MAX_CORRECTION_PPM) desired = -ASRC_MAX_CORRECTION_PPM;
    asrc_step_ppm = (asrc_step_ppm * 7 + desired) / 8;
    if (backlog < asrc_backlog_min) asrc_backlog_min = backlog;
    if (backlog > asrc_backlog_max) asrc_backlog_max = backlog;
}
static uint64_t asrc_step_q32(void) {
    const uint64_t nominal = asrc_nominal_step_q32();
    int64_t delta = ((int64_t)nominal * (int64_t)asrc_step_ppm) / 1000000LL;
    return (uint64_t)((int64_t)nominal + delta);
}
static void asrc_filter_stereo(unsigned idx, unsigned phase,
                               int16_t *out_l, int16_t *out_r) {
    int64_t acc_l = 0, acc_r = 0;
    const int16_t *coef = kASRC_FIR_TABLE[phase];
    unsigned t;
    for (t = 0U; t < ASRC_FIR_TAPS; ++t) {
        unsigned fi = idx - ASRC_FIR_HALF + t;
        acc_l += (int64_t)asrc_stage[fi * 2U] * (int64_t)coef[t];
        acc_r += (int64_t)asrc_stage[fi * 2U + 1U] * (int64_t)coef[t];
    }
    acc_l = (acc_l + (acc_l >= 0 ? 8192 : -8192)) / 16384;
    acc_r = (acc_r + (acc_r >= 0 ? 8192 : -8192)) / 16384;
    if (acc_l > 32767) acc_l = 32767;
    if (acc_l < -32768) acc_l = -32768;
    if (acc_r > 32767) acc_r = 32767;
    if (acc_r < -32768) acc_r = -32768;
    *out_l = (int16_t)acc_l;
    *out_r = (int16_t)acc_r;
}
static int asrc_prepare_period(unsigned out_frames, uint64_t *step_out) {
    uint64_t step, final_phase;
    unsigned required_index;
    if (!step_out || out_frames == 0U) return 0;

    if (!asrc_stage_primed) {
        if (asrc_stage_append() == 0U || asrc_stage_frames <= ASRC_FIR_LOOKAHEAD)
            return 0;
    } else if (asrc_total_backlog() < ASRC_HOLD_FLOOR_FRAMES) {
        /* H43 HOLD FLOOR: never render below the floor.  The pre-H43 loop
         * starved to zero on every producer burst trough and the SP404's
         * ADAPTIVE endpoint underran until the device stalled at ~60 s.
         * Holding here forwards a zero period (the caller zero-fills and
         * skips phase advance), which the device tolerates far longer than
         * running dry. */
        return 0;
    }
    asrc_update_step();
    step = asrc_step_q32();
    final_phase = asrc_phase_q32 + step * (uint64_t)(out_frames - 1U);
    required_index = (unsigned)(final_phase >> 32) + ASRC_FIR_LOOKAHEAD;
    while (asrc_stage_frames <= required_index) {
        if (asrc_stage_append() == 0U) return 0;
    }
    *step_out = step;
    return 1;
}
static void asrc_render_exact(int16_t *out, unsigned out_frames, uint64_t step) {
    unsigned produced;

    for (produced = 0U; produced < out_frames; ++produced) {
        unsigned idx = (unsigned)(asrc_phase_q32 >> 32);
        uint32_t frac = (uint32_t)asrc_phase_q32;
        unsigned phase = (unsigned)(((uint64_t)frac * 160ULL) >> 32);
        int16_t l = 0, r = 0;
        asrc_filter_stereo(idx, phase, &l, &r);
        out[produced * 2U] = l;
        out[produced * 2U + 1U] = r;
        asrc_phase_q32 += step;
    }
    {
        unsigned idx = (unsigned)(asrc_phase_q32 >> 32);
        if (idx > ASRC_FIR_HALF) {
            unsigned drop = idx - ASRC_FIR_HALF;
            if (drop > asrc_stage_frames) drop = asrc_stage_frames;
            /* P1 F: memmove audit. Typical drop ~ hw_period_frames (~480) => 480*4=1920B per period => ~192kB/s at 100 periods/s. Skip if drop==0 (no-op). */
            if (drop > 0) {
                memmove(asrc_stage, asrc_stage + drop * 2U,
                        (size_t)(asrc_stage_frames - drop) * 2U * sizeof(int16_t));
                asrc_stage_frames -= drop;
                asrc_phase_q32 -= (uint64_t)drop << 32;
            }
        }
    }
    asrc_output_frames += out_frames;
    ++asrc_periods_rendered;
}

static void write_wav_header(int fd, uint32_t data_bytes, uint32_t rate, uint16_t channels) {
    uint8_t h[44]; memset(h, 0, sizeof(h));
    uint32_t riff_size = 36 + data_bytes;
    memcpy(h + 0, "RIFF", 4); h[4]=riff_size&255; h[5]=(riff_size>>8)&255; h[6]=(riff_size>>16)&255; h[7]=(riff_size>>24)&255;
    memcpy(h + 8, "WAVEfmt ", 8); h[16]=16; h[20]=1; h[22]=channels&255; h[23]=(channels>>8)&255;
    h[24]=rate&255; h[25]=(rate>>8)&255; h[26]=(rate>>16)&255; h[27]=(rate>>24)&255;
    uint32_t byte_rate = rate * channels * 2; h[28]=byte_rate&255; h[29]=(byte_rate>>8)&255; h[30]=(byte_rate>>16)&255; h[31]=(byte_rate>>24)&255;
    uint16_t block_align = channels * 2; h[32]=block_align&255; h[33]=(block_align>>8)&255; h[34]=16;
    memcpy(h + 36, "data", 4); h[40]=data_bytes&255; h[41]=(data_bytes>>8)&255; h[42]=(data_bytes>>16)&255; h[43]=(data_bytes>>24)&255;
    lseek(fd, 0, SEEK_SET); write(fd, h, 44); lseek(fd, 0, SEEK_END);
}

/*
 * U2.53.0 FIFO_DUMP diagnostic: on every primed stream, capture the first
 * 2 s of RAW fifo content (pre-gain) to
 * /tmp/r36sx_lgpt_logs/fifo_capture.wav (tmpfs: never writes the SD card;
 * the shutdown flush moves it to SD LGPT_OTG_LOGS).
 * This settles whether the permanent full-scale tone is produced by the core
 * (present in the fifo) or injected by the daemon path (absent from the
 * fifo). The WAV is plain 48 kHz stereo S16LE.
 */
#if SP404_ENABLE_FIFO_DUMP
#define FIFO_DUMP_DIR "/tmp/r36sx_lgpt_logs"
#define FIFO_DUMP_PATH FIFO_DUMP_DIR "/fifo_capture.wav"
#define FIFO_DUMP_FRAMES 96000
#define FIFO_DUMP_BUF_FRAMES 32768
static int fdump_fd = -1;
static long fdump_frames_written = 0;
static long fdump_frames_left = 0;
static int fdump_peak = 0;
static int16_t fdump_buf[FIFO_DUMP_BUF_FRAMES * 2];
static unsigned fdump_buf_frames = 0;

static void fifo_dump_finish(const char *why);

static void fifo_dump_flush(void) {
    if (fdump_fd < 0 || fdump_buf_frames == 0) return;
    (void)write(fdump_fd, fdump_buf,
                (size_t)fdump_buf_frames * 4);
    fdump_buf_frames = 0;
}

static void fifo_dump_finish(const char *why) {
    if (fdump_fd < 0) return;
    fifo_dump_flush();
    uint32_t data_bytes = (uint32_t)fdump_frames_written * 4u;
    write_wav_header(fdump_fd, data_bytes, 48000, 2);
    close(fdump_fd);
    fdump_fd = -1;
    fprintf(stderr,
            "FIFO_DUMP_COMPLETE why=%s path=%s frames=%ld bytes=%u peak=%d\n",
            why, FIFO_DUMP_PATH, fdump_frames_written, data_bytes, fdump_peak);
}

static void fifo_dump_start(void) {
    if (fdump_fd >= 0) return;
    mkdir(FIFO_DUMP_DIR, 0777);
    unlink(FIFO_DUMP_PATH);
    fdump_fd = open(FIFO_DUMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fdump_fd < 0) {
        loge("fifo dump open");
        return;
    }
    fdump_frames_written = 0;
    fdump_frames_left = FIFO_DUMP_FRAMES;
    fdump_peak = 0;
    fdump_buf_frames = 0;
    write_wav_header(fdump_fd, 0, 48000, 2);
    fprintf(stderr, "FIFO_DUMP_START path=%s frames=%d\n",
            FIFO_DUMP_PATH, FIFO_DUMP_FRAMES);
}

static void fifo_dump_write(const int16_t frm[2]) {
    if (fdump_fd < 0 || fdump_frames_left <= 0) return;
    int p0 = frm[0] < 0 ? -frm[0] : frm[0];
    int p1 = frm[1] < 0 ? -frm[1] : frm[1];
    if (p0 > fdump_peak) fdump_peak = p0;
    if (p1 > fdump_peak) fdump_peak = p1;
    if (fdump_buf_frames < FIFO_DUMP_BUF_FRAMES) {
        fdump_buf[fdump_buf_frames * 2] = frm[0];
        fdump_buf[fdump_buf_frames * 2 + 1] = frm[1];
        ++fdump_buf_frames;
        ++fdump_frames_written;
    }
    --fdump_frames_left;
    if (fdump_buf_frames >= FIFO_DUMP_BUF_FRAMES)
        fifo_dump_flush();
    if (fdump_frames_left <= 0) fifo_dump_finish("complete");
}
#else
/* P1: FIFO dump disabled in production (zero per-frame overhead) - macros avoid unused warnings */
#define fifo_dump_finish(why) ((void)(why))
#define fifo_dump_start() ((void)0)
#define fifo_dump_write(frm) ((void)(frm))
#define fifo_dump_flush() ((void)0)
#endif


static int mon_fd = -1;
static int mon_enabled = 0;
static time_t mon_mtime = 0;
static unsigned long long mon_last_flag_check_ms = 0;
static void monitor_fifo_reset(void) {
    if (mon_fd >= 0) { close(mon_fd); mon_fd = -1; }
}
static int ensure_monitor_fifo_node(void) {
    struct stat info;
    if (lstat(CAPTURE_MONITOR_FIFO, &info) == 0) {
        if (S_ISFIFO(info.st_mode)) {
            chmod(CAPTURE_MONITOR_FIFO, 0666);
            return 1;
        }
        if (unlink(CAPTURE_MONITOR_FIFO) != 0) {
            loge("monitor invalid node unlink");
            return 0;
        }
    } else if (errno != ENOENT) {
        loge("monitor fifo lstat");
        return 0;
    }
    if (mkfifo(CAPTURE_MONITOR_FIFO, 0666) < 0 && errno != EEXIST) {
        loge("monitor mkfifo");
        return 0;
    }
    chmod(CAPTURE_MONITOR_FIFO, 0666);
    return 1;
}
static void monitor_refresh_flag(void) {
    // H44: monitor is legacy for IN; count but still respect OUT
    g_cnt_monitor_polls++;
    const unsigned long long now = monotonic_milliseconds();
    if (mon_last_flag_check_ms != 0 && now >= mon_last_flag_check_ms &&
        (now - mon_last_flag_check_ms) < 10ULL) return;
    mon_last_flag_check_ms = now;
    struct stat st;
    int enabled = 0;
    if (stat(CAPTURE_MONITOR, &st) == 0) {
        if (st.st_mtime != mon_mtime) mon_mtime = st.st_mtime;
        char b[16]; int fd = open(CAPTURE_MONITOR, O_RDONLY);
        if (fd >= 0) { ssize_t n = read(fd, b, sizeof(b)-1); close(fd); if (n > 0) { b[n]=0; enabled = atoi(b) != 0; } }
    }
    if (enabled != mon_enabled) {
        mon_enabled = enabled;
        monitor_fifo_reset();
        fprintf(stderr, "CAPTURE_MONITOR_%s fifo=%s\n", mon_enabled ? "ON" : "OFF", CAPTURE_MONITOR_FIFO);
    }
}
static void monitor_fifo_open_if_needed(void) {
    if (!mon_enabled || mon_fd >= 0) return;
    if (!ensure_monitor_fifo_node()) return;
    mon_fd = open(CAPTURE_MONITOR_FIFO, O_WRONLY | O_NONBLOCK);
    if (mon_fd >= 0) fprintf(stderr, "CAPTURE_MONITOR_FIFO_OPENED fd=%d\n", mon_fd);
}
static void write_perf_stats(void){
    unsigned long long now = monotonic_milliseconds();
    if (g_last_perf_stats_ms !=0 && now - g_last_perf_stats_ms < 1000ULL) return;
    g_last_perf_stats_ms = now;
    char path[96];
    snprintf(path,sizeof(path),"%s/sp404_perf_stats", RUNTIME_DIR);
    char tmp[256];
    snprintf(tmp,sizeof(tmp), "%s.tmp", path);
    char buf[512];
    snprintf(buf,sizeof(buf),
        "usb_marker_checks=%lu\n"
        "usb_marker_reads=%lu\n"
        "card_rechecks=%lu\n"
        "audio_mode_reads=%lu\n"
        "capture_cmd_polls=%lu\n"
        "monitor_polls=%lu\n"
        "slow_control_ticks=%lu\n"
        "usb_state=%d\n"
        "pcm_open=%d\n"
        "stream_primed=%d\n",
        g_cnt_usb_marker_checks,
        g_cnt_usb_marker_reads,
        g_cnt_card_rechecks,
        g_cnt_audio_mode_reads,
        g_cnt_capture_cmd_polls,
        g_cnt_monitor_polls,
        g_cnt_slow_control_ticks,
        g_usb_state,
        0,0);
    // we will fill pcm/stream from caller if needed; keep simple
    int fd=open(tmp, O_WRONLY|O_CREAT|O_TRUNC,0666);
    if(fd>=0){ write(fd,buf,strlen(buf)); close(fd); rename(tmp,path); }
}
static void slow_control_tick(void){
    unsigned long long now=monotonic_milliseconds();
    if(g_last_slow_control_ms!=0 && now - g_last_slow_control_ms < 2000ULL) return;
    g_last_slow_control_ms=now;
    g_cnt_slow_control_ticks++;
    // Intentionally minimal: no filesystem discovery, no SD reads.
    // Only called when not in critical ALSA deadline path (between periods).
    write_perf_stats();
}
static void monitor_write_samples(const int16_t *buf, int frames) {
    monitor_refresh_flag();
    if (!mon_enabled) return;
    monitor_fifo_open_if_needed();
    if (mon_fd < 0) return;
    ssize_t w = write(
        mon_fd,
        buf,
        (size_t)frames * (size_t)g_audio_channels * sizeof(int16_t));
    if (w < 0) {
        if (errno == EPIPE || errno == ENXIO || errno == EBADF) {
            fprintf(stderr, "CAPTURE_MONITOR_PIPE_RESET errno=%d (%s)\n", errno, strerror(errno));
            monitor_fifo_reset();
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            loge("monitor write");
        }
    }
}

enum {
    CAP_STATE_IDLE = 0,
    CAP_STATE_STARTING = 1,
    CAP_STATE_RECORDING = 2,
    CAP_STATE_STOPPING = 3,
    CAP_STATE_READY = 4,
    CAP_STATE_ERROR = 5
};

typedef struct CaptureState {
    int active;
    int state;
    int pcm;
    int wav;
    char path[256];
    char name[96];
    char token[96];
    char error[128];
    char pcm_path[64];
    unsigned long long start_ms;
    unsigned long long last_data_ms;
    int max_seconds;
    unsigned rate;
    unsigned channels;
    uint32_t data_bytes;
    long frames;
    int last_meta_second;
} CaptureState;

static CaptureState cap = {
    .active = 0,
    .state = CAP_STATE_IDLE,
    .pcm = -1,
    .wav = -1,
    .rate = 48000,
    .channels = 2,
    .last_meta_second = -1
};

static char last_record_command_token[96] = "";
static int mon_cap_pcm = -1;
static int force_playback_reopen = 0;
static int playback_parked = 0;
static int last_level_percent = -1;
static int last_level_l_percent = -1;
static int last_level_r_percent = -1;
static int last_elapsed_seconds = -1;

static void set_status(const char *s) {
    write_text_file(CAPTURE_STATUS, s ? s : "");
}

static void set_level_percent(int pct) {
    char b[32];
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    if (pct == last_level_percent) return;
    last_level_percent = pct;
    snprintf(b, sizeof(b), "%d\n", pct);
    write_text_file(CAPTURE_LEVEL, b);
}

static void set_lr_level_percent(int l, int r) {
    char b[32];
    if (l < 0) l = 0;
    if (l > 100) l = 100;
    if (r < 0) r = 0;
    if (r > 100) r = 100;
    if (l != last_level_l_percent) {
        last_level_l_percent = l;
        snprintf(b, sizeof(b), "%d\n", l);
        write_text_file(CAPTURE_LEVEL_L, b);
    }
    if (r != last_level_r_percent) {
        last_level_r_percent = r;
        snprintf(b, sizeof(b), "%d\n", r);
        write_text_file(CAPTURE_LEVEL_R, b);
    }
    set_level_percent((l > r) ? l : r);
}

static void set_elapsed_seconds(int sec) {
    char b[32];
    if (sec < 0) sec = 0;
    if (sec > 120) sec = 120;
    if (sec == last_elapsed_seconds) return;
    last_elapsed_seconds = sec;
    snprintf(b, sizeof(b), "%d\n", sec);
    write_text_file(CAPTURE_ELAPSED, b);
}


static const char *capture_state_name(int state) {
    switch (state) {
        case CAP_STATE_STARTING: return "STARTING";
        case CAP_STATE_RECORDING: return "RECORDING";
        case CAP_STATE_STOPPING: return "STOPPING";
        case CAP_STATE_READY: return "READY";
        case CAP_STATE_ERROR: return "ERROR";
        default: return "IDLE";
    }
}

static void write_capture_meta(void) {
    char meta[1024];
    int elapsed = 0;
    if (cap.start_ms > 0) {
        unsigned long long now = monotonic_milliseconds();
        if (now >= cap.start_ms)
            elapsed = (int)((now - cap.start_ms) / 1000ULL);
    }
    if (elapsed < 0) elapsed = 0;
    if (elapsed > 120) elapsed = 120;

    snprintf(
        meta,
        sizeof(meta),
        "STATE=%s\n"
        "TOKEN=%s\n"
        "PATH=%s\n"
        "NAME=%s\n"
        "FRAMES=%ld\n"
        "BYTES=%u\n"
        "RATE=%u\n"
        "CHANNELS=%u\n"
        "ELAPSED=%d\n"
        "ERROR=%s\n",
        capture_state_name(cap.state),
        cap.token,
        cap.path,
        cap.name,
        cap.frames,
        cap.data_bytes,
        cap.rate,
        cap.channels,
        elapsed,
        cap.error);

    char temporary[256];
    snprintf(temporary, sizeof(temporary), "%s.tmp", CAPTURE_META);

    int fd = open(
        temporary,
        O_WRONLY | O_CREAT | O_TRUNC,
        0666);
    if (fd >= 0) {
        size_t total = strlen(meta);
        size_t offset = 0;
        while (offset < total) {
            ssize_t written = write(
                fd,
                meta + offset,
                total - offset);
            if (written > 0) {
                offset += (size_t)written;
                continue;
            }
            if (written < 0 && errno == EINTR) continue;
            break;
        }
        close(fd);
        rename(temporary, CAPTURE_META);
    }

    /* The UI reads live metadata from tmpfs. Mirroring once per second to
     * the SD while recording creates avoidable I/O contention. Persist only
     * state transitions/final metadata; the WAV data path is unchanged. */
    if (cap.state != CAP_STATE_RECORDING)
        mirror_runtime_state(CAPTURE_META, meta);
}

static void set_capture_state(
    int state,
    const char *status,
    const char *error_text) {
    cap.state = state;
    snprintf(
        cap.error,
        sizeof(cap.error),
        "%s",
        error_text ? error_text : "");
    if (status) {
        set_status(status);
        if (state != CAP_STATE_RECORDING)
            mirror_runtime_state(CAPTURE_STATUS, status);
    }
    write_capture_meta();
}

static int write_all_bytes(
    int fd,
    const void *buffer,
    size_t length) {
    const unsigned char *data =
        (const unsigned char *)buffer;
    size_t offset = 0;

    while (offset < length) {
        ssize_t written = write(
            fd,
            data + offset,
            length - offset);
        if (written > 0) {
            offset += (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        return -1;
    }

    return 0;
}
static void peak_percent_lr_s16(
    const int16_t *buf,
    int frames,
    unsigned channels,
    int *pl,
    int *pr) {
    int peak_l = 0;
    int peak_r = 0;
    int i;
    if (channels < 1) channels = 1;
    if (channels > 2) channels = 2;
    for (i = 0; i < frames; ++i) {
        int left = buf[i * channels];
        int right = channels == 2 ? buf[i * channels + 1] : left;
        if (left < 0) left = -left;
        if (right < 0) right = -right;
        if (left > peak_l) peak_l = left;
        if (right > peak_r) peak_r = right;
    }
    *pl = (peak_l * 100 + 16383) / 32767;
    *pr = (peak_r * 100 + 16383) / 32767;
}
static void stop_capture(const char *why) {
    if (!cap.active) {
        if (cap.state == CAP_STATE_STOPPING)
            write_capture_meta();
        return;
    }

    const int failed =
        why && strstr(why, "error") != 0;

    cap.state = CAP_STATE_STOPPING;
    write_capture_meta();

    write_wav_header(cap.wav, cap.data_bytes, cap.rate, (uint16_t)cap.channels);
    fsync(cap.wav);
    close(cap.wav);
    cap.wav = -1;

    if (cap.pcm >= 0) {
        close(cap.pcm);
        cap.pcm = -1;
    }

    cap.active = 0;

    if (failed) {
        char status[192];
        snprintf(
            status,
            sizeof(status),
            "USB capture error: %s",
            cap.error[0] ? cap.error : (why ? why : "capture failure"));
        set_capture_state(
            CAP_STATE_ERROR,
            status,
            cap.error[0] ? cap.error : (why ? why : "capture failure"));
        fprintf(
            stderr,
            "CAPTURE_ERROR_FINALIZED path=%s frames=%ld bytes=%u why=%s\n",
            cap.path,
            cap.frames,
            cap.data_bytes,
            why ? why : "error");
        return;
    }

    if (cap.data_bytes == 0 || cap.frames == 0) {
        if (cap.path[0]) unlink(cap.path);
        char status[192];
        snprintf(
            status,
            sizeof(status),
            "USB capture error: no audio data (%s)",
            why ? why : "stop");
        set_capture_state(
            CAP_STATE_ERROR,
            status,
            "recording contained no audio frames");
        fprintf(
            stderr,
            "CAPTURE_STOP_EMPTY path=%s why=%s\n",
            cap.path,
            why ? why : "stop");
        return;
    }

    write_text_file(CAPTURE_LAST_NAME, cap.name);
    write_text_file(CAPTURE_LAST_PATH, cap.path);

    char status[192];
    snprintf(
        status,
        sizeof(status),
        "USB capture ready %s frames=%ld bytes=%u",
        cap.name,
        cap.frames,
        cap.data_bytes);

    set_capture_state(CAP_STATE_READY, status, "");

    fprintf(
        stderr,
        "CAPTURE_READY path=%s frames=%ld bytes=%u why=%s\n",
        cap.path,
        cap.frames,
        cap.data_bytes,
        why ? why : "stop");

    set_level_percent(0);
    set_lr_level_percent(0, 0);
    set_elapsed_seconds(0);
}

static int acquire_capture_pcm(
    const char *pcmc,
    int *from_passive) {
    if (from_passive) *from_passive = 0;

    /*
     * U2.51.0 RECORDING_PCM_HANDOFF
     *
     * The passive meter already owns a prepared pcmC0D0c descriptor.
     * Closing it and reopening immediately failed on the R36SX kernel.
     * Transfer that live descriptor directly to the recorder.
     */
    if (mon_cap_pcm >= 0) {
        int fd = mon_cap_pcm;
        mon_cap_pcm = -1;
        if (from_passive) *from_passive = 1;
        fprintf(
            stderr,
            "CAPTURE_PCM_HANDOFF_FROM_PASSIVE fd=%d\n",
            fd);
        return fd;
    }

    int last_error = 0;
    for (int attempt = 1; attempt <= 160; ++attempt) {
        int fd = open(pcmc, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            fprintf(
                stderr,
                "CAPTURE_PCM_OPEN_RETRY_OK attempt=%d fd=%d\n",
                attempt,
                fd);
            return fd;
        }

        last_error = errno;
        if (last_error != EBUSY &&
            last_error != EAGAIN &&
            last_error != EEXIST &&
            last_error != EINTR) {
            break;
        }
        sleep_ms(25);
    }

    errno = last_error;
    fprintf(
        stderr,
        "CAPTURE_PCM_ACQUIRE_FAILED errno=%d (%s)\n",
        last_error,
        strerror(last_error));
    return -1;
}

static void start_capture(
    const char *pcmc,
    const char *path,
    const char *name,
    const char *token,
    int seconds) {
    if (cap.active) stop_capture("restart");

    if (!path || !path[0]) {
        memset(&cap, 0, sizeof(cap));
        cap.pcm = -1;
        cap.wav = -1;
        snprintf(cap.token, sizeof(cap.token), "%s", token ? token : "");
        set_capture_state(
            CAP_STATE_ERROR,
            "USB capture error: missing path",
            "missing path");
        return;
    }

    if (seconds <= 0) seconds = 10;
    if (seconds > 120) seconds = 120;

    memset(&cap, 0, sizeof(cap));
    cap.pcm = -1;
    cap.wav = -1;
    cap.state = CAP_STATE_STARTING;
    cap.max_seconds = seconds;
    cap.rate = g_audio_rate;
    cap.channels = g_audio_channels;
    cap.start_ms = monotonic_milliseconds();
    cap.last_data_ms = cap.start_ms;
    cap.last_meta_second = -1;
    snprintf(cap.pcm_path, sizeof(cap.pcm_path), "%s", pcmc);

    snprintf(
        cap.name,
        sizeof(cap.name),
        "%s",
        (name && name[0]) ? name : "USBREC.wav");
    snprintf(
        cap.token,
        sizeof(cap.token),
        "%s",
        token ? token : "");

    set_capture_state(
        CAP_STATE_STARTING,
        "USB capture preparing",
        "");

    int from_passive = 0;
    cap.pcm = acquire_capture_pcm(
        pcmc,
        &from_passive);

    if (cap.pcm < 0) {
        const int open_error = errno;
        char detail[128];
        snprintf(
            detail,
            sizeof(detail),
            "pcmC0D0c acquire failed errno=%d (%s)",
            open_error,
            strerror(open_error));
        set_capture_state(
            CAP_STATE_ERROR,
            "USB capture error: input PCM unavailable",
            detail);
        return;
    }

    if (!from_passive &&
        pcm_prepare_capture_safe(
            cap.pcm,
            cap.rate,
            cap.channels,
            480,
            4) < 0) {
        const int prepare_error = errno;
        close(cap.pcm);
        cap.pcm = -1;

        char detail[128];
        snprintf(
            detail,
            sizeof(detail),
            "capture PCM prepare failed errno=%d (%s)",
            prepare_error,
            strerror(prepare_error));
        set_capture_state(
            CAP_STATE_ERROR,
            "USB capture error: input PCM prepare failed",
            detail);
        return;
    }

    capture_start_diag(cap.pcm);

    /*
     * Open the WAV only after PCM ownership is secured. A failed ALSA
     * acquisition can no longer leave a 44-byte header-only file.
     */
    cap.wav = open_capture_wav_with_fallback(
        path,
        cap.name,
        cap.path,
        sizeof(cap.path));

    if (cap.wav < 0) {
        const int wav_error = errno;
        close(cap.pcm);
        cap.pcm = -1;

        char detail[128];
        snprintf(
            detail,
            sizeof(detail),
            "WAV open failed errno=%d (%s)",
            wav_error,
            strerror(wav_error));
        set_capture_state(
            CAP_STATE_ERROR,
            "USB capture error: WAV open failed",
            detail);
        return;
    }

    write_wav_header(cap.wav, 0, cap.rate, (uint16_t)cap.channels);

    cap.active = 1;
    cap.state = CAP_STATE_RECORDING;
    set_elapsed_seconds(0);
    set_lr_level_percent(0, 0);

    char status[192];
    snprintf(
        status,
        sizeof(status),
        "USB capture recording %ds %s",
        seconds,
        cap.name);
    set_capture_state(CAP_STATE_RECORDING, status, "");

    unlink(CAPTURE_LAST_NAME);
    unlink(CAPTURE_LAST_PATH);

    fprintf(
        stderr,
        "CAPTURE_START token=%s pcm=%s path=%s seconds=%d "
        "ownership=%s fd=%d rate=%u channels=%u\n",
        cap.token,
        pcmc,
        cap.path,
        seconds,
        from_passive ? "handoff" : "fresh-open",
        cap.pcm,
        cap.rate,
        cap.channels);
}

static void discard_capture(const char *token) {
    char path[256] = "";

    if (cap.active) {
        snprintf(path, sizeof(path), "%s", cap.path);
        stop_capture("discard");
    }

    if (!path[0] && cap.path[0])
        snprintf(path, sizeof(path), "%s", cap.path);

    if (!path[0]) {
        int fd = open(CAPTURE_LAST_PATH, O_RDONLY);
        if (fd >= 0) {
            ssize_t count = read(fd, path, sizeof(path)-1);
            close(fd);
            if (count > 0) {
                path[count] = 0;
                char *newline = strchr(path, '\n');
                if (newline) *newline = 0;
            }
        }
    }

    if (path[0]) {
        unlink(path);
        fprintf(
            stderr,
            "CAPTURE_DISCARD token=%s path=%s\n",
            token ? token : "",
            path);
    }

    unlink(CAPTURE_LAST_NAME);
    unlink(CAPTURE_LAST_PATH);

    memset(&cap, 0, sizeof(cap));
    cap.pcm = -1;
    cap.wav = -1;
    cap.state = CAP_STATE_IDLE;
    cap.last_meta_second = -1;
    snprintf(cap.token, sizeof(cap.token), "%s", token ? token : "");

    set_level_percent(0);
    set_lr_level_percent(0, 0);
    set_elapsed_seconds(0);
    set_capture_state(CAP_STATE_IDLE, "USB capture idle", "");
}

static void commit_capture(const char *token) {
    /*
     * Preserve the finalized source file.  Only runtime state is cleared.
     */
    if (cap.active)
        stop_capture("commit");

    fprintf(
        stderr,
        "CAPTURE_COMMIT token=%s path=%s\n",
        token ? token : "",
        cap.path);

    unlink(CAPTURE_LAST_NAME);
    unlink(CAPTURE_LAST_PATH);

    memset(&cap, 0, sizeof(cap));
    cap.pcm = -1;
    cap.wav = -1;
    cap.state = CAP_STATE_IDLE;
    cap.last_meta_second = -1;
    snprintf(
        cap.token,
        sizeof(cap.token),
        "%s",
        token ? token : "");

    set_level_percent(0);
    set_lr_level_percent(0, 0);
    set_elapsed_seconds(0);
    set_capture_state(
        CAP_STATE_IDLE,
        "USB capture committed",
        "");
}


static int parse_value(const char *cmd, const char *key, char *out, int len) {
    const char *p = strstr(cmd, key); if (!p) return 0; p += strlen(key);
    const char *e = strchr(p, '\n'); int n = e ? (int)(e - p) : (int)strlen(p); if (n >= len) n = len - 1; memcpy(out, p, n); out[n] = 0; return 1;
}
static void poll_capture_command(const char *pcmc) {
    // H44 SAMPLER OUT ONLY: capture not used in OUT mode, zero polls
    if (g_dir_out) return;
    g_cnt_capture_cmd_polls++;
    static unsigned long long last_poll_ms = 0;
    const unsigned long long now = monotonic_milliseconds();
    if (last_poll_ms != 0 && now >= last_poll_ms &&
        (now - last_poll_ms) < 10ULL) return;
    last_poll_ms = now;
    char command[768];
    int fd = open(CAPTURE_CMD, O_RDONLY);
    if (fd < 0) return;

    ssize_t count = read(fd, command, sizeof(command)-1);
    close(fd);
    if (count <= 0) return;
    command[count] = 0;

    char token[96] = "";
    parse_value(command, "TOKEN=", token, sizeof(token));

    const int is_record_command =
        strstr(command, "START") ||
        strstr(command, "STOP") ||
        strstr(command, "DISCARD") ||
        strstr(command, "COMMIT");

    if (!is_record_command) return;

    if (!token[0]) {
        fprintf(stderr, "CAPTURE_COMMAND_REJECTED missing token\n");
        return;
    }

    if (strcmp(token, last_record_command_token) == 0)
        return;

    snprintf(
        last_record_command_token,
        sizeof(last_record_command_token),
        "%s",
        token);

    if (strstr(command, "DISCARD")) {
        discard_capture(token);
        return;
    }

    if (strstr(command, "COMMIT")) {
        commit_capture(token);
        return;
    }

    if (strstr(command, "STOP")) {
        if (cap.active) {
            snprintf(cap.token, sizeof(cap.token), "%s", token);
            stop_capture("cmd-stop");
        } else {
            write_capture_meta();
        }
        return;
    }

    if (strstr(command, "START")) {
        if (g_dir_out) {
            fprintf(stderr, "U2517_CAPTURE_REJECTED_MODE_OUT token=%s\n",
                    token);
            set_capture_state(
                CAP_STATE_ERROR,
                "USB capture error: driver is Sampler OUT",
                "switch to Sampler IN to record");
            write_capture_meta();
            return;
        }

        char path[256] = "";
        char name[96] = "";
        char seconds_text[32] = "";
        int seconds = 10;

        parse_value(command, "PATH=", path, sizeof(path));
        parse_value(command, "NAME=", name, sizeof(name));
        if (parse_value(
                command,
                "SECONDS=",
                seconds_text,
                sizeof(seconds_text))) {
            seconds = atoi(seconds_text);
        }

        start_capture(
            pcmc,
            path,
            name,
            token,
            seconds);
    }
}

static void passive_monitor_tick(const char *pcmc, int configured) {
    // H44 SAMPLER OUT ONLY: no passive monitor in OUT, zero overhead
    if (g_dir_out) return;
    monitor_refresh_flag();
    if (!configured || !mon_enabled || cap.active || g_dir_out) {
        if (mon_cap_pcm >= 0) { close(mon_cap_pcm); mon_cap_pcm = -1; }
        return;
    }
    if (mon_cap_pcm < 0) {
        static unsigned long long mon_last_retry_ms = 0;
        unsigned long long mon_now_ms = monotonic_milliseconds();
        if (mon_now_ms - mon_last_retry_ms < 250) return;
        mon_last_retry_ms = mon_now_ms;
        mon_cap_pcm = open(pcmc, O_RDONLY | O_NONBLOCK);
        if (mon_cap_pcm < 0) { if (errno != ENOENT) loge("passive monitor capture open"); return; }
        if (pcm_prepare_capture_safe(
                mon_cap_pcm,
                g_audio_rate,
                g_audio_channels,
                480,
                4) < 0) {
            close(mon_cap_pcm);
            mon_cap_pcm = -1;
            fprintf(stderr, "CAPTURE_PASSIVE_MONITOR_PREPARE_RETRY\n");
            return;
        }
        fprintf(stderr, "CAPTURE_PASSIVE_MONITOR_OPEN pcm=%s rate=%u channels=%u\n", pcmc, g_audio_rate, g_audio_channels);
        capture_start_diag(mon_cap_pcm);
    }
    static unsigned char raw[480 * 2 * 4];
    static int16_t conv[480 * 2];
    int r = read_frames(mon_cap_pcm, raw, 480, g_dev_cap_channels, g_dev_cap_bytes);
    if (r > 0) {
        convert_device_to_s16(raw, r, g_dev_cap_channels, g_dev_cap_bytes, g_dev_cap_shift, conv, g_audio_channels);
        int ll = 0, rr = 0; peak_percent_lr_s16(conv, r, g_audio_channels, &ll, &rr); int level = (ll > rr) ? ll : rr;
        set_lr_level_percent(ll, rr);
        monitor_write_samples(conv, r);
        static long mon_frames = 0; mon_frames += r;
        if ((mon_frames % 48000) == 0) fprintf(stderr, "CAPTURE_PASSIVE_LEVEL percent=%d frames=%ld\n", level, mon_frames);
    } else if (r < 0 && (r == -EIO || r == -ENODEV || r == -ESHUTDOWN)) { close(mon_cap_pcm); mon_cap_pcm = -1; }
}

static void capture_tick(void) {
    if (!cap.active) return;

    unsigned long long now = monotonic_milliseconds();
    int elapsed = 0;
    if (now >= cap.start_ms)
        elapsed = (int)((now - cap.start_ms) / 1000ULL);
    if (elapsed < 0) elapsed = 0;
    if (elapsed > 120) elapsed = 120;

    set_elapsed_seconds(elapsed);

    if (cap.max_seconds > 0 &&
        elapsed >= cap.max_seconds) {
        stop_capture("duration");
        return;
    }

    static unsigned char raw[480 * 2 * 4];
    static int16_t buffer[480 * 2];
    int frames = read_frames(cap.pcm, raw, 480, g_dev_cap_channels, g_dev_cap_bytes);
    if (frames > 0) {
        convert_device_to_s16(raw, frames, g_dev_cap_channels, g_dev_cap_bytes, g_dev_cap_shift, buffer, cap.channels);
        int left = 0;
        int right = 0;
        peak_percent_lr_s16(
            buffer,
            frames,
            cap.channels,
            &left,
            &right);
        int level = left > right ? left : right;
        set_lr_level_percent(left, right);

        /*
         * U2.51.10 CAPTURE_GAIN: leave headroom so the SP-404MKII output
         * (hot by design) never saturates the 16-bit capture path.
         */
        {
            unsigned i;
            for (i = 0; i < (unsigned)(frames * cap.channels); ++i) {
                int32_t v = (int32_t)((float)buffer[i] * CAPTURE_GAIN);
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                buffer[i] = (int16_t)v;
            }
        }

        size_t byte_count =
            (size_t)frames *
            (size_t)cap.channels *
            sizeof(int16_t);

        if (write_all_bytes(
                cap.wav,
                buffer,
                byte_count) < 0) {
            char error_text[128];
            snprintf(
                error_text,
                sizeof(error_text),
                "WAV write failed errno=%d",
                errno);
            set_capture_state(
                CAP_STATE_ERROR,
                "USB capture error: WAV write failed",
                error_text);
            stop_capture("write-error");
            return;
        }

        cap.data_bytes += (uint32_t)byte_count;
        cap.frames += (long)frames;
        cap.last_data_ms = now;
        monitor_write_samples(buffer, frames);

        int meta_second = elapsed;
        if (meta_second != cap.last_meta_second) {
            cap.last_meta_second = meta_second;
            char status[192];
            snprintf(
                status,
                sizeof(status),
                "USB capture recording %s %ds level=%d%%",
                cap.name,
                elapsed,
                level);
            set_status(status);
            write_capture_meta();
            fprintf(
                stderr,
                "CAPTURE_PROGRESS token=%s elapsed=%d frames=%ld bytes=%u level=%d\n",
                cap.token,
                elapsed,
                cap.frames,
                cap.data_bytes,
                level);
        }
        return;
    }

    if (frames == 0 &&
        now > cap.start_ms &&
        (now - cap.start_ms) >= 5000ULL &&
        cap.frames == 0) {
        fprintf(stderr, "CAPTURE_NO_DATA diag start_elapsed=%llu\n",
                (unsigned long long)(now - cap.start_ms));
        dump_asound_state("capture-no-data");
        snprintf(
            cap.error,
            sizeof(cap.error),
            "no USB capture frames after 5s; check host playback and stereo/mono profile");
        stop_capture("no-data-error");
        return;
    }

    if (frames < 0) {
        if (frames == -ENODEV || frames == -EIO ||
            frames == -ESHUTDOWN) {
            /* U2.51.9 CAPTURE_DEVICE_RECOVERY: the SP-404 re-enumerates
             * when USB-REC engages. Retry reopening for ~8s and keep the
             * take (with a silence gap) instead of failing it. */
            unsigned long long gap_start = monotonic_milliseconds();
            fprintf(stderr,
                    "CAPTURE_DEVICE_LOST rc=%d frames=%ld trying_reopen\n",
                    frames, cap.frames);
            if (cap.pcm >= 0) {
                close(cap.pcm);
                cap.pcm = -1;
            }
            int reopened = 0;
            for (int attempt = 0; attempt < 32; ++attempt) {
                int fd = open(cap.pcm_path, O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    if (pcm_prepare_capture_safe(
                            fd, g_audio_rate, g_audio_channels,
                            480, 4) == 0) {
                        cap.pcm = fd;
                        reopened = 1;
                        break;
                    }
                    close(fd);
                }
                sleep_ms(250);
            }
            if (reopened) {
                unsigned long long gap_ms =
                    monotonic_milliseconds() - gap_start;
                unsigned gap_frames = (unsigned)((gap_ms * g_audio_rate) /
                                                 1000ULL);
                if (gap_frames > 48000) gap_frames = 48000;
                size_t gap_bytes =
                    (size_t)gap_frames * cap.channels *
                    sizeof(int16_t);
                static int16_t gap_zero[48000 * 2];
                memset(gap_zero, 0, gap_bytes);
                if (gap_bytes > 0 &&
                    write_all_bytes(cap.wav, gap_zero, gap_bytes) >= 0) {
                    cap.data_bytes += (uint32_t)gap_bytes;
                    cap.frames += (long)gap_frames;
                }
                fprintf(stderr,
                        "CAPTURE_DEVICE_RECOVERED gap_ms=%llu gap_frames=%u total_frames=%ld\n",
                        (unsigned long long)gap_ms, gap_frames,
                        cap.frames);
                return;
            }
        }
        char error_text[128];
        snprintf(
            error_text,
            sizeof(error_text),
            "capture read failed rc=%d",
            frames);
        snprintf(
            cap.error,
            sizeof(cap.error),
            "%s",
            error_text);
        stop_capture("read-error");
    }
}

static void probe_fill_tone(int16_t *buf, int frames, unsigned channels, unsigned long long *phase) {
    int f, c;
    for (f = 0; f < frames; ++f) {
        int32_t v = (((*phase) / 24) & 1) ? 8000 : -8000;
        (*phase)++;
        for (c = 0; c < (int)channels; ++c)
            buf[(size_t)f * channels + c] = (c <= 1) ? (int16_t)v : 0;
    }
}

static int probe_play_once(const char *pcmp, unsigned ch, unsigned rate,
                           unsigned period, unsigned periods, unsigned dur_ms) {
    int fd = open(pcmp, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "PROBE_PLAY ch=%u rate=%u p=%u n=%u OPEN_FAIL errno=%d (%s)\n",
                ch, rate, period, periods, errno, strerror(errno));
        return -1;
    }
    int rc = pcm_prepare_common(fd, 0, rate, ch, period, periods);
    if (rc < 0) {
        fprintf(stderr, "PROBE_PLAY ch=%u rate=%u p=%u n=%u PREPARE_FAIL rc=%d\n",
                ch, rate, period, periods, rc);
        close(fd);
        return -1;
    }
    unsigned dev_ch = g_dev_play_channels;
    unsigned dev_bytes = g_dev_play_bytes;
    unsigned long long phase = 0;
    static int16_t tone[2048 * 4];
    static unsigned char devbuf[2048 * 2 * 4];
    unsigned long long start = monotonic_milliseconds();
    long writes = 0, poll_errs = 0, epipes = 0, timeouts = 0, short_writes = 0;
    int first = 1;
    while (monotonic_milliseconds() - start < dur_ms) {
        int w = wait_playback_writable(fd, 25);
        if (w < 0) {
            ++poll_errs;
            if (first) {
                first = 0;
                fprintf(stderr, "PROBE_PLAY ch=%u rate=%u FIRST_POLL_ERR rc=%d errno=%d (%s) dev_ch=%u\n",
                        ch, rate, w, -w, strerror(-w), dev_ch);
            }
            break;
        }
        if (w == 0) { ++timeouts; continue; }
        probe_fill_tone(tone, (int)period, dev_ch, &phase);
        convert_s16_to_device(tone, (int)period, dev_ch, devbuf, dev_ch, dev_bytes, g_dev_play_shift);
        int wr = write_frames_exact(fd, devbuf, (int)period, dev_ch, dev_bytes);
        if (wr < 0) { ++epipes; break; }
        if (wr != (int)period) ++short_writes;
        ++writes;
    }
    fprintf(stderr, "PROBE_PLAY_RESULT ch=%u rate=%u p=%u n=%u dur=%u writes=%ld poll_errs=%ld timeouts=%ld epipes=%ld short_writes=%ld dev_ch=%u\n",
            ch, rate, period, periods, dur_ms, writes, poll_errs, timeouts, epipes, short_writes, dev_ch);
    close(fd);
    return 0;
}

static int probe_cap_once(const char *pcmc, unsigned ch, unsigned rate,
                          unsigned period, unsigned periods, unsigned dur_ms) {
    int fd = open(pcmc, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "PROBE_CAP ch=%u rate=%u p=%u n=%u OPEN_FAIL errno=%d (%s)\n",
                ch, rate, period, periods, errno, strerror(errno));
        return -1;
    }
    int rc = pcm_prepare_common(fd, 1, rate, ch, period, periods);
    if (rc < 0) {
        fprintf(stderr, "PROBE_CAP ch=%u rate=%u p=%u n=%u PREPARE_FAIL rc=%d\n",
                ch, rate, period, periods, rc);
        close(fd);
        return -1;
    }
    unsigned dev_ch = g_dev_cap_channels;
    unsigned dev_bytes = g_dev_cap_bytes;
    if (ioctl(fd, SNDRV_PCM_IOCTL_START) < 0) {
        fprintf(stderr, "PROBE_CAP ch=%u rate=%u START_FAIL errno=%d (%s)\n",
                ch, rate, errno, strerror(errno));
        close(fd);
        return -1;
    }
    static unsigned char raw[2048 * 2 * 4];
    unsigned long long start = monotonic_milliseconds();
    long frames_total = 0, poll_errs = 0, reads = 0;
    int first = 1;
    while (monotonic_milliseconds() - start < dur_ms) {
        struct pollfd pfd;
        memset(&pfd, 0, sizeof(pfd));
        pfd.fd = fd;
        pfd.events = POLLIN | POLLERR | POLLHUP;
        int pr = poll(&pfd, 1, 25);
        if (pr < 0) { if (errno == EINTR) continue; break; }
        if (pr == 0) continue;
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            ++poll_errs;
            if (first) {
                first = 0;
                fprintf(stderr, "PROBE_CAP ch=%u rate=%u FIRST_POLL_ERR revents=0x%x\n",
                        ch, rate, pfd.revents);
            }
            break;
        }
        if (!(pfd.revents & POLLIN)) continue;
        int n = read_frames(fd, raw, (int)period, dev_ch, dev_bytes);
        if (n > 0) { frames_total += n; ++reads; }
        else if (n < 0) {
            fprintf(stderr, "PROBE_CAP ch=%u rate=%u READ_FAIL rc=%d\n", ch, rate, n);
            break;
        }
    }
    fprintf(stderr, "PROBE_CAP_RESULT ch=%u rate=%u p=%u n=%u dur=%u frames=%ld (%.2fs) reads=%ld poll_errs=%ld dev_ch=%u\n",
            ch, rate, period, periods, dur_ms, frames_total,
            (double)frames_total / (double)rate, reads, poll_errs, dev_ch);
    close(fd);
    return 0;
}

static int probe_combo2(const char *pcmp, const char *pcmc, const char *tag,
                        unsigned pch_w, unsigned prate_w, unsigned pperiod_w, unsigned pperiods_w,
                        unsigned cch_w, unsigned crate_w, unsigned cperiod_w, unsigned cperiods_w,
                        unsigned cap_delay_ms, unsigned play_stop_ms, unsigned dur_ms) {
    int pf = open(pcmp, O_WRONLY);
    int cf = open(pcmc, O_RDONLY);
    if (pf < 0 || cf < 0) {
        fprintf(stderr, "PROBE_COMBO2[%s] OPEN_FAIL play=%d cap=%d\n", tag, pf, cf);
        if (pf >= 0) close(pf);
        if (cf >= 0) close(cf);
        return -1;
    }
    int prc = pcm_prepare_common(pf, 0, prate_w, pch_w, pperiod_w, pperiods_w);
    int crc = pcm_prepare_common(cf, 1, crate_w, cch_w, cperiod_w, cperiods_w);
    if (prc < 0 || crc < 0) {
        fprintf(stderr, "PROBE_COMBO2[%s] PREPARE_FAIL play=%d cap=%d\n", tag, prc, crc);
        close(pf);
        close(cf);
        return -1;
    }
    unsigned pch = g_dev_play_channels, pbytes = g_dev_play_bytes;
    unsigned cch = g_dev_cap_channels, cbytes = g_dev_cap_bytes;
    int cap_running = 0;
    if (cap_delay_ms == 0) {
        if (ioctl(cf, SNDRV_PCM_IOCTL_START) < 0) {
            fprintf(stderr, "PROBE_COMBO2[%s] CAP_START_FAIL errno=%d (%s)\n", tag, errno, strerror(errno));
            close(pf);
            close(cf);
            return -1;
        }
        cap_running = 1;
    }
    unsigned long long phase = 0;
    static int16_t tone[2048 * 4];
    static unsigned char devbuf[2048 * 2 * 4];
    static unsigned char raw[2048 * 2 * 4];
    unsigned long long start = monotonic_milliseconds();
    long writes = 0, cap_frames = 0, poll_errs = 0;
    int first = 1;
    while (monotonic_milliseconds() - start < dur_ms) {
        if (!cap_running && monotonic_milliseconds() - start >= cap_delay_ms) {
            if (ioctl(cf, SNDRV_PCM_IOCTL_START) < 0) {
                fprintf(stderr, "PROBE_COMBO2[%s] CAP_START_FAIL errno=%d (%s)\n", tag, errno, strerror(errno));
                break;
            }
            cap_running = 1;
        }
        if (pf >= 0 && play_stop_ms > 0 &&
            monotonic_milliseconds() - start >= play_stop_ms) {
            close(pf);
            pf = -1;
            fprintf(stderr, "PROBE_COMBO2[%s] PLAY_CLOSED_AT_%u\n", tag, play_stop_ms);
        }
        if (pf >= 0) {
        int w = wait_playback_writable(pf, 5);
        if (w < 0) { ++poll_errs; break; }
        if (w > 0) {
            probe_fill_tone(tone, pperiod_w, pch, &phase);
            convert_s16_to_device(tone, pperiod_w, pch, devbuf, pch, pbytes, g_dev_play_shift);
            int wr = write_frames_exact(pf, devbuf, pperiod_w, pch, pbytes);
            if (wr < 0) {
                if (first) {
                    first = 0;
                    fprintf(stderr, "PROBE_COMBO2[%s] PLAY_FAIL rc=%d\n", tag, wr);
                }
                break;
            }
            ++writes;
        }
        }
        if (cap_running) {
            struct pollfd pfd;
            memset(&pfd, 0, sizeof(pfd));
            pfd.fd = cf;
            pfd.events = POLLIN | POLLERR | POLLHUP;
            int pr = poll(&pfd, 1, 0);
            if (pr > 0 && (pfd.revents & POLLIN)) {
                int n = read_frames(cf, raw, cperiod_w, cch, cbytes);
                if (n > 0) cap_frames += n;
            }
        }
    }
    fprintf(stderr, "PROBE_COMBO2_RESULT tag=%s dur=%u writes=%ld cap_frames=%ld poll_errs=%ld dev_pch=%u dev_cch=%u cap_delay=%u\n",
            tag, dur_ms, writes, cap_frames, poll_errs, pch, cch, cap_delay_ms);
    close(pf);
    close(cf);
    return 0;
}

static int run_probe_all(const char *pcmp, const char *pcmc) {
    fprintf(stderr, "SP404_PROBE_START play=%s cap=%s\n", pcmp, pcmc);
    dump_asound_state("probe-start");
    probe_play_once(pcmp, 4, 48000, 480, 4, 2500);
    probe_play_once(pcmp, 4, 48000, 480, 8, 2500);
    probe_play_once(pcmp, 2, 48000, 480, 4, 2500);
    probe_play_once(pcmp, 4, 44100, 480, 4, 2500);
    probe_cap_once(pcmc, 2, 48000, 480, 4, 2500);
    probe_cap_once(pcmc, 2, 48000, 1024, 4, 2500);
    probe_cap_once(pcmc, 1, 48000, 480, 4, 2500);
    probe_combo2(pcmp, pcmc, "a_play_p8cap", 4, 48000, 480, 8, 2, 48000, 480, 4, 0, 0, 3000);
    probe_combo2(pcmp, pcmc, "b_lowlat", 4, 48000, 240, 4, 2, 48000, 480, 4, 0, 0, 3000);
    probe_combo2(pcmp, pcmc, "c_cap_after_play", 4, 48000, 480, 4, 2, 48000, 480, 4, 1500, 0, 4000);
    probe_combo2(pcmp, pcmc, "d_play_stop_then_cap", 4, 48000, 480, 4, 2, 48000, 480, 4, 2000, 1500, 4500);
    probe_cap_once(pcmc, 2, 48000, 480, 4, 2000);
    fprintf(stderr, "SP404_PROBE_DONE\n");
    return 0;
}

/*
 * U2.52.5 SAMPLER OUT_ONLY (duplex and console input abandoned)
 *
 * The SP-404MKII USB function is simplex (proven by --probe-all); the port
 * is now playback-only: console -> SP. A persisted "SP404_IN" token from an
 * older build is ignored (logged once); the daemon never switches to capture,
 * so the "Sampler IN" path is fully removed from the runtime.
 */
static int g_sampler_in_seen = 0;
static void refresh_audio_direction(void) {
    // H44 SAMPLER OUT ONLY: daemon is created as OUT, direction change is via stop_host_runtime
    // No SD polling. Keep counter for safety if ever needed.
    if (g_dir_out) return;
    unsigned long long now = monotonic_milliseconds();
    if (now - g_dir_check_ms < 5000) return;
    g_dir_check_ms = now;
    g_cnt_audio_mode_reads++;
    int fd = open(AUDIO_MODE_FILE, O_RDONLY);
    if (fd < 0) return;
    char b[96];
    ssize_t n = read(fd, b, sizeof(b) - 1);
    close(fd);
    if (n <= 0) return;
    b[n] = 0;

    if ((strstr(b, "SP404_IN") || strstr(b, "SP404IN")) &&
        !g_sampler_in_seen) {
        g_sampler_in_seen = 1;
        fprintf(stderr,
                "U2517_AUDIO_DIRECTION_CHANGE OUT reason=sampler-in-abandoned mode_file=%s\n",
                b);
        write_text_file(PLAYBACK_PCM_STATUS, "mode-out\n");
    }
}

int main(int argc, char **argv) {
    struct sigaction pipe_action;
    memset(&pipe_action, 0, sizeof(pipe_action));
    pipe_action.sa_handler = SIG_IGN;
    sigemptyset(&pipe_action.sa_mask);
    sigaction(SIGPIPE, &pipe_action, 0);

    struct sigaction stop_action;
    memset(&stop_action, 0, sizeof(stop_action));
    stop_action.sa_handler = sp404_stop_signal_handler;
    sigemptyset(&stop_action.sa_mask);
    sigaction(SIGUSR1, &stop_action, 0);
    sigaction(SIGTERM, &stop_action, 0);
    sigaction(SIGINT, &stop_action, 0);

    if (argc > 1 && strcmp(argv[1], "--probe-all") == 0) {
        const char *probe_play = argc > 2 ? argv[2] : "/dev/snd/pcmC0D0p";
        const char *probe_cap = argc > 3 ? argv[3] : "/dev/snd/pcmC0D0c";
        return run_probe_all(probe_play, probe_cap);
    }

    if (!ensure_monitor_fifo_node()) {
        fprintf(stderr, "U2517_MONITOR_FIFO_STARTUP_CREATE_FAILED path=%s\n", CAPTURE_MONITOR_FIFO);
    } else {
        fprintf(stderr, "U2517_MONITOR_FIFO_STARTUP_READY path=%s\n", CAPTURE_MONITOR_FIFO);
    }

    const char *fifo = argc > 1 ? argv[1] : "/tmp/r36sx_sp404_pcm_fifo";
    char pcm_play_buf[96], pcm_cap_buf[96];
    int sp404_card = 1;
    {
        int cfd = open(SP404_CARD, O_RDONLY);
        if (cfd >= 0) {
            char cbuf[16];
            ssize_t cn = read(cfd, cbuf, sizeof(cbuf) - 1);
            close(cfd);
            if (cn > 0) {
                cbuf[cn] = 0;
                int v = atoi(cbuf);
                if (v > 0) sp404_card = v;
            }
        }
    }
    snprintf(pcm_play_buf, sizeof(pcm_play_buf), "/dev/snd/pcmC%dD0p", sp404_card);
    snprintf(pcm_cap_buf, sizeof(pcm_cap_buf), "/dev/snd/pcmC%dD0c", sp404_card);
    /* H38.5 FULL_PCM_PATHS: the detector now writes complete PCM node paths
     * (real D index, resolved from the USB parent). Prefer them over guessing
     * pcmC{N}D0. Command-line args still win for direct invocations. */
    if (argc <= 2) {
        int fd = open("/tmp/r36sx_lgpt_usb/sp404_playback_pcm", O_RDONLY);
        if (fd >= 0) {
            char b[128];
            ssize_t n = read(fd, b, sizeof(b) - 1);
            close(fd);
            if (n > 0) {
                b[n] = 0;
                char *nl = strchr(b, '\n');
                if (nl) *nl = 0;
                if (strlen(b) > 0 && strncmp(b, "/dev/snd/pcm", 12) == 0)
                    snprintf(pcm_play_buf, sizeof(pcm_play_buf), "%s", b);
            }
        }
    }
    if (argc <= 3) {
        int fd = open("/tmp/r36sx_lgpt_usb/sp404_capture_pcm", O_RDONLY);
        if (fd >= 0) {
            char b[128];
            ssize_t n = read(fd, b, sizeof(b) - 1);
            close(fd);
            if (n > 0) {
                b[n] = 0;
                char *nl = strchr(b, '\n');
                if (nl) *nl = 0;
                if (strlen(b) > 0 && strncmp(b, "/dev/snd/pcm", 12) == 0)
                    snprintf(pcm_cap_buf, sizeof(pcm_cap_buf), "%s", b);
            }
        }
    }
    const char *pcmp = argc > 2 ? argv[2] : pcm_play_buf;
    const char *pcmc = argc > 3 ? argv[3] : pcm_cap_buf;
    const int pcm_path_overridden = (argc > 2) || (argc > 3);
    static unsigned long long last_card_recheck_ms = 0;
    int reread_sp404_card(void) {
        // H44: during streaming never reread marker; kernel FD is truth (event-driven)
        if (g_usb_state == USB_STREAMING) return sp404_card;
        g_cnt_card_rechecks++;
        /* Only auto-track when the pcm paths were not forced on the command
         * line. Reread the live markers so a late/hotplug enumeration on a
         * different card index is picked up without a daemon restart. */
        if (pcm_path_overridden) return sp404_card;
        unsigned long long now = monotonic_milliseconds();
        if (last_card_recheck_ms != 0 && now >= last_card_recheck_ms &&
            (now - last_card_recheck_ms) < 5000ULL) return sp404_card;
        last_card_recheck_ms = now;
        int new_card = sp404_card;
        int cfd = open(SP404_CARD, O_RDONLY);
        if (cfd >= 0) {
            char vbuf[16];
            ssize_t cn = read(cfd, vbuf, sizeof(vbuf) - 1);
            close(cfd);
            if (cn > 0) { vbuf[cn] = 0; int v = atoi(vbuf); if (v > 0) new_card = v; }
        }
        if (new_card == sp404_card) return sp404_card;
        sp404_card = new_card;
        /* Re-read the full-path markers, which may carry a non-standard D index. */
        {
            char pb[128] = {0};
            int fd = open("/tmp/r36sx_lgpt_usb/sp404_playback_pcm", O_RDONLY);
            if (fd >= 0) {
                ssize_t n = read(fd, pb, sizeof(pb) - 1);
                close(fd);
                if (n > 0) { pb[n] = 0; char *nl = strchr(pb, '\n'); if (nl) *nl = 0;
                    if (strncmp(pb, "/dev/snd/pcm", 12) == 0) snprintf(pcm_play_buf, sizeof(pcm_play_buf), "%s", pb); }
            }
            pb[0] = 0;
            fd = open("/tmp/r36sx_lgpt_usb/sp404_capture_pcm", O_RDONLY);
            if (fd >= 0) {
                ssize_t n = read(fd, pb, sizeof(pb) - 1);
                close(fd);
                if (n > 0) { pb[n] = 0; char *nl = strchr(pb, '\n'); if (nl) *nl = 0;
                    if (strncmp(pb, "/dev/snd/pcm", 12) == 0) snprintf(pcm_cap_buf, sizeof(pcm_cap_buf), "%s", pb); }
            }
        }
        fprintf(stderr, "CARD_RECHECK_UPDATED card=%d pcmp=%s pcmc=%s\n",
                sp404_card, pcm_play_buf, pcm_cap_buf);
        return sp404_card;
    }
    /* v12.1: find the SP-404MKII ALSA card by name and republish the live
     * markers (the SD detector may not have noticed a re-enumeration yet). */
    int rescan_card_by_name(void) {
        FILE *f = fopen("/proc/asound/cards", "r");
        if (!f) return -1;
        char line[256];
        int found = -1;
        while (fgets(line, sizeof(line), f)) {
            int id = -1;
            char nm[96];
            if (sscanf(line, " %d [%95[^]]", &id, nm) == 2) {
                if (strstr(nm, "SP-404") || strstr(nm, "SP404") ||
                    strstr(nm, "404MKII")) {
                    found = id;
                    break;
                }
            }
        }
        fclose(f);
        if (found > 0) {
            char b[16];
            snprintf(b, sizeof(b), "%d\n", found);
            write_text_file(SP404_CARD, b);
            sp404_card = found;
            snprintf(pcm_play_buf, sizeof(pcm_play_buf),
                     "/dev/snd/pcmC%dD0p", found);
            snprintf(pcm_cap_buf, sizeof(pcm_cap_buf),
                     "/dev/snd/pcmC%dD0c", found);
            write_text_file("/tmp/r36sx_lgpt_usb/sp404_playback_pcm",
                            pcm_play_buf);
            write_text_file("/tmp/r36sx_lgpt_usb/sp404_capture_pcm",
                            pcm_cap_buf);
            fprintf(stderr,
                    "CARD_RESCAN_FOUND card=%d pcmp=%s pcmc=%s\n",
                    found, pcm_play_buf, pcm_cap_buf);
        }
        return found;
    }
    /* v12.1: software unplug/replug of the SP-404MKII USB port. */
    int toggle_sp404_usb_reenum(void) {
        int card = rescan_card_by_name();
        if (card <= 0) return -1;
        char link[192];
        snprintf(link, sizeof(link), "/sys/class/sound/card%d/device", card);
        char real[512];
        if (!realpath(link, real)) return -1;
        char *base = strrchr(real, '/');
        if (base && strchr(base + 1, ':')) *base = 0;
        char auth[288];
        snprintf(auth, sizeof(auth), "%s/authorized", real);
        int af = open(auth, O_WRONLY);
        if (af < 0) return -1;
        if (write(af, "0\n", 2) < 0) { close(af); return -1; }
        close(af);
        usleep(150000);
        af = open(auth, O_WRONLY);
        if (af < 0) return -1;
        write(af, "1\n", 2);
        close(af);
        ++g_reenum_count;
        g_last_reenum_ms = monotonic_milliseconds();
        fprintf(stderr,
                "SP404_USB_REENUM_TOGGLED card=%d dev=%s count=%d\n",
                card, real, g_reenum_count);
        return 0;
    }
    /* v14.2: real USB bus reset (USBDEVFS_RESET) fallback. Some SP-404MKII
     * firmware revisions ignore the authorized-toggle unplug/replug and only
     * recover from an actual bus reset. Resolve the card's device node under
     * /dev/bus/usb and reset it, then let snd-usb-audio rebind fresh. */
    int sp404_usb_usbdevfs_reset(void) {
        int card = rescan_card_by_name();
        if (card <= 0) return -1;
        char link[192];
        snprintf(link, sizeof(link), "/sys/class/sound/card%d/device", card);
        char real[512];
        if (!realpath(link, real)) return -1;
        char *base = strrchr(real, '/');
        if (!base || !*(base + 1)) return -1;
        char path[320];
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s", base + 1);
        char buf[64];
        int bus = -1, dev = -1;
        int fd = open(path, O_RDONLY);
        if (fd < 0) return -1;
        close(fd);
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/busnum", base + 1);
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (n > 0) { buf[n] = 0; bus = atoi(buf); }
        }
        snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s/devnum", base + 1);
        fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (n > 0) { buf[n] = 0; dev = atoi(buf); }
        }
        if (bus <= 0 || dev <= 0) return -1;
        snprintf(path, sizeof(path), "/dev/bus/usb/%03d/%03d", bus, dev);
        fd = open(path, O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "SP404_USBDEVFS_RESET_OPEN_FAILED node=%s errno=%d (%s)\n",
                    path, errno, strerror(errno));
            return -1;
        }
        int rc = ioctl(fd, USBDEVFS_RESET, 0);
        int saved_errno = errno;
        close(fd);
        ++g_reenum_count;
        g_last_reenum_ms = monotonic_milliseconds();
        fprintf(stderr,
                "SP404_USBDEVFS_RESET card=%d bus=%d dev=%d rc=%d errno=%d (%s)\n",
                card, bus, dev, rc, saved_errno, strerror(saved_errno));
        return rc;
    }
    int requested_channels = argc > 4 ? atoi(argv[4]) : 1;
    if (requested_channels != 1 && requested_channels != 2)
        requested_channels = 1;
    g_audio_channels = (unsigned)requested_channels;
    g_audio_rate = 48000;
    const int lowlat = path_exists(LOWLAT_SENTINEL);
    const int period_frames = lowlat ? 240 : 480;
    const int periods = lowlat ? 4 : 8;
    const int starvation_grace_ms = 24;

    setvbuf(stderr, 0, _IOLBF, 4096);
    mkdir(RUNTIME_DIR, 0777);
    fprintf(
        stderr,
        "R36SX_SP404_HOST_AUDIO_IO_START SP404MKII_HOST_UAC2_ABI1 "
        "fifo=%s pcmp=%s pcmc=%s card=%d period_frames=%d periods=%d lowlat=%d "
        "rate=%u channels=%u runtime=%s starvation_grace_ms=%d\n",
        fifo, pcmp, pcmc, sp404_card, period_frames, periods, lowlat,
        g_audio_rate, g_audio_channels, RUNTIME_DIR, starvation_grace_ms);
    dump_asound_state("boot");
    write_text_file(
        DAEMON_VERSION,
        "R36SX_SP404_AUDIO_DAEMON_ABI=1\n");
    write_text_file(CAPTURE_ABI, "R36SX_SP404_CAPTURE_ABI=1\n");
    write_text_file(AUDIO_CHANNELS, g_audio_channels == 2 ? "2\n" : "1\n");
    write_text_file(AUDIO_RATE, "48000\n");
    write_text_file(
        AUDIO_PROFILE,
        g_audio_channels == 2 ? "STEREO_48K\n" : "MONO_48K\n");
    write_text_file(PLAYBACK_PCM_STATUS, "waiting-for-usb\n");
    set_status("USB capture idle");
    set_level_percent(0);
    set_lr_level_percent(0, 0);
    set_elapsed_seconds(0);
    write_text_file(CAPTURE_MONITOR, "0\n");
    cap.state = CAP_STATE_IDLE;
    write_capture_meta();
    mark_inactive();

    if (mkfifo(fifo, 0666) < 0 && errno != EEXIST)
        die_errno("mkfifo");
    chmod(fifo, 0666);
    int in = open(fifo, O_RDONLY | O_NONBLOCK);
    if (in < 0) die_errno("open fifo read nonblock");
    int keep = open(fifo, O_WRONLY | O_NONBLOCK);
    if (keep < 0) loge("open fifo keepalive optional");

    int pcm = -1;
    int active_period_frames = period_frames;
    int16_t out[2048];
    long total_frames = 0;
    long period_writes = 0;
    long signal_periods = 0;
    long source_silence_periods = 0;
    long starvation_silence_periods = 0;
    long starvation_events = 0;
    long poll_timeouts = 0;
    long xruns = 0;
    long dropped = 0;
    long reconnects = 0;
    long playback_prepare_failures = 0;
    long latency_trimmed_samples = 0;
    long latency_trim_events = 0;
    long short_write_events = 0;
    long consec_xrun_in_place = 0;
    int good_write_streak = 0;
    int last_conf = -1;
    int play_peak = 0;
    int stream_primed = 0;
    long fifo_samples_last = 0;
    unsigned sustained_overflow_periods = 0;
    unsigned long long asrc_resync_events = 0;

    for (;;) {
        if (g_sp404_stop_requested) {
            if (cap.active) stop_capture("daemon-stop");
            if (mon_cap_pcm >= 0) { close(mon_cap_pcm); mon_cap_pcm = -1; }
            if (pcm >= 0) { close(pcm); pcm = -1; }
            fifo_dump_finish("daemon-stop");
            close(in);
            if (keep >= 0) close(keep);
            mark_inactive();
            write_text_file(PLAYBACK_PCM_STATUS, "stopped-graceful\n");
            fprintf(stderr,
                    "R36SX_SP404_AUDIO_DAEMON_STOP_REQUESTED graceful_shutdown=1\n");
            break;
        }
        // H44 EVENT-DRIVEN USB STATE: no periodic marker polling while streaming
        // Only revalidate card/direction at startup/recovery; conf is event-driven
        int conf;
        if (g_usb_state == USB_STREAMING && pcm >= 0 && stream_primed) {
            conf = 1;
        } else {
            // startup / recovering: allow one check
            reread_sp404_card();
            refresh_audio_direction();
            // Use cached with 5000ms safety, but count it
            conf = usb_configured_cached();
            if (conf != last_conf) {
                fprintf(stderr,
                        "USB_STATE_CHANGE configured=%d ring_fill=%u\n",
                        conf, rfill);
                last_conf = conf;
            }
            if (conf) g_usb_state = USB_READY; else g_usb_state = USB_UNKNOWN;
        }

        drain_fifo(in, &dropped);
        // H44 OUT ONLY: monitor/capture/passive only if needed (guarded inside)
        if (!g_dir_out) {
            monitor_refresh_flag();
            poll_capture_command(pcmc);
            capture_tick();
            passive_monitor_tick(pcmc, conf);
        } else {
            // In OUT mode, only capture_tick if cap.active (should be 0)
            if (cap.active) capture_tick();
        }
        // Slow control plane outside critical ALSA path (2000ms)
        slow_control_tick();

        /*
         * U2.51.5 PLAYBACK_PARK_FOR_CAPTURE
         *
         * The SP-404MKII USB audio interface is simplex at full speed:
         * the probe proved capture delivers zero frames whenever the
         * host playback PCM is open, even if not streaming. Park (close)
         * the playback stream while a capture session is active and
         * reopen it once the session ends.
         */
        if (cap.active && pcm >= 0) {
            close(pcm);
            pcm = -1;
            playback_parked = 1;
            ring_reset();
            stream_primed = 0;
            good_write_streak = 0;
            mark_inactive();
            write_text_file(PLAYBACK_PCM_STATUS, "parked-for-capture\n");
            fprintf(stderr, "U2517_PCM_PLAY_PARKED_FOR_CAPTURE\n");
            sleep_ms(30);
        }
        if (!cap.active && playback_parked) {
            playback_parked = 0;
            force_playback_reopen = 1;
            fprintf(stderr, "U2517_PCM_PLAY_UNPARK_AFTER_CAPTURE\n");
        }

        if (force_playback_reopen) {
            force_playback_reopen = 0;
            if (pcm >= 0) {
                close(pcm);
                pcm = -1;
                fprintf(stderr,
                        "U2517_PCM_PLAY_FORCE_REOPEN_AFTER_USB_REC_EXIT\n");
            }
            ring_reset();
            asrc_source_reset();
            stream_primed = 0;
            good_write_streak = 0;
            mark_inactive();
            sleep_ms(80);
        }

        if (!conf) {
            // H44: only respect !conf when not streaming; streaming disconnect is via ALSA errors
            if (g_usb_state == USB_STREAMING && pcm >= 0) {
                // streaming healthy: ignore transient marker glitch, trust ALSA FD
                conf = 1;
                g_usb_state = USB_STREAMING;
            } else {
                if (pcm >= 0) {
                    close(pcm);
                    pcm = -1;
                    fifo_dump_finish("usb-disconnected");
                    fprintf(stderr, "PCM_PLAY_CLOSED_USB_DISCONNECTED\n");
                    g_usb_state = USB_RECOVERING;
                }
                active_period_frames = period_frames;
                good_write_streak = 0;
                write_text_file(PLAYBACK_PCM_STATUS, "waiting-for-usb\n");
                mark_inactive();
                ring_reset();
                asrc_source_reset();
                stream_primed = 0;
                sleep_ms(20);
                continue;
            }
        } else {
            if (g_usb_state == USB_READY && pcm >= 0 && stream_primed) g_usb_state = USB_STREAMING;
        }

        if (cap.active) {
            sleep_ms(5);
            continue;
        }

        /*
         * U2.51.10 DIRECTION_IN: host playback stays closed. The SP-404MKII
         * USB function is simplex; in IN mode the SP output is captured only
         * (direction from /mnt/sdcard/lgpt/otg/audio_driver_mode, see
         * refresh_audio_direction). The FIFO is drained above so the console
         * audio engine never blocks.
         */
        if (!g_dir_out) {
            sleep_ms(10);
            continue;
        }

        if (pcm < 0) {
            /* v12.1: if the stream has been stalled for 5 s and the device is
             * still present, force a software USB re-enumeration (unplug/
             * replug equivalent) instead of retrying the dead stream until
             * the user physically reconnects. */
            {
                unsigned long long now_ms = monotonic_milliseconds();
                if (g_eio_storm_start_ms != 0 && now_ms > 0 &&
                    now_ms >= g_eio_storm_start_ms &&
                    (now_ms - g_eio_storm_start_ms) >= 5000 &&
                    (g_last_reenum_ms == 0 ||
                     now_ms - g_last_reenum_ms >= 30000) &&
                    g_reenum_count < 8) {
                    /* v14.2: alternate soft unplug/replug with a real
                     * USBDEVFS_RESET bus reset. Some firmware revisions ignore
                     * the authorized toggle and only recover from a bus reset.
                     * Escalate up to 8 attempts (~4 min). */
                    int rc = (g_reenum_count % 2 == 0)
                                 ? toggle_sp404_usb_reenum()
                                 : sp404_usb_usbdevfs_reset();
                    fprintf(stderr,
                            "SP404_REENUM_ATTEMPT rc=%d stalled_ms=%llu count=%d\n",
                            rc, now_ms - g_eio_storm_start_ms, g_reenum_count);
                    /* H43 STALL RELEASE: gate and bookkeeping live in the
                     * caller.  On a stalled device the attempted helpers were
                     * observed to bail out before touching g_last_reenum_ms /
                     * g_reenum_count, so the retry loop fired roughly every
                     * 1.2 s forever with count=0, keeping the musb host role
                     * locked and preventing any switch away from Sampler.
                     * After 8 attempts the daemon exits; the SD supervisor
                     * retries on its normal backoff and the host role is
                     * released immediately, so Android/Windows accepts the
                     * mode switch without the manual detour. */
                    g_last_reenum_ms = monotonic_milliseconds();
                    ++g_reenum_count;
                    if (g_reenum_count >= 8) {
                        fprintf(stderr,
                                "SP404_REENUM_EXHAUSTED attempts=%d stalled_ms=%llu exiting daemon for supervisor backoff\n",
                                g_reenum_count,
                                now_ms - g_eio_storm_start_ms);
                        write_text_file(PLAYBACK_PCM_STATUS,
                                        "sp404-stall-exhausted\n");
                        if (cap.active) stop_capture("daemon-stop");
                        mark_inactive();
                        fifo_dump_finish("stall-exhausted");
                        if (in >= 0) close(in);
                        if (keep >= 0) close(keep);
                        return 3;
                    }
                    sleep_ms(1200);
                    continue;
                }
            }
            pcm = open(pcmp, O_WRONLY);
            if (pcm < 0) {
                good_write_streak = 0;
                write_text_file(PLAYBACK_PCM_STATUS, "open-failed\n");
                mark_inactive();
                loge("open playback pcm retry");
                sleep_ms(200);
                continue;
            }

            int prepare_rc = pcm_prepare_common(
                pcm, 0, g_audio_rate, g_audio_channels,
                (unsigned)active_period_frames, (unsigned)periods);

            if (prepare_rc < 0 && active_period_frames != 480) {
                fprintf(stderr,
                        "PLAY_PCM_RETRY_BASELINE_PERIOD previous=%d next=480 rc=%d\n",
                        active_period_frames, prepare_rc);
                close(pcm);
                pcm = open(pcmp, O_WRONLY);
                active_period_frames = 480;
                if (pcm < 0) {
                    prepare_rc = -errno;
                } else {
                    prepare_rc = pcm_prepare_common(
                        pcm, 0, g_audio_rate, g_audio_channels,
                        480, (unsigned)periods);
                }
            }

            if (prepare_rc < 0) {
                char pcm_status[160];
                ++playback_prepare_failures;
                snprintf(
                    pcm_status, sizeof(pcm_status),
                    "prepare-failed rc=%d rate=%u channels=%u period=%d failures=%ld\n",
                    prepare_rc, g_audio_rate, g_audio_channels,
                    active_period_frames, playback_prepare_failures);
                write_text_file(PLAYBACK_PCM_STATUS, pcm_status);
                close(pcm);
                pcm = -1;
                good_write_streak = 0;
                mark_inactive();
                sleep_ms(300);
                continue;
            }

            {
                char pcm_status[128];
                snprintf(
                    pcm_status, sizeof(pcm_status),
                    "ready rate=%u channels=%u period=%d engine=alsa-clocked\n",
                    g_audio_rate, g_audio_channels, active_period_frames);
                write_text_file(PLAYBACK_PCM_STATUS, pcm_status);
            }
            good_write_streak = 0;
            stream_primed = 0;
            g_usb_state = USB_READY;
            ++reconnects;
            fprintf(stderr,
                    "PCM_PLAY_OPENED reconnects=%ld period_frames=%d channels=%u start_threshold_frames=%d\n",
                    reconnects, active_period_frames, g_audio_channels,
                    active_period_frames * periods);
            if ((reconnects % 5) == 0)
                dump_asound_state("play-reopen");
            /* v12.1: start the freshly opened stream from current audio; the
             * core's stale backlog is the initial static burst. */
            flush_input_fifo(in);
            /* v14: do NOT reset the storm timer here. A stalled device
             * (e.g. SP404 in standalone mode with ext source unlatched)
             * still succeeds open(), so resetting on reopen kept the 5 s
             * EIO re-enumeration window from ever firing. The timer is only
             * cleared by 8 consecutive good writes below. */
        }

        const unsigned required_samples =
            (unsigned)active_period_frames * 2u;
        const unsigned hw_period_frames = (unsigned)active_period_frames;

        /* ---- U2.63 ASRC clocked write path ---- */
        if (!stream_primed) {
            if (asrc_total_backlog() < ASRC_PRIME_BACKLOG_FRAMES) {
                asrc_stage_append();
                sleep_ms(1);
                continue;
            }
            stream_primed = 1;
            g_usb_state = USB_STREAMING;
            fprintf(stderr,
                    "PLAYBACK_ASRC_PRIMED backlog=%u target=%u prime=%u "
                    "period=%u gain=%.2f\n",
                    asrc_total_backlog(), ASRC_TARGET_BACKLOG_FRAMES,
                    ASRC_PRIME_BACKLOG_FRAMES, hw_period_frames,
                    g_playback_gain);
            fifo_dump_start();
            load_playback_gain();
        }

        /* U2.63.1 parity with the u2523 Windows daemon: if the producer
         * outpaced us beyond 4x target for a full second, force a clean
         * re-prime instead of limping with multi-hundred-ms latency. */
        if (asrc_total_backlog() > 3U * ASRC_STAGE_FRAMES) {
            if (++sustained_overflow_periods >= 8) {
                ++asrc_resync_events;
                ring_reset();
                asrc_source_reset();
                stream_primed = 0;
                sustained_overflow_periods = 0;
                fprintf(stderr,
                        "SP404_ASRC_BACKLOG_RESYNC event=%llu backlog=%u target=%u\n",
                        asrc_resync_events, asrc_total_backlog(),
                        ASRC_TARGET_BACKLOG_FRAMES);
                continue;
            }
        } else {
            sustained_overflow_periods = 0;
        }

        int playback_poll_timeout_ms =
            (active_period_frames * 1000) / (int)g_audio_rate;
        if (playback_poll_timeout_ms < 2) playback_poll_timeout_ms = 2;
        int writable =
            wait_playback_writable(pcm, playback_poll_timeout_ms);
        if (writable < 0) {
            ++xruns;
            g_usb_state = USB_RECOVERING;
            if (xruns <= 8 || (xruns % 25) == 0)
                fprintf(stderr,
                        "PLAY_WAIT_WRITABLE_ERR rc=%d errno=%d (%s) ring_fill=%u reconnects=%ld\n",
                        writable, -writable, strerror(-writable), rfill, reconnects);
            if (g_eio_storm_start_ms == 0)
                g_eio_storm_start_ms = monotonic_milliseconds();
            close(pcm);
            pcm = -1;
            ring_reset();
            asrc_source_reset();
            stream_primed = 0;
            good_write_streak = 0;
            mark_inactive();
            eio_backoff_sleep();
            continue;
        }
        if (writable == 0) {
            ++poll_timeouts;
            continue;
        }

        /* Capture data may have arrived while poll waited. */
        drain_fifo(in, &dropped);

        uint64_t period_step = 0ULL;
        int used_starvation_silence = 0;
        int local_peak = 0;
        if (asrc_prepare_period(hw_period_frames, &period_step)) {
            asrc_render_exact(out, hw_period_frames, period_step);
            /* P1 C: fused peak + gain. Preserves exact semantics for play_peak/signal/silence, handles INT16_MIN (-32768 -> 32768). */
            if (g_playback_gain != 1.0f) {
                unsigned g;
                local_peak = 0;
                for (g = 0U; g < required_samples; ++g) {
                    int16_t v = (int16_t)((float)out[g] * g_playback_gain);
                    out[g] = v;
                    int av = v;
                    if (av < 0) av = -av; /* -32768 -> 32768 fits int */
                    if (av > local_peak) local_peak = av;
                }
            } else {
                unsigned g;
                local_peak = 0;
                for (g = 0U; g < required_samples; ++g) {
                    int av = out[g];
                    if (av < 0) av = -av;
                    if (av > local_peak) local_peak = av;
                }
            }
            if (local_peak > play_peak) play_peak = local_peak;
            if (local_peak == 0)
                ++source_silence_periods;
            else
                ++signal_periods;
#if SP404_ENABLE_FIFO_DUMP
            /* P1 B: FIFO dump only when enabled, zero overhead when SP404_ENABLE_FIFO_DUMP==0 */
            {
                unsigned pd;
                for (pd = 0U; pd < hw_period_frames; ++pd)
                    fifo_dump_write(out + pd * 2U);
            }
#endif
        } else {
            memset(out, 0, required_samples * sizeof(int16_t));
            used_starvation_silence = 1;
            local_peak = 0;
            ++starvation_events;
            ++asrc_clock_hold_periods;
            asrc_clock_hold_frames += hw_period_frames;
            if (starvation_events <= 4 ||
                (starvation_events % 100) == 0) {
                fprintf(stderr,
                        "PLAYBACK_ASRC_CLOCK_HOLD event=%ld backlog=%u "
                        "stage=%u action=zero-period-no-phase-advance-no-prepare\n",
                        starvation_events, asrc_total_backlog(),
                        asrc_stage_frames);
            }
            /* starvation silence does not affect play_peak/signal counters (preserved) */
        }

        /* P1 D: format conversion fastpath. If device is 2ch S16LE shift0, out is already exact ALSA payload -> direct pointer, no memcpy. For 4ch S16LE shift0, specialized L R 0 0. */
        static unsigned char devbuf[2048 * 2 * 4];
        unsigned char *play_buf = devbuf;
        int use_direct = 0;
        if (!used_starvation_silence) {
            /* fastpath checks are branch-predicted; dev config stable after open */
            if (g_dev_play_channels == 2 && g_dev_play_bytes == 2 && g_dev_play_shift == 0) {
                play_buf = (unsigned char *)out;
                use_direct = 1;
            } else if (g_dev_play_channels == 4 && g_dev_play_bytes == 2 && g_dev_play_shift == 0) {
                /* P1 D 4ch S16 specialized: L R 0 0 without generic byte shift loop */
                int16_t *d = (int16_t *)devbuf;
                unsigned f;
                for (f = 0; f < (unsigned)active_period_frames; ++f) {
                    d[f*4] = out[f*2];
                    d[f*4+1] = out[f*2+1];
                    d[f*4+2] = 0;
                    d[f*4+3] = 0;
                }
                play_buf = devbuf;
                use_direct = 0;
            } else {
                convert_s16_to_device(
                    out,
                    active_period_frames,
                    2u,
                    devbuf,
                    g_dev_play_channels,
                    g_dev_play_bytes,
                    g_dev_play_shift);
                play_buf = devbuf;
                use_direct = 0;
            }
        } else {
            /* starvation silence: still need correct silent payload for negotiated format */
            if (g_dev_play_channels == 2 && g_dev_play_bytes == 2 && g_dev_play_shift == 0) {
                play_buf = (unsigned char *)out; /* out already zeroed */
                use_direct = 1;
            } else if (g_dev_play_channels == 4 && g_dev_play_bytes == 2 && g_dev_play_shift == 0) {
                memset(devbuf, 0, (size_t)active_period_frames * 4 * 2);
                play_buf = devbuf;
                use_direct = 0;
            } else {
                convert_s16_to_device(
                    out,
                    active_period_frames,
                    2u,
                    devbuf,
                    g_dev_play_channels,
                    g_dev_play_bytes,
                    g_dev_play_shift);
                play_buf = devbuf;
                use_direct = 0;
            }
        }
        (void)use_direct;
        int wr = write_frames_exact(
            pcm, play_buf, active_period_frames,
            g_dev_play_channels, g_dev_play_bytes);
        if (wr < 0) {
            ++xruns;
            g_usb_state = USB_RECOVERING;
            good_write_streak = 0;
            mark_inactive();
            stream_primed = 0;
            if (xruns <= 4 || (xruns % 10) == 0) {
                fprintf(stderr,
                        "PLAY_XRUN_DETAIL rc=%d errno=%d (%s) ring_fill=%u reconnects=%ld\n",
                        wr, -wr, strerror(-wr), rfill, reconnects);
                dump_asound_state("play-xrun");
            }
            if (wr == -EIO || wr == -ENODEV || wr == -ESHUTDOWN) {
                if (xruns <= 4 || (xruns % 10) == 0)
                    fprintf(stderr,
                            "PLAY_PCM_CLOSE_FATAL rc=%d errno=%d (%s)\n",
                            wr, -wr, strerror(-wr));
                close(pcm);
                pcm = -1;
                fifo_dump_finish("fatal-close");
                eio_backoff_sleep();
                continue;
            }
            if (wr == -EPIPE || wr == -ESTRPIPE) {
                ++consec_xrun_in_place;
                if (consec_xrun_in_place >= 4) {
                    consec_xrun_in_place = 0;
                    fprintf(stderr,
                            "PLAY_PCM_CLOSE_AFTER_EPIPE_STORM reconnects=%ld\n",
                            reconnects);
                    close(pcm);
                    pcm = -1;
                    fifo_dump_finish("epipe-storm");
                    eio_backoff_sleep();
                    continue;
                }
                if (xruns <= 8 || (xruns % 50) == 0)
                    fprintf(stderr,
                            "PLAY_EPIPE_RESUME_IN_PLACE xruns=%ld ring_fill=%u\n",
                            xruns, rfill);
                continue;
            }
        } else if (wr != active_period_frames) {
            ++short_write_events;
            good_write_streak = 0;
            stream_primed = 0;
            mark_inactive();
            fprintf(stderr,
                    "PLAYBACK_SHORT_WRITE event=%ld wrote=%d expected=%d\n",
                    short_write_events, wr, active_period_frames);
        } else {
            if (good_write_streak < 1000) ++good_write_streak;
            if (consec_xrun_in_place != 0) consec_xrun_in_place = 0;
            if (good_write_streak >= 8) {
                mark_active();
                if (g_eio_backoff_ms != 250) g_eio_backoff_ms = 250;
                g_eio_storm_start_ms = 0;
            }
        }

        total_frames += active_period_frames;
        ++period_writes;
        if ((period_writes % 200) == 0) {
            long fifo_delta = rs_fifo_total - fifo_samples_last;
            fifo_samples_last = rs_fifo_total;
            /* FIFO rate diagnostic: producer frames per second delivered by
             * the core into the fifo (48000 = realtime; no resampling in the
             * daemon, so this is the source pace only). */
            long producer_rate_per_s = (fifo_delta / 2) / 2;
            fprintf(stderr,
                    "BRIDGE_PROGRESS_U2517 frames=%ld seconds=%.2f writes=%ld signal=%ld source_silence=%ld starvation_silence=%ld starvation_events=%ld xruns=%ld dropped=%ld ring_fill=%u reconnects=%ld configured=%d good_streak=%d cap_active=%d cap_frames=%ld monitor=%d play_peak=%d play_xrun_recoveries=%ld cap_xrun_recoveries=%ld period=%d channels=%u prepare_failures=%ld latency_trim_events=%ld latency_trimmed_samples=%ld poll_timeouts=%ld short_writes=%ld primed=%d fifo_total=%ld fifo_rate_per_s=%ld\n",
                    total_frames, (double)total_frames / 48000.0,
                    period_writes, signal_periods,
                    source_silence_periods, starvation_silence_periods,
                    starvation_events, xruns, dropped, rfill,
                    reconnects, conf, good_write_streak,
                    cap.active, cap.frames, mon_enabled, play_peak,
                    play_xrun_recoveries, cap_xrun_recoveries,
                    active_period_frames, g_audio_channels,
                    playback_prepare_failures, latency_trim_events,
                    latency_trimmed_samples, poll_timeouts,
                    short_write_events, stream_primed,
                    rs_fifo_total, producer_rate_per_s);
            play_peak = 0;
        }
    }
    return 0;
}
