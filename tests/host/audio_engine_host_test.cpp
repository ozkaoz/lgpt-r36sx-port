// F4e: AudioEngine - politica de estado del motor de audio (capa pura).
// Oraculos golden de TreeFrogUac2Bridge.cpp (should_mute_now y
// MixUsbCaptureMonitorStereo48000): mute local por modo (U2.52.5
// ANDROID_NO_MUTE), paso ASRC del monitor y ganancia conservadora 75%.
#include <cstdio>
#include <cstdlib>
#include "Application/Audio/AudioEngine.h"

static int g_checks = 0;

static void expect_int(int got, int want, const char *what) {
    ++g_checks;
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: got %d want %d\n", what, got, want);
        std::exit(1);
    }
}

static void expect_double(double got, double want, const char *what) {
    ++g_checks;
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: got %f want %f\n", what, got, want);
        std::exit(1);
    }
}

int main() {
    // Constantes golden del monitor.
    expect_int(kAudioEngineMonitorPrebufferSamples, 960, "prebuffer");
    expect_int(kAudioEngineMonitorGainPercent, 75, "monitor gain %");

    // Replica de should_mute_now():
    // ANDROID (2) NUNCA muta, cualquiera sea el estado runtime.
    expect_int(AudioEngineShouldMute(2, 0, 1, 0), 0, "android+raw no mute");
    expect_int(AudioEngineShouldMute(2, 0, 0, 0), 0, "android no raw no mute");
    expect_int(AudioEngineShouldMute(2, 1, 1, 1), 0, "android+nomute no mute");

    // LOCAL (0): no tiene salida USB -> nunca muta.
    expect_int(AudioEngineShouldMute(0, 0, 1, 0), 0, "local+raw no mute");
    expect_int(AudioEngineShouldMute(0, 0, 0, 0), 0, "local no raw no mute");

    // WINDOWS (1): duplex gadget; muta si raw y no nomute.
    expect_int(AudioEngineShouldMute(1, 0, 1, 0), 1, "win+raw mute");
    expect_int(AudioEngineShouldMute(1, 0, 0, 0), 0, "win no raw no mute");
    expect_int(AudioEngineShouldMute(1, 0, 1, 1), 0, "win+raw+nomute no mute");
    expect_int(AudioEngineShouldMute(1, 1, 1, 0), 1, "win dir1+raw mute");

    // USB_OUT (3): out solo con dir=0; con dir=1 no hay salida -> no muta.
    expect_int(AudioEngineShouldMute(3, 0, 1, 0), 1, "usb_out dir0 mute");
    expect_int(AudioEngineShouldMute(3, 1, 1, 0), 0, "usb_out dir1 no mute");

    // SP404_IN (5): input-only -> nunca muta.
    expect_int(AudioEngineShouldMute(5, 0, 1, 0), 0, "sp404_in no mute");
    expect_int(AudioEngineShouldMute(5, 1, 1, 0), 0, "sp404_in dir1 no mute");

    // MIDI (4): sin salida USB -> nunca muta.
    expect_int(AudioEngineShouldMute(4, 0, 1, 0), 0, "midi no mute");

    // Fallback (99): como LOCAL (sin salida) -> nunca muta.
    expect_int(AudioEngineShouldMute(99, 0, 1, 0), 0, "fallback no mute");

    // Paso ASRC del monitor: usb/engine, fallback 1.0 con engine=0.
    expect_double(AudioEngineMonitorStep(48000, 48000), 1.0, "step 48/48");
    expect_double(AudioEngineMonitorStep(44100, 48000),
                  48000.0 / 44100.0, "step 48/44.1");
    expect_double(AudioEngineMonitorStep(0, 48000), 1.0, "step engine 0");
    expect_double(AudioEngineMonitorStep(0, 0), 1.0, "step both 0");
    expect_double(AudioEngineMonitorStep(96000, 48000), 0.5, "step 48/96");

    // Ganancia del monitor 75% (enteros).
    expect_int(AudioEngineMonitorApplyGain(100), 75, "gain 100");
    expect_int(AudioEngineMonitorApplyGain(4), 3, "gain 4");
    expect_int(AudioEngineMonitorApplyGain(-4), -3, "gain -4");
    expect_int(AudioEngineMonitorApplyGain(0), 0, "gain 0");

    // Consistencia con el router: mute implica salida USB del modo.
    for (int mode = 0; mode <= 5; ++mode) {
        for (int dir = 0; dir <= 1; ++dir) {
            const int mute = AudioEngineShouldMute(mode, dir, 1, 0);
            const int hasOut = AudioDriverModeHasOut(mode, dir);
            if (hasOut && mode != kAudioDriverModeAndroid) {
                expect_int(mute, 1, "mute == hasOut (no android)");
            } else {
                expect_int(mute, 0, "no mute sin hasOut");
            }
        }
    }

    std::printf("AUDIO_ENGINE_HOST_ALL_OK (%d checks)\n", g_checks);
    return 0;
}
