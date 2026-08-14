#ifndef _UI_DRAW_H_
#define _UI_DRAW_H_

#include "View.h"

/*
 * TREEFROG_UI_DRAW_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 19)
 * + RC4 P4 (PLAN_RC4 section 8).
 *
 * Shared rendering primitives for the LGPT R36SX UI.  Every view draws
 * through these helpers (or the semantic colors in UiColors.h) so the
 * whole port shares one visual language: centered titles, solid bars,
 * ON/OFF toggles and modal frames.
 *
 * Screen geometry is fixed at 40x30 characters (AppWindow flush loop).
 * All helpers clamp their output into [0,40)x[0,30).
 */

// Layout of a vertically stacked value menu: a centered block with a left
// label column and a fixed value column.  Used by MakeCenteredMenuLayout.
struct MenuLayout {
    int screenWidth;
    int screenHeight;
    int contentTop;
    int contentBottom;
    int blockWidth;
    int blockHeight;
    int startX;
    int startY;
    int labelX;
    int valueX;
};

class UiDraw {
  public:
    // Shared screen geometry and safe layout bands.  The logical screen is
    // 40x30 characters.  Menu-style blocks stay inside kMenuBandTop..kMenuBandBottom
    // (rows 3..25): row 26 is the status/buffer line and rows 27..29 are the
    // footer, which centering never overlaps.  Header text uses rows
    // kTitleRow (0) and kSubtitleRow (1).
    enum {
        kScreenWidth = 40,
        kScreenHeight = 30,
        kTitleRow = 0,
        kSubtitleRow = 1,
        kMenuBandTop = 3,
        kMenuBandBottom = 25,
        kFooterTop = 27,
        kFooterBottom = 29
    };

    // RC4 P4 (PLAN_RC4 section 8) + RC5: x that centers `text` inside a
    // viewport of viewportWidth cells: (viewportWidth - len) / 2.  With the
    // default viewport this centers on the whole 40-cell screen; modal views
    // pass their own inner width so the x is local to the modal.
    static int CenterTextX(const char *text,
                           int viewportWidth = kScreenWidth);

    // RC4 P4 + RC5: computes a centered block layout for a vertical menu of
    // `rowCount` rows whose label column is labelWidth cells and value
    // column valueWidth cells, with preferredSpacing between them.  The
    // block spans rows contentTop..contentBottom inside a viewport
    // viewportWidth cells wide and is centered on that viewport.  Returns
    // local coordinates (relative to the viewport origin); modal views use
    // their inner width and band, full-screen views use the defaults.
    static MenuLayout MakeCenteredMenuLayout(int rowCount, int labelWidth,
                                             int valueWidth,
                                             int preferredSpacing,
                                             int viewportWidth = kScreenWidth,
                                             int contentTop = kMenuBandTop,
                                             int contentBottom = kMenuBandBottom);
    // Centered title at row 0 using CD_HILITE1.  x is computed as
    // (screenWidth - textWidth)/2 per the RC3 plan.
    static void DrawCenteredTitle(View &view, const char *title);

    // Centered title on an explicit row (for sub-pages whose row 0 is the
    // app header).  Same centering rule, same CD_HILITE1 style.
    static void DrawCenteredTitleAt(View &view, int y, const char *title);

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

    // RC4 P2 (PLAN_RC4 section 12): unified Bypass row for the four master
    // FX pages.  "BYPASS" label + "[ ON ]"/"[ OFF ]" toggle, inverted on
    // CD_HILITE2 when selected.  labelX is the label column; valueX the
    // toggle column.
    static void DrawBypassRow(View &view, int labelX, int valueX, int y,
                              bool on, bool selected = false);

    // Legacy Bypass row with a single x: the toggle sits at x+8.
    static void DrawBypassRow(View &view, int x, int y, bool on,
                              bool selected = false);

    // Progress indicator (e.g. sample load) with 3-cell blocks out of width.
    static void DrawProgressBar(View &view, int x, int y, int width,
                                int filled);

    // Conceptual tab strip, e.g. "<-Page1-" style navigation hint.
    // TREEFROG_HELP_NAV_V1 (Bacon 1.1.1 V13): when highlightCurrent is set
    // the current tab is drawn inverted in CD_HILITE2 (help browser).
    static void DrawTabs(View &view, int y, const char *left,
                         const char *current, const char *right,
                         bool highlightCurrent = false);

    // Modal frame: centered window with a border box and title bar.
    static void DrawModalFrame(View &view, int width, int height,
                               const char *title);

    // Scroll indicator: up/down arrows near the right edge when content
    // overflows the viewport.
    static void DrawScrollIndicator(View &view, bool canScrollUp,
                                    bool canScrollDown);

    // Horizontal separator of solid cells across the given column span.
    static void DrawSeparator(View &view, int y, int x, int width);

    // RC4 P5 (PLAN_RC4 section 12): fills a rectangular selection region
    // (x..x+w-1, y..y+h-1) with inverted CD_HILITE2 cells.  The region is
    // clamped to the 40x30 screen.  Used for chopper selection windows and
    // keyboard-focus highlights.
    static void DrawSelectionRegion(View &view, int x, int y, int w, int h);

    // RC4 P5: one-line status message at (x,y) using the WARNING semantic
    // color (CD_WARNING).  Drawn with the shared props, no invert.
    static void DrawStatusMessage(View &view, int x, int y,
                                  const char *message);

    // RC4 P5: one-line error message at (x,y) using the ERROR semantic
    // color (CD_ERROR).  Drawn with the shared props, no invert.
    static void DrawErrorMessage(View &view, int x, int y,
                                 const char *message);
};

#endif  // _UI_DRAW_H_