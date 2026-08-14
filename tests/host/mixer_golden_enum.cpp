/*
 * mixer_golden_enum.cpp -- enumeracion EXHAUSTIVA de CTX_MIXER y
 * CTX_MIXER_FX contra un modelo fiel de golden MixerView.cpp:534-697.
 *
 * F1b-Mixer. Para cada una de las 8192 mascaras de 13 bits, en 2 contextos
 * y 2 estados de masterSelected_, compara el resultado del resolver REAL
 * (tabla ActionMap.cpp) contra la PRIMERA accion del modelo golden.
 *
 * El modelo es la misma estructura de if/else del golden (los numeros de
 * linea refieren al golden Bacon 1.2.1 @ 951b7b3). Cualquier mascara que el
 * modelo resuelve con >1 accion (multi-fire: bloques A/L1/base y R1+UP+START)
 * se cuenta por separado: el resolver devuelve la primera y el adapter F1b
 * debe replar las restantes en orden.
 *
 * masterSelected_: precondicion RUNTIME del dispatch (MixerView.cpp:594,
 * el bloque L2 entero se aborta con master seleccionado). La tabla estatica
 * no puede modelarla; el enumerador exige que el resolver coincida con el
 * golden SIN gate y verifica que TODAS las mascaras con gate (master=1)
 * resuelvan a acciones PAN (exactamente las que el dispatch debe suprimir).
 *
 * Exit 0 si no hay mismatches.
 */
#include <cstdio>
#include <cstdlib>

#include "Application/UI/Input/ChordResolver.h"

using namespace UI::Input;

static const int KEY_ANY = 0x3FFF;  /* 13 bits */

static ActionId golden_first(bool fx, unsigned m) {
    if (m & KEY_SELECT) return ACTION_CYCLE_FX_PAGE;                  /* :540   */
    if (m & KEY_R1) {                                                 /* :549   */
        if (m & KEY_B) return ACTION_TOGGLE_MUTE;                     /* :550   */
        if (m & KEY_A) return ACTION_TOGGLE_SOLO;                     /* :554   */
        if (m & KEY_UP) return ACTION_SWITCH_VIEW_SONG;               /* :558   */
        if (m & KEY_START) return ACTION_STOP;                        /* :564   */
        return ACTION_NONE;
    }
    if (m & KEY_R2) {                                                 /* :572   */
        if (m & KEY_A) return ACTION_OPEN_INSTRUMENT_FX;              /* :573   */
        if (!fx) return ACTION_CYCLE_FX_EDIT_TARGET;                  /* :579   */
        return ACTION_NONE;
    }
    if (m & KEY_L2) {                                                 /* :593   */
        /* masterSelected_ gate eliminado: es precondicion del dispatch. */
        if ((m & KEY_A) && (m & KEY_B) &&
            !(m & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_L1 |
                   KEY_R1 | KEY_X | KEY_Y | KEY_SELECT | KEY_START)))
            return ACTION_RESET_PAN;                                  /* :595   */
        if (m & KEY_LEFT)                                             /* :608   */
            return (m & KEY_A) ? ACTION_PAN_NUDGE_LEFT_COARSE
                               : ACTION_PAN_NUDGE_LEFT;
        if (m & KEY_RIGHT)                                            /* :618   */
            return (m & KEY_A) ? ACTION_PAN_NUDGE_RIGHT_COARSE
                               : ACTION_PAN_NUDGE_RIGHT;
        return ACTION_NONE;
    }
    if ((m & KEY_L1) && (m & KEY_A) &&
        !(m & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_R1 |
               KEY_L2 | KEY_R2 | KEY_X | KEY_Y | KEY_SELECT | KEY_START)))
        return ACTION_OPEN_MENU;                                      /* :635   */
    if (fx) {                                                         /* :645   */
        if ((m & KEY_A) && (m & KEY_B) &&
            !(m & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_L1 |
                   KEY_R1 | KEY_L2 | KEY_R2 | KEY_X | KEY_Y |
                   KEY_SELECT | KEY_START)))
            return ACTION_RESET_PARAMETER;                            /* :648   */
        if (m & KEY_A) {                                              /* :655   */
            if (m & KEY_UP) return ACTION_EDIT_PARAM_UP;
            if (m & KEY_DOWN) return ACTION_EDIT_PARAM_DOWN;
            if (m & KEY_LEFT) return ACTION_EDIT_PARAM_LEFT;
            if (m & KEY_RIGHT) return ACTION_EDIT_PARAM_RIGHT;
            return ACTION_NONE;
        }
        if (m & KEY_UP) return ACTION_ROW_UP;                         /* :662   */
        if (m & KEY_DOWN) return ACTION_ROW_DOWN;
        if (m & KEY_LEFT) return ACTION_EDIT_PARAM_LEFT;
        if (m & KEY_RIGHT) return ACTION_EDIT_PARAM_RIGHT;
        if (m & KEY_START) return ACTION_PLAY_STOP;                   /* :666   */
        return ACTION_NONE;
    }
    if (m & KEY_A) {                                                  /* :672   */
        if (m & KEY_UP) return ACTION_VOLUME_COARSE_UP;
        if (m & KEY_DOWN) return ACTION_VOLUME_COARSE_DOWN;
        if (m & KEY_LEFT) return ACTION_VOLUME_FINE_DECREASE;
        if (m & KEY_RIGHT) return ACTION_VOLUME_FINE_INCREASE;
        return ACTION_NONE;
    }
    if (m & KEY_L1) {                                                 /* :681   */
        if (m & KEY_UP) return ACTION_VOLUME_COARSE_UP;
        if (m & KEY_DOWN) return ACTION_VOLUME_COARSE_DOWN;
        if (m & KEY_LEFT) return ACTION_MIX_CURSOR_LEFT;
        if (m & KEY_RIGHT) return ACTION_MIX_CURSOR_RIGHT;
        return ACTION_NONE;
    }
    if (m & KEY_START) return ACTION_PLAY_STOP;                       /* :690   */
    if (m & KEY_LEFT) return ACTION_MIX_CURSOR_LEFT;                  /* :693   */
    if (m & KEY_RIGHT) return ACTION_MIX_CURSOR_RIGHT;
    if (m & KEY_UP) return ACTION_VOLUME_FINE_UP;
    if (m & KEY_DOWN) return ACTION_VOLUME_FINE_DOWN;
    return ACTION_NONE;
}

/* Numero de acciones que dispara el golden SIN gate de master (para
 * contabilizar multi-fire): solo se cuentan los puntos donde el golden
 * encadena varias llamadas sin return intermedio. */
static int golden_action_count(bool fx, unsigned m) {
    int n = 0;
    if (m & KEY_SELECT) return 1;
    if (m & KEY_R1) {
        if (m & KEY_B) return 1;
        if (m & KEY_A) return 1;
        if (m & KEY_UP) n++;
        if (m & KEY_START) n++;
        return n;
    }
    if (m & KEY_R2) {
        if (m & KEY_A) return 1;
        if (!fx) return 1;
        return 0;
    }
    if (m & KEY_L2) {
        if ((m & KEY_A) && (m & KEY_B) &&
            !(m & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_L1 |
                   KEY_R1 | KEY_X | KEY_Y | KEY_SELECT | KEY_START)))
            return 1;
        if (m & KEY_LEFT) return 1;
        if (m & KEY_RIGHT) return 1;
        return 0;
    }
    if ((m & KEY_L1) && (m & KEY_A) &&
        !(m & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_R1 |
               KEY_L2 | KEY_R2 | KEY_X | KEY_Y | KEY_SELECT | KEY_START)))
        return 1;
    if (fx) {
        if ((m & KEY_A) && (m & KEY_B) &&
            !(m & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_L1 |
                   KEY_R1 | KEY_L2 | KEY_R2 | KEY_X | KEY_Y |
                   KEY_SELECT | KEY_START)))
            return 1;
        if (m & KEY_A) {
            if (m & KEY_UP) n++;
            if (m & KEY_DOWN) n++;
            if (m & KEY_LEFT) n++;
            if (m & KEY_RIGHT) n++;
            return n;
        }
        if (m & KEY_UP) return 1;
        if (m & KEY_DOWN) return 1;
        if (m & KEY_LEFT) return 1;
        if (m & KEY_RIGHT) return 1;
        if (m & KEY_START) return 1;
        return 0;
    }
    if (m & KEY_A) {
        if (m & KEY_UP) n++;
        if (m & KEY_DOWN) n++;
        if (m & KEY_LEFT) n++;
        if (m & KEY_RIGHT) n++;
        return n;
    }
    if (m & KEY_L1) {
        if (m & KEY_UP) n++;
        if (m & KEY_DOWN) n++;
        if (m & KEY_LEFT) n++;
        if (m & KEY_RIGHT) n++;
        return n;
    }
    if (m & KEY_START) n++;
    if (m & KEY_LEFT) n++;
    if (m & KEY_RIGHT) n++;
    if (m & KEY_UP) n++;
    if (m & KEY_DOWN) n++;
    return n;
}

static const char *name(ActionId a) {
    switch (a) {
        case ACTION_NONE: return "NONE";
        case ACTION_CYCLE_FX_PAGE: return "CYCLE_FX_PAGE";
        case ACTION_TOGGLE_MUTE: return "TOGGLE_MUTE";
        case ACTION_TOGGLE_SOLO: return "TOGGLE_SOLO";
        case ACTION_SWITCH_VIEW_SONG: return "SWITCH_VIEW_SONG";
        case ACTION_STOP: return "STOP";
        case ACTION_OPEN_INSTRUMENT_FX: return "OPEN_INSTRUMENT_FX";
        case ACTION_CYCLE_FX_EDIT_TARGET: return "CYCLE_FX_EDIT_TARGET";
        case ACTION_RESET_PAN: return "RESET_PAN";
        case ACTION_PAN_NUDGE_LEFT_COARSE: return "PAN_NUDGE_LEFT_COARSE";
        case ACTION_PAN_NUDGE_RIGHT_COARSE: return "PAN_NUDGE_RIGHT_COARSE";
        case ACTION_PAN_NUDGE_LEFT: return "PAN_NUDGE_LEFT";
        case ACTION_PAN_NUDGE_RIGHT: return "PAN_NUDGE_RIGHT";
        case ACTION_OPEN_MENU: return "OPEN_MENU";
        case ACTION_RESET_PARAMETER: return "RESET_PARAMETER";
        case ACTION_EDIT_PARAM_UP: return "EDIT_PARAM_UP";
        case ACTION_EDIT_PARAM_DOWN: return "EDIT_PARAM_DOWN";
        case ACTION_EDIT_PARAM_LEFT: return "EDIT_PARAM_LEFT";
        case ACTION_EDIT_PARAM_RIGHT: return "EDIT_PARAM_RIGHT";
        case ACTION_ROW_UP: return "ROW_UP";
        case ACTION_ROW_DOWN: return "ROW_DOWN";
        case ACTION_PLAY_STOP: return "PLAY_STOP";
        case ACTION_VOLUME_COARSE_UP: return "VOLUME_COARSE_UP";
        case ACTION_VOLUME_COARSE_DOWN: return "VOLUME_COARSE_DOWN";
        case ACTION_VOLUME_FINE_DECREASE: return "VOLUME_FINE_DECREASE";
        case ACTION_VOLUME_FINE_INCREASE: return "VOLUME_FINE_INCREASE";
        case ACTION_VOLUME_FINE_UP: return "VOLUME_FINE_UP";
        case ACTION_VOLUME_FINE_DOWN: return "VOLUME_FINE_DOWN";
        case ACTION_MIX_CURSOR_LEFT: return "MIX_CURSOR_LEFT";
        case ACTION_MIX_CURSOR_RIGHT: return "MIX_CURSOR_RIGHT";
        default: return "?";
    }
}

static const char *ctx_name(bool fx) { return fx ? "FX" : "MIX"; }

int main() {
    long total = 0, mismatches = 0, multifire = 0, none_both = 0;
    long maingate = 0, fxgate = 0;
    for (int fx = 0; fx < 2; fx++) {
        for (unsigned m = 0; m <= (unsigned)KEY_ANY; m++) {
            ContextId ctx = fx ? CTX_MIXER_FX : CTX_MIXER;
            ActionId expected = golden_first(fx != 0, m);
            ActionId got = ChordResolver_Resolve((PadMask)m, ctx);
            total++;
            if (got != expected) {
                if (mismatches < 40)
                    printf("MISMATCH ctx=%s mask=0x%04x "
                           "golden=%s(%d) resolver=%s\n",
                           ctx_name(fx), m,
                           name(expected), expected,
                           name(got));
                mismatches++;
            }
            int cnt = golden_action_count(fx != 0, m);
            if (cnt > 1) multifire++;
            if (cnt == 0 && got == ACTION_NONE) none_both++;

            /* masterSelected_ (solo afecta al bloque L2: MixerView.cpp:594
             * aborta el bloque entero). Solo importa cuando la mascara
             * llega al bloque L2, i.e. sin SELECT/R1/R2 (esos retornan
             * antes). Con gate el golden daria NONE: el dispatch debe
             * suprimir TODAS las acciones PAN de esas mascaras. */
            if ((m & KEY_L2) && !(m & (KEY_SELECT | KEY_R1 | KEY_R2))) {
                if (expected != ACTION_NONE) {
                    if (fx) fxgate++; else maingate++;
                    if (!(expected == ACTION_PAN_NUDGE_LEFT ||
                          expected == ACTION_PAN_NUDGE_RIGHT ||
                          expected == ACTION_PAN_NUDGE_LEFT_COARSE ||
                          expected == ACTION_PAN_NUDGE_RIGHT_COARSE ||
                          expected == ACTION_RESET_PAN)) {
                        if (mismatches < 40)
                            printf("MASTER-GATE-NOT-PAN ctx=%s mask=0x%04x "
                                   "golden=%s\n", ctx_name(fx), m,
                                   name(expected));
                        mismatches++;
                    }
                }
            }
        }
    }
    printf("MIXER_GOLDEN_ENUM total=%ld mismatches=%ld "
           "multifire_masks=%ld none_agreed=%ld "
           "master_gated_mix=%ld master_gated_fx=%ld\n",
           total, mismatches, multifire, none_both, maingate, fxgate);
    return mismatches ? 1 : 0;
}
