// F3-4c: MixerMenu - oraculos golden del menu L1+A del Mixer (header-only).
// Compila en host (g++ ASAN/UBSAN) sin ningun include de GUI/audio/Player.
#include "Application/Mixer/MixerMenu.h"
#include <stdio.h>
#include <string.h>

static int g_failures = 0 ;
static int g_checks = 0 ;

static void check(bool cond, const char *what) {
    g_checks++ ;
    if (!cond) {
        g_failures++ ;
        printf("FAIL: %s\n", what) ;
    }
}

int main() {
    // 1. Numero de filas de cada menu (golden Bacon 1.2.1).
    check(kMixerMasterMenuRowCount == 6, "master rows 6") ;
    check(kMixerTrackMenuRowCount == 5, "track rows 5") ;
    check(mixerMenuRowCount(true) == 6, "mixerMenuRowCount master 6") ;
    check(mixerMenuRowCount(false) == 5, "mixerMenuRowCount track 5") ;

    // 2. Etiquetas del menu MASTER, en orden exacto.
    const char *masterLabels[6] = { "LIMITER", "CLIP GAIN", "FX DELAY",
                                    "FX REVERB", "FX EQ", "FX COMP" } ;
    for (int i = 0; i < 6; i++) {
        check(!strcmp(mixerMenuLabel(true, i), masterLabels[i]),
              "master label order") ;
    }

    // 3. Etiquetas del menu TRACK, en orden exacto.
    // BASS_SYNTH_EQ8_MENU (bacon-1.5, feedback): the PLAYBACK section was
    // removed from the instrument page; the menu row now jumps to EQ8.
    const char *trackLabels[5] = { "FILTER", "BITCRUSHER", "EQ8",
                                   "FX SENDS", "AUTOMATION" } ;
    for (int i = 0; i < 5; i++) {
        check(!strcmp(mixerMenuLabel(false, i), trackLabels[i]),
              "track label order") ;
    }

    // 4. Clamps golden de los valores editables.
    check(mixerMenuClampSoftclip(-1) == 0, "softclip clamp -1 -> 0") ;
    check(mixerMenuClampSoftclip(0) == 0, "softclip clamp 0") ;
    check(mixerMenuClampSoftclip(4) == 4, "softclip clamp 4") ;
    check(mixerMenuClampSoftclip(5) == 4, "softclip clamp 5 -> 4") ;
    check(mixerMenuClampSoftclipGain(-1) == 0, "gain clamp -1 -> 0") ;
    check(mixerMenuClampSoftclipGain(0) == 0, "gain clamp 0") ;
    check(mixerMenuClampSoftclipGain(1) == 1, "gain clamp 1") ;
    check(mixerMenuClampSoftclipGain(2) == 1, "gain clamp 2 -> 1") ;
    check(kMixerSoftclipMax == 4, "softclip range max 4") ;
    check(kMixerSoftclipGainMax == 1, "gain range max 1") ;

    // 5. Codificado de accion del menu.
    check(mixerMenuActionForRow(true, 0) == 0, "master row0 -> no action") ;
    check(mixerMenuActionForRow(true, 1) == 0, "master row1 -> no action") ;
    check(mixerMenuActionForRow(true, 2) == 1, "master row2 -> page 1") ;
    check(mixerMenuActionForRow(true, 3) == 2, "master row3 -> page 2") ;
    check(mixerMenuActionForRow(true, 4) == 3, "master row4 -> page 3") ;
    check(mixerMenuActionForRow(true, 5) == 4, "master row5 -> page 4") ;
    check(mixerMenuActionForRow(false, 0) == 101, "track row0 -> 101") ;
    check(mixerMenuActionForRow(false, 4) == 105, "track row4 -> 105") ;

    // 6. Hints FourCC de las secciones TRACK (SIP_* golden).
    check(mixerMenuSectionHint(0) == MAKE_FOURCC('F','M','I','X'),
          "hint FILTER = SIP_FILTMIX") ;
    check(mixerMenuSectionHint(1) == MAKE_FOURCC('C','R','S','H'),
          "hint BITCRUSHER = SIP_CRUSH") ;
    check(mixerMenuSectionHint(2) == MAKE_FOURCC('E','Q','E','N'),
          "hint EQ8 = SIP_EQEN") ;
    check(mixerMenuSectionHint(3) == MAKE_FOURCC('D','R','Y','_'),
          "hint FX SENDS = SIP_DRY") ;
    check(mixerMenuSectionHint(4) == MAKE_FOURCC('T','B','L','A'),
          "hint AUTOMATION = SIP_TABLEAUTO") ;
    check(mixerMenuSectionHint(-1) == 0, "hint out of range -1 -> 0") ;
    check(mixerMenuSectionHint(5) == 0, "hint out of range 5 -> 0") ;

    if (g_failures == 0) {
        printf("ALL OK (%d checks)\n", g_checks) ;
        return 0 ;
    }
    printf("%d/%d checks FAILED\n", g_failures, g_checks) ;
    return 1 ;
}
