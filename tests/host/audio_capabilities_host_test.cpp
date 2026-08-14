// F4b: vocabulario de capacidades de audio (AudioCapabilities.h) y
// derivacion per-modo (AudioDriverModeTable.h).  Oraculos derivados SOLO
// de los primitivos golden del bridge (mode_has_out/mode_has_in y el rol
// gadget vs host-role declarado en TreeFrogUac2Bridge.cpp).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Application/Audio/AudioCapabilities.h"
#include "Application/Audio/AudioDriverModeTable.h"

static int g_checks = 0;

static void expect_int(unsigned int got, unsigned int want,
                       const char *what) {
    ++g_checks;
    if (got != want) {
        std::fprintf(stderr,
                     "FAIL %s: got 0x%X want 0x%X\n", what, got, want);
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

int main() {
    // Vocabulario: 8 capacidades, nombres en el orden de los bits.
    expect_int(AudioCapabilityCount(), 8, "capability count");
    expect_str(AudioCapabilityName(0), "Stereo Output", "cap name 0");
    expect_str(AudioCapabilityName(1), "Stereo Input", "cap name 1");
    expect_str(AudioCapabilityName(2), "USB Device", "cap name 2");
    expect_str(AudioCapabilityName(3), "USB Host", "cap name 3");
    expect_str(AudioCapabilityName(4), "MIDI", "cap name 4");
    expect_str(AudioCapabilityName(5), "Capture", "cap name 5");
    expect_str(AudioCapabilityName(6), "Clock Sync", "cap name 6");
    expect_str(AudioCapabilityName(7), "Hotplug", "cap name 7");
    expect_int(AudioCapabilityBit(0), kAudioCapStereoOutput, "bit 0");
    expect_int(AudioCapabilityBit(1), kAudioCapStereoInput, "bit 1");
    expect_int(AudioCapabilityBit(2), kAudioCapUsbDevice, "bit 2");
    expect_int(AudioCapabilityBit(3), kAudioCapUsbHost, "bit 3");
    expect_int(AudioCapabilityBit(4), kAudioCapMidi, "bit 4");
    expect_int(AudioCapabilityBit(5), kAudioCapCapture, "bit 5");
    expect_int(AudioCapabilityBit(6), kAudioCapClockSync, "bit 6");
    expect_int(AudioCapabilityBit(7), kAudioCapHotplug, "bit 7");

    const unsigned int CAP_OUT = kAudioCapStereoOutput;
    const unsigned int CAP_IN = kAudioCapStereoInput;
    const unsigned int CAP_DEV = kAudioCapUsbDevice;
    const unsigned int CAP_HOST = kAudioCapUsbHost;
    const unsigned int CAP_MIDI = kAudioCapMidi;
    const unsigned int CAP_CAP = kAudioCapCapture;

    // LOCAL_CONSOLE (0): suena local (OUT), sin USB.
    expect_int(AudioDriverModeCapabilities(0, 0), CAP_OUT, "caps(0,dir0)");
    expect_int(AudioDriverModeCapabilities(0, 1), CAP_OUT, "caps(0,dir1)");

    // WINDOWS (1): unico gadget (USB Device), duplex out+in, capture.
    expect_int(AudioDriverModeCapabilities(1, 0),
               CAP_OUT | CAP_IN | CAP_DEV | CAP_CAP, "caps(1,dir0)");
    expect_int(AudioDriverModeCapabilities(1, 1),
               CAP_OUT | CAP_IN | CAP_DEV | CAP_CAP, "caps(1,dir1)");

    // ANDROID (2): host-role input-only (in, capture, host).
    expect_int(AudioDriverModeCapabilities(2, 0),
               CAP_IN | CAP_HOST | CAP_CAP, "caps(2,dir0)");
    expect_int(AudioDriverModeCapabilities(2, 1),
               CAP_IN | CAP_HOST | CAP_CAP, "caps(2,dir1)");

    // USB_OUT (3): sampler slot; out cuando dir=0, in+capture cuando dir=1.
    expect_int(AudioDriverModeCapabilities(3, 0),
               CAP_OUT | CAP_HOST, "caps(3,dir0)");
    expect_int(AudioDriverModeCapabilities(3, 1),
               CAP_IN | CAP_HOST | CAP_CAP, "caps(3,dir1)");

    // MIDI (4): host-role, solo MIDI.
    expect_int(AudioDriverModeCapabilities(4, 0),
               CAP_HOST | CAP_MIDI, "caps(4,dir0)");
    expect_int(AudioDriverModeCapabilities(4, 1),
               CAP_HOST | CAP_MIDI, "caps(4,dir1)");

    // SP404_IN (5): host-role input-only (in, capture, host).
    expect_int(AudioDriverModeCapabilities(5, 0),
               CAP_IN | CAP_HOST | CAP_CAP, "caps(5,dir0)");
    expect_int(AudioDriverModeCapabilities(5, 1),
               CAP_IN | CAP_HOST | CAP_CAP, "caps(5,dir1)");

    // Fallback (id fuera de rango): sin capacidades (selectable=0).
    expect_int(AudioDriverModeCapabilities(99, 0), 0, "caps(99,dir0)");
    expect_int(AudioDriverModeCapabilities(-1, 0), 0, "caps(-1,dir0)");

    // Consistencia con los primitivos golden: la proyeccion no inventa
    // direcciones.  StereoOutput <=> hasOut || LOCAL; StereoInput <=> hasIn.
    for (int mode = 0; mode <= 5; ++mode) {
        for (int dir = 0; dir <= 1; ++dir) {
            const unsigned int caps =
                AudioDriverModeCapabilities(mode, dir);
            const int hasOut = AudioDriverModeHasOut(mode, dir);
            const int hasIn = AudioDriverModeHasIn(mode, dir);
            const int outBit = (caps & CAP_OUT) ? 1 : 0;
            const int inBit = (caps & CAP_IN) ? 1 : 0;
            const int wantOut = hasOut || (mode == kAudioDriverModeLocalConsole);
            const int wantIn = hasIn;
            expect_int(outBit, wantOut, "consistency out");
            expect_int(inBit, wantIn, "consistency in");
        }
    }

    std::printf("AUDIO_CAPABILITIES_HOST_ALL_OK (%d checks)\n", g_checks);
    return 0;
}
