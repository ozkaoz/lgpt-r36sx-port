#ifndef _COMMAND_SELECTOR_COMMON_H_
#define _COMMAND_SELECTOR_COMMON_H_

#include "Application/Instruments/CommandList.h"
#include "Application/UI/Views/BaseClasses/View.h"

namespace CommandSelectorCommon {

static const int kColumns = 5;

// TREEFROG_SELECTOR_FAMILIES_RC2 (RC2): the family header row of the FX
// selector needs 6 chars per column (INST FILTER DELAY REVERB MASTER), so the
// cell pitch grew from 5 to 6.  This keeps every column exactly under its
// family header.
static const int kCellPitch = 6;
static const int kScreenCenterX = 20;
inline int getPopupInnerWidth() { return kColumns * kCellPitch; }
inline int getPopupStartX() { return kScreenCenterX - (getPopupInnerWidth() / 2); }
inline int getDisplayCount() { return CommandList::GetCount() - 1; }

// TREEFROG_SELECTOR_FAMILIES_RC2 (RC2): the FX selector is paginated
// (FX 1/2 = families, FX 2/2 = legacy comb).  Page 1 is the tallest popup:
// title + family header + 3 command rows = 5.  Return that max so the play
// cursor / live indicators are always suppressed over the whole popup.
inline int getRows() {
    return 5;
}

inline int getPopupStartY(const GUIPoint &anchor) {
    int startY = anchor._y + (16 - getRows()) / 2;
    if (startY < anchor._y) {
        startY = anchor._y;
    }
    return startY;
}

/*
* Used to not overwrite the popup with the play cursor
*/
inline bool popupContainsPoint(const GUIPoint &anchor, int x, int y) {
    int startX = getPopupStartX() - 1;
    int endX = getPopupStartX() + getPopupInnerWidth();
    int startY = getPopupStartY(anchor) - 4; // Magic number for top border
    int endY = getPopupStartY(anchor) + getRows() - 1; // Magic number for bottom border
    return x >= startX && x <= endX && y >= startY && y <= endY;
}

inline bool isCommandColumn(int col, int c1, int c2) {
    return col == c1 || col == c2;
}

inline bool isCommandColumn(int col, int c1, int c2, int c3) {
    return col == c1 || col == c2 || col == c3;
}

inline FourCC *getCommandPointerByCol(int col, int c1, FourCC *p1, int c2,
                                      FourCC *p2) {
    if (col == c1) {
        return p1;
    }
    if (col == c2) {
        return p2;
    }
    return 0;
}

inline FourCC *getCommandPointerByCol(int col, int c1, FourCC *p1, int c2,
                                      FourCC *p2, int c3, FourCC *p3) {
    if (col == c1) {
        return p1;
    }
    if (col == c2) {
        return p2;
    }
    if (col == c3) {
        return p3;
    }
    return 0;
}

} // namespace CommandSelectorCommon

#endif
