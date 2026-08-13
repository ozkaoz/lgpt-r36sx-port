/*
 * ChordResolver.h -- resolucion de acordes: mascara fisica -> accion semantica.
 *
 * F1 de REFACTOR_ROADMAP_ES.md.
 *
 * Semantica de acorde exclusivo: un Binding declara los bits REQUERIDOS y
 * los bits PROHIBIDOS. La resolucion recorre la tabla del contexto en orden
 * de prioridad (el orden en que la vista dorada evalua sus ramas) y devuelve
 * la primera accion cuyo require esta completo y cuyo forbid esta vacio.
 * Sin match -> ACTION_NONE (la vista dorada, o no hace nada, o solo pinta
 * texto de estado; esos casos se documentan en el ActionMap sin accion).
 *
 * La tabla por contexto es la MISMA fuente de verdad que consumira en F1b
 * el adapter de las vistas y en F2 el MenuRegistry/Help (mismo fichero).
 */
#ifndef UI_INPUT_CHORD_RESOLVER_H_
#define UI_INPUT_CHORD_RESOLVER_H_

#include "PhysicalInput.h"
#include "ActionId.h"

namespace UI {
namespace Input {

struct Binding {
    ActionId action;           /* accion semantica */
    PadMask require;           /* bits que DEBEN estar presentes */
    PadMask forbid;            /* bits que DEBEN estar ausentes */
    const char *provenance;    /* ref. al codigo dorado: archivo y branch */
};

enum ContextId {
    CTX_GLOBAL,          /* AppWindow: help, audio driver, undo/redo */
    CTX_MIXER,           /* MixerView pagina MIX */
    CTX_MIXER_FX,        /* MixerView paginas DELAY/REVERB/EQ/COMP */
    CTX_CHOPPER,         /* SampleChopperModal, modo normal */
    CTX_CHOPPER_TRIM,    /* SampleChopperModal, trim mode (SELECT) */
    CTX_CHOPPER_PITCH,   /* SampleChopperModal, pitch mode (L1+R1) */
    CTX_COUNT
};

/* Tabla de bindings del contexto (orden = prioridad). Devuelve el numero de
 * entradas y deja *out apuntando a la tabla estatica. */
int ActionMap_GetBindings(ContextId ctx, const Binding **out);

/* Resuelve una mascara fisica en el contexto dado. */
ActionId ChordResolver_Resolve(PadMask mask, ContextId ctx);

/* Variante sobre tabla explicita (tests, F8 harness). */
ActionId ChordResolver_ResolveIn(PadMask mask, const Binding *table, int count);

}  /* namespace Input */
}  /* namespace UI */

#endif  /* UI_INPUT_CHORD_RESOLVER_H_ */
