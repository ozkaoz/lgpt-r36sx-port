/*
 * ActionMap.cpp -- CATALOGO DORADO de bindings del port (Bacon 1.2.1).
 *
 * F1 de REFACTOR_ROADMAP_ES.md. UNICA fuente de verdad de la politica de
 * input: cada Binding transcribe literalmente la rama del codigo dorado que
 * lo ejecuta (require/forbid = las condiciones de la rama). La accion
 * semantica se separa del boton fisico; el adapter F1b conectara las vistas
 * a esta tabla sin cambiar ninguna mascara ni prioridad.
 *
 * Convenciones de transcripcion:
 *  - El orden dentro de cada contexto es el orden de evaluacion del codigo.
 *  - "puro" en una nota = acorde exclusivo (forbid = el resto de teclas).
 *  - Los bloques que RETORNAN antes (R2, L2) o los branches que solo pintan
 *    texto de estado se reflejan en las forbids de las ramas posteriores
 *    (para que el resolver no dispare algo que el codigo nunca alcanzaria)
 *    o se anotan sin accion.
 *  - Casos "multi-disparo" del golden (START+flecha, flechas diagonales)
 *    devuelven UNA accion aqui (la primera en el orden del codigo); la
 *    entrega de la segunda se documenta para el adapter F1b/F8.
 *  - L1 = EPBM_L, R1 = EPBM_R (convencion del port, PhysicalInput.h).
 */
#include "ActionMap.h"

namespace UI {
namespace Input {

/* ------------------------------------------------------------------ */
/* CTX_GLOBAL -- AppWindow.cpp onEvent + SynchronizeInputMask.        */
/* ------------------------------------------------------------------ */
static const Binding kGlobalBindings[] = {

    /* AppWindow.cpp (SynchronizeInputMask/onEvent, ~line 630): helpCombo
     * SELECT+R1 abre el Help contextual del view en foco (latched: se
     * mantiene mientras el acorde siga pulsado; el latch lo gestiona el
     * adapter F1b, aqui solo importa el acorde). Sin forbid: el codigo
     * dorado no comprueba otras teclas. */
    BIND(ACTION_OPEN_HELP,
         KEY_SELECT | KEY_R1, 0,
         "AppWindow.cpp:638-660, SELECT+R1 latched"),

    /* audioCombo SELECT+R2 -> dialogo Audio Driver (latched). */
    BIND(ACTION_OPEN_AUDIO_DRIVER,
         KEY_SELECT | KEY_R2, 0,
         "AppWindow.cpp:630/701, SELECT+R2 latched"),

    /* L1+X undo global / R1+X redo global. La desambiguacion del acorde
     * L1+R1+X la resuelve AppWindow con el ultimo hombro pulsado (V16):
     * necesita historia, asi que la tabla NO lo atiende (resuelve NONE) y
     * el adapter aplica la regla V16 ANTES de consultar la tabla. */
    BIND(ACTION_UNDO,
         KEY_L1 | KEY_X, KEY_R1,
         "AppWindow.cpp:739-760, L1+X (V16 fresh-shoulder rules)"),
    BIND(ACTION_REDO,
         KEY_R1 | KEY_X, KEY_L1,
         "AppWindow.cpp:739-760, R1+X (V16 fresh-shoulder rules)"),
};

/* ------------------------------------------------------------------ */
/* CTX_MIXER -- MixerView.cpp ProcessButtonMask, pagina MIX.          */
/* ------------------------------------------------------------------ */
static const Binding kMixerBindings[] = {

    /* R2: bloque que retorna; el unico R2 puro con accion es el ciclo de
     * objetivo de edicion en la pagina MIX. R2+B/L1/L2+... tambien ciclan
     * (el codigo no comprueba nada mas que la ausencia de A). */
    BIND(ACTION_OPEN_INSTRUMENT_FX,
         KEY_R2 | KEY_A, 0,
         "MixerView.cpp:578-582, R2+A showInstrumentFxMenu"),
    BIND(ACTION_CYCLE_FX_EDIT_TARGET,
         KEY_R2, KEY_A,
         "MixerView.cpp:585-590, R2 sin A en FX_PAGE_MIX (VOL->DLY->RVB)"),

    /* L2: bloque que retorna. L2+A+B puro resetea pan; L2(+A)+L/R nudgea
     * pan; cualquier otro L2 sin L/R -> nada. */
    BIND(ACTION_RESET_PAN,
         KEY_L2 | KEY_A | KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_L1 | KEY_R1 |
         KEY_X | KEY_Y | KEY_SELECT | KEY_START,
         "MixerView.cpp:589-601, L2+A+B pure chord -> pan 0"),

    BIND(ACTION_PAN_NUDGE_LEFT_COARSE,
         KEY_L2 | KEY_A | KEY_LEFT, KEY_RIGHT,
         "MixerView.cpp:604-616, L2+A+LEFT step 10"),
    BIND(ACTION_PAN_NUDGE_RIGHT_COARSE,
         KEY_L2 | KEY_A | KEY_RIGHT, KEY_LEFT,
         "MixerView.cpp:604-616, L2+A+RIGHT step 10"),
    BIND(ACTION_PAN_NUDGE_LEFT,
         KEY_L2 | KEY_LEFT, KEY_A | KEY_RIGHT,
         "MixerView.cpp:604-616, L2+LEFT step 1"),
    BIND(ACTION_PAN_NUDGE_RIGHT,
         KEY_L2 | KEY_RIGHT, KEY_A | KEY_LEFT,
         "MixerView.cpp:604-616, L2+RIGHT step 1"),

    /* L1+A puro: menu de accion (master/limiter o canal). */
    BIND(ACTION_OPEN_MENU,
         KEY_L1 | KEY_A,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_R1 | KEY_L2 |
         KEY_R2 | KEY_X | KEY_Y | KEY_SELECT | KEY_START,
         "MixerView.cpp:630-641, L1+A pure chord MixerActionMenuModal"),

    /* Bloque A (edicion gruesa/fina). El bloque R2/L2 retorna antes, asi
     * que A+... con R2/L2 nunca llega aqui: se prohiben en estas bindings.
     * B/X/Y/SELECT/START NO se prohiben (el codigo no los comprueba). */
    BIND(ACTION_VOLUME_COARSE_UP,
         KEY_A | KEY_UP,
         KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_L2 | KEY_R2,
         "MixerView.cpp:670-674, A+UP updateVolume(10)"),
    BIND(ACTION_VOLUME_COARSE_DOWN,
         KEY_A | KEY_DOWN,
         KEY_UP | KEY_LEFT | KEY_RIGHT | KEY_L2 | KEY_R2,
         "MixerView.cpp:670-674, A+DOWN updateVolume(-10)"),
    BIND(ACTION_VOLUME_FINE_DECREASE,
         KEY_A | KEY_LEFT,
         KEY_UP | KEY_DOWN | KEY_RIGHT | KEY_L2 | KEY_R2,
         "MixerView.cpp:675-678, A+LEFT updateVolume(-1)"),
    BIND(ACTION_VOLUME_FINE_INCREASE,
         KEY_A | KEY_RIGHT,
         KEY_UP | KEY_DOWN | KEY_LEFT | KEY_L2 | KEY_R2,
         "MixerView.cpp:675-678, A+RIGHT updateVolume(1)"),

    /* Bloque L1 (camino grueso secundario; requiere A ausente: el bloque A
     * retorna antes). */
    BIND(ACTION_VOLUME_COARSE_UP,
         KEY_L1 | KEY_UP,
         KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_L2 | KEY_R2,
         "MixerView.cpp:681-686, L1+UP updateVolume(10)"),
    BIND(ACTION_VOLUME_COARSE_DOWN,
         KEY_L1 | KEY_DOWN,
         KEY_UP | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_L2 | KEY_R2,
         "MixerView.cpp:681-686, L1+DOWN updateVolume(-10)"),
    BIND(ACTION_MIX_CURSOR_LEFT,
         KEY_L1 | KEY_LEFT, KEY_RIGHT | KEY_A | KEY_L2 | KEY_R2,
         "MixerView.cpp:687-689, L1+LEFT updateCursor(-1,0)"),
    BIND(ACTION_MIX_CURSOR_RIGHT,
         KEY_L1 | KEY_RIGHT, KEY_LEFT | KEY_A | KEY_L2 | KEY_R2,
         "MixerView.cpp:687-689, L1+RIGHT updateCursor(1,0)"),

    /* Sin modificador. START+flecha dispara las DOS cosas en el golden;
     * aqui gana PLAY_STOP y la segunda (nudge de volumen/cursor) queda
     * documentada para el adapter F1b (entrega secuencial). */
    BIND(ACTION_PLAY_STOP,
         KEY_START, 0,
         "MixerView.cpp:691, START onStart() (START+flecha: dual-fire)"),

    BIND(ACTION_MIX_CURSOR_LEFT,
         KEY_LEFT,
         KEY_RIGHT | KEY_L1 | KEY_L2 | KEY_R2 | KEY_A | KEY_START,
         "MixerView.cpp:692, LEFT updateCursor(-1,0)"),
    BIND(ACTION_MIX_CURSOR_RIGHT,
         KEY_RIGHT,
         KEY_LEFT | KEY_L1 | KEY_L2 | KEY_R2 | KEY_A | KEY_START,
         "MixerView.cpp:693, RIGHT updateCursor(1,0)"),
    BIND(ACTION_VOLUME_FINE_UP,
         KEY_UP,
         KEY_DOWN | KEY_L1 | KEY_L2 | KEY_R2 | KEY_A | KEY_START,
         "MixerView.cpp:694, UP updateVolume(1)"),
    BIND(ACTION_VOLUME_FINE_DOWN,
         KEY_DOWN,
         KEY_UP | KEY_L1 | KEY_L2 | KEY_R2 | KEY_A | KEY_START,
         "MixerView.cpp:695, DOWN updateVolume(-1)"),
};

/* ------------------------------------------------------------------ */
/* CTX_MIXER_FX -- paginas DELAY/REVERB/EQ/COMP.                      */
/* ------------------------------------------------------------------ */
static const Binding kMixerFxBindings[] = {

    /* R2+A abre el menu FX del instrumento en cualquier pagina. R2 puro en
     * paginas FX no tiene accion (el ciclo exige pagina MIX) y la rama
     * consume el boton. */
    BIND(ACTION_OPEN_INSTRUMENT_FX,
         KEY_R2 | KEY_A, 0,
         "MixerView.cpp:578-582, R2+A showInstrumentFxMenu"),
    /* nota: R2 solo en FX pages -> NONE (return del bloque R2, sin ciclo). */

    /* A+B puro: restaura el parametro en foco al default legacy. */
    BIND(ACTION_RESET_PARAMETER,
         KEY_A | KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_L1 | KEY_R1 |
         KEY_L2 | KEY_R2 | KEY_X | KEY_Y | KEY_SELECT | KEY_START,
         "MixerView.cpp:649-658, A+B pure chord fxResetRow"),

    /* A+flechas editan el parametro (sin comprobaciones extra del codigo;
     * R2/L2 retornan antes y se prohiben). */
    BIND(ACTION_EDIT_PARAM_UP,
         KEY_A | KEY_UP, KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_L2 | KEY_R2,
         "MixerView.cpp:659-662, A+UP fxEditRow(1,true)"),
    BIND(ACTION_EDIT_PARAM_DOWN,
         KEY_A | KEY_DOWN, KEY_UP | KEY_LEFT | KEY_RIGHT | KEY_L2 | KEY_R2,
         "MixerView.cpp:659-662, A+DOWN fxEditRow(-1,true)"),
    BIND(ACTION_EDIT_PARAM_LEFT,
         KEY_A | KEY_LEFT, KEY_UP | KEY_DOWN | KEY_RIGHT | KEY_L2 | KEY_R2,
         "MixerView.cpp:663-665, A+LEFT fxEditRow(-1,false)"),
    BIND(ACTION_EDIT_PARAM_RIGHT,
         KEY_A | KEY_RIGHT, KEY_UP | KEY_DOWN | KEY_LEFT | KEY_L2 | KEY_R2,
         "MixerView.cpp:663-665, A+RIGHT fxEditRow(1,false)"),

    /* Flechas solas: UP/DOWN mueven fila, LEFT/RIGHT editan. */
    BIND(ACTION_ROW_UP,
         KEY_UP, KEY_DOWN | KEY_A | KEY_L2 | KEY_R2 | KEY_START,
         "MixerView.cpp:666-668, UP fxMoveRow(-1); START+flecha dual-fire"),
    BIND(ACTION_ROW_DOWN,
         KEY_DOWN, KEY_UP | KEY_A | KEY_L2 | KEY_R2 | KEY_START,
         "MixerView.cpp:666-668, DOWN fxMoveRow(1)"),
    BIND(ACTION_EDIT_PARAM_LEFT,
         KEY_LEFT, KEY_RIGHT | KEY_A | KEY_L2 | KEY_R2 | KEY_START,
         "MixerView.cpp:669, LEFT fxEditRow(-1,false)"),
    BIND(ACTION_EDIT_PARAM_RIGHT,
         KEY_RIGHT, KEY_LEFT | KEY_A | KEY_L2 | KEY_R2 | KEY_START,
         "MixerView.cpp:670, RIGHT fxEditRow(1,false)"),

    BIND(ACTION_PLAY_STOP,
         KEY_START, 0,
         "MixerView.cpp:671, START onStart()"),
};

/* ------------------------------------------------------------------ */
/* CTX_CHOPPER -- SampleChopperModal::ProcessButtonMask (main).       */
/* Los numeros de linea refieren a SampleChopperModal.cpp (3290-3460). */
/* ------------------------------------------------------------------ */
static const Binding kChopperBindings[] = {

    /* Undo/redo globales, tambien dentro del chopper (incl. overlay de
     * operacion completada: primero los resuelve, luego el normal). */
    BIND(ACTION_UNDO,
         KEY_L1 | KEY_X,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_Y |
         KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3305-3312, L1+X pure undo (overlay complete + normal)"),
    BIND(ACTION_REDO,
         KEY_R1 | KEY_X,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_Y |
         KEY_L1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3305-3312, R1+X pure redo (overlay complete + normal)"),

    /* L1+R1 puro: entra/sale del modo Pitch. */
    BIND(ACTION_TOGGLE_PITCH_MODE,
         KEY_L1 | KEY_R1,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X |
         KEY_Y | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3317-3318, L1+R1 pure togglePitchMode"),

    /* L1+B puro: en main cicla el split 4/8/16/32; en trim, snap del fin. */
    BIND(ACTION_SPLIT_PARTS,
         KEY_L1 | KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_X | KEY_Y |
         KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3325-3329, L1+B pure cycleSplitParts"),

    /* R1+B: cierra el modal (EndModal) y para el preview. */
    BIND(ACTION_CLOSE,
         KEY_R1 | KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_X | KEY_Y,
         "SampleChopperModal.cpp:3382-3384, R1+B EndModal(0)"),

    /* L2+B: para la reproduccion. */
    BIND(ACTION_STOP_PREVIEW,
         KEY_L2 | KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_X | KEY_Y,
         "SampleChopperModal.cpp:3386-3388, L2+B stopSamplePreview"),

    /* SELECT puro: alterna trim mode. */
    BIND(ACTION_TOGGLE_TRIM_MODE,
         KEY_SELECT,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X |
         KEY_Y | KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2,
         "SampleChopperModal.cpp:3390-3391, SELECT pure toggleTrimMode"),

    /* R1+A fuera de trim: auto-save + status. (En trim es CROP, ver
     * CTX_CHOPPER_TRIM.) */
    BIND(ACTION_AUTOSAVE_CHOPS,
         KEY_R1 | KEY_A, 0,
         "SampleChopperModal.cpp:3434-3436, R1+A saveChopStateForCurrentSample"),

    /* R1+L/R: selecciona el sample anterior/siguiente de la lista. */
    BIND(ACTION_SELECT_PREV_SAMPLE,
         KEY_R1 | KEY_LEFT, KEY_RIGHT,
         "SampleChopperModal.cpp:3437-3438, R1+LEFT selectSample(-1)"),
    BIND(ACTION_SELECT_NEXT_SAMPLE,
         KEY_R1 | KEY_RIGHT, KEY_LEFT,
         "SampleChopperModal.cpp:3437-3438, R1+RIGHT selectSample(1)"),

    /* R2+A: reproduce el sample completo desde el inicio. */
    BIND(ACTION_PLAY_FULL,
         KEY_R2 | KEY_A, 0,
         "SampleChopperModal.cpp:3439, R2+A playFullSample"),

    /* R2+L/R: selecciona chop anterior/siguiente. */
    BIND(ACTION_SELECT_PREV_CHOP,
         KEY_R2 | KEY_LEFT, KEY_RIGHT | KEY_A,
         "SampleChopperModal.cpp:3440-3441, R2+LEFT selectChop(-1)"),
    BIND(ACTION_SELECT_NEXT_CHOP,
         KEY_R2 | KEY_RIGHT, KEY_LEFT | KEY_A,
         "SampleChopperModal.cpp:3440-3441, R2+RIGHT selectChop(1)"),

    /* Edicion de chops (main): Y borra, B preview, A anade (orden Y,B,A). */
    BIND(ACTION_DELETE_CHOP,
         KEY_Y, KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2,
         "SampleChopperModal.cpp:3444, Y pure deleteSelectedChop"),

    BIND(ACTION_PLAY_CHOP_PREVIEW,
         KEY_B, KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2,
         "SampleChopperModal.cpp:3445, B pure playSelectedChop"),

    BIND(ACTION_ADD_CHOP,
         KEY_A, KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2,
         "SampleChopperModal.cpp:3446, A pure addChopAtCursor"),

    /* Cursor y zoom, con L1 grueso (misma rama, L1 multiplica). Flechas
     * diagonales: el golden dispara cursor Y zoom (dual-fire); aqui gana
     * el cursor (orden del codigo). */
    BIND(ACTION_NUDGE_CURSOR_LEFT_COARSE,
         KEY_L1 | KEY_LEFT, KEY_RIGHT,
         "SampleChopperModal.cpp:3448-3450, L1+LEFT deltaPx 24 (diagonal dual-fire)"),
    BIND(ACTION_NUDGE_CURSOR_RIGHT_COARSE,
         KEY_L1 | KEY_RIGHT, KEY_LEFT,
         "SampleChopperModal.cpp:3448-3450, L1+RIGHT deltaPx 24"),
    BIND(ACTION_ZOOM_IN_COARSE,
         KEY_L1 | KEY_UP, KEY_DOWN,
         "SampleChopperModal.cpp:3452-3455, L1+UP deltaPercent 10"),
    BIND(ACTION_ZOOM_OUT_COARSE,
         KEY_L1 | KEY_DOWN, KEY_UP,
         "SampleChopperModal.cpp:3452-3455, L1+DOWN deltaPercent 10"),
    BIND(ACTION_NUDGE_CURSOR_LEFT,
         KEY_LEFT, KEY_RIGHT | KEY_L1,
         "SampleChopperModal.cpp:3448-3450, LEFT deltaPx 2"),
    BIND(ACTION_NUDGE_CURSOR_RIGHT,
         KEY_RIGHT, KEY_LEFT | KEY_L1,
         "SampleChopperModal.cpp:3448-3450, RIGHT deltaPx 2"),
    BIND(ACTION_ZOOM_IN,
         KEY_UP, KEY_DOWN | KEY_L1,
         "SampleChopperModal.cpp:3452-3455, UP deltaPercent 5"),
    BIND(ACTION_ZOOM_OUT,
         KEY_DOWN, KEY_UP | KEY_L1,
         "SampleChopperModal.cpp:3452-3455, DOWN deltaPercent 5"),
};

/* ------------------------------------------------------------------ */
/* CTX_CHOPPER_TRIM -- mismas prioridades, ramas de trim activas.     */
/* ------------------------------------------------------------------ */
static const Binding kChopperTrimBindings[] = {

    /* undo/redo (igual que main: se evaluan antes de todo). */
    BIND(ACTION_UNDO,
         KEY_L1 | KEY_X,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_Y |
         KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3299-3312, trim undo"),
    BIND(ACTION_REDO,
         KEY_R1 | KEY_X,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_Y |
         KEY_L1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3299-3312, trim redo"),

    BIND(ACTION_TOGGLE_PITCH_MODE,
         KEY_L1 | KEY_R1,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X |
         KEY_Y | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3317-3318, trim L1+R1"),

    /* L1+B en trim: snap del limite seleccionado al zero-cross. */
    BIND(ACTION_SNAP_BOUNDARY_END,
         KEY_L1 | KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_X | KEY_Y |
         KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3325-3329, trim L1+B snapSelectedBoundaryToZeroCross(false)"),

    /* L1+A en trim: snap del inicio. */
    BIND(ACTION_SNAP_BOUNDARY_START,
         KEY_L1 | KEY_A,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_B | KEY_X | KEY_Y |
         KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3331-3332, trim L1+A snapSelectedBoundaryToZeroCross(true)"),

    /* R2+Y en trim: normalizar (solo trim). */
    BIND(ACTION_NORMALIZE,
         KEY_R2 | KEY_Y,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_SELECT,
         "SampleChopperModal.cpp:3337-3338, trim R2+Y normalizeSample"),

    BIND(ACTION_CLOSE,
         KEY_R1 | KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_X | KEY_Y,
         "SampleChopperModal.cpp:3382-3384, trim R1+B EndModal(0)"),

    BIND(ACTION_STOP_PREVIEW,
         KEY_L2 | KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_X | KEY_Y,
         "SampleChopperModal.cpp:3386-3388, trim L2+B stop"),

    BIND(ACTION_TOGGLE_TRIM_MODE,
         KEY_SELECT,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X |
         KEY_Y | KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2,
         "SampleChopperModal.cpp:3390-3391, trim SELECT toggle off"),

    /* R1+A en trim: crop destructivo al rango seleccionado. */
    BIND(ACTION_CROP,
         KEY_R1 | KEY_A,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_B | KEY_X | KEY_Y |
         KEY_L2 | KEY_R2,
         "SampleChopperModal.cpp:3394-3396, trim R1+A destructiveCropToSelectedRange"),

    /* L2+Y en trim: borrado destructivo del rango. */
    BIND(ACTION_DELETE_RANGE,
         KEY_L2 | KEY_Y,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X |
         KEY_R1 | KEY_R2,
         "SampleChopperModal.cpp:3398-3400, trim L2+Y destructiveDeleteSelectedRange"),

    /* A/flechas editan el inicio, B/flechas el fin; L1 multiplica x10. */
    BIND(ACTION_NUDGE_START_LEFT_COARSE,
         KEY_L1 | KEY_A | KEY_LEFT, KEY_RIGHT | KEY_B,
         "SampleChopperModal.cpp:3402-3406, trim L1+A+LEFT delta x10"),
    BIND(ACTION_NUDGE_START_RIGHT_COARSE,
         KEY_L1 | KEY_A | KEY_RIGHT, KEY_LEFT | KEY_B,
         "SampleChopperModal.cpp:3402-3406, trim L1+A+RIGHT delta x10"),
    BIND(ACTION_NUDGE_END_LEFT_COARSE,
         KEY_L1 | KEY_B | KEY_LEFT, KEY_RIGHT | KEY_A,
         "SampleChopperModal.cpp:3402-3406, trim L1+B+LEFT delta x10"),
    BIND(ACTION_NUDGE_END_RIGHT_COARSE,
         KEY_L1 | KEY_B | KEY_RIGHT, KEY_LEFT | KEY_A,
         "SampleChopperModal.cpp:3402-3406, trim L1+B+RIGHT delta x10"),
    BIND(ACTION_NUDGE_START_LEFT,
         KEY_A | KEY_LEFT, KEY_RIGHT | KEY_B | KEY_L1,
         "SampleChopperModal.cpp:3402-3406, trim A+LEFT nudgeSelectedStart"),
    BIND(ACTION_NUDGE_START_RIGHT,
         KEY_A | KEY_RIGHT, KEY_LEFT | KEY_B | KEY_L1,
         "SampleChopperModal.cpp:3402-3406, trim A+RIGHT nudgeSelectedStart"),
    BIND(ACTION_NUDGE_END_LEFT,
         KEY_B | KEY_LEFT, KEY_RIGHT | KEY_A | KEY_L1,
         "SampleChopperModal.cpp:3402-3406, trim B+LEFT nudgeSelectedEnd"),
    BIND(ACTION_NUDGE_END_RIGHT,
         KEY_B | KEY_RIGHT, KEY_LEFT | KEY_A | KEY_L1,
         "SampleChopperModal.cpp:3402-3406, trim B+RIGHT nudgeSelectedEnd"),

    /* Y/X puros: preview del inicio / del fin del rango. */
    BIND(ACTION_TRIM_PREVIEW_START,
         KEY_Y,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2,
         "SampleChopperModal.cpp:3408-3410, trim Y pure previewTrimStart"),
    BIND(ACTION_TRIM_PREVIEW_END,
         KEY_X,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_Y |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2,
         "SampleChopperModal.cpp:3412-3414, trim X pure previewTrimEnd"),

    /* R2+A / R2+L/R / R1+L/R siguen activos en trim (ramas posteriores). */
    BIND(ACTION_PLAY_FULL,
         KEY_R2 | KEY_A, 0,
         "SampleChopperModal.cpp:3439, trim R2+A playFullSample"),
    BIND(ACTION_SELECT_PREV_CHOP,
         KEY_R2 | KEY_LEFT, KEY_RIGHT | KEY_A,
         "SampleChopperModal.cpp:3440-3441, trim R2+LEFT selectChop(-1)"),
    BIND(ACTION_SELECT_NEXT_CHOP,
         KEY_R2 | KEY_RIGHT, KEY_LEFT | KEY_A,
         "SampleChopperModal.cpp:3440-3441, trim R2+RIGHT selectChop(1)"),
    BIND(ACTION_SELECT_PREV_SAMPLE,
         KEY_R1 | KEY_LEFT, KEY_RIGHT,
         "SampleChopperModal.cpp:3437-3438, trim R1+LEFT selectSample(-1)"),
    BIND(ACTION_SELECT_NEXT_SAMPLE,
         KEY_R1 | KEY_RIGHT, KEY_LEFT,
         "SampleChopperModal.cpp:3437-3438, trim R1+RIGHT selectSample(1)"),

    /* L/R cursor y U/D zoom siguen activos en trim (A/B+flecha ya se
     * resolvieron arriba; flechas diagonales: dual-fire anotado). */
    BIND(ACTION_NUDGE_CURSOR_LEFT_COARSE,
         KEY_L1 | KEY_LEFT, KEY_RIGHT,
         "SampleChopperModal.cpp:3448-3450, trim L1+LEFT"),
    BIND(ACTION_NUDGE_CURSOR_RIGHT_COARSE,
         KEY_L1 | KEY_RIGHT, KEY_LEFT,
         "SampleChopperModal.cpp:3448-3450, trim L1+RIGHT"),
    BIND(ACTION_ZOOM_IN_COARSE,
         KEY_L1 | KEY_UP, KEY_DOWN,
         "SampleChopperModal.cpp:3452-3455, trim L1+UP"),
    BIND(ACTION_ZOOM_OUT_COARSE,
         KEY_L1 | KEY_DOWN, KEY_UP,
         "SampleChopperModal.cpp:3452-3455, trim L1+DOWN"),
    BIND(ACTION_NUDGE_CURSOR_LEFT,
         KEY_LEFT, KEY_RIGHT | KEY_L1 | KEY_A | KEY_B,
         "SampleChopperModal.cpp:3448-3450, trim LEFT"),
    BIND(ACTION_NUDGE_CURSOR_RIGHT,
         KEY_RIGHT, KEY_LEFT | KEY_L1 | KEY_A | KEY_B,
         "SampleChopperModal.cpp:3448-3450, trim RIGHT"),
    BIND(ACTION_ZOOM_IN,
         KEY_UP, KEY_DOWN | KEY_L1,
         "SampleChopperModal.cpp:3452-3455, trim UP"),
    BIND(ACTION_ZOOM_OUT,
         KEY_DOWN, KEY_UP | KEY_L1,
         "SampleChopperModal.cpp:3452-3455, trim DOWN"),
};

/* ------------------------------------------------------------------ */
/* CTX_CHOPPER_PITCH -- ramas del bloque pitchMode_.                  */
/* ------------------------------------------------------------------ */
static const Binding kChopperPitchBindings[] = {

    /* undo/redo siguen activos (antes del bloque pitch). */
    BIND(ACTION_UNDO,
         KEY_L1 | KEY_X,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_Y |
         KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3299-3312, pitch undo"),
    BIND(ACTION_REDO,
         KEY_R1 | KEY_X,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_Y |
         KEY_L1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3299-3312, pitch redo"),

    /* L1+R1 puro: sale del pitch (la rama es anterior al bloque). */
    BIND(ACTION_TOGGLE_PITCH_MODE,
         KEY_L1 | KEY_R1,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X |
         KEY_Y | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3317-3318, pitch L1+R1 exit"),

    /* L1+B: la rama es anterior al bloque pitch y no chequea pitchMode:
     * en pitch sin trim CICLA el split. (pitch+trim -> SNAP_BOUNDARY_END;
     * el adapter F1b desambigua con trimMode_.) */
    BIND(ACTION_SPLIT_PARTS,
         KEY_L1 | KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_X | KEY_Y |
         KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3325-3329, pitch L1+B cycleSplitParts"),
    /* nota: pitch+trim: L1+A -> SNAP_START, R2+Y -> NORMALIZE (mismas
     * condiciones que CTX_CHOPPER_TRIM); el bloque pitch no las ve. */

    /* L2+B: para el preview del pitch. */
    BIND(ACTION_STOP_PREVIEW,
         KEY_L2 | KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_X | KEY_Y |
         KEY_L1 | KEY_R1 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3341-3343, pitch L2+B stopSamplePreview"),

    /* R2+L/R: selecciona el chop del scope de pitch. */
    BIND(ACTION_PITCH_SCOPE_PREV,
         KEY_R2 | KEY_LEFT,
         KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X | KEY_Y |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_SELECT,
         "SampleChopperModal.cpp:3347-3354, pitch R2+LEFT selectChop(-1)"),
    BIND(ACTION_PITCH_SCOPE_NEXT,
         KEY_R2 | KEY_RIGHT,
         KEY_LEFT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X | KEY_Y |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_SELECT,
         "SampleChopperModal.cpp:3347-3354, pitch R2+RIGHT selectChop(1)"),

    /* U/D: cambia el parametro de pitch editado. DOWN = siguiente. */
    BIND(ACTION_PITCH_PARAM_NEXT,
         KEY_DOWN,
         KEY_UP | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_X | KEY_Y |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3357-3360, pitch DOWN selectPitchEditParam(+1)"),
    BIND(ACTION_PITCH_PARAM_PREV,
         KEY_UP,
         KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_X | KEY_Y |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3357-3360, pitch UP selectPitchEditParam(-1)"),

    /* L/R: nudge del valor de pitch. */
    BIND(ACTION_PITCH_VALUE_UP,
         KEY_RIGHT,
         KEY_LEFT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X | KEY_Y |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3362-3365, pitch RIGHT nudgePitchEnvelopeValue(+1)"),
    BIND(ACTION_PITCH_VALUE_DOWN,
         KEY_LEFT,
         KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_B | KEY_X | KEY_Y |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3362-3365, pitch LEFT nudgePitchEnvelopeValue(-1)"),

    /* B = PREVIEW, A = APPLY (bindings estables del golden, REQUISITO 2). */
    BIND(ACTION_PITCH_PREVIEW,
         KEY_B,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_A | KEY_X | KEY_Y |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3367-3369, pitch B puro previewPitchSetting"),

    BIND(ACTION_PITCH_APPLY,
         KEY_A,
         KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN | KEY_B | KEY_X | KEY_Y |
         KEY_L1 | KEY_R1 | KEY_L2 | KEY_R2 | KEY_SELECT,
         "SampleChopperModal.cpp:3371-3373, pitch A puro destructivePitchSample"),

    /* nota: en pitch, R1+B / R2+A / SELECT puro caen en el status del
     * bloque pitch (sin accion): NO se incluyen aqui. Cualquier acorde no
     * resuelto del bloque pitch pinta "Pitch/env: UD item LR value". */
};

/* ------------------------------------------------------------------ */
/* Despachador de contextos.                                          */
/* ------------------------------------------------------------------ */
int ActionMap_GetBindings(ContextId ctx, const Binding **out) {
    if (out == 0) return 0;
    switch (ctx) {
        case CTX_GLOBAL:
            *out = kGlobalBindings;
            return (int)(sizeof(kGlobalBindings) / sizeof(kGlobalBindings[0]));
        case CTX_MIXER:
            *out = kMixerBindings;
            return (int)(sizeof(kMixerBindings) / sizeof(kMixerBindings[0]));
        case CTX_MIXER_FX:
            *out = kMixerFxBindings;
            return (int)(sizeof(kMixerFxBindings) / sizeof(kMixerFxBindings[0]));
        case CTX_CHOPPER:
            *out = kChopperBindings;
            return (int)(sizeof(kChopperBindings) / sizeof(kChopperBindings[0]));
        case CTX_CHOPPER_TRIM:
            *out = kChopperTrimBindings;
            return (int)(sizeof(kChopperTrimBindings) / sizeof(kChopperTrimBindings[0]));
        case CTX_CHOPPER_PITCH:
            *out = kChopperPitchBindings;
            return (int)(sizeof(kChopperPitchBindings) / sizeof(kChopperPitchBindings[0]));
        default:
            *out = 0;
            return 0;
    }
}

}  /* namespace Input */
}  /* namespace UI */