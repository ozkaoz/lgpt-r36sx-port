/*
 * ChordResolver.cpp -- resolucion de acordes (ver ChordResolver.h).
 *
 * F1a: capa pura, sin dependencias de UI. Compila en host (g++ -std=gnu++03)
 * y en el build MIPS del core.
 */
#include "ChordResolver.h"

namespace UI {
namespace Input {

ActionId ChordResolver_ResolveIn(PadMask mask, const Binding *table, int count) {
    for (int i = 0; i < count; ++i) {
        const Binding &b = table[i];
        if ((mask & b.require) == b.require && (mask & b.forbid) == 0) {
            return b.action;
        }
    }
    return ACTION_NONE;
}

ActionId ChordResolver_Resolve(PadMask mask, ContextId ctx) {
    const Binding *table = 0;
    const int count = ActionMap_GetBindings(ctx, &table);
    if (count == 0 || table == 0) return ACTION_NONE;
    return ChordResolver_ResolveIn(mask, table, count);
}

}  /* namespace Input */
}  /* namespace UI */