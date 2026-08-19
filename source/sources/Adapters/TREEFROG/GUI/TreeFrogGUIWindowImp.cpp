#include "TreeFrogGUIWindowImp.h"

#include <string.h>
#include <stddef.h>
#include "UIFramework/SimpleBaseClasses/GUIWindow.h"
#include "UIFramework/BasicDatas/GUIEvent.h"
#include "System/System/System.h"


// TREEFROG_V51_ESTABLE_FONT_ZERO_IS_INK_NO_WHITE_BOXES
// Prueba controlada: interpretar font.h con cero = tinta y limpiar fondo oscuro.
#ifndef TREEFROG_FONT_ZERO_IS_INK
#define TREEFROG_FONT_ZERO_IS_INK 1
#endif
#ifndef TREEFROG_TEXT_INVERT_BG_RGB565
#define TREEFROG_TEXT_INVERT_BG_RGB565 0x9dbf
#endif
#ifndef TREEFROG_TEXT_INVERT_FG_RGB565
#define TREEFROG_TEXT_INVERT_FG_RGB565 0xffff
#endif

#ifndef TREEFROG_HIGH_CONTRAST_SELECTION
#define TREEFROG_HIGH_CONTRAST_SELECTION 1
#endif

#ifndef TREEFROG_PURPLE_FOCUS_SELECTION
#define TREEFROG_PURPLE_FOCUS_SELECTION 1
#endif

#ifndef TREEFROG_PURPLE_FOCUS_RGB565
#define TREEFROG_PURPLE_FOCUS_RGB565 0x9dbf
#endif

static const unsigned char treefrog_font[] = {
#include "Resources/font.h"
0
};

static uint16_t g_fallback_framebuffer[TREEFROG_LGPT_WIDTH * TREEFROG_LGPT_HEIGHT];
static TreeFrogGUIWindowImp *g_window_imp = 0;

TreeFrogGUIWindowImp *TreeFrogGetWindowImp() {
    return g_window_imp;
}

uint16_t *TreeFrogGetFramebuffer() {
    return g_window_imp ? g_window_imp->GetFramebuffer() : g_fallback_framebuffer;
}

void TreeFrogDrawText8(const char *text, int x, int y, uint16_t fg) {
    if (!text) return;
    uint16_t *fb = TreeFrogGetFramebuffer();
    if (!fb) return;
    const size_t FONT_SCANLINE_WIDTH = 128 * 8;
    const size_t fontSize = sizeof(treefrog_font);
    int px = x;
    for (const char *s = text; *s; ++s) {
        unsigned int ch = (unsigned char)*s;
        if (ch >= 128) ch = (unsigned int)'?';
        const size_t glyphBase = (size_t)ch * 8;
        for (int yg = 0; yg < 8; ++yg) {
            int yy = y + yg;
            if (yy < 0 || yy >= TREEFROG_LGPT_HEIGHT) continue;
            if ((size_t)yg * FONT_SCANLINE_WIDTH + glyphBase + 8 > fontSize) continue;
            const unsigned char *row =
                treefrog_font + (size_t)yg * FONT_SCANLINE_WIDTH + glyphBase;
            uint16_t *dst = fb + (size_t)yy * TREEFROG_LGPT_WIDTH;
            for (int xg = 0; xg < 8; ++xg) {
                int xx = px + xg;
                if (xx < 0 || xx >= TREEFROG_LGPT_WIDTH) continue;
                // ZERO_IS_INK (TreeFrogGUIWindowImp.cpp:13): ink = byte 0.
                if (row[xg] == 0) dst[xx] = fg;
            }
        }
        px += 8;
    }
}

uint16_t TreeFrogGUIWindowImp::rgb565(const GUIColor &c) {
    uint16_t r = (uint16_t)((c._r & 0xff) >> 3);
    uint16_t g = (uint16_t)((c._g & 0xff) >> 2);
    uint16_t b = (uint16_t)((c._b & 0xff) >> 3);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

bool TreeFrogGUIWindowImp::clipRect(int &x0, int &y0, int &x1, int &y1) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > TREEFROG_LGPT_WIDTH) x1 = TREEFROG_LGPT_WIDTH;
    if (y1 > TREEFROG_LGPT_HEIGHT) y1 = TREEFROG_LGPT_HEIGHT;
    if (x0 >= x1 || y0 >= y1) return false;
    if (x0 >= TREEFROG_LGPT_WIDTH || y0 >= TREEFROG_LGPT_HEIGHT) return false;
    if (x1 <= 0 || y1 <= 0) return false;
    return true;
}

TreeFrogGUIWindowImp::TreeFrogGUIWindowImp(GUICreateWindowParams &p)
: framebuffer_(g_fallback_framebuffer), currentColor_(0xffff), backgroundColor_(0x0000) {
    TreeFrogCreateWindowParams *tp = (TreeFrogCreateWindowParams *)&p;
    if (tp && tp->framebuffer_) framebuffer_ = tp->framebuffer_;
    g_window_imp = this;
    memset(framebuffer_, 0, TREEFROG_LGPT_WIDTH * TREEFROG_LGPT_HEIGHT * sizeof(uint16_t));
}

TreeFrogGUIWindowImp::~TreeFrogGUIWindowImp() {
    if (g_window_imp == this) g_window_imp = 0;
}

void TreeFrogGUIWindowImp::SetColor(GUIColor &c) {
    currentColor_ = rgb565(c);
}

void TreeFrogGUIWindowImp::DrawRect(GUIRect &r) {
    int x0 = r.Left();
    int y0 = r.Top();
    int x1 = r.Right();
    int y1 = r.Bottom();
    if (!clipRect(x0, y0, x1, y1)) return;

    for (int y = y0; y < y1; ++y) {
        uint16_t *dst = framebuffer_ + y * TREEFROG_LGPT_WIDTH + x0;
        for (int x = x0; x < x1; ++x) {
            *dst++ = currentColor_;
        }
    }
}

void TreeFrogGUIWindowImp::DrawCharInternal(const char c, GUIPoint &pos, GUITextProperties &props, bool overlay) {
    int x0 = (int)pos._x;
    int y0 = (int)pos._y;

    if (x0 <= -8 || y0 <= -8 || x0 >= TREEFROG_LGPT_WIDTH || y0 >= TREEFROG_LGPT_HEIGHT) {
        return;
    }

    unsigned int ch = (unsigned char)c;
    if (ch >= 128) ch = (unsigned int)'?';

    /* LittleGPTracker's mkfont.py generates font.h in scanline-major order:
     * for each glyph row, it stores all 128 characters, 8 pixels each.
     * Pixel address = y * (128 * 8) + character * 8 + x.
     */
    const size_t FONT_SCANLINE_WIDTH = 128 * 8;
    const size_t glyphBase = (size_t)ch * 8;
    const size_t fontSize = sizeof(treefrog_font);
    const bool spaceChar = ((unsigned char)c == (unsigned char)' ');

    uint16_t fg = currentColor_;
    uint16_t bg = backgroundColor_;

#if TREEFROG_HIGH_CONTRAST_SELECTION
    if (props.invert_) {
        /* Classic LGPT invert: glyph pixels use the screen background,
         * the cell/background pixels use the current AppWindow color.
         * This lets map blocks, song cursor cells and bottom bangers render
         * with their own CD_HILITE1/CD_HILITE2 colors instead of a fixed RGB565.
         */
        fg = backgroundColor_;
        bg = currentColor_;
    }
#endif

    for (int y = 0; y < 8; ++y) {
        int yy = y0 + y;
        if (yy < 0 || yy >= TREEFROG_LGPT_HEIGHT) continue;

        /* H38.7 OPT_PERF: hoist the glyph row pointer and the column
         * clipping out of the inner loop. The bounds guard is preserved
         * (same result as the per-pixel index check). */
        const unsigned char *row =
            ((size_t)y * FONT_SCANLINE_WIDTH + glyphBase + 8 <= fontSize)
                ? treefrog_font + (size_t)y * FONT_SCANLINE_WIDTH + glyphBase
                : 0;
        uint16_t *dst = framebuffer_ + (size_t)yy * TREEFROG_LGPT_WIDTH;

        int xStart = (x0 < 0) ? (-x0) : 0;
        int xEnd = (x0 + 8) > TREEFROG_LGPT_WIDTH ? (TREEFROG_LGPT_WIDTH - x0) : 8;
        if (xEnd < xStart) xEnd = xStart;

        for (int x = xStart; x < xEnd; ++x) {
            int xx = x0 + x;

            bool rawOn = row && row[x] != 0;
#if TREEFROG_FONT_ZERO_IS_INK
            bool on = (row != 0) && !rawOn;
            if (spaceChar) on = false;
#else
            bool on = rawOn;
#endif

            if (on) {
                dst[xx] = fg;
            } else if (!overlay || props.invert_) {
                dst[xx] = bg;
            }
        }
    }
}

void TreeFrogGUIWindowImp::DrawChar(const char c, GUIPoint &pos, GUITextProperties &props) {
    DrawCharInternal(c, pos, props, false);
}

void TreeFrogGUIWindowImp::DrawString(const char *string, GUIPoint &pos, GUITextProperties &props, bool overlay) {
    (void)overlay;

    if (!string) return;

    GUIPoint p = pos;

    for (const char *s = string; *s; ++s) {
        DrawCharInternal(*s, p, props, overlay);
        p._x += 8;
    }
}

GUIRect TreeFrogGUIWindowImp::GetRect() {
    return GUIRect(0, 0, TREEFROG_LGPT_WIDTH, TREEFROG_LGPT_HEIGHT);
}

void TreeFrogGUIWindowImp::Invalidate() {
}

void TreeFrogGUIWindowImp::Flush() {
}

void TreeFrogGUIWindowImp::Lock() {
}

void TreeFrogGUIWindowImp::Unlock() {
}

void TreeFrogGUIWindowImp::Clear(GUIColor &c, bool overlay) {
    (void)overlay;

    backgroundColor_ = rgb565(c);

    for (int i = 0; i < TREEFROG_LGPT_WIDTH * TREEFROG_LGPT_HEIGHT; ++i) {
        framebuffer_[i] = backgroundColor_;
    }
}

void TreeFrogGUIWindowImp::ClearRect(GUIRect &r) {
    int x0 = r.Left();
    int y0 = r.Top();
    int x1 = r.Right();
    int y1 = r.Bottom();
    if (!clipRect(x0, y0, x1, y1)) return;

    for (int y = y0; y < y1; ++y) {
        uint16_t *dst = framebuffer_ + y * TREEFROG_LGPT_WIDTH + x0;
        for (int x = x0; x < x1; ++x) {
            *dst++ = backgroundColor_;
        }
    }
}

void TreeFrogGUIWindowImp::PushEvent(GUIEvent &event) {
    if (_window) {
        _window->DispatchEvent(event);
    }
}
