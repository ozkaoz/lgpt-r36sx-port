// F8: ScenarioCatalog - base de datos de escenarios funcionales por vista.
//
// Un escenario es una secuencia de input EPBM_* que la vista golden
// convierte en UNA O MAS acciones semanticas (ActionId).  La tabla
// complementa al ActionMap (F1): este define los bindings; aqui se
// cataloga el COMPORTAMIENTO observable por vista, incluyendo:
//   - escenarios multi-fire (el golden dispara varias acciones bajo una
//     misma prensa; la primera la entrega el resolver, la COLAs se
//     documentan y el runner las verifica contra la tabla de bindings),
//   - requisitos estables del port (B/A en pitch, R1+A en trim, ...).
//
// Cada fila transcribe una rama del codigo dorado (Bacon 1.2.1).  El
// runner host (tests/host/scenario_runner_host_test.cpp) inyecta cada
// mascara en ChordResolver_Resolve y compara contra el esperado; las
// invariantes (determinismo, unicidad dentro del contexto, coherencia de
// colas) se comprueban sobre el ActionMap real, no duplicado.
#ifndef UI_INPUT_SCENARIO_CATALOG_H_
#define UI_INPUT_SCENARIO_CATALOG_H_

#include "ActionId.h"
#include "ChordResolver.h"

namespace UI {
namespace Input {

struct Scenario {
    const char *view;     // vista donde ocurre el escenario
    ContextId ctx;        // contexto de resolucion
    PadMask mask;         // mascara EPBM_* inyectada
    ActionId expected;    // primera accion (la del resolver)
    ActionId queued;      // segunda accion de la cola (ACTION_NONE si no)
    const char *doc;      // rama del golden que transcribe
};

static const Scenario kScenarios[] = {

    /* ---------------- CTX_GLOBAL (AppWindow.cpp) ---------------- */
    { "AppWindow", CTX_GLOBAL,
      KEY_SELECT | KEY_R1, ACTION_OPEN_HELP, ACTION_NONE,
      "AppWindow.cpp:638-660, SELECT+R1 latched help" },
    { "AppWindow", CTX_GLOBAL,
      KEY_SELECT | KEY_R2, ACTION_OPEN_AUDIO_DRIVER, ACTION_NONE,
      "AppWindow.cpp:630/701, SELECT+R2 latched audio driver" },
    { "AppWindow", CTX_GLOBAL,
      KEY_L1 | KEY_X, ACTION_UNDO, ACTION_NONE,
      "AppWindow.cpp:739-760, L1+X undo (V16 fresh-shoulder)" },
    { "AppWindow", CTX_GLOBAL,
      KEY_R1 | KEY_X, ACTION_REDO, ACTION_NONE,
      "AppWindow.cpp:739-760, R1+X redo (V16 fresh-shoulder)" },

    /* --------- CTX_MIXER (MixerView.cpp, pagina MIX) ------------ */
    { "MixerView(MIX)", CTX_MIXER,
      KEY_SELECT, ACTION_CYCLE_FX_PAGE, ACTION_NONE,
      "MixerView.cpp:540, SELECT cycleFxPage" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_R1 | KEY_B, ACTION_TOGGLE_MUTE, ACTION_NONE,
      "MixerView.cpp:550-552, R1+B toggleMute (B gana)" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_R1 | KEY_A, ACTION_TOGGLE_SOLO, ACTION_NONE,
      "MixerView.cpp:554-556, R1+A switchSoloMode" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_R1 | KEY_UP, ACTION_SWITCH_VIEW_SONG, ACTION_NONE,
      "MixerView.cpp:558-563, R1+UP VT_SONG (START en la secuencia = onStop)" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_R1 | KEY_START, ACTION_STOP, ACTION_NONE,
      "MixerView.cpp:564-566, R1+START onStop" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_R2 | KEY_A, ACTION_OPEN_INSTRUMENT_FX, ACTION_NONE,
      "MixerView.cpp:573-576, R2+A showInstrumentFxMenu" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_R2, ACTION_CYCLE_FX_EDIT_TARGET, ACTION_NONE,
      "MixerView.cpp:579-583, R2 puro en FX_PAGE_MIX (VOL->DLY->RVB)" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_L2 | KEY_A | KEY_B, ACTION_RESET_PAN, ACTION_NONE,
      "MixerView.cpp:595-601, L2+A+B puro -> pan 0" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_L1 | KEY_A, ACTION_OPEN_MENU, ACTION_NONE,
      "MixerView.cpp:635-641, L1+A puro MixerActionMenuModal" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_A | KEY_UP, ACTION_VOLUME_COARSE_UP, ACTION_VOLUME_COARSE_DOWN,
      "MixerView.cpp:672-677, A+UP -> vol 10; cola multi-fire D/L/R" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_L1 | KEY_UP, ACTION_VOLUME_COARSE_UP, ACTION_VOLUME_COARSE_DOWN,
      "MixerView.cpp:681-686, L1+UP -> vol 10; cola D/L/R" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_START, ACTION_PLAY_STOP, ACTION_MIX_CURSOR_LEFT,
      "MixerView.cpp:690-696, START onStart; cola flechas L,R,U,D" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_LEFT, ACTION_MIX_CURSOR_LEFT, ACTION_MIX_CURSOR_RIGHT,
      "MixerView.cpp:693-696, LEFT cursor; cola R,U,D" },
    { "MixerView(MIX)", CTX_MIXER,
      KEY_UP, ACTION_VOLUME_FINE_UP, ACTION_VOLUME_FINE_DOWN,
      "MixerView.cpp:695-696, UP vol+1; cola D" },

    /* --- CTX_MIXER_FX (MixerView.cpp, paginas DELAY/REVERB/EQ/COMP) --- */
    { "MixerView(FX)", CTX_MIXER_FX,
      KEY_SELECT, ACTION_CYCLE_FX_PAGE, ACTION_NONE,
      "MixerView.cpp:540, SELECT cycleFxPage (todas las paginas)" },
    { "MixerView(FX)", CTX_MIXER_FX,
      KEY_A | KEY_B, ACTION_RESET_PARAMETER, ACTION_NONE,
      "MixerView.cpp:648-653, A+B puro fxResetRow" },
    { "MixerView(FX)", CTX_MIXER_FX,
      KEY_A | KEY_UP, ACTION_EDIT_PARAM_UP, ACTION_EDIT_PARAM_DOWN,
      "MixerView.cpp:655-660, A+UP fxEditRow(1); cola D/L/R" },
    { "MixerView(FX)", CTX_MIXER_FX,
      KEY_UP, ACTION_ROW_UP, ACTION_NONE,
      "MixerView.cpp:662-665, UP fxMoveRow (single-fire, UP gana)" },
    { "MixerView(FX)", CTX_MIXER_FX,
      KEY_START, ACTION_PLAY_STOP, ACTION_NONE,
      "MixerView.cpp:671, START onStart (sin flechas en FX)" },

    /* ------------- CTX_CHOPPER (SampleChopperModal, main) ------------- */
    { "Chopper", CTX_CHOPPER,
      KEY_L1 | KEY_X, ACTION_UNDO, ACTION_NONE,
      "SampleChopperModal.cpp:3305-3312, L1+X undo puro" },
    { "Chopper", CTX_CHOPPER,
      KEY_L1 | KEY_R1, ACTION_TOGGLE_PITCH_MODE, ACTION_NONE,
      "SampleChopperModal.cpp:3317-3318, L1+R1 puro pitch" },
    { "Chopper", CTX_CHOPPER,
      KEY_L1 | KEY_B, ACTION_SPLIT_PARTS, ACTION_NONE,
      "SampleChopperModal.cpp:3325-3329, L1+B puro cycleSplitParts" },
    { "Chopper", CTX_CHOPPER,
      KEY_R1 | KEY_B, ACTION_CLOSE, ACTION_NONE,
      "SampleChopperModal.cpp:3382-3384, R1+B EndModal(0)" },
    { "Chopper", CTX_CHOPPER,
      KEY_L2 | KEY_B, ACTION_STOP_PREVIEW, ACTION_NONE,
      "SampleChopperModal.cpp:3386-3388, L2+B stopPreview" },
    { "Chopper", CTX_CHOPPER,
      KEY_SELECT, ACTION_TOGGLE_TRIM_MODE, ACTION_NONE,
      "SampleChopperModal.cpp:3390-3391, SELECT puro trim" },
    { "Chopper", CTX_CHOPPER,
      KEY_R1 | KEY_A, ACTION_AUTOSAVE_CHOPS, ACTION_NONE,
      "SampleChopperModal.cpp:3434-3436, R1+A saveChopState" },
    { "Chopper", CTX_CHOPPER,
      KEY_R2 | KEY_A, ACTION_PLAY_FULL, ACTION_NONE,
      "SampleChopperModal.cpp:3439, R2+A playFullSample" },
    { "Chopper", CTX_CHOPPER,
      KEY_Y, ACTION_DELETE_CHOP, ACTION_NONE,
      "SampleChopperModal.cpp:3444, Y puro deleteSelectedChop" },
    { "Chopper", CTX_CHOPPER,
      KEY_B, ACTION_PLAY_CHOP_PREVIEW, ACTION_NONE,
      "SampleChopperModal.cpp:3445, B puro playSelectedChop" },
    { "Chopper", CTX_CHOPPER,
      KEY_A, ACTION_ADD_CHOP, ACTION_NONE,
      "SampleChopperModal.cpp:3446, A puro addChopAtCursor" },
    { "Chopper", CTX_CHOPPER,
      KEY_R1 | KEY_RIGHT, ACTION_SELECT_NEXT_SAMPLE, ACTION_NONE,
      "SampleChopperModal.cpp:3437-3438, R1+RIGHT selectSample(1)" },
    { "Chopper", CTX_CHOPPER,
      KEY_R2 | KEY_RIGHT, ACTION_SELECT_NEXT_CHOP, ACTION_NONE,
      "SampleChopperModal.cpp:3440-3441, R2+RIGHT selectChop(1)" },

    /* ------------- CTX_CHOPPER_TRIM (trim mode) ------------- */
    { "Chopper(trim)", CTX_CHOPPER_TRIM,
      KEY_L1 | KEY_B, ACTION_SNAP_BOUNDARY_END, ACTION_NONE,
      "SampleChopperModal.cpp:3325-3329, trim L1+B snap fin" },
    { "Chopper(trim)", CTX_CHOPPER_TRIM,
      KEY_L1 | KEY_A, ACTION_SNAP_BOUNDARY_START, ACTION_NONE,
      "SampleChopperModal.cpp:3331-3332, trim L1+A snap inicio" },
    { "Chopper(trim)", CTX_CHOPPER_TRIM,
      KEY_R2 | KEY_Y, ACTION_NORMALIZE, ACTION_NONE,
      "SampleChopperModal.cpp:3337-3338, trim R2+Y normalize" },
    { "Chopper(trim)", CTX_CHOPPER_TRIM,
      KEY_R1 | KEY_A, ACTION_CROP, ACTION_NONE,
      "SampleChopperModal.cpp:3394-3396, trim R1+A destructiveCrop" },
    { "Chopper(trim)", CTX_CHOPPER_TRIM,
      KEY_L2 | KEY_Y, ACTION_DELETE_RANGE, ACTION_NONE,
      "SampleChopperModal.cpp:3398-3400, trim L2+Y deleteRange" },
    { "Chopper(trim)", CTX_CHOPPER_TRIM,
      KEY_Y, ACTION_TRIM_PREVIEW_START, ACTION_NONE,
      "SampleChopperModal.cpp:3408-3410, trim Y preview inicio" },
    { "Chopper(trim)", CTX_CHOPPER_TRIM,
      KEY_X, ACTION_TRIM_PREVIEW_END, ACTION_NONE,
      "SampleChopperModal.cpp:3412-3414, trim X preview fin" },
    { "Chopper(trim)", CTX_CHOPPER_TRIM,
      KEY_A | KEY_LEFT, ACTION_NUDGE_START_LEFT, ACTION_NONE,
      "SampleChopperModal.cpp:3402-3406, trim A+LEFT nudge inicio" },
    { "Chopper(trim)", CTX_CHOPPER_TRIM,
      KEY_B | KEY_RIGHT, ACTION_NUDGE_END_RIGHT, ACTION_NONE,
      "SampleChopperModal.cpp:3402-3406, trim B+RIGHT nudge fin" },

    /* ------------- CTX_CHOPPER_PITCH (pitch mode) ------------- */
    { "Chopper(pitch)", CTX_CHOPPER_PITCH,
      KEY_DOWN, ACTION_PITCH_PARAM_NEXT, ACTION_NONE,
      "SampleChopperModal.cpp:3357-3360, pitch DOWN param next" },
    { "Chopper(pitch)", CTX_CHOPPER_PITCH,
      KEY_UP, ACTION_PITCH_PARAM_PREV, ACTION_NONE,
      "SampleChopperModal.cpp:3357-3360, pitch UP param prev" },
    { "Chopper(pitch)", CTX_CHOPPER_PITCH,
      KEY_RIGHT, ACTION_PITCH_VALUE_UP, ACTION_NONE,
      "SampleChopperModal.cpp:3362-3365, pitch RIGHT value +1" },
    { "Chopper(pitch)", CTX_CHOPPER_PITCH,
      KEY_LEFT, ACTION_PITCH_VALUE_DOWN, ACTION_NONE,
      "SampleChopperModal.cpp:3362-3365, pitch LEFT value -1" },
    { "Chopper(pitch)", CTX_CHOPPER_PITCH,
      KEY_B, ACTION_PITCH_PREVIEW, ACTION_NONE,
      "SampleChopperModal.cpp:3367-3369, pitch B puro preview (REQUISITO 2)" },
    { "Chopper(pitch)", CTX_CHOPPER_PITCH,
      KEY_A, ACTION_PITCH_APPLY, ACTION_NONE,
      "SampleChopperModal.cpp:3371-3373, pitch A puro apply (REQUISITO 2)" },
    { "Chopper(pitch)", CTX_CHOPPER_PITCH,
      KEY_R2 | KEY_RIGHT, ACTION_PITCH_SCOPE_NEXT, ACTION_NONE,
      "SampleChopperModal.cpp:3347-3354, pitch R2+RIGHT scope next" },
    { "Chopper(pitch)", CTX_CHOPPER_PITCH,
      KEY_L1 | KEY_R1, ACTION_TOGGLE_PITCH_MODE, ACTION_NONE,
      "SampleChopperModal.cpp:3317-3318, pitch L1+R1 exit" },

    /* Escenarios de negacion: acordes que el golden NO resuelve (los
     * comentarios del ActionMap lo documentan explicitamente). */
    { "MixerView(FX)", CTX_MIXER_FX,
      KEY_R2, ACTION_NONE, ACTION_NONE,
      "MixerView.cpp:579-583, R2 puro en paginas FX = sin accion (exige MIX)" },
    { "Chopper(pitch)", CTX_CHOPPER_PITCH,
      KEY_SELECT, ACTION_NONE, ACTION_NONE,
      "SampleChopperModal.cpp pitch, SELECT puro sin accion (status)" },
    { "Chopper(pitch)", CTX_CHOPPER_PITCH,
      KEY_R2 | KEY_A, ACTION_NONE, ACTION_NONE,
      "SampleChopperModal.cpp pitch, R2+A puro sin accion (status)" },
};

static int ScenarioCatalogCount() {
    return (int)(sizeof(kScenarios) / sizeof(kScenarios[0]));
}

static const Scenario *ScenarioCatalogAt(int i) {
    if (i < 0 || i >= ScenarioCatalogCount()) return 0;
    return &kScenarios[i];
}

}  /* namespace Input */
}  /* namespace UI */

#endif  /* UI_INPUT_SCENARIO_CATALOG_H_ */