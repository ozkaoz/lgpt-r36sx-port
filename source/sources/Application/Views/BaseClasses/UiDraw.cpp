#include "UiDraw.h"

#include <stdio.h>
#include <string.h>

/*
 * TREEFROG_UI_DRAW_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 19).
 *
 * Implementation of the shared rendering primitives.  All coordinates are
 * clamped into [0,kScreenWidth)x[0,kScreenHeight) so no view can draw
 * outside the 40x30 character screen (point 32 of the plan).
 */

namespace {
const int kScreenWidth = 40;
const int kScreenHeight = 30;

// Clamp helpers keep every draw inside the logical screen bounds.
int clampX(int x) {
    if (x < 0) return 0;
    if (x >= kScreenWidth) return kScreenWidth - 1;
    return x;
}
int clampY(int y) {
    if (y < 0) return 0;
    if (y >= kScreenHeight) return kScreenHeight - 1;
    return y;
}
}  // namespace

void UiDraw::DrawCenteredTitle(View &view, const char *title) {
    int len = (int)strlen(title);
    int x = (kScreenWidth - len) / 2;
    if (x < 0) x = 0;
    GUITextProperties props;
    view.SetColor(CD_HILITE1);
    props.invert_ = false;
    view.DrawString(clampX(x), 0, title, props);
}

void UiDraw::DrawCenteredTitleAt(View &view, int y, const char *title) {
    int len = (int)strlen(title);
    int x = (kScreenWidth - len) / 2;
    if (x < 0) x = 0;
    GUITextProperties props;
    view.SetColor(CD_HILITE1);
    props.invert_ = false;
    view.DrawString(clampX(x), clampY(y), title, props);
}

void UiDraw::DrawSectionHeader(View &view, int x, int y, const char *header) {
    GUITextProperties props;
    view.SetColor(CD_HILITE1);
    props.invert_ = false;
    view.DrawString(clampX(x), clampY(y), header, props);
}

void UiDraw::DrawValueRow(View &view, int x, int y, const char *label,
                          const char *value, bool selected) {
    GUITextProperties props;
    props.invert_ = false;
    view.SetColor(CD_NORMAL);
    view.DrawString(clampX(x), clampY(y), label, props);
    view.SetColor(selected ? CD_HILITE2 : CD_HILITE1);
    props.invert_ = selected;
    view.DrawString(clampX(x + (int)strlen(label)), clampY(y), value, props);
    props.invert_ = false;
    view.SetColor(CD_NORMAL);
}

void UiDraw::DrawSolidBar(View &view, int x, int y, int width, int filled) {
    GUITextProperties props;
    props.invert_ = true;
    int cx = clampX(x);
    int cy = clampY(y);
    for (int i = 0; i < width; i++) {
        char cell[2] = {' ', 0};
        if (i < filled) {
            view.SetColor(CD_HILITE1);
            view.DrawString(cx + i, cy, cell, props);
        } else {
            view.SetColor(CD_HILITE1);
            props.invert_ = false;
            view.DrawString(cx + i, cy, cell, props);
            props.invert_ = true;
        }
    }
    props.invert_ = false;
    view.SetColor(CD_NORMAL);
}

void UiDraw::DrawBipolarBar(View &view, int x, int y, int width, int value) {
    // value in [-100,100]; zero centered.  Uses the same solid-cell style.
    int half = width / 2;
    GUITextProperties props;
    props.invert_ = true;
    int cx = clampX(x);
    int cy = clampY(y);
    for (int i = 0; i < width; i++) {
        char cell[2] = {' ', 0};
        int pos = i - half;
        bool on = (value >= 0) ? (pos >= 0 && pos <= (value * half) / 100)
                               : (pos < 0 && -pos <= (-value * half) / 100);
        if (on) {
            view.SetColor(CD_HILITE1);
            view.DrawString(cx + i, cy, cell, props);
        } else {
            view.SetColor(CD_HILITE1);
            props.invert_ = false;
            view.DrawString(cx + i, cy, cell, props);
            props.invert_ = true;
        }
    }
    props.invert_ = false;
    view.SetColor(CD_NORMAL);
}

void UiDraw::DrawToggle(View &view, int x, int y, bool on, bool selected) {
    GUITextProperties props;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "[ %s ]", on ? "ON" : "OFF");
    view.SetColor(selected ? CD_HILITE2 : (on ? CD_HILITE1 : CD_MUTE));
    props.invert_ = selected;
    view.DrawString(clampX(x), clampY(y), buffer, props);
    props.invert_ = false;
    view.SetColor(CD_NORMAL);
}

void UiDraw::DrawProgressBar(View &view, int x, int y, int width, int filled) {
    GUITextProperties props;
    props.invert_ = true;
    int cx = clampX(x);
    int cy = clampY(y);
    if (filled > width) filled = width;
    for (int i = 0; i < width; i++) {
        char cell[2] = {' ', 0};
        if (i < filled) {
            view.SetColor(CD_HILITE1);
            view.DrawString(cx + i, cy, cell, props);
        } else {
            view.SetColor(CD_NORMAL);
            props.invert_ = false;
            view.DrawString(cx + i, cy, cell, props);
            props.invert_ = true;
        }
    }
    props.invert_ = false;
    view.SetColor(CD_NORMAL);
}

void UiDraw::DrawTabs(View &view, int y, const char *left,
                      const char *current, const char *right) {
    char buffer[64];
    GUITextProperties props;
    props.invert_ = false;
    snprintf(buffer, sizeof(buffer), "<- %s [%s] %s ->", left, current, right);
    view.SetColor(CD_HILITE1);
    int len = (int)strlen(buffer);
    int x = (kScreenWidth - len) / 2;
    if (x < 0) x = 0;
    view.DrawString(clampX(x), clampY(y), buffer, props);
    view.SetColor(CD_NORMAL);
}

void UiDraw::DrawModalFrame(View &view, int width, int height,
                            const char *title) {
    if (width > kScreenWidth - 2) width = kScreenWidth - 2;
    if (height > kScreenHeight - 2) height = kScreenHeight - 2;
    int x0 = (kScreenWidth - width) / 2;
    int y0 = (kScreenHeight - height) / 2;
    GUITextProperties props;
    props.invert_ = false;
    view.SetColor(CD_BORDER);
    for (int i = 0; i < width; i++) {
        view.DrawString(clampX(x0 + i), clampY(y0), "-", props);
        view.DrawString(clampX(x0 + i), clampY(y0 + height - 1), "-", props);
    }
    for (int j = 0; j < height; j++) {
        view.DrawString(clampX(x0), clampY(y0 + j), "|", props);
        view.DrawString(clampX(x0 + width - 1), clampY(y0 + j), "|", props);
    }
    if (title) {
        view.SetColor(CD_HILITE1);
        int len = (int)strlen(title);
        int tx = x0 + (width - len) / 2;
        if (tx < x0) tx = x0;
        view.DrawString(clampX(tx), clampY(y0), title, props);
    }
    view.SetColor(CD_NORMAL);
}

void UiDraw::DrawScrollIndicator(View &view, bool canScrollUp,
                                 bool canScrollDown) {
    GUITextProperties props;
    props.invert_ = false;
    view.SetColor(CD_HILITE1);
    if (canScrollUp) view.DrawString(38, 27, "^", props);
    if (canScrollDown) view.DrawString(38, 28, "v", props);
    view.SetColor(CD_NORMAL);
}

void UiDraw::DrawSeparator(View &view, int y, int x, int width) {
    GUITextProperties props;
    props.invert_ = false;
    view.SetColor(CD_BORDER);
    int cx = clampX(x);
    int cy = clampY(y);
    for (int i = 0; i < width; i++) {
        view.DrawString(cx + i, cy, "-", props);
    }
    view.SetColor(CD_NORMAL);
}