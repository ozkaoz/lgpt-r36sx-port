// F4c: AudioRouter - politica declarativa de seleccion/routing de backends.
// Oraculos golden de TreeFrogUac2Bridge.cpp (SetDriverMode/CycleDriverMode):
// mapeo efectivo (USB_OUT + dir IN -> SP404_IN), clasificacion host-role
// (U2.52 HOST_ROLE_MODE_ALWAYS_APPLY: ANDROID/USB_OUT/SP404_IN/MIDI) y la
// secuencia del ciclo LOCAL -> ... -> MIDI -> LOCAL.
#include <cstdio>
#include <cstdlib>
#include "Application/Audio/AudioRouter.h"

static int g_checks = 0;

static void expect_int(int got, int want, const char *what) {
    ++g_checks;
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: got %d want %d\n", what, got, want);
        std::exit(1);
    }
}

int main() {
    // Mapeo efectivo golden de SetDriverMode:
    // USB_OUT (3) con direccion sampler IN (1) se ejecuta como SP404_IN (5).
    expect_int(AudioRouteEffectiveMode(3, 1), 5, "effective(USB_OUT,IN)");
    expect_int(AudioRouteEffectiveMode(3, 0), 3, "effective(USB_OUT,OUT)");
    // El resto de modos no se mapea, con cualquier direccion.
    expect_int(AudioRouteEffectiveMode(0, 1), 0, "effective(LOCAL,IN)");
    expect_int(AudioRouteEffectiveMode(1, 1), 1, "effective(WINDOWS,IN)");
    expect_int(AudioRouteEffectiveMode(2, 1), 2, "effective(ANDROID,IN)");
    expect_int(AudioRouteEffectiveMode(4, 1), 4, "effective(MIDI,IN)");
    expect_int(AudioRouteEffectiveMode(5, 0), 5, "effective(SP404_IN,OUT)");
    expect_int(AudioRouteEffectiveMode(0, 0), 0, "effective(LOCAL,OUT)");

    // Clasificacion host-role golden (U2.52 HOST_ROLE_MODE_ALWAYS_APPLY):
    // host-role = ANDROID(2), USB_OUT(3), SP404_IN(5), MIDI(4).
    expect_int(AudioRouteIsHostRoleMode(0), 0, "host-role(LOCAL)");
    expect_int(AudioRouteIsHostRoleMode(1), 0, "host-role(WINDOWS)");
    expect_int(AudioRouteIsHostRoleMode(2), 1, "host-role(ANDROID)");
    expect_int(AudioRouteIsHostRoleMode(3), 1, "host-role(USB_OUT)");
    expect_int(AudioRouteIsHostRoleMode(4), 1, "host-role(MIDI)");
    expect_int(AudioRouteIsHostRoleMode(5), 1, "host-role(SP404_IN)");
    expect_int(AudioRouteIsHostRoleMode(99), 0, "host-role(fallback)");

    // Ciclo golden: next = (mode+1) % 5 (SP404_IN no se lista).
    for (int mode = 0; mode < 5; ++mode) {
        expect_int(AudioRouteCycleNext(mode), (mode + 1) % 5,
                   "cycle next");
    }
    expect_int(AudioRouteCycleNext(5), 1, "cycle next from SP404_IN");

    // Movimiento inverso del modal (UP): prev = (mode+4) % 5.
    expect_int(AudioRouteCyclePrev(0), 4, "cycle prev 0");
    expect_int(AudioRouteCyclePrev(1), 0, "cycle prev 1");
    expect_int(AudioRouteCyclePrev(4), 3, "cycle prev 4");

    // Coherencia host-role <-> capacidad UsbHost (misma fuente de verdad).
    for (int mode = 0; mode <= 5; ++mode) {
        const int hostCap =
            (AudioDriverModeCapabilities(mode, 0) & kAudioCapUsbHost) ? 1 : 0;
        expect_int(AudioRouteIsHostRoleMode(mode), hostCap,
                   "host-role == UsbHost cap");
    }

    std::printf("AUDIO_ROUTER_HOST_ALL_OK (%d checks)\n", g_checks);
    return 0;
}
