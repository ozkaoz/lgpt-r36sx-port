/*
 * r36s_au11_usb_audio_io.c
 * Experimental R36SX LGPT USB-C audio I/O daemon.
 *
 * Playback/output path inherited from AU9V:
 *   LGPT core -> FIFO 48 kHz mono S16_LE -> /dev/snd/pcmC0D0p -> Windows microphone/input.
 *
 * Capture/input path AU11:
 *   Windows playback/output -> UAC2 capture endpoint -> /dev/snd/pcmC0D0c -> WAV file.
 *
 * Capture is command driven through /mnt/sdcard/lgpt/otg/usb_capture_cmd.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
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
static const char *CAPTURE_CMD = "/tmp/r36sx_lgpt_usb/usb_capture_cmd";
static const char *CAPTURE_STATUS = "/tmp/r36sx_lgpt_usb/usb_capture_status";
static const char *CAPTURE_LAST_NAME = "/tmp/r36sx_lgpt_usb/usb_capture_last_name";
static const char *CAPTURE_LAST_PATH = "/tmp/r36sx_lgpt_usb/usb_capture_last_path";
static const char *CAPTURE_LEVEL = "/tmp/r36sx_lgpt_usb/usb_capture_level";
static const char *CAPTURE_LEVEL_L = "/tmp/r36sx_lgpt_usb/usb_capture_level_l";
static const char *CAPTURE_LEVEL_R = "/tmp/r36sx_lgpt_usb/usb_capture_level_r";
static const char *CAPTURE_ELAPSED = "/tmp/r36sx_lgpt_usb/usb_capture_elapsed";
static const char *CAPTURE_MONITOR = "/tmp/r36sx_lgpt_usb/usb_capture_monitor";
static const char *CAPTURE_MONITOR_FIFO = "/tmp/r36sx_usb_capture_monitor_fifo";
static const char *RUNTIME_MIRROR_DIR = "/mnt/sdcard/lgpt/otg/logs/runtime_state";
static const char *CAPTURE_STAGING_DIR = "/mnt/sdcard/lgpt/usbrecs";

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
    if (file_contains("/sys/class/udc/musb-hdrc.0.auto/state", "configured")) return 1;
    if (file_contains("/sys/class/udc/musb-hdrc.1.auto/state", "configured")) return 1;
    return 0;
}
static void write_text_file(const char *path, const char *text) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) { if (text) write(fd, text, strlen(text)); close(fd); }
    if (path && strstr(path, RUNTIME_DIR) == path) mirror_runtime_state(path, text);
}
static void mark_active(void) { char b[64]; snprintf(b, sizeof(b), "%ld\n", (long)time(NULL)); write_text_file(ACTIVE_MARKER, b); }
static void mark_inactive(void) { unlink(ACTIVE_MARKER); }

static void ensure_capture_staging_dir(void) {
    mkdir("/mnt/sdcard", 0777);
    mkdir("/mnt/sdcard/lgpt", 0777);
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
    int fd = open(actual, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) return fd;
    int first_errno = errno;
    fprintf(stderr, "CAPTURE_OPEN_REQUESTED_FAILED path=%s errno=%d (%s)\n", actual, first_errno, strerror(first_errno));
    char safe_name[128]; basename_from_path(name && name[0] ? name : requested, safe_name, sizeof(safe_name));
    snprintf(actual, actual_len, "%s/%s", CAPTURE_STAGING_DIR, safe_name);
    fd = open(actual, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        fprintf(stderr, "CAPTURE_OPEN_FALLBACK_OK path=%s from_errno=%d\n", actual, first_errno);
        return fd;
    }
    fprintf(stderr, "CAPTURE_OPEN_FALLBACK_FAILED path=%s errno=%d (%s)\n", actual, errno, strerror(errno));
    return -1;
}

static int pcm_prepare_common(int fd, int is_capture, unsigned rate, unsigned channels, unsigned period_frames, unsigned periods) {
    int version = 0;
    if (ioctl(fd, SNDRV_PCM_IOCTL_PVERSION, &version) < 0) die_errno(is_capture ? "CAP_PVERSION" : "PLAY_PVERSION");
    fprintf(stderr, "%s_PCM_VERSION=%d.%d.%d\n", is_capture ? "CAP" : "PLAY", (version >> 16) & 0xff, (version >> 8) & 0xff, version & 0xff);
    struct snd_pcm_info info; memset(&info, 0, sizeof(info));
    if (ioctl(fd, SNDRV_PCM_IOCTL_INFO, &info) < 0) die_errno(is_capture ? "CAP_PCM_INFO" : "PLAY_PCM_INFO");
    fprintf(stderr, "%s_PCM_INFO card=%u device=%u subdevice=%u id=%s name=%s subname=%s\n", is_capture ? "CAP" : "PLAY", info.card, info.device, info.subdevice, info.id, info.name, info.subname);
    struct snd_pcm_hw_params hw; init_hw_params(&hw);
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
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, &hw) < 0) die_errno(is_capture ? "CAP_HW_REFINE" : "PLAY_HW_REFINE");
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hw) < 0) die_errno(is_capture ? "CAP_HW_PARAMS" : "PLAY_HW_PARAMS");
    struct snd_pcm_sw_params sw; memset(&sw, 0, sizeof(sw));
    sw.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
    sw.period_step = 1;
    sw.avail_min = period_frames;
    sw.xfer_align = period_frames;
    sw.start_threshold = is_capture ? 1 : period_frames;
    sw.stop_threshold = period_frames * periods;
    sw.boundary = period_frames * periods;
    while (sw.boundary < 0x40000000UL && sw.boundary > 0) sw.boundary *= 2;
    if (ioctl(fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sw) < 0) die_errno(is_capture ? "CAP_SW_PARAMS" : "PLAY_SW_PARAMS");
    if (ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0) die_errno(is_capture ? "CAP_PREPARE" : "PLAY_PREPARE");
    return 0;
}
static int write_frames(int pcm, int16_t *buf, int frames) {
    struct snd_xferi x; memset(&x, 0, sizeof(x)); x.buf = buf; x.frames = frames;
    if (ioctl(pcm, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x) < 0) {
        int e = errno; fprintf(stderr, "WRITE_ERR errno=%d (%s) frames=%d\n", e, strerror(e), frames);
        if (e == EPIPE || e == ESTRPIPE) ioctl(pcm, SNDRV_PCM_IOCTL_PREPARE);
        return -e;
    }
    return x.result > 0 ? (int)x.result : 0;
}
static int read_frames(int pcm, int16_t *buf, int frames) {
    struct snd_xferi x; memset(&x, 0, sizeof(x)); x.buf = buf; x.frames = frames;
    if (ioctl(pcm, SNDRV_PCM_IOCTL_READI_FRAMES, &x) < 0) {
        int e = errno; if (e != EAGAIN) fprintf(stderr, "READ_ERR errno=%d (%s) frames=%d\n", e, strerror(e), frames);
        if (e == EPIPE || e == ESTRPIPE) ioctl(pcm, SNDRV_PCM_IOCTL_PREPARE);
        return -e;
    }
    return x.result > 0 ? (int)x.result : 0;
}

#define RING_FRAMES 32768
static int16_t ring[RING_FRAMES];
static unsigned rpos = 0, wpos = 0, rfill = 0;
static void ring_reset(void) { rpos = wpos = rfill = 0; }
static unsigned ring_push_samples(const int16_t *s, unsigned n) { unsigned pushed = 0; while (pushed < n && rfill < RING_FRAMES) { ring[wpos] = s[pushed++]; wpos = (wpos + 1) % RING_FRAMES; rfill++; } return pushed; }
static unsigned ring_pop_samples(int16_t *d, unsigned n) { unsigned popped = 0; while (popped < n && rfill > 0) { d[popped++] = ring[rpos]; rpos = (rpos + 1) % RING_FRAMES; rfill--; } return popped; }
static void drain_fifo(int in, long *dropped) { int16_t inbuf[2048]; for (;;) { ssize_t r = read(in, inbuf, sizeof(inbuf)); if (r > 0) { unsigned samples = (unsigned)(r / 2); unsigned pushed = ring_push_samples(inbuf, samples); if (pushed < samples && dropped) *dropped += (long)(samples - pushed); continue; } if (r == 0) break; if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break; loge("read fifo"); break; } }

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
static void monitor_fifo_reset(void) {
    if (mon_fd >= 0) { close(mon_fd); mon_fd = -1; }
}
static void monitor_refresh_flag(void) {
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
    if (mkfifo(CAPTURE_MONITOR_FIFO, 0666) < 0 && errno != EEXIST) { loge("monitor mkfifo"); return; }
    chmod(CAPTURE_MONITOR_FIFO, 0666);
    mon_fd = open(CAPTURE_MONITOR_FIFO, O_WRONLY | O_NONBLOCK);
    if (mon_fd >= 0) fprintf(stderr, "CAPTURE_MONITOR_FIFO_OPENED fd=%d\n", mon_fd);
}
static void monitor_write_samples(const int16_t *buf, int frames) {
    monitor_refresh_flag();
    if (!mon_enabled) return;
    monitor_fifo_open_if_needed();
    if (mon_fd < 0) return;
    ssize_t w = write(mon_fd, buf, (size_t)frames * sizeof(int16_t));
    if (w < 0) {
        if (errno == EPIPE || errno == ENXIO || errno == EBADF) monitor_fifo_reset();
        else if (errno != EAGAIN && errno != EWOULDBLOCK) loge("monitor write");
    }
}

typedef struct CaptureState { int active; int pcm; int wav; char path[256]; char name[96]; time_t start_time; int max_seconds; uint32_t data_bytes; long frames; } CaptureState;
static CaptureState cap = {0, -1, -1, "", "", 0, 0, 0, 0};
static time_t cmd_mtime = 0;
static int mon_cap_pcm = -1;
static int force_playback_reopen = 0;
static void set_status(const char *s) { write_text_file(CAPTURE_STATUS, s ? s : ""); }
static void set_level_percent(int pct) { char b[32]; if (pct < 0) pct = 0; if (pct > 100) pct = 100; snprintf(b, sizeof(b), "%d\n", pct); write_text_file(CAPTURE_LEVEL, b); }
static void set_lr_level_percent(int l, int r) { char b[32]; if (l < 0) l = 0; if (l > 100) l = 100; if (r < 0) r = 0; if (r > 100) r = 100; snprintf(b, sizeof(b), "%d\n", l); write_text_file(CAPTURE_LEVEL_L, b); snprintf(b, sizeof(b), "%d\n", r); write_text_file(CAPTURE_LEVEL_R, b); set_level_percent((l > r) ? l : r); }
static void set_elapsed_seconds(int sec) { char b[32]; if (sec < 0) sec = 0; if (sec > 120) sec = 120; snprintf(b, sizeof(b), "%d\n", sec); write_text_file(CAPTURE_ELAPSED, b); }
static int peak_percent_s16(const int16_t *buf, int frames) { int i; int peak = 0; for (i = 0; i < frames; ++i) { int v = buf[i]; if (v < 0) v = -v; if (v > peak) peak = v; } return (peak * 100 + 16383) / 32767; }
static void peak_percent_lr_s16(const int16_t *buf, int frames, int *pl, int *pr) { int i; int peak_l = 0, peak_r = 0; for (i = 0; i < frames; ++i) { int v = buf[i]; if (v < 0) v = -v; if (v > peak_l) peak_l = v; if (v > peak_r) peak_r = v; } *pl = (peak_l * 100 + 16383) / 32767; *pr = (peak_r * 100 + 16383) / 32767; }
static void stop_capture(const char *why) {
    if (!cap.active) return;
    write_wav_header(cap.wav, cap.data_bytes, 48000, 1);
    close(cap.wav); cap.wav = -1;
    if (cap.pcm >= 0) { close(cap.pcm); cap.pcm = -1; }
    char st[160]; snprintf(st, sizeof(st), "USB capture saved %s frames=%ld bytes=%u why=%s", cap.name, cap.frames, cap.data_bytes, why ? why : "stop");
    set_status(st); write_text_file(CAPTURE_LAST_NAME, cap.name); write_text_file(CAPTURE_LAST_PATH, cap.path);
    fprintf(stderr, "CAPTURE_STOP path=%s frames=%ld bytes=%u why=%s\n", cap.path, cap.frames, cap.data_bytes, why ? why : "stop");
    cap.active = 0; set_level_percent(0); set_lr_level_percent(0, 0); set_elapsed_seconds(0);
}
static void start_capture(const char *pcmc, const char *path, const char *name, int seconds) {
    if (cap.active) stop_capture("restart");
    if (!path || !path[0]) { set_status("USB capture error: missing path"); return; }
    if (seconds <= 0) seconds = 15; if (seconds > 120) seconds = 120;
    memset(&cap, 0, sizeof(cap)); cap.pcm = -1; cap.wav = -1;
    snprintf(cap.name, sizeof(cap.name), "%s", (name && name[0]) ? name : "USBREC.wav");
    cap.max_seconds = seconds; cap.start_time = time(NULL);
    cap.wav = open_capture_wav_with_fallback(path, cap.name, cap.path, sizeof(cap.path));
    if (cap.wav < 0) { set_status("USB capture error: WAV open failed"); loge("capture wav open"); return; }
    write_wav_header(cap.wav, 0, 48000, 1);
    cap.pcm = open(pcmc, O_RDONLY | O_NONBLOCK);
    if (cap.pcm < 0) { set_status("USB capture error: pcmC0D0c open failed"); loge("capture pcm open"); close(cap.wav); cap.wav = -1; return; }
    pcm_prepare_common(cap.pcm, 1, 48000, 1, 480, 4);
    cap.active = 1; set_elapsed_seconds(0); set_lr_level_percent(0, 0);
    char st[160]; snprintf(st, sizeof(st), "USB capture recording %ds %s", seconds, cap.name); set_status(st);
    fprintf(stderr, "CAPTURE_START pcm=%s path=%s seconds=%d\n", pcmc, cap.path, seconds);
}

static void discard_capture(void) {
    char path[256] = "";
    if (cap.active) { snprintf(path, sizeof(path), "%s", cap.path); stop_capture("discard"); }
    if (!path[0]) {
        int fd = open(CAPTURE_LAST_PATH, O_RDONLY);
        if (fd >= 0) {
            ssize_t n = read(fd, path, sizeof(path)-1);
            close(fd);
            if (n > 0) { path[n] = 0; char *nl = strchr(path, '\n'); if (nl) *nl = 0; }
        }
    }
    if (path[0]) { unlink(path); fprintf(stderr, "CAPTURE_DISCARD path=%s\n", path); }
    unlink(CAPTURE_LAST_NAME);
    unlink(CAPTURE_LAST_PATH);
    set_level_percent(0); set_lr_level_percent(0, 0); set_elapsed_seconds(0);
    set_status("USB capture discarded");
}

static int parse_value(const char *cmd, const char *key, char *out, int len) {
    const char *p = strstr(cmd, key); if (!p) return 0; p += strlen(key);
    const char *e = strchr(p, '\n'); int n = e ? (int)(e - p) : (int)strlen(p); if (n >= len) n = len - 1; memcpy(out, p, n); out[n] = 0; return 1;
}
static void poll_capture_command(const char *pcmc) {
    struct stat st; if (stat(CAPTURE_CMD, &st) != 0) return; if (st.st_mtime == cmd_mtime) return; cmd_mtime = st.st_mtime;
    char cmd[512]; int fd = open(CAPTURE_CMD, O_RDONLY); if (fd < 0) return; ssize_t n = read(fd, cmd, sizeof(cmd)-1); close(fd); if (n <= 0) return; cmd[n] = 0;
    if (strstr(cmd, "DISCARD")) { discard_capture(); return; }
    if (strstr(cmd, "STOP")) { stop_capture("cmd-stop"); return; }
    if (strstr(cmd, "MONITOR")) {
        monitor_refresh_flag();
        if (strstr(cmd, "MONITOR=0") || strstr(cmd, "RECOVER_OUT")) {
            /* AU11M_NO_REOPEN_ON_RECORD_EXIT
               Keep both ALSA endpoints open and stable.  The old RECOVER_OUT path
               closed pcmC0D0p and the passive capture drain, which made Windows
               keep the device visible but broke audible routing in common setups. */
            force_playback_reopen = 0;
            monitor_fifo_reset();
            fprintf(stderr, "AU11M_MONITOR_OFF_KEEP_ENDPOINTS_OPEN cmd=%s\n", cmd);
        }
        fprintf(stderr, "CAPTURE_MONITOR_REQUEST cmd=%s\n", cmd);
    }
    if (strstr(cmd, "START")) { char path[256] = ""; char name[96] = ""; char secbuf[32] = ""; int seconds = 15; parse_value(cmd, "PATH=", path, sizeof(path)); parse_value(cmd, "NAME=", name, sizeof(name)); if (parse_value(cmd, "SECONDS=", secbuf, sizeof(secbuf))) seconds = atoi(secbuf); start_capture(pcmc, path, name, seconds); }
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
        pcm_prepare_common(mon_cap_pcm, 1, 48000, 1, 480, 4);
        fprintf(stderr, "CAPTURE_PASSIVE_MONITOR_OPEN pcm=%s\n", pcmc);
    }
    int16_t buf[480]; int r = read_frames(mon_cap_pcm, buf, 480);
    if (r > 0) {
        int ll = 0, rr = 0; peak_percent_lr_s16(buf, r, &ll, &rr); int level = (ll > rr) ? ll : rr;
        set_lr_level_percent(ll, rr);
        monitor_write_samples(buf, r);
        static long mon_frames = 0; mon_frames += r;
        if ((mon_frames % 2400) == 0) fprintf(stderr, "CAPTURE_PASSIVE_LEVEL percent=%d frames=%ld\n", level, mon_frames);
    } else if (r < 0 && (r == -EIO || r == -ENODEV || r == -ESHUTDOWN)) { close(mon_cap_pcm); mon_cap_pcm = -1; }
}

static void capture_tick(void) {
    if (!cap.active) return;
    int elapsed_now = (int)(time(NULL) - cap.start_time);
    if (elapsed_now < 0) elapsed_now = 0;
    set_elapsed_seconds(elapsed_now);
    if (cap.max_seconds > 0 && elapsed_now >= cap.max_seconds) { stop_capture("duration"); return; }
    int16_t buf[480]; int r = read_frames(cap.pcm, buf, 480);
    if (r > 0) {
        int ll = 0, rr = 0; peak_percent_lr_s16(buf, r, &ll, &rr); int level = (ll > rr) ? ll : rr;
        set_lr_level_percent(ll, rr);
        ssize_t w = write(cap.wav, buf, (size_t)r * 2);
        if (w > 0) { cap.data_bytes += (uint32_t)w; cap.frames += (long)r; }
        monitor_write_samples(buf, r);
        if ((cap.frames % 2400) == 0 && cap.frames > 0) {
            char st[192];
            snprintf(st, sizeof(st), "USB capture recording %s frames=%ld level=%d%%", cap.name, cap.frames, level);
            set_status(st);
            fprintf(stderr, "CAPTURE_LEVEL percent=%d frames=%ld bytes=%u\n", level, cap.frames, cap.data_bytes);
        }
    }
}

int main(int argc, char **argv) {
    const char *fifo = argc > 1 ? argv[1] : "/tmp/r36sx_uac2_bridge_fifo";
    const char *pcmp = argc > 2 ? argv[2] : "/dev/snd/pcmC0D0p";
    const char *pcmc = argc > 3 ? argv[3] : "/dev/snd/pcmC0D0c";
    const int lowlat = path_exists(LOWLAT_SENTINEL);
    const int period_frames = lowlat ? 240 : 480;
    const int periods = 4;
    mkdir(RUNTIME_DIR, 0777);
    fprintf(stderr, "AU11M_USB_AUDIO_IO_START AU11M_CAPTURE_WRITES_STAGING_AND_COUNTDOWN fifo=%s pcmp=%s pcmc=%s period_frames=%d periods=%d lowlat=%d runtime=%s\n", fifo, pcmp, pcmc, period_frames, periods, lowlat, RUNTIME_DIR);
    set_status("USB capture idle"); set_level_percent(0); set_lr_level_percent(0,0); set_elapsed_seconds(0); write_text_file(CAPTURE_MONITOR, "0\n"); mark_inactive();
    if (mkfifo(fifo, 0666) < 0 && errno != EEXIST) die_errno("mkfifo"); chmod(fifo, 0666);
    int in = open(fifo, O_RDONLY | O_NONBLOCK); if (in < 0) die_errno("open fifo read nonblock");
    int keep = open(fifo, O_WRONLY | O_NONBLOCK); if (keep < 0) loge("open fifo keepalive optional");
    int pcm = -1; int16_t out[480]; long total = 0, loops = 0, active = 0, silent = 0, xruns = 0, dropped = 0, reconnects = 0; int good_write_streak = 0; int last_conf = -1;
    for (;;) {
        int conf = usb_configured();
        if (conf != last_conf) { fprintf(stderr, "USB_STATE_CHANGE configured=%d ring_fill=%u\n", conf, rfill); last_conf = conf; }
        drain_fifo(in, &dropped); monitor_refresh_flag(); poll_capture_command(pcmc); capture_tick(); passive_monitor_tick(pcmc, conf);
        /* AU11M_WINDOWS_MONITORING_FIX
           Do NOT close/reopen pcmC0D0p while USB-C RECORD monitor is active.
           AU11G generated thousands of reconnects here. Windows then kept the
           R36SX input endpoint alive but did not present stable audible monitor
           behaviour after leaving RECORD.

           Correct sampler behaviour: while recording from PC -> console, keep the
           console -> PC stream clocked, but send digital silence and discard any
           LGPT FIFO audio. This blocks feedback without destabilizing the UAC2
           playback/capture pair. When monitor turns off, normal FIFO audio resumes
           on the already-open endpoint. */
        if (mon_enabled) {
            ring_reset();
            good_write_streak = 0;
            mark_inactive();
        }
        if (force_playback_reopen) {
            force_playback_reopen = 0;
            if (pcm >= 0) { close(pcm); pcm = -1; fprintf(stderr, "AU11M_PCM_PLAY_FORCE_REOPEN_AFTER_USB_REC_EXIT\n"); }
            ring_reset();
            good_write_streak = 0;
            mark_inactive();
            sleep_ms(120);
        }
        if (!conf) { if (pcm >= 0) { close(pcm); pcm = -1; fprintf(stderr, "PCM_PLAY_CLOSED_USB_DISCONNECTED\n"); } good_write_streak = 0; mark_inactive(); ring_reset(); sleep_ms(80); continue; }
        if (pcm < 0) { pcm = open(pcmp, O_WRONLY); if (pcm < 0) { good_write_streak = 0; mark_inactive(); loge("open playback pcm retry"); sleep_ms(250); continue; } pcm_prepare_common(pcm, 0, 48000, 1, (unsigned)period_frames, (unsigned)periods); good_write_streak = 0; reconnects++; fprintf(stderr, "PCM_PLAY_OPENED reconnects=%ld period_frames=%d\n", reconnects, period_frames); }
        unsigned got = ring_pop_samples(out, (unsigned)period_frames); if (got < (unsigned)period_frames) { memset(out + got, 0, ((unsigned)period_frames - got) * sizeof(int16_t)); silent++; } else active++;
        int wr = write_frames(pcm, out, period_frames); if (wr < 0) { xruns++; good_write_streak = 0; mark_inactive(); if (wr == -EPIPE || wr == -EIO || wr == -ENODEV || wr == -ESHUTDOWN) { close(pcm); pcm = -1; ring_reset(); sleep_ms(300); continue; } } else { if (good_write_streak < 1000) good_write_streak++; if (good_write_streak >= 8) mark_active(); }
        total += period_frames; loops++; if (got == 0) sleep_ms(2);
        if ((loops % 100) == 0) fprintf(stderr, "BRIDGE_PROGRESS frames=%ld seconds=%.2f active=%ld silent=%ld xruns=%ld dropped=%ld ring_fill=%u reconnects=%ld configured=%d good_streak=%d cap_active=%d cap_frames=%ld\n", total, (double)total/48000.0, active, silent, xruns, dropped, rfill, reconnects, conf, good_write_streak, cap.active, cap.frames);
    }
    return 0;
}
