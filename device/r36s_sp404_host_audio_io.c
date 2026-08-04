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
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sound/asound.h>

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
static const char *CAPTURE_STAGING_DIR = "/mnt/sdcard/lgpt/samples/records";
static const char *AUDIO_PROFILE = "/tmp/r36sx_lgpt_usb/audio_profile";
static const char *AUDIO_CHANNELS = "/tmp/r36sx_lgpt_usb/audio_channels";
static const char *AUDIO_RATE = "/tmp/r36sx_lgpt_usb/audio_rate";
static const char *PLAYBACK_PCM_STATUS = "/tmp/r36sx_lgpt_usb/playback_pcm_status";
static unsigned g_audio_channels = 1;
static unsigned g_audio_rate = 48000;

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
static int path_exists(const char *p) { struct stat st; return p && stat(p, &st) == 0; }
static void sleep_ms(int ms) { usleep((useconds_t)ms * 1000U); }

static unsigned long long monotonic_milliseconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return ((unsigned long long)ts.tv_sec * 1000ULL) +
           ((unsigned long long)ts.tv_nsec / 1000000ULL);
}

static void mirror_runtime_state(const char *path, const char *text) {
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
}

static int file_contains(const char *p, const char *needle) {
    char buf[128]; int fd = open(p, O_RDONLY); if (fd < 0) return 0;
    ssize_t n = read(fd, buf, sizeof(buf)-1); close(fd); if (n <= 0) return 0;
    buf[n] = 0; return strstr(buf, needle) != 0;
}
static int usb_configured(void) {
    return file_contains(SP404_CARD, "FAILED") == 0 &&
           file_contains(SP404_CARD, "none") == 0 &&
           path_exists(SP404_CARD);
}

static int usb_configured_cached(void) {
    static unsigned long long last_check_ms = 0;
    static int cached = 0;
    const unsigned long long now = monotonic_milliseconds();
    if (last_check_ms == 0 || now < last_check_ms ||
        (now - last_check_ms) >= 25ULL) {
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
     * values volatile; status, metadata, ABI and profile state remain mirrored.
     */
    if (path &&
        strstr(path, RUNTIME_DIR) == path &&
        !runtime_value_is_volatile(path)) {
        mirror_runtime_state(path, text);
    }
}
static void mark_active(void) { char b[64]; snprintf(b, sizeof(b), "%ld\n", (long)time(NULL)); write_text_file(ACTIVE_MARKER, b); }
static void mark_inactive(void) { unlink(ACTIVE_MARKER); }

static void ensure_capture_staging_dir(void) {
    mkdir("/mnt/sdcard", 0777);
    mkdir("/mnt/sdcard/lgpt", 0777);
    mkdir("/mnt/sdcard/lgpt/samples", 0777);
    mkdir(CAPTURE_STAGING_DIR, 0777);
}

static void basename_from_path(const char *path, char *dst, int len) {
    const char *b = path ? strrchr(path, '/') : 0;
    b = b ? b + 1 : path;
    if (!b || !b[0]) b = "USBREC.wav";
    snprintf(dst, len, "%s", b);
}

static int open_capture_wav_with_fallback(const char *requested, const char *name, char *actual, int actual_len) {
    ensure_capture_staging_dir();
    if (requested && requested[0]) snprintf(actual, actual_len, "%s", requested);
    else snprintf(actual, actual_len, "%s/%s", CAPTURE_STAGING_DIR, name && name[0] ? name : "USBREC.wav");
    int fd = open(actual, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd >= 0) return fd;
    int first_errno = errno;
    if (first_errno == EEXIST) {
        fprintf(stderr, "CAPTURE_DUPLICATE_BLOCKED path=%s\n", actual);
        return -1;
    }
    fprintf(stderr, "CAPTURE_OPEN_REQUESTED_FAILED path=%s errno=%d (%s)\n", actual, first_errno, strerror(first_errno));
    char safe_name[128]; basename_from_path(name && name[0] ? name : requested, safe_name, sizeof(safe_name));
    snprintf(actual, actual_len, "%s/%s", CAPTURE_STAGING_DIR, safe_name);
    fd = open(actual, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd >= 0) {
        fprintf(stderr, "CAPTURE_OPEN_FALLBACK_OK path=%s from_errno=%d\n", actual, first_errno);
        return fd;
    }
    fprintf(stderr, "CAPTURE_OPEN_FALLBACK_FAILED path=%s errno=%d (%s)\n", actual, errno, strerror(errno));
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

    init_hw_params(&hw);
    param_set_mask(
        &hw,
        SNDRV_PCM_HW_PARAM_ACCESS,
        SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    param_set_mask(
        &hw,
        SNDRV_PCM_HW_PARAM_FORMAT,
        SNDRV_PCM_FORMAT_S16_LE);
    param_set_mask(
        &hw,
        SNDRV_PCM_HW_PARAM_SUBFORMAT,
        SNDRV_PCM_SUBFORMAT_STD);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
        16);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_FRAME_BITS,
        16 * channels);
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
        period_frames * channels * 2);
    interval_set(
        &hw,
        SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
        period_frames * periods * channels * 2);
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
        is_capture ? 1 : period_frames * periods;
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

    struct snd_pcm_hw_params hw;
    init_hw_params(&hw);
    param_set_mask(&hw, SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    param_set_mask(&hw, SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FORMAT_S16_LE);
    param_set_mask(&hw, SNDRV_PCM_HW_PARAM_SUBFORMAT, SNDRV_PCM_SUBFORMAT_STD);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 16);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_FRAME_BITS, 16 * channels);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_CHANNELS, channels);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_RATE, rate);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, period_frames);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_PERIODS, periods);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, period_frames * periods);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, period_frames * channels * 2);
    interval_set(&hw, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, period_frames * periods * channels * 2);
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

static int write_frames_exact(
    int pcm,
    int16_t *buf,
    int frames,
    unsigned channels) {
    int completed = 0;
    int idle_retries = 0;
    while (completed < frames) {
        struct snd_xferi x;
        memset(&x, 0, sizeof(x));
        x.buf = buf + ((size_t)completed * channels);
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

static int read_frames(int pcm, int16_t *buf, int frames) {
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
static unsigned ring_push_samples(const int16_t *s, unsigned n) { unsigned pushed = 0; while (pushed < n && rfill < RING_SAMPLES) { ring[wpos] = s[pushed++]; wpos = (wpos + 1) % RING_SAMPLES; rfill++; } return pushed; }
static unsigned ring_pop_samples(int16_t *d, unsigned n) { unsigned popped = 0; while (popped < n && rfill > 0) { d[popped++] = ring[rpos]; rpos = (rpos + 1) % RING_SAMPLES; rfill--; } return popped; }
static unsigned ring_drop_oldest_samples(unsigned n) {
    unsigned dropped = n < rfill ? n : rfill;
    rpos = (rpos + dropped) % RING_SAMPLES;
    rfill -= dropped;
    return dropped;
}
static void drain_fifo(int in, long *dropped) { int16_t inbuf[4096]; for (;;) { ssize_t r = read(in, inbuf, sizeof(inbuf)); if (r > 0) { unsigned samples = (unsigned)(r / 2); unsigned pushed = ring_push_samples(inbuf, samples); if (pushed < samples && dropped) *dropped += (long)(samples - pushed); continue; } if (r == 0) break; if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break; loge("read fifo"); break; } }

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
    monitor_refresh_flag();
    if (!configured || !mon_enabled || cap.active) {
        if (mon_cap_pcm >= 0) { close(mon_cap_pcm); mon_cap_pcm = -1; }
        return;
    }
    if (mon_cap_pcm < 0) {
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
    }
    int16_t buf[480 * 2]; int r = read_frames(mon_cap_pcm, buf, 480);
    if (r > 0) {
        int ll = 0, rr = 0; peak_percent_lr_s16(buf, r, g_audio_channels, &ll, &rr); int level = (ll > rr) ? ll : rr;
        set_lr_level_percent(ll, rr);
        monitor_write_samples(buf, r);
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

    int16_t buffer[480 * 2];
    int frames = read_frames(cap.pcm, buffer, 480);

    if (frames > 0) {
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
        snprintf(
            cap.error,
            sizeof(cap.error),
            "no USB capture frames after 5s; check host playback and stereo/mono profile");
        stop_capture("no-data-error");
        return;
    }

    if (frames < 0) {
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

int main(int argc, char **argv) {
    struct sigaction pipe_action;
    memset(&pipe_action, 0, sizeof(pipe_action));
    pipe_action.sa_handler = SIG_IGN;
    sigemptyset(&pipe_action.sa_mask);
    sigaction(SIGPIPE, &pipe_action, 0);

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
        /* Only auto-track when the pcm paths were not forced on the command
         * line. Reread the live markers so a late/hotplug enumeration on a
         * different card index is picked up without a daemon restart. */
        if (pcm_path_overridden) return sp404_card;
        unsigned long long now = monotonic_milliseconds();
        if (last_card_recheck_ms != 0 && now >= last_card_recheck_ms &&
            (now - last_card_recheck_ms) < 500ULL) return sp404_card;
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
    int requested_channels = argc > 4 ? atoi(argv[4]) : 1;
    if (requested_channels != 1 && requested_channels != 2)
        requested_channels = 1;
    g_audio_channels = (unsigned)requested_channels;
    g_audio_rate = 48000;
    const int lowlat = path_exists(LOWLAT_SENTINEL);
    const int period_frames = lowlat ? 240 : 480;
    const int periods = 4;
    const unsigned producer_burst_frames = 800U;
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
    int16_t out[480 * 2];
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
    int good_write_streak = 0;
    int last_conf = -1;
    int play_peak = 0;
    int stream_primed = 0;
    unsigned long long starvation_since_ms = 0;

    for (;;) {
        reread_sp404_card();
        int conf = usb_configured_cached();
        if (conf != last_conf) {
            fprintf(stderr,
                    "USB_STATE_CHANGE configured=%d ring_fill=%u\n",
                    conf, rfill);
            last_conf = conf;
        }

        drain_fifo(in, &dropped);
        monitor_refresh_flag();
        poll_capture_command(pcmc);
        capture_tick();
        passive_monitor_tick(pcmc, conf);

        if (force_playback_reopen) {
            force_playback_reopen = 0;
            if (pcm >= 0) {
                close(pcm);
                pcm = -1;
                fprintf(stderr,
                        "U2517_PCM_PLAY_FORCE_REOPEN_AFTER_USB_REC_EXIT\n");
            }
            ring_reset();
            stream_primed = 0;
            starvation_since_ms = 0;
            good_write_streak = 0;
            mark_inactive();
            sleep_ms(80);
        }

        if (!conf) {
            if (pcm >= 0) {
                close(pcm);
                pcm = -1;
                fprintf(stderr, "PCM_PLAY_CLOSED_USB_DISCONNECTED\n");
            }
            active_period_frames = period_frames;
            good_write_streak = 0;
            write_text_file(PLAYBACK_PCM_STATUS, "waiting-for-usb\n");
            mark_inactive();
            ring_reset();
            stream_primed = 0;
            starvation_since_ms = 0;
            sleep_ms(20);
            continue;
        }

        if (pcm < 0) {
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
            starvation_since_ms = 0;
            ++reconnects;
            fprintf(stderr,
                    "PCM_PLAY_OPENED reconnects=%ld period_frames=%d channels=%u start_threshold_frames=%d\n",
                    reconnects, active_period_frames, g_audio_channels,
                    active_period_frames * periods);
        }

        const unsigned required_samples =
            (unsigned)active_period_frames * g_audio_channels;
        const unsigned hardware_buffer_samples =
            required_samples * (unsigned)periods;
        const unsigned producer_burst_samples =
            producer_burst_frames * g_audio_channels;
        /* Fill exactly the hardware buffer before auto-start. Once ALSA
         * starts, POLLOUT is the consumer clock and the 24 ms grace window
         * bridges the 16.67 ms libretro producer cadence. An extra producer
         * burst here only adds latency and is not required for continuity. */
        const unsigned prime_target = hardware_buffer_samples;
        const unsigned latency_limit =
            prime_target + (producer_burst_samples * 2U);

        if (rfill > latency_limit) {
            unsigned trim = rfill - prime_target;
            trim -= trim % g_audio_channels;
            latency_trimmed_samples +=
                (long)ring_drop_oldest_samples(trim);
            ++latency_trim_events;
            if (latency_trim_events <= 4 ||
                (latency_trim_events % 50) == 0) {
                fprintf(stderr,
                        "PLAYBACK_LATENCY_TRIM event=%ld trimmed_samples=%u total_trimmed=%ld ring_fill=%u target=%u limit=%u\n",
                        latency_trim_events, trim,
                        latency_trimmed_samples, rfill,
                        prime_target, latency_limit);
            }
        }

        if (!stream_primed) {
            if (rfill < prime_target) {
                sleep_ms(1);
                continue;
            }
            stream_primed = 1;
            starvation_since_ms = 0;
            fprintf(stderr,
                    "PLAYBACK_CLOCKED_PRIMED ring_fill=%u target=%u hw_buffer_samples=%u producer_burst_samples=%u period_frames=%d channels=%u\n",
                    rfill, prime_target, hardware_buffer_samples,
                    producer_burst_samples, active_period_frames,
                    g_audio_channels);
        }

        int playback_poll_timeout_ms =
            (active_period_frames * 1000) / (int)g_audio_rate;
        if (playback_poll_timeout_ms < 2) playback_poll_timeout_ms = 2;
        int writable =
            wait_playback_writable(pcm, playback_poll_timeout_ms);
        if (writable < 0) {
            ++xruns;
            close(pcm);
            pcm = -1;
            ring_reset();
            stream_primed = 0;
            starvation_since_ms = 0;
            good_write_streak = 0;
            mark_inactive();
            sleep_ms(100);
            continue;
        }
        if (writable == 0) {
            ++poll_timeouts;
            continue;
        }

        /* Capture data may have arrived while poll waited. */
        drain_fifo(in, &dropped);

        int used_starvation_silence = 0;
        if (rfill >= required_samples) {
            unsigned got = ring_pop_samples(out, required_samples);
            if (got != required_samples) {
                fprintf(stderr,
                        "INTERNAL_RING_SHORT_READ got=%u required=%u\n",
                        got, required_samples);
                stream_primed = 0;
                continue;
            }
            starvation_since_ms = 0;
        } else {
            unsigned long long now_ms = monotonic_milliseconds();
            if (starvation_since_ms == 0)
                starvation_since_ms = now_ms;
            if (now_ms >= starvation_since_ms &&
                (now_ms - starvation_since_ms) <
                    (unsigned long long)starvation_grace_ms) {
                sleep_ms(1);
                continue;
            }

            memset(out, 0, required_samples * sizeof(int16_t));
            used_starvation_silence = 1;
            ++starvation_events;
            ++starvation_silence_periods;
            starvation_since_ms = 0;
            if (starvation_events <= 4 ||
                (starvation_events % 100) == 0) {
                fprintf(stderr,
                        "PLAYBACK_SOURCE_STARVATION event=%ld ring_fill=%u required=%u grace_ms=%d action=full-silence-period-no-reset\n",
                        starvation_events, rfill,
                        required_samples, starvation_grace_ms);
            }
        }

        int local_peak = 0;
        if (!used_starvation_silence) {
            unsigned i;
            for (i = 0; i < required_samples; ++i) {
                int value = out[i];
                if (value < 0) value = -value;
                if (value > local_peak) local_peak = value;
            }
            if (local_peak > play_peak) play_peak = local_peak;
            if (local_peak == 0)
                ++source_silence_periods;
            else
                ++signal_periods;
        }

        int wr = write_frames_exact(
            pcm, out, active_period_frames, g_audio_channels);
        if (wr < 0) {
            ++xruns;
            good_write_streak = 0;
            mark_inactive();
            stream_primed = 0;
            starvation_since_ms = 0;
            if (wr == -EIO || wr == -ENODEV || wr == -ESHUTDOWN ||
                wr == -EPIPE || wr == -ESTRPIPE) {
                close(pcm);
                pcm = -1;
                sleep_ms(100);
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
            if (good_write_streak >= 8) mark_active();
        }

        total_frames += active_period_frames;
        ++period_writes;
        if ((period_writes % 200) == 0) {
            fprintf(stderr,
                    "BRIDGE_PROGRESS_U2517 frames=%ld seconds=%.2f writes=%ld signal=%ld source_silence=%ld starvation_silence=%ld starvation_events=%ld xruns=%ld dropped=%ld ring_fill=%u reconnects=%ld configured=%d good_streak=%d cap_active=%d cap_frames=%ld monitor=%d play_peak=%d play_xrun_recoveries=%ld cap_xrun_recoveries=%ld period=%d channels=%u prepare_failures=%ld latency_trim_events=%ld latency_trimmed_samples=%ld poll_timeouts=%ld short_writes=%ld primed=%d\n",
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
                    short_write_events, stream_primed);
            play_peak = 0;
        }
    }
    return 0;
}
