#ifndef _UI_COLORS_H_
#define _UI_COLORS_H_

#include "View.h"

/*
 * TREEFROG_UI_COLORS_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 20)
 * + RC4 P5 (PLAN_RC4 section 12).
 *
 * Semantic color roles mapped onto the existing CD_* palette (View.h).
 * Views must draw with these roles, never with raw RGB.
 *
 *   UI_TITLE            -> CD_HILITE1   (page titles, section headers)
 *   UI_LABEL            -> CD_NORMAL    (static row labels, hints)
 *   UI_VALUE            -> CD_HILITE1   (current value text)
 *   UI_TEXT_EDIT        -> CD_HILITE2   (value being edited / selected row)
 *   UI_CURSOR           -> CD_HILITE2   (active cell accent)
 *   UI_BORDER           -> CD_BORDER     (modal frames, separators)
 *   UI_BACKGROUND       -> CD_BACKGROUND (modal interiors)
 *   UI_OK/UI_ACTIVE     -> CD_HILITE1   (ON state / enabled)
 *   UI_MUTED            -> CD_MUTE       (disabled / OFF state)
 *   UI_SECTION          -> CD_HILITE1   (family headers, LOW/MID/HIGH)
 *   UI_TEXT             -> CD_NORMAL    (command / body text)
 *   UI_SELECTED         -> CD_HILITE2   (selected row background/invert)
 *   UI_SELECTED_TEXT    -> CD_HILITE2   (text of the selected row)
 *   UI_DISABLED         -> CD_MUTE      (unavailable action)
 *   UI_WARNING          -> CD_WARNING   (non-fatal status)
 *   UI_ERROR            -> CD_ERROR     (fatal / error status)
 *   UI_SUCCESS          -> CD_HILITE1   (positive result)
 *   UI_LEGACY           -> CD_MUTE      (legacy comb items)
 *   UI_BAR_FILL         -> CD_HILITE1   (solid bar fill cells)
 *   UI_BAR_EMPTY        -> CD_NORMAL    (solid bar empty cells)
 *   UI_WAVEFORM         -> CD_HILITE1   (chopper waveform)
 *   UI_WAVEFORM_SELECTED-> CD_HILITE2   (selected waveform region)
 *   UI_MARKER           -> CD_CURSOR    (chopper cut markers)
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
    UI_COLOR_DISABLED = CD_MUTE,
    UI_COLOR_SECTION = CD_HILITE1,
    UI_COLOR_TEXT = CD_NORMAL,
    UI_COLOR_SELECTED = CD_HILITE2,
    UI_COLOR_SELECTED_TEXT = CD_HILITE2,
    UI_COLOR_WARNING = CD_WARNING,
    UI_COLOR_ERROR = CD_ERROR,
    UI_COLOR_SUCCESS = CD_HILITE1,
    UI_COLOR_LEGACY = CD_MUTE,
    UI_COLOR_BAR_FILL = CD_HILITE1,
    UI_COLOR_BAR_EMPTY = CD_NORMAL,
    UI_COLOR_WAVEFORM = CD_HILITE1,
    UI_COLOR_WAVEFORM_SELECTED = CD_HILITE2,
    UI_COLOR_MARKER = CD_CURSOR
};

namespace UiColors {
// Resolves a semantic role to the concrete CD_* ColorDefinition.
inline ColorDefinition Resolve(UiColorRole role) { return (ColorDefinition)role; }
}  // namespace UiColors

#endif  // _UI_COLORS_H_