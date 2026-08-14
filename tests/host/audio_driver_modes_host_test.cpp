// F4a: tabla declarativa de modos de audio del driver UAC2 (capa pura).
// Oraculos golden byte-identicos a las tablas switch que vivian en
// TreeFrogUac2Bridge.cpp (golden Bacon 1.2.1): nombres, descripciones,
// tokens de modo, tokens de politica OTG, ramas del daemon, selectable y
// capacidades de direccion (out/in) con el toggle del sampler.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Application/Audio/AudioDriverModeTable.h"

static int g_checks = 0;

static void expect_str(const char *got, const char *want, const char *what) {
    ++g_checks;
    if (!got || std::strcmp(got, want) != 0) {
        std::fprintf(stderr,
                     "FAIL %s: got '%s' want '%s'\n",
                     what, got ? got : "(null)", want);
        std::exit(1);
    }
}

static void expect_int(int got, int want, const char *what) {
    ++g_checks;
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: got %d want %d\n", what, got, want);
        std::exit(1);
    }
}

int main() {
    // Conteo golden de la UI (SP404_IN no se lista).
    expect_int(AudioDriverModeCount(), 5, "count");

    // Nombres golden (mode_name).
    expect_str(AudioDriverModeName(0), "Local Console", "name(0)");
    expect_str(AudioDriverModeName(1), "Windows", "name(1)");
    expect_str(AudioDriverModeName(2), "Android", "name(2)");
    expect_str(AudioDriverModeName(3), "Sampler", "name(3)");
    expect_str(AudioDriverModeName(4), "MIDI", "name(4)");
    expect_str(AudioDriverModeName(5), "Sampler", "name(5)");
    expect_str(AudioDriverModeName(99), "Local Console", "name(99) fallback");
    expect_str(AudioDriverModeName(-1), "Local Console", "name(-1) fallback");

    // Descripciones golden (mode_desc).
    expect_str(AudioDriverModeDescription(0),
               "Console sound, OTG may stay connected", "desc(0)");
    expect_str(AudioDriverModeDescription(1),
               "Duplex UAC2 gadget (PC host)", "desc(1)");
    expect_str(AudioDriverModeDescription(2),
               "Duplex UAC2 gadget (phone host)", "desc(2)");
    expect_str(AudioDriverModeDescription(3),
               "SP404: console sound to sampler (EXT SOURCE)", "desc(3)");
    expect_str(AudioDriverModeDescription(4),
               "MIDI: USB piano/controller", "desc(4)");
    expect_str(AudioDriverModeDescription(5),
               "SP404 IN: sampler->console, recording only", "desc(5)");
    expect_str(AudioDriverModeDescription(99),
               "Console sound, OTG may stay connected", "desc(99) fallback");

    // Tokens de modo golden (mode_token, escrito en audio_driver_mode).
    expect_str(AudioDriverModeToken(0), "LOCAL_CONSOLE", "token(0)");
    expect_str(AudioDriverModeToken(1), "USB_DUPLEX", "token(1)");
    expect_str(AudioDriverModeToken(2), "USB_IN", "token(2)");
    expect_str(AudioDriverModeToken(3), "USB_OUT", "token(3)");
    expect_str(AudioDriverModeToken(4), "MIDI", "token(4)");
    expect_str(AudioDriverModeToken(5), "SP404_IN", "token(5)");
    expect_str(AudioDriverModeToken(99), "LOCAL_CONSOLE", "token(99) fallback");

    // Tokens de politica OTG golden (policy_token).
    expect_str(AudioDriverModePolicyToken(0), "LOCAL_CONSOLE", "policy(0)");
    expect_str(AudioDriverModePolicyToken(1), "USB_DUPLEX_OTG", "policy(1)");
    expect_str(AudioDriverModePolicyToken(2), "USB_IN_OTG", "policy(2)");
    expect_str(AudioDriverModePolicyToken(3), "USB_OUT_OTG", "policy(3)");
    expect_str(AudioDriverModePolicyToken(4), "MIDI_OTG", "policy(4)");
    expect_str(AudioDriverModePolicyToken(5), "USB_OUT_OTG", "policy(5)");
    expect_str(AudioDriverModePolicyToken(99), "LOCAL_CONSOLE",
               "policy(99) fallback");

    // Ramas del daemon golden (branch_name_for_mode).
    expect_str(AudioDriverModeBranchName(0), "audio_driver_local_console",
               "branch(0)");
    expect_str(AudioDriverModeBranchName(1), "audio_driver_usb_duplex",
               "branch(1)");
    expect_str(AudioDriverModeBranchName(2), "audio_driver_usb_in",
               "branch(2)");
    expect_str(AudioDriverModeBranchName(3), "audio_driver_usb_out",
               "branch(3)");
    expect_str(AudioDriverModeBranchName(4), "audio_driver_midi",
               "branch(4)");
    expect_str(AudioDriverModeBranchName(5), "audio_driver_sp404_in",
               "branch(5)");
    expect_str(AudioDriverModeBranchName(99), "audio_driver_local_console",
               "branch(99) fallback");

    // Selectable golden (selectable_mode): 0..4 si, SP404_IN no, fallback no.
    expect_int(AudioDriverModeIsSelectable(0), 1, "selectable(0)");
    expect_int(AudioDriverModeIsSelectable(1), 1, "selectable(1)");
    expect_int(AudioDriverModeIsSelectable(2), 1, "selectable(2)");
    expect_int(AudioDriverModeIsSelectable(3), 1, "selectable(3)");
    expect_int(AudioDriverModeIsSelectable(4), 1, "selectable(4)");
    expect_int(AudioDriverModeIsSelectable(5), 0, "selectable(5)");
    expect_int(AudioDriverModeIsSelectable(99), 0, "selectable(99) fallback");
    expect_int(AudioDriverModeIsSelectable(-1), 0, "selectable(-1) fallback");

    // Capacidades de direccion golden (mode_has_out / mode_has_in) con el
    // toggle del sampler como parametro.
    // OUT: USB_OUT = !dir, WINDOWS = 1, resto = 0.
    expect_int(AudioDriverModeHasOut(0, 0), 0, "out(0,dir0)");
    expect_int(AudioDriverModeHasOut(1, 0), 1, "out(1,dir0)");
    expect_int(AudioDriverModeHasOut(1, 1), 1, "out(1,dir1)");
    expect_int(AudioDriverModeHasOut(2, 0), 0, "out(2,dir0)");
    expect_int(AudioDriverModeHasOut(3, 0), 1, "out(3,dir0)");
    expect_int(AudioDriverModeHasOut(3, 1), 0, "out(3,dir1)");
    expect_int(AudioDriverModeHasOut(4, 0), 0, "out(4,dir0)");
    expect_int(AudioDriverModeHasOut(5, 0), 0, "out(5,dir0)");
    expect_int(AudioDriverModeHasOut(99, 0), 0, "out(99) fallback");
    // IN: USB_OUT = dir, WINDOWS/ANDROID/SP404_IN = 1, resto = 0.
    expect_int(AudioDriverModeHasIn(0, 1), 0, "in(0,dir1)");
    expect_int(AudioDriverModeHasIn(1, 1), 1, "in(1,dir1)");
    expect_int(AudioDriverModeHasIn(2, 1), 1, "in(2,dir1)");
    expect_int(AudioDriverModeHasIn(3, 0), 0, "in(3,dir0)");
    expect_int(AudioDriverModeHasIn(3, 1), 1, "in(3,dir1)");
    expect_int(AudioDriverModeHasIn(4, 1), 0, "in(4,dir1)");
    expect_int(AudioDriverModeHasIn(5, 1), 1, "in(5,dir1)");
    expect_int(AudioDriverModeHasIn(99, 1), 0, "in(99) fallback");

    // Identidades de la tabla (id por posicion).
    for (int i = 0; i < 6; ++i) {
        expect_int(kAudioDriverModes[i].id, i, "table id by position");
    }

    std::printf("AUDIO_DRIVER_MODES_HOST_ALL_OK (%d checks)\n", g_checks);
    return 0;
}
