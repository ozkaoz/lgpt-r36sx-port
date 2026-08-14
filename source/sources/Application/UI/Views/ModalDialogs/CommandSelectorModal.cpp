#include "CommandSelectorModal.h"
#include "Application/Utils/HelpLegend.h"
#include "Application/Utils/char.h"

// TREEFROG_SELECTOR_FAMILIES_RC2 (RC2):
// The FX selector groups commands by functional family.  Page 1 (FX 1/2) is a
// 5-column grid with one family per column:
//   INST | FILTER | DELAY | REVERB | MASTER
//   NDL    FCU     DSE     RSE     CTH
//   PFT    FRS     DTM     RSZ     --
//   BCR    FCR     DFB     RDC     --
// Page 2 (FX 2/2) holds the legacy comb pair (LEGACY COMB: CFM / CFT).
// The tables below are row-major over kColumns; I_CMD_NONE marks empty cells,
// which navigateGrid always skips (a cell is never selected).  The
// CommandList::_specs_ order is untouched: only this selector reorders by
// family.  Projects that already contain FBMX/FBTN keep playing, show CFM/CFT
// and stay editable (the stored FourCC never changes).
static const FourCC kFamiliesLayout[] = {
    I_CMD_DLAY, I_CMD_FCUT, I_CMD_DLYS, I_CMD_RVBS, I_CMD_CMPT,
    I_CMD_PFIN, I_CMD_FRES, I_CMD_DLYT, I_CMD_RVSZ, I_CMD_NONE,
    I_CMD_CRSH, I_CMD_FLTR, I_CMD_DLYF, I_CMD_RVDC, I_CMD_NONE,
};

static const FourCC kLegacyLayout[] = {
    I_CMD_FBMX, I_CMD_NONE, I_CMD_NONE, I_CMD_NONE, I_CMD_NONE,
    I_CMD_FBTN, I_CMD_NONE, I_CMD_NONE, I_CMD_NONE, I_CMD_NONE,
};

static const int kFamiliesRows = 3;
static const int kLegacyRows = 2;

static const char *kFamilyHeaders[CommandSelectorCommon::kColumns] = {
    "INST", "FILTER", "DELAY", "REVERB", "MASTER",
};

CommandSelectorModal::CommandSelectorModal(View &parentView,
                                           FourCC *liveTarget,
                                           ModalViewCallback previewCb):
    ModalView(parentView),
    page_(PAGE_FAMILIES),
    selectedRow_(0),
    selectedCol_(0),
    selectedCommand_(I_CMD_NONE),
    parentView_(parentView),
    liveTarget_(liveTarget),
    savedCmd_(liveTarget ? *liveTarget : I_CMD_NONE),
    previewCb_(previewCb) {
    FourCC initial = liveTarget_ ? *liveTarget_ : I_CMD_NONE;
    moveToCommand(initial != I_CMD_NONE ? initial : kFamiliesLayout[0]);
}

CommandSelectorModal::~CommandSelectorModal() {}

int CommandSelectorModal::contentRows(int page) const {
    return (page == PAGE_LEGACY) ? kLegacyRows : kFamiliesRows;
}

int CommandSelectorModal::popupRows() const {
    // title + header + content rows (page 1) or title + subtitle + content
    // rows (page 2).
    return contentRows(page_) + 2;
}

FourCC CommandSelectorModal::cellAtGridPos(int page, int row, int col) const {
    if (row < 0 || row >= contentRows(page) || col < 0 ||
        col >= GRID_COLUMNS) {
        return I_CMD_NONE;
    }
    const FourCC *layout =
        (page == PAGE_LEGACY) ? kLegacyLayout : kFamiliesLayout;
    return layout[row * GRID_COLUMNS + col];
}

void CommandSelectorModal::moveToCommand(FourCC command) {
    for (int p = 0; p < PAGE_COUNT; p++) {
        for (int r = 0; r < contentRows(p); r++) {
            for (int c = 0; c < GRID_COLUMNS; c++) {
                if (cellAtGridPos(p, r, c) == command) {
                    page_ = p;
                    selectedRow_ = r;
                    selectedCol_ = c;
                    selectedCommand_ = command;
                    return;
                }
            }
        }
    }
    // Not on the selector (hidden command): fall back to the first cell.
    page_ = PAGE_FAMILIES;
    selectedRow_ = 0;
    selectedCol_ = 0;
    selectedCommand_ = kFamiliesLayout[0];
}

void CommandSelectorModal::navigateGrid(int deltaRow, int deltaCol) {
    int newRow = selectedRow_;
    int newCol = selectedCol_;
    int newPage = page_;
    // Generous bound: covers cross-page scans on the 5x3 grid.
    int tries = 6 * GRID_COLUMNS;

    while (tries-- > 0) {
        newRow += deltaRow;
        newCol += deltaCol;

        // Crossing a horizontal edge switches page (pages chain in a loop).
        if (newCol < 0) {
            newPage = (newPage - 1 + PAGE_COUNT) % PAGE_COUNT;
            newCol = GRID_COLUMNS - 1;
        } else if (newCol >= GRID_COLUMNS) {
            newPage = (newPage + 1) % PAGE_COUNT;
            newCol = 0;
        }

        int rows = contentRows(newPage);
        if (newRow < 0) {
            newRow = rows - 1;
        } else if (newRow >= rows) {
            newRow = 0;
        }
        if (newRow < 0) {
            newRow = 0;
        }

        FourCC candidate = cellAtGridPos(newPage, newRow, newCol);
        if (candidate != I_CMD_NONE) {
            page_ = newPage;
            selectedRow_ = newRow;
            selectedCol_ = newCol;
            selectedCommand_ = candidate;
            if (liveTarget_) {
                *liveTarget_ = selectedCommand_;
            }
            if (previewCb_) {
                previewCb_(parentView_, *this);
            }
            return;
        }
    }
}

void CommandSelectorModal::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) {
        return;
    }

    if (mask & EPBM_UP) {
        navigateGrid(-1, 0);
        isDirty_ = true;
    } else if (mask & EPBM_DOWN) {
        navigateGrid(1, 0);
        isDirty_ = true;
    } else if (mask & EPBM_LEFT) {
        navigateGrid(0, -1);
        isDirty_ = true;
    } else if (mask & EPBM_RIGHT) {
        navigateGrid(0, 1);
        isDirty_ = true;
    } else if (mask & EPBM_A) {
        EndModal(1);  // Confirm selection
    } else if (mask & EPBM_B) {
        if (liveTarget_) {
            *liveTarget_ = savedCmd_;
        }
        EndModal(0);  // Cancel
    }
}

void CommandSelectorModal::drawContentRows(int firstY, GUITextProperties &props) {
    char cellStr[6];
    int rows = contentRows(page_);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < GRID_COLUMNS; c++) {
            FourCC cmd = cellAtGridPos(page_, r, c);
            if (cmd == I_CMD_NONE) {
                continue;
            }
            bool isSelected = (r == selectedRow_ && c == selectedCol_);

            getCommandDisplayName(cmd, cellStr);
            cellStr[4] = 0;

            props.invert_ = isSelected;
            SetColor(isSelected ? CD_HILITE2 : CD_NORMAL);
            DrawString(c * CommandSelectorCommon::kCellPitch, firstY + r,
                       cellStr, props);
        }
    }
    props.invert_ = false;
}

void CommandSelectorModal::DrawView() {
    int width = GRID_COLUMNS * CommandSelectorCommon::kCellPitch;
    SetWindow(width, popupRows());

    GUITextProperties props;
    props.invert_ = false;

    if (page_ == PAGE_LEGACY) {
        SetColor(CD_HILITE1);
        DrawString((width - 6) / 2, 0, "FX 2/2", props);
        SetColor(CD_NORMAL);
        DrawString((width - 11) / 2, 1, "LEGACY COMB", props);
    } else {
        SetColor(CD_HILITE1);
        DrawString((width - 6) / 2, 0, "FX 1/2", props);
        SetColor(CD_NORMAL);
        for (int c = 0; c < GRID_COLUMNS; c++) {
            DrawString(c * CommandSelectorCommon::kCellPitch, 1,
                       kFamilyHeaders[c], props);
        }
    }

    drawContentRows(2, props);

    SetColor(CD_NORMAL);

    std::string *cmdStr = getHelpLegend(selectedCommand_);
    for (int i = 0; i < 3; i++) {
        // Clear legend area first so shorter lines don't leave stale text.
        View::DrawString(10, i, "                              ", props);
        View::DrawString(10, i, cmdStr[i].c_str(), props);
    }
}

void CommandSelectorModal::OnPlayerUpdate(PlayerEventType, unsigned int) {}
void CommandSelectorModal::OnFocus() {}
