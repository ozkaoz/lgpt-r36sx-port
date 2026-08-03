#ifndef _UI_DRAW_H_
#define _UI_DRAW_H_

#include "View.h"

/*
 * TREEFROG_UI_DRAW_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 19).
 *
 * Shared rendering primitives for the LGPT R36SX UI.  Every view draws
 * through these helpers (or the semantic colors in UiColors.h) so the
 * whole port shares one visual language: centered titles, solid bars,
 * ON/OFF toggles and modal frames.
 *
 * Screen geometry is fixed at 40x30 characters (AppWindow flush loop).
 * All helpers clamp their output into [0,40)x[0,30).
 */
class UiDraw {
  public:
    // Centered title at row 0 using CD_HILITE1.  x is computed as
    // (screenWidth - textWidth)/2 per the RC3 plan.
    static void DrawCenteredTitle(View &view, const char *title);

    // Section header on its own row, left aligned, CD_HILITE1.
    static void DrawSectionHeader(View &view, int x, int y,
                                  const char *header);

    // One value row: label (CD_NORMAL) + value (CD_HILITE1); the edited row
    // inverts with CD_HILITE2.  Mirrors the DELAY/REVERB MASTER hierarchy.
    static void DrawValueRow(View &view, int x, int y, const char *label,
                             const char *value, bool selected = false);

    // Solid bar of width cells starting at (x,y).  filled filled-cells are
    // drawn inverted (solid), the rest as CD_HILITE1 hollow cells.
    static void DrawSolidBar(View &view, int x, int y, int width,
                             int filled);

    // Bipolar bar: negative/positive halves share the same solid style.
    static void DrawBipolarBar(View &view, int x, int y, int width,
                               int value);

    // Toggle control: "[ ON ]" / "[ OFF ]" at (x,y).  CD_HILITE1 when on,
    // CD_MUTE when off; selected rows invert with CD_HILITE2.
    static void DrawToggle(View &view, int x, int y, bool on,
                           bool selected = false);

    // Progress indicator (e.g. sample load) with 3-cell blocks out of width.
    static void DrawProgressBar(View &view, int x, int y, int width,
                                int filled);

    // Conceptual tab strip, e.g. "<-Page1-" style navigation hint.
    static void DrawTabs(View &view, int y, const char *left,
                         const char *current, const char *right);

    // Modal frame: centered window with a border box and title bar.
    static void DrawModalFrame(View &view, int width, int height,
                               const char *title);

    // Scroll indicator: up/down arrows near the right edge when content
    // overflows the viewport.
    static void DrawScrollIndicator(View &view, bool canScrollUp,
                                    bool canScrollDown);

    // Horizontal separator of solid cells across the given column span.
    static void DrawSeparator(View &view, int y, int x, int width);
};

#endif  // _UI_DRAW_H_