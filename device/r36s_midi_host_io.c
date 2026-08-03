/*
 * r36s_midi_host_io.c
 * R36SX LGPT unified driver host-side USB-MIDI backend.
 *
 * The R36SX is USB host; the piano/controller is a class-compliant USB MIDI
 * device. snd-usbmidi exposes it as an ALSA rawmidi node (/dev/snd/midiC{N}D0).
 *
 * IN path (piano -> LGPT):
 *   rawmidi -> /tmp/r36sx_midi_pcm_fifo (core MIDI mode).
 *
 * OUT path (LGPT -> piano):
 *   /tmp/r36sx_midi_pcm_fifo -> rawmidi (echoed back to the device).
 *
 * Readiness is derived from the midi_rawmidi marker written by the SD setup
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
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static const char *ACTIVE_MARKER = "/tmp/r36sx_uac2_usb_active";
static const char *RUNTIME_DIR = "/tmp/r36sx_lgpt_usb";
static const char *MIDI_RAWMIDI = "/tmp/r36sx_lgpt_usb/midi_rawmidi";
static const char *DAEMON_VERSION = "/tmp/r36sx_lgpt_usb/daemon_version";
static const char *CAPTURE_ABI = "/tmp/r36sx_lgpt_usb/capture_abi";
static const char *DAEMON_PID = "/tmp/r36sx_lgpt_usb/daemon_pid";
static const char *MIDI_FIFO = "/tmp/r36sx_midi_pcm_fifo";

static void die_errno(const char *msg) {
    fprintf(stderr, "%s errno=%d (%s)\n", msg, errno, strerror(errno));
    exit(2);
}

static void write_text_file(const char *path, const char *text) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) { if (text) write(fd, text, strlen(text)); close(fd); }
}

static void mark_active(void) {
    char b[64];
    snprintf(b, sizeof(b), "%ld\n", (long)time(NULL));
    write_text_file(ACTIVE_MARKER, b);
}

static void mark_inactive(void) { unlink(ACTIVE_MARKER); }

static int rawmidi_present(void) {
    return access(MIDI_RAWMIDI, F_OK) == 0;
}

int main(int argc, char **argv) {
    struct sigaction pipe_action;
    memset(&pipe_action, 0, sizeof(pipe_action));
    pipe_action.sa_handler = SIG_IGN;
    sigemptyset(&pipe_action.sa_mask);
    sigaction(SIGPIPE, &pipe_action, 0);

    const char *rawmidi = argc > 1 ? argv[1] : "/dev/snd/midiC1D0";
    const char *fifo = argc > 2 ? argv[2] : MIDI_FIFO;

    setvbuf(stderr, 0, _IOLBF, 4096);
    mkdir(RUNTIME_DIR, 0777);
    fprintf(stderr,
            "R36SX_MIDI_HOST_IO_START USB_MIDI_ABI1 rawmidi=%s fifo=%s\n",
            rawmidi, fifo);
    write_text_file(DAEMON_VERSION, "R36SX_MIDI_DAEMON_ABI=1\n");
    write_text_file(CAPTURE_ABI, "R36SX_MIDI_CAPTURE_ABI=1\n");
    {
        char b[32];
        snprintf(b, sizeof(b), "%ld\n", (long)getpid());
        write_text_file(DAEMON_PID, b);
    }

    if (mkfifo(fifo, 0666) < 0 && errno != EEXIST)
        die_errno("mkfifo");

    int in_fd = open(fifo, O_RDONLY | O_NONBLOCK);
    if (in_fd < 0) die_errno("open fifo read");
    int keep_fd = open(fifo, O_WRONLY | O_NONBLOCK);

    int dev = -1;
    int connected = 0;
    unsigned long long connected_since_ms = 0;
    long in_bytes = 0, out_bytes = 0;
    uint8_t inbuf[4096], outbuf[4096];
    for (;;) {
        if (rawmidi_present()) {
            if (dev < 0) {
                dev = open(rawmidi, O_RDWR | O_NONBLOCK);
                if (dev >= 0) {
                    connected = 1;
                    connected_since_ms = 0;
                    mark_active();
                    fprintf(stderr, "MIDI_DEVICE_CONNECTED rawmidi=%s fd=%d\n", rawmidi, dev);
                } else {
                    fprintf(stderr, "MIDI_DEVICE_OPEN_FAILED rawmidi=%s errno=%d (%s)\n",
                            rawmidi, errno, strerror(errno));
                }
            }
        } else {
            if (dev >= 0) {
                close(dev);
                dev = -1;
                connected = 0;
                mark_inactive();
                fprintf(stderr, "MIDI_DEVICE_DISCONNECTED rawmidi=%s\n", rawmidi);
            }
        }

        if (connected && dev >= 0) {
            for (;;) {
                ssize_t n = read(dev, inbuf, sizeof(inbuf));
                if (n > 0) {
                    in_bytes += (long)n;
                    if (keep_fd >= 0)
                        (void)write(keep_fd, inbuf, (size_t)n);
                    if (in_bytes && (in_bytes % 4096) == 0)
                        fprintf(stderr, "MIDI_IN_PROGRESS bytes=%ld\n", in_bytes);
                    continue;
                }
                if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
                if (n == 0) break;
                if (errno != EINTR) {
                    fprintf(stderr, "MIDI_READ_ERROR errno=%d (%s)\n", errno, strerror(errno));
                    break;
                }
            }
        }

        for (;;) {
            ssize_t n = read(in_fd, outbuf, sizeof(outbuf));
            if (n > 0) {
                out_bytes += (long)n;
                if (dev >= 0)
                    (void)write(dev, outbuf, (size_t)n);
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            if (n == 0) break;
            if (errno != EINTR) break;
        }

        if (connected && (connected_since_ms == 0)) {
            fprintf(stderr, "MIDI_DAEMON_READY rawmidi=%s in_bytes=%ld out_bytes=%ld\n",
                    rawmidi, in_bytes, out_bytes);
            connected_since_ms = 1;
        }

        usleep(4000);
    }
    return 0;
}
