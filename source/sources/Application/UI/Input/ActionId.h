/*
 * ActionId.h -- acciones semanticas del port (F1 REFACTOR_ROADMAP_ES.md).
 *
 * Una accion semantica NO es un boton: describe INTENCION. El mismo boton
 * puede resolver acciones distintas segun el contexto (p.ej. B en el pitch
 * = ACTION_PITCH_PREVIEW; B en el chopper main = ACTION_PLAY_CHOP_PREVIEW).
 * El componente de negocio consume la accion, nunca la mascara fisica.
 *
 * Catálogo inicial cerrado sobre los bindings del golden Bacon 1.2.1; las
 * fases siguientes pueden ampliarlo sin romper los existentes (los IDs son
 * estables y unicos; la tabla del ActionMap es la unica fuente de verdad).
 */
#ifndef UI_INPUT_ACTION_ID_H_
#define UI_INPUT_ACTION_ID_H_

namespace UI {
namespace Input {

enum ActionId {
    ACTION_NONE = 0,

    /* Navegacion genérica */
    ACTION_NAV_UP,
    ACTION_NAV_DOWN,
    ACTION_NAV_LEFT,
    ACTION_NAV_RIGHT,

    /* Modificacion fina/gruesa de valores (parametros, volumen, pan, zoom) */
    ACTION_FINE_INCREASE,
    ACTION_FINE_DECREASE,
    ACTION_COARSE_INCREASE,
    ACTION_COARSE_DECREASE,

    /* Ciclo de vida de preview/play */
    ACTION_PREVIEW,          /* preview el item en foco (significado por contexto) */
    ACTION_APPLY,            /* aplica la edicion (destructivo si corresponde) */
    ACTION_STOP_PREVIEW,
    ACTION_PLAY_FULL,        /* reproduce el sample completo desde el inicio */

    /* Volumen del mixer (pagina MIX) */
    ACTION_VOLUME_FINE_INCREASE,
    ACTION_VOLUME_FINE_DECREASE,

    /* Historia de edicion (global a todo el port) */
    ACTION_UNDO,
    ACTION_REDO,

    /* Sistema / globales */
    ACTION_OPEN_HELP,        /* SELECT+R1 (latched) */
    ACTION_OPEN_AUDIO_DRIVER,/* SELECT+R2 (latched) */
    ACTION_PLAY_STOP,        /* START */

    /* Chopper: modo y estructura */
    ACTION_TOGGLE_TRIM_MODE, /* SELECT */
    ACTION_TOGGLE_PITCH_MODE,/* L1+R1 puro */
    ACTION_CLOSE,            /* R1+B puro: cierra el modal (EndModal) */
    ACTION_ADD_CHOP,         /* A puro en main */
    ACTION_DELETE_CHOP,      /* Y puro en main */
    ACTION_SPLIT_PARTS,      /* L1+B puro en main */
    ACTION_AUTOSAVE_CHOPS,   /* R1+A en main */
    ACTION_SELECT_NEXT_SAMPLE,
    ACTION_SELECT_PREV_SAMPLE,
    ACTION_SELECT_NEXT_CHOP,   /* R2+L/R */
    ACTION_SELECT_PREV_CHOP,
    ACTION_PLAY_CHOP_PREVIEW,  /* B puro en main */
    ACTION_NUDGE_CURSOR_LEFT,
    ACTION_NUDGE_CURSOR_RIGHT,
    ACTION_NUDGE_CURSOR_LEFT_COARSE,   /* con L1 */
    ACTION_NUDGE_CURSOR_RIGHT_COARSE,
    ACTION_ZOOM_IN,
    ACTION_ZOOM_OUT,
    ACTION_ZOOM_IN_COARSE,
    ACTION_ZOOM_OUT_COARSE,

    /* Chopper trim */
    ACTION_TRIM_PREVIEW_START, /* Y puro en trim */
    ACTION_TRIM_PREVIEW_END,   /* X puro en trim */
    ACTION_SNAP_BOUNDARY_START,/* L1+A en trim */
    ACTION_SNAP_BOUNDARY_END,  /* L1+B en trim */
    ACTION_CROP,               /* R1+A en trim */
    ACTION_DELETE_RANGE,       /* L2+Y en trim */
    ACTION_NORMALIZE,          /* R2+Y en trim */
    ACTION_NUDGE_START_LEFT,
    ACTION_NUDGE_START_RIGHT,
    ACTION_NUDGE_START_LEFT_COARSE,
    ACTION_NUDGE_START_RIGHT_COARSE,
    ACTION_NUDGE_END_LEFT,
    ACTION_NUDGE_END_RIGHT,
    ACTION_NUDGE_END_LEFT_COARSE,
    ACTION_NUDGE_END_RIGHT_COARSE,

    /* Chopper pitch */
    ACTION_PITCH_PARAM_NEXT,    /* DOWN en pitch */
    ACTION_PITCH_PARAM_PREV,    /* UP en pitch */
    ACTION_PITCH_VALUE_UP,      /* RIGHT en pitch */
    ACTION_PITCH_VALUE_DOWN,    /* LEFT en pitch */
    ACTION_PITCH_PREVIEW,       /* B puro en pitch */
    ACTION_PITCH_APPLY,         /* A puro en pitch (destructivePitchSample) */
    ACTION_PITCH_SCOPE_NEXT,    /* R2+RIGHT en pitch con scope */
    ACTION_PITCH_SCOPE_PREV,    /* R2+LEFT en pitch con scope */

    /* Mixer (pagina MIX) */
    ACTION_OPEN_MENU,           /* L1+A puro: menu de accion (MixerActionMenuModal) */
    ACTION_OPEN_INSTRUMENT_FX,  /* R2+A: menu FX del instrumento */
    ACTION_CYCLE_FX_EDIT_TARGET,/* R2 puro en pagina MIX: VOL -> DLY RET -> RVB RET */
    ACTION_CYCLE_FX_PAGE,       /* SELECT: MIX -> DELAY -> REVERB -> MASTER -> MIX */
    ACTION_TOGGLE_MUTE,         /* R1+B */
    ACTION_TOGGLE_SOLO,         /* R1+A */
    ACTION_SWITCH_VIEW_SONG,    /* R1+UP en MixerView (cola: +START onStop) */
    ACTION_STOP,                /* R1+START: onStop (cola de R1+UP+START) */
    ACTION_PAN_NUDGE_LEFT,
    ACTION_PAN_NUDGE_RIGHT,
    ACTION_PAN_NUDGE_LEFT_COARSE,  /* con A */
    ACTION_PAN_NUDGE_RIGHT_COARSE,
    ACTION_RESET_PAN,           /* L2+A+B puro */
    ACTION_VOLUME_FINE_UP,
    ACTION_VOLUME_FINE_DOWN,
    ACTION_VOLUME_COARSE_UP,    /* A+UP / L1+UP */
    ACTION_VOLUME_COARSE_DOWN,  /* A+DOWN / L1+DOWN */
    ACTION_MIX_CURSOR_LEFT,
    ACTION_MIX_CURSOR_RIGHT,

    /* Mixer (paginas FX: DELAY/REVERB/EQ/COMP) */
    ACTION_RESET_PARAMETER,     /* A+B puro */
    ACTION_EDIT_PARAM_UP,       /* A+UP */
    ACTION_EDIT_PARAM_DOWN,     /* A+DOWN */
    ACTION_EDIT_PARAM_LEFT,
    ACTION_EDIT_PARAM_RIGHT,
    ACTION_ROW_UP,
    ACTION_ROW_DOWN,

    ACTION_COUNT
};

}  /* namespace Input */
}  /* namespace UI */

#endif  /* UI_INPUT_ACTION_ID_H_ */