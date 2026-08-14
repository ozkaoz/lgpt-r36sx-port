// F4d: registro declarativo de clases de backend de audio (AudioBackend.h).
// Oraculos golden: mapa modo -> clase (daemons reales del port), contrato de
// operaciones y capacidades agregadas por clase (derivadas de los modos).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Application/Audio/AudioBackend.h"

static int g_checks = 0;

static void expect_int(int got, int want, const char *what) {
    ++g_checks;
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: got %d want %d\n", what, got, want);
        std::exit(1);
    }
}

static void expect_str(const char *got, const char *want, const char *what) {
    ++g_checks;
    if (!got || std::strcmp(got, want) != 0) {
        std::fprintf(stderr,
                     "FAIL %s: got '%s' want '%s'\n",
                     what, got ? got : "(null)", want);
        std::exit(1);
    }
}

static void expect_bit(unsigned int caps, unsigned int bit,
                       const char *what) {
    ++g_checks;
    if (!(caps & bit)) {
        std::fprintf(stderr, "FAIL %s: bit 0x%X missing in 0x%X\n",
                     what, bit, caps);
        std::exit(1);
    }
}

static void expect_no_bit(unsigned int caps, unsigned int bit,
                          const char *what) {
    ++g_checks;
    if (caps & bit) {
        std::fprintf(stderr, "FAIL %s: bit 0x%X present in 0x%X\n",
                     what, bit, caps);
        std::exit(1);
    }
}

int main() {
    // Registro: 5 clases con nombres golden.
    expect_int(kAudioBackendClassCount, 5, "backend class count");
    expect_str(kAudioBackendClassNames[0], "LocalAudioBackend", "name 0");
    expect_str(kAudioBackendClassNames[1], "WindowsUac2Backend", "name 1");
    expect_str(kAudioBackendClassNames[2], "AndroidBackend", "name 2");
    expect_str(kAudioBackendClassNames[3], "Sp404Backend", "name 3");
    expect_str(kAudioBackendClassNames[4], "MidiBackend", "name 4");

    // Contrato de operaciones golden (objetivo 6).
    expect_int(kAudioBackendOpCount, 6, "op count");
    for (int i = 0; i < 6; ++i) {
        const char *ops[] = {"open", "start", "caps", "stream",
                             "write", "close"};
        expect_str(kAudioBackendOps[i], ops[i], "op name");
    }

    // Mapa modo -> clase golden (daemons reales del port).
    expect_int(AudioBackendClassForMode(0), kAudioBackendLocal,
               "mode0 -> Local");
    expect_int(AudioBackendClassForMode(1), kAudioBackendWindowsUac2,
               "mode1 -> WindowsUac2");
    expect_int(AudioBackendClassForMode(2), kAudioBackendAndroid,
               "mode2 -> Android");
    expect_int(AudioBackendClassForMode(3), kAudioBackendSp404,
               "mode3 -> Sp404");
    expect_int(AudioBackendClassForMode(4), kAudioBackendMidi,
               "mode4 -> Midi");
    expect_int(AudioBackendClassForMode(5), kAudioBackendSp404,
               "mode5 -> Sp404");
    expect_int(AudioBackendClassForMode(99), kAudioBackendLocal,
               "fallback -> Local");
    expect_str(AudioBackendClassName(1), "WindowsUac2Backend", "class name");
    expect_str(AudioBackendClassName(5), "Sp404Backend", "class name 5");

    // Capacidades agregadas por clase (derivadas de los modos).
    const unsigned int LOCAL = AudioBackendClassCapabilities(kAudioBackendLocal);
    const unsigned int WIN =
        AudioBackendClassCapabilities(kAudioBackendWindowsUac2);
    const unsigned int ANDR =
        AudioBackendClassCapabilities(kAudioBackendAndroid);
    const unsigned int SP404 =
        AudioBackendClassCapabilities(kAudioBackendSp404);
    const unsigned int MIDI =
        AudioBackendClassCapabilities(kAudioBackendMidi);

    // LocalAudioBackend: solo salida estereo local.
    expect_bit(LOCAL, kAudioCapStereoOutput, "local out");
    expect_no_bit(LOCAL, kAudioCapStereoInput, "local no in");
    expect_no_bit(LOCAL, kAudioCapUsbDevice | kAudioCapUsbHost, "local no usb");

    // WindowsUac2Backend: duplex gadget (device) + capture.
    expect_bit(WIN, kAudioCapStereoOutput, "win out");
    expect_bit(WIN, kAudioCapStereoInput, "win in");
    expect_bit(WIN, kAudioCapUsbDevice, "win usb device");
    expect_bit(WIN, kAudioCapCapture, "win capture");

    // AndroidBackend: input-only host-role + capture.
    expect_no_bit(ANDR, kAudioCapStereoOutput, "android no out");
    expect_bit(ANDR, kAudioCapStereoInput, "android in");
    expect_bit(ANDR, kAudioCapUsbHost, "android usb host");
    expect_bit(ANDR, kAudioCapCapture, "android capture");

    // Sp404Backend: out (dir0) + in/capture (dir1) host-role.
    expect_bit(SP404, kAudioCapStereoOutput, "sp404 out (dir0)");
    expect_bit(SP404, kAudioCapStereoInput, "sp404 in (dir1)");
    expect_bit(SP404, kAudioCapUsbHost, "sp404 usb host");
    expect_bit(SP404, kAudioCapCapture, "sp404 capture");
    expect_no_bit(SP404, kAudioCapUsbDevice, "sp404 not device");
    expect_no_bit(SP404, kAudioCapMidi, "sp404 no midi");

    // MidiBackend: host-role MIDI (sin audio estereo).
    expect_bit(MIDI, kAudioCapUsbHost, "midi usb host");
    expect_bit(MIDI, kAudioCapMidi, "midi midi");
    expect_no_bit(MIDI, kAudioCapStereoOutput, "midi no stereo out");
    expect_no_bit(MIDI, kAudioCapStereoInput, "midi no stereo in");

    // Consistencia: la union por clase <= union de todos los modos.
    const unsigned int ALL =
        AudioDriverModeCapabilities(0, 0) |
        AudioDriverModeCapabilities(1, 0) |
        AudioDriverModeCapabilities(2, 0) |
        AudioDriverModeCapabilities(3, 0) |
        AudioDriverModeCapabilities(3, 1) |
        AudioDriverModeCapabilities(4, 0) |
        AudioDriverModeCapabilities(5, 0);
    expect_no_bit(LOCAL & ~ALL, 0xFFFFFFFFu, "local subset ALL");
    expect_no_bit(WIN & ~ALL, 0xFFFFFFFFu, "win subset ALL");
    expect_no_bit(ANDR & ~ALL, 0xFFFFFFFFu, "android subset ALL");
    expect_no_bit(SP404 & ~ALL, 0xFFFFFFFFu, "sp404 subset ALL");
    expect_no_bit(MIDI & ~ALL, 0xFFFFFFFFu, "midi subset ALL");

    std::printf("AUDIO_BACKEND_HOST_ALL_OK (%d checks)\n", g_checks);
    return 0;
}