#ifndef _UI_COLORS_H_
#define _UI_COLORS_H_

#include "View.h"

/*
 * TREEFROG_UI_COLORS_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 20).
 *
 * Semantic color roles mapped onto the existing CD_* palette (View.h).
 * Views must draw with these roles, never with raw RGB.
 *
 *   UI_TITLE        -> CD_HILITE1   (page titles, section headers)
 *   UI_LABEL        -> CD_NORMAL    (static row labels, hints)
 *   UI_VALUE        -> CD_HILITE1   (current value text)
 *   UI_VALUE_EDIT   -> CD_HILITE2   (value being edited / selected row, inverted)
 *   UI_CURSOR       -> CD_HILITE2   (active cell accent)
 *   UI_BORDER       -> CD_BORDER     (modal frames, separators)
 *   UI_BACKGROUND   -> CD_BACKGROUND (modal interiors)
 *   UI_OK/UI_ACTIVE -> CD_HILITE1   (ON state / enabled)
 *   UI_MUTED        -> CD_MUTE       (disabled / OFF state)
 */
enum UiColorRole {
    UI_COLOR_TITLE = CD_HILITE1,
    UI_COLOR_LABEL = CD_NORMAL,
    UI_COLOR_VALUE = CD_HILITE1,
    UI_COLOR_TEXT_EDIT = CD_HILITE2,
    UI_COLOR_CURSOR = CD_HILITE2,
    UI_COLOR_BORDER = CD_BORDER,
    UI_COLOR_BACKGROUND = CD_BACKGROUND,
    UI_COLOR_ACTIVE = CD_HILITE1,
    UI_COLOR_DISABLED = CD_MUTE
};

namespace UiColors {
// Resolves a semantic role to the concrete CD_* ColorDefinition.
inline ColorDefinition Resolve(UiColorRole role) { return (ColorDefinition)role; }
}  // namespace UiColors

#endif  // _UI_COLORS_H_