#include "Adapters/TREEFROG/Audio/TreeFrogWindowsSpscTransport.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

namespace {

enum {
    kRingFrames = 16384,
    kRingMask = kRingFrames - 1,
    kWorkerChunkFrames = 2048,
    kMonitorRingFrames = 32768,
    kMonitorRingMask = kMonitorRingFrames - 1,
    kMonitorStageFrames = 8192,
    kMonitorOutputChunkFrames = 2048
};

typedef char RingSizeMustBePowerOfTwo[
    ((kRingFrames & (kRingFrames - 1)) == 0) ? 1 : -1];
typedef char MonitorRingSizeMustBePowerOfTwo[
    ((kMonitorRingFrames & (kMonitorRingFrames - 1)) == 0) ? 1 : -1];

/*
 * All cross-thread scalar state is 32-bit. GCC __atomic builtins are used so
 * the MIPS32 build never requires non-lock-free 64-bit atomics/libatomic.
 */
static unsigned load_acquire(const volatile unsigned *value) {
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}
static unsigned load_relaxed(const volatile unsigned *value) {
    return __atomic_load_n(value, __ATOMIC_RELAXED);
}
static void store_release(volatile unsigned *value, unsigned next) {
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
}
static void store_relaxed(volatile unsigned *value, unsigned next) {
    __atomic_store_n(value, next, __ATOMIC_RELAXED);
}
static unsigned add_relaxed(volatile unsigned *value, unsigned delta) {
    return __atomic_fetch_add(value, delta, __ATOMIC_RELAXED);
}

/* Audio callback producer -> worker consumer, native 44.1 kHz stereo. */
static int16_t g_ring[kRingFrames * 2];
static volatile unsigned g_head = 0U;
static volatile unsigned g_tail = 0U;
static volatile unsigned g_running = 0U;
static volatile unsigned g_started = 0U;
static pthread_t g_thread;

/* Callback-owned writes, worker-owned snapshots. */
static volatile unsigned g_submitted_frames = 0U;
static volatile unsigned g_callback_drop_frames = 0U;
static volatile unsigned g_callback_drop_blocks = 0U;
static volatile unsigned g_max_fill_frames = 0U;

/* Worker-owned counters. */
static unsigned g_fifo_written_frames = 0U;
static unsigned g_fifo_open_errors = 0U;
static unsigned g_fifo_eagain = 0U;
static unsigned g_fifo_short_writes = 0U;
static unsigned g_worker_drop_frames = 0U;
static unsigned g_worker_loops = 0U;

static volatile unsigned g_mixer_percent = 100U;
static volatile unsigned g_master_percent = 100U;
static volatile unsigned g_should_mute_local = 0U;
static volatile unsigned g_monitor_enabled = 0U;
static volatile unsigned g_monitor_flush_requested = 1U;

/* Worker producer -> audio callback consumer monitor ring, 44.1 kHz stereo. */
static int16_t g_monitor_ring[kMonitorRingFrames * 2];
static volatile unsigned g_monitor_head = 0U;
static volatile unsigned g_monitor_tail = 0U;
static unsigned g_monitor_input_frames = 0U;
static unsigned g_monitor_output_frames = 0U;
static unsigned g_monitor_drop_frames = 0U;
static volatile unsigned g_monitor_underflow_frames = 0U;
static int16_t g_monitor_stage[kMonitorStageFrames];
static unsigned g_monitor_stage_fill = 0U;
static unsigned long long g_monitor_phase_q32 = 0ULL;
static unsigned g_monitor_carry_bytes = 0U;
static unsigned char g_monitor_carry[2];

static const char *kFifo = "/tmp/r36sx_uac2_bridge_fifo";
static const char *kRuntimeDir = "/tmp/r36sx_lgpt_usb";
static const char *kMetrics =
    "/tmp/r36sx_lgpt_usb/h38_windows_core_transport";
static const char *kGain =
    "/tmp/r36sx_lgpt_usb/h38_windows_gain";
static const char *kProtocol =
    "/tmp/r36sx_lgpt_usb/windows_producer_protocol";
static const char *kActiveMarker = "/tmp/r36sx_uac2_usb_active";
static const char *kNoMute = "/mnt/sdcard/lgpt/otg/disable_mute_local";
static const char *kMonitorFifo = "/tmp/r36sx_usb_capture_monitor_fifo";

static unsigned long long monotonic_milliseconds() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0ULL;
    return ((unsigned long long)ts.tv_sec * 1000ULL) +
        ((unsigned long long)ts.tv_nsec / 1000000ULL);
}

static void sleep_milliseconds(unsigned ms) {
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000U);
    req.tv_nsec = (long)(ms % 1000U) * 1000000L;
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {}
}

static int write_all(int fd, const char *data, size_t length) {
    size_t offset = 0U;
    while (offset < length) {
        const ssize_t count = write(fd, data + offset, length - offset);
        if (count > 0) {
            offset += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        return 0;
    }
    return 1;
}

static int write_text_atomic(const char *path, const char *text) {
    char tmp[256];
    int fd;
    size_t len;
    int close_ok;
    int count;
    if (!path || !text) return 0;
    if (mkdir(kRuntimeDir, 0777) != 0 && errno != EEXIST) return 0;
    count = snprintf(tmp, sizeof(tmp), "%s.h38tmp.%ld", path, (long)getpid());
    if (count <= 0 || (size_t)count >= sizeof(tmp)) return 0;
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return 0;
    len = strlen(text);
    if (!write_all(fd, text, len)) {
        (void)close(fd);
        (void)unlink(tmp);
        return 0;
    }
    close_ok = close(fd) == 0;
    if (!close_ok || rename(tmp, path) != 0) {
        (void)unlink(tmp);
        return 0;
    }
    return 1;
}

static unsigned ring_fill_frames() {
    const unsigned head = load_acquire(&g_head);
    const unsigned tail = load_acquire(&g_tail);
    return (head - tail) & kRingMask;
}

static void discard_ring_from_worker() {
    const unsigned head = load_acquire(&g_head);
    const unsigned tail = load_relaxed(&g_tail);
    const unsigned fill = (head - tail) & kRingMask;
    g_worker_drop_frames += fill;
    store_release(&g_tail, head);
}

static int clamp_s16(int value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return value;
}

static int marker_fresh(const char *path, time_t now, time_t max_age) {
    struct stat st;
    if (!path || stat(path, &st) != 0) return 0;
    if (now < st.st_mtime) return 1;
    return (now - st.st_mtime) <= max_age;
}

/* Worker owns the resampler stage. Ring indices are not reset by the worker. */
static void monitor_reset_worker() {
    g_monitor_stage_fill = 0U;
    g_monitor_phase_q32 = 0ULL;
    g_monitor_carry_bytes = 0U;
}

static void monitor_push_worker(const int16_t *stereo, unsigned frames) {
    unsigned head;
    unsigned tail;
    unsigned used;
    unsigned free_frames;
    unsigned first;
    if (!stereo || frames == 0U) return;
    head = load_relaxed(&g_monitor_head);
    tail = load_acquire(&g_monitor_tail);
    used = (head - tail) & kMonitorRingMask;
    free_frames = (unsigned)(kMonitorRingFrames - 1) - used;
    if (frames > free_frames) {
        g_monitor_drop_frames += frames;
        return;
    }
    first = frames < (unsigned)(kMonitorRingFrames - head)
        ? frames : (unsigned)(kMonitorRingFrames - head);
    memcpy(g_monitor_ring + head * 2U, stereo,
        (size_t)first * 2U * sizeof(int16_t));
    if (frames > first) {
        memcpy(g_monitor_ring, stereo + first * 2U,
            (size_t)(frames - first) * 2U * sizeof(int16_t));
    }
    store_release(&g_monitor_head, (head + frames) & kMonitorRingMask);
    g_monitor_output_frames += frames;
}

static void monitor_stage_append_worker(const int16_t *mono, unsigned frames) {
    if (!mono || frames == 0U) return;
    if (frames >= (unsigned)kMonitorStageFrames) {
        mono += frames - (unsigned)kMonitorStageFrames + 1U;
        frames = (unsigned)kMonitorStageFrames - 1U;
        g_monitor_stage_fill = 0U;
        g_monitor_phase_q32 = 0ULL;
    }
    if (g_monitor_stage_fill + frames > (unsigned)kMonitorStageFrames) {
        const unsigned keep = g_monitor_stage_fill > 1U ? 1U : g_monitor_stage_fill;
        if (keep) g_monitor_stage[0] = g_monitor_stage[g_monitor_stage_fill - 1U];
        g_monitor_stage_fill = keep;
        g_monitor_phase_q32 = 0ULL;
    }
    memcpy(g_monitor_stage + g_monitor_stage_fill, mono,
        (size_t)frames * sizeof(int16_t));
    g_monitor_stage_fill += frames;
    g_monitor_input_frames += frames;
}

static void monitor_render_worker() {
    const unsigned long long step = 1ULL << 32;
    const unsigned long long one_q32 = 1ULL << 32;
    int16_t out[kMonitorOutputChunkFrames * 2];
    unsigned produced = 0U;
    while (produced < (unsigned)kMonitorOutputChunkFrames) {
        const unsigned idx = (unsigned)(g_monitor_phase_q32 >> 32);
        const unsigned frac = (unsigned)g_monitor_phase_q32;
        const unsigned long long inverse = one_q32 - (unsigned long long)frac;
        long long mixed;
        int value;
        if (idx + 1U >= g_monitor_stage_fill) break;
        mixed = (long long)g_monitor_stage[idx] * (long long)inverse +
            (long long)g_monitor_stage[idx + 1U] * (long long)frac;
        value = (int)(mixed >> 32);
        out[produced * 2U] = (int16_t)value;
        out[produced * 2U + 1U] = (int16_t)value;
        ++produced;
        g_monitor_phase_q32 += step;
    }
    if (produced) monitor_push_worker(out, produced);
    {
        const unsigned consumed = (unsigned)(g_monitor_phase_q32 >> 32);
        if (consumed > 0U) {
            if (consumed >= g_monitor_stage_fill) {
                g_monitor_stage_fill = 0U;
                g_monitor_phase_q32 = 0ULL;
            } else {
                memmove(g_monitor_stage, g_monitor_stage + consumed,
                    (size_t)(g_monitor_stage_fill - consumed) * sizeof(int16_t));
                g_monitor_stage_fill -= consumed;
                g_monitor_phase_q32 -= (unsigned long long)consumed << 32;
            }
        }
    }
}

static void monitor_read_worker(int *monitor_fd) {
    unsigned char raw[4096];
    int16_t mono[2048];
    unsigned total;
    unsigned samples;
    ssize_t n;
    const unsigned enabled = load_acquire(&g_monitor_enabled);
    const unsigned flush_pending = load_acquire(&g_monitor_flush_requested);
    if (!enabled || flush_pending) {
        if (*monitor_fd >= 0) {
            (void)close(*monitor_fd);
            *monitor_fd = -1;
        }
        monitor_reset_worker();
        return;
    }
    if (*monitor_fd < 0) {
        *monitor_fd = open(kMonitorFifo, O_RDWR | O_NONBLOCK);
        if (*monitor_fd < 0) return;
    }
    /*
     * MONITOR_READ_BOUNDS_H37_1R2
     * g_monitor_carry_bytes is worker-owned and logically limited to 0 or 1.
     * Materialize that invariant in a bounded local before pointer arithmetic;
     * GCC 13/glibc fortify cannot safely infer the range of the global value.
     */
    const size_t carry_bytes =
        (g_monitor_carry_bytes == 1U) ? (size_t)1U : (size_t)0U;
    if (g_monitor_carry_bytes > 1U) g_monitor_carry_bytes = 0U;
    if (carry_bytes != 0U) raw[0] = g_monitor_carry[0];
    n = read(*monitor_fd, raw + carry_bytes, sizeof(raw) - carry_bytes);
    if (n <= 0) {
        if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            (void)close(*monitor_fd);
            *monitor_fd = -1;
        }
        monitor_render_worker();
        return;
    }
    total = (unsigned)carry_bytes + (unsigned)n;
    samples = total / 2U;
    memcpy(mono, raw, (size_t)samples * sizeof(int16_t));
    g_monitor_carry_bytes = total & 1U;
    if (g_monitor_carry_bytes) g_monitor_carry[0] = raw[total - 1U];
    monitor_stage_append_worker(mono, samples);
    monitor_render_worker();
}

static void publish_gain_if_changed(unsigned *last_mixer, unsigned *last_master) {
    const unsigned mixer = load_acquire(&g_mixer_percent);
    const unsigned master = load_acquire(&g_master_percent);
    char text[128];
    if (mixer == *last_mixer && master == *last_master) return;
    snprintf(text, sizeof(text),
        "mixer_percent=%u\nproject_master_percent=%u\n",
        mixer, master);
    if (write_text_atomic(kGain, text)) {
        *last_mixer = mixer;
        *last_master = master;
    }
}

static void publish_metrics(int fifo_fd, unsigned pending_bytes) {
    char text[1024];
    const unsigned fill = ring_fill_frames();
    snprintf(text, sizeof(text),
        "version=H38.1R1_WINDOWS_SPSC_AUDIO_ONLY_SAFE_FRONTEND\n"
        "protocol=RAW_48000_STEREO\n"
        "callback_contract=BOUNDED_COPY_ONLY\n"
        "callback_timing=NOT_MEASURED_IN_CALLBACK\n"
        "worker_running=%u\n"
        "fifo_open=%d\n"
        "ring_capacity_frames=%u\n"
        "ring_fill_frames=%u\n"
        "ring_max_fill_frames=%u\n"
        "submitted_frames=%u\n"
        "callback_drop_frames=%u\n"
        "callback_drop_blocks=%u\n"
        "fifo_written_frames=%u\n"
        "fifo_open_errors=%u\n"
        "fifo_eagain=%u\n"
        "fifo_short_writes=%u\n"
        "worker_drop_frames=%u\n"
        "worker_pending_bytes=%u\n"
        "worker_loops=%u\n"
        "should_mute_local=%u\n"
        "monitor_enabled=%u\n"
        "monitor_flush_requested=%u\n"
        "monitor_input_frames=%u\n"
        "monitor_output_frames=%u\n"
        "monitor_drop_frames=%u\n"
        "monitor_underflow_frames=%u\n",
        load_acquire(&g_running),
        fifo_fd >= 0 ? 1 : 0,
        (unsigned)kRingFrames,
        fill,
        load_relaxed(&g_max_fill_frames),
        load_relaxed(&g_submitted_frames),
        load_relaxed(&g_callback_drop_frames),
        load_relaxed(&g_callback_drop_blocks),
        g_fifo_written_frames,
        g_fifo_open_errors,
        g_fifo_eagain,
        g_fifo_short_writes,
        g_worker_drop_frames,
        pending_bytes,
        g_worker_loops,
        load_acquire(&g_should_mute_local),
        load_acquire(&g_monitor_enabled),
        load_acquire(&g_monitor_flush_requested),
        g_monitor_input_frames,
        g_monitor_output_frames,
        g_monitor_drop_frames,
        load_relaxed(&g_monitor_underflow_frames));
    (void)write_text_atomic(kMetrics, text);
}

static void *worker_main(void *) {
    int fifo_fd = -1;
    int16_t pending[kWorkerChunkFrames * 2];
    unsigned pending_bytes = 0U;
    unsigned pending_offset = 0U;
    unsigned long long last_publish_ms = 0ULL;
    unsigned last_mixer = 0xffffffffU;
    unsigned last_master = 0xffffffffU;
    int monitor_fd = -1;
    unsigned long long last_mute_refresh_ms = 0ULL;

    (void)write_text_atomic(kProtocol, "RAW_48000_STEREO\n");
    publish_gain_if_changed(&last_mixer, &last_master);

    while (load_acquire(&g_running)) {
        ++g_worker_loops;

        if (fifo_fd < 0) {
            fifo_fd = open(kFifo, O_WRONLY | O_NONBLOCK);
            if (fifo_fd < 0) {
                ++g_fifo_open_errors;
                sleep_milliseconds(2U);
            }
        }

        if (pending_offset >= pending_bytes) {
            const unsigned tail = load_relaxed(&g_tail);
            const unsigned head = load_acquire(&g_head);
            const unsigned available = (head - tail) & kRingMask;
            const unsigned frames = available > kWorkerChunkFrames
                ? (unsigned)kWorkerChunkFrames : available;
            if (frames > 0U) {
                const unsigned first = frames < (unsigned)(kRingFrames - tail)
                    ? frames : (unsigned)(kRingFrames - tail);
                memcpy(pending, g_ring + tail * 2U,
                    (size_t)first * 2U * sizeof(int16_t));
                if (frames > first) {
                    memcpy(pending + first * 2U, g_ring,
                        (size_t)(frames - first) * 2U * sizeof(int16_t));
                }
                store_release(&g_tail, (tail + frames) & kRingMask);
                pending_bytes = frames * 2U * (unsigned)sizeof(int16_t);
                pending_offset = 0U;
            }
        }

        if (fifo_fd >= 0 && pending_offset < pending_bytes) {
            const unsigned remaining = pending_bytes - pending_offset;
            const ssize_t n = write(
                fifo_fd,
                ((const unsigned char *)pending) + pending_offset,
                remaining);
            if (n > 0) {
                if ((unsigned)n < remaining) ++g_fifo_short_writes;
                pending_offset += (unsigned)n;
                if (pending_offset >= pending_bytes) {
                    g_fifo_written_frames += pending_bytes /
                        (2U * (unsigned)sizeof(int16_t));
                }
            } else if (n < 0 &&
                       (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                ++g_fifo_eagain;
                sleep_milliseconds(1U);
            } else if (n < 0) {
                (void)close(fifo_fd);
                fifo_fd = -1;
                g_worker_drop_frames +=
                    (pending_bytes - pending_offset) /
                    (2U * (unsigned)sizeof(int16_t));
                pending_bytes = pending_offset = 0U;
                discard_ring_from_worker();
                sleep_milliseconds(2U);
            }
        } else if (pending_offset >= pending_bytes) {
            sleep_milliseconds(1U);
        }

        publish_gain_if_changed(&last_mixer, &last_master);
        monitor_read_worker(&monitor_fd);
        {
            const unsigned long long now = monotonic_milliseconds();
            if (last_mute_refresh_ms == 0ULL || now - last_mute_refresh_ms >= 50ULL) {
                const time_t wall_now = time(0);
                const int active = marker_fresh(kActiveMarker, wall_now, 2);
                const int no_mute = access(kNoMute, F_OK) == 0;
                store_release(&g_should_mute_local,
                    (active && !no_mute) ? 1U : 0U);
                last_mute_refresh_ms = now;
            }
            if (last_publish_ms == 0ULL || now - last_publish_ms >= 1000ULL) {
                publish_metrics(fifo_fd, pending_bytes - pending_offset);
                last_publish_ms = now;
            }
        }
    }

    if (fifo_fd >= 0) (void)close(fifo_fd);
    if (monitor_fd >= 0) (void)close(monitor_fd);
    store_release(&g_should_mute_local, 0U);
    publish_metrics(-1, pending_bytes - pending_offset);
    return 0;
}

}  // namespace

extern "C" const char *TreeFrogWindowsSpscTransport_BuildMarker(void) {
    return "H38_1_WINDOWS_SPSC_RAW441_CALLBACK_BOUNDED_COPY_ONLY_ATOMIC32";
}

extern "C" int TreeFrogWindowsSpscTransport_Start(void) {
    int rc;
    if (load_acquire(&g_started)) return 1;
    store_relaxed(&g_head, 0U);
    store_relaxed(&g_tail, 0U);
    store_relaxed(&g_submitted_frames, 0U);
    store_relaxed(&g_callback_drop_frames, 0U);
    store_relaxed(&g_callback_drop_blocks, 0U);
    store_relaxed(&g_max_fill_frames, 0U);
    g_fifo_written_frames = 0U;
    g_fifo_open_errors = 0U;
    g_fifo_eagain = 0U;
    g_fifo_short_writes = 0U;
    g_worker_drop_frames = 0U;
    g_worker_loops = 0U;
    g_monitor_input_frames = 0U;
    g_monitor_output_frames = 0U;
    g_monitor_drop_frames = 0U;
    store_relaxed(&g_monitor_underflow_frames, 0U);
    store_relaxed(&g_monitor_head, 0U);
    store_relaxed(&g_monitor_tail, 0U);
    store_relaxed(&g_monitor_enabled, 0U);
    store_relaxed(&g_monitor_flush_requested, 1U);
    store_relaxed(&g_should_mute_local, 0U);
    monitor_reset_worker();
    store_release(&g_running, 1U);
    rc = pthread_create(&g_thread, 0, worker_main, 0);
    if (rc != 0) {
        store_release(&g_running, 0U);
        return 0;
    }
    store_release(&g_started, 1U);
    return 1;
}

extern "C" void TreeFrogWindowsSpscTransport_Stop(void) {
    if (!load_acquire(&g_started)) return;
    store_release(&g_running, 0U);
    (void)pthread_join(g_thread, 0);
    store_release(&g_started, 0U);
    store_relaxed(&g_head, 0U);
    store_relaxed(&g_tail, 0U);
}

extern "C" void TreeFrogWindowsSpscTransport_Submit(
    const int16_t *stereo44100,
    unsigned frames) {
    unsigned head;
    unsigned tail;
    unsigned used;
    unsigned free_frames;
    unsigned first;
    unsigned new_fill;
    unsigned previous_max;

    if (!load_acquire(&g_running) || !stereo44100 || frames == 0U) return;
    if (frames >= (unsigned)kRingFrames) {
        (void)add_relaxed(&g_callback_drop_frames, frames);
        (void)add_relaxed(&g_callback_drop_blocks, 1U);
        return;
    }

    head = load_relaxed(&g_head);
    tail = load_acquire(&g_tail);
    used = (head - tail) & kRingMask;
    free_frames = (unsigned)(kRingFrames - 1) - used;
    if (frames > free_frames) {
        (void)add_relaxed(&g_callback_drop_frames, frames);
        (void)add_relaxed(&g_callback_drop_blocks, 1U);
        return;
    }

    first = frames < (unsigned)(kRingFrames - head)
        ? frames : (unsigned)(kRingFrames - head);
    memcpy(g_ring + head * 2U, stereo44100,
        (size_t)first * 2U * sizeof(int16_t));
    if (frames > first) {
        memcpy(g_ring, stereo44100 + first * 2U,
            (size_t)(frames - first) * 2U * sizeof(int16_t));
    }
    store_release(&g_head, (head + frames) & kRingMask);
    (void)add_relaxed(&g_submitted_frames, frames);
    new_fill = used + frames;
    previous_max = load_relaxed(&g_max_fill_frames);
    if (new_fill > previous_max) store_relaxed(&g_max_fill_frames, new_fill);
}

extern "C" int TreeFrogWindowsSpscTransport_ShouldMuteLocal(void) {
    return load_acquire(&g_should_mute_local) ? 1 : 0;
}

extern "C" void TreeFrogWindowsSpscTransport_SetMonitorEnabled(int enabled) {
    /* The callback is the sole owner of monitor_tail and performs the flush. */
    store_release(&g_monitor_flush_requested, 1U);
    store_release(&g_monitor_enabled, enabled ? 1U : 0U);
}

extern "C" void TreeFrogWindowsSpscTransport_MixMonitorStereo44100(
    int16_t *stereo44100,
    unsigned frames) {
    unsigned tail;
    unsigned head;
    unsigned available;
    unsigned count;
    unsigned i;
    const unsigned enabled = load_acquire(&g_monitor_enabled);
    const unsigned flush_requested = load_acquire(&g_monitor_flush_requested);

    if (!stereo44100 || frames == 0U) return;
    if (!enabled || flush_requested) {
        head = load_acquire(&g_monitor_head);
        store_release(&g_monitor_tail, head);
        store_release(&g_monitor_flush_requested, 0U);
        return;
    }

    tail = load_relaxed(&g_monitor_tail);
    head = load_acquire(&g_monitor_head);
    available = (head - tail) & kMonitorRingMask;
    count = available < frames ? available : frames;
    for (i = 0U; i < count; ++i) {
        const unsigned pos = (tail + i) & kMonitorRingMask;
        const int left = (int)stereo44100[i * 2U] +
            ((int)g_monitor_ring[pos * 2U] * 3) / 4;
        const int right = (int)stereo44100[i * 2U + 1U] +
            ((int)g_monitor_ring[pos * 2U + 1U] * 3) / 4;
        stereo44100[i * 2U] = (int16_t)clamp_s16(left);
        stereo44100[i * 2U + 1U] = (int16_t)clamp_s16(right);
    }
    store_release(&g_monitor_tail, (tail + count) & kMonitorRingMask);
    if (count < frames)
        (void)add_relaxed(&g_monitor_underflow_frames, frames - count);
}

extern "C" void TreeFrogWindowsSpscTransport_SetGainPercent(
    int mixer_percent,
    int project_master_percent) {
    if (mixer_percent < 0) mixer_percent = 0;
    if (mixer_percent > 100) mixer_percent = 100;
    if (project_master_percent < 0) project_master_percent = 0;
    if (project_master_percent > 100) project_master_percent = 100;
    store_release(&g_mixer_percent, (unsigned)mixer_percent);
    store_release(&g_master_percent, (unsigned)project_master_percent);
}
