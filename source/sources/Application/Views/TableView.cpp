// TREEFROG_V42_NO_WHITE_BOX_UI
// TREEFROG_TABLE_COLUMNS_V1 (H38.5): FX3 column removed, 4 columns left
// (FX1 P1 FX2 P2). Grid centered on the 40-cell screen: row numbers at
// anchor-1, columns at anchor+2/+7/+12/+17. Block spans cells 9..30 with
// symmetric margins.
#define TABLE_GRID_BASE_OFFSET 2
#define TABLE_COL_PITCH 5
#define TABLE_COL_COUNT 4
#define TABLE_HEADER_X1 13
#define TABLE_HEADER_P1 18
#define TABLE_HEADER_X2 23
#define TABLE_HEADER_P2 28
#include "TableView.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Player/TablePlayback.h"
#include "Application/Utils/HelpLegend.h"
#include "Application/Utils/char.h"
#include "Application/Views/BaseClasses/UiColors.h"
#include "Application/Views/CommandSelectorCommon.h"
#include "Application/Views/ModalDialogs/CommandSelectorModal.h"
#include <string.h>

#define FCC_EDIT MAKE_FOURCC('T', 'B', 'E', 'D')

static void CommandSelectorCallback(View &v, ModalView &d) {
    ((TableView &)v).onCommandSelectorResult(d);
}

static void CommandSelectorPreviewCallback(View &v, ModalView &d) {
    ((TableView &)v).onCommandSelectorPreview(d);
}

TableView::TableView(GUIWindow &w, ViewData *viewData)
    : View(w, viewData), cmdEdit_("edit", FCC_EDIT, 0) {
    row_ = 0;
    col_ = 0;
    GUIPoint pos(0, 10);
    cmdEditField_ =
        new UIBigHexVarField(pos, cmdEdit_, 4, "%4.4X", 0, 0xFFFF, 16, true);

    lastVol_ = 0;
    lastTick_ = 0;
    lastTsp_ = 0;
    lastCmd_ = I_CMD_NONE;
    lastParam_ = 0;
    commandSelectorModalActive_ = false;

    clipboard_.active_ = false;
    clipboard_.width_ = 0;
    clipboard_.height_ = 0;
    tableUndoCount_ = 0;
    tableRedoCount_ = 0;
}

TableView::~TableView() {}

// TREEFROG_COMMAND_SPECS_V1 (Fase 6): adapt the shared hex editor to the
// command in the cell's cmd column (HEX4 legacy / HEX8 Fase 4 FX).
void TableView::applyCmdEditModeForCommand(FourCC command) {
	cmdEditField_->SetHexMode(CommandList::GetParamPrecision(command),
	                         CommandList::GetParamFormatString(command),
	                         CommandList::GetParamMin(command),
	                         CommandList::GetParamMax(command),
	                         CommandList::GetParamWrap(command)) ;
}

void TableView::applyCmdEditMode(int paramCol) {
	Table &table =
	    TableHolder::GetInstance()->GetTable(viewData_->currentTable_) ;
	FourCC *cmd = 0 ;
	if (paramCol == 1) {
		cmd = table.cmd1_ + row_ ;
	} else if (paramCol == 3) {
		cmd = table.cmd2_ + row_ ;
	}
	if (cmd) applyCmdEditModeForCommand(*cmd) ;
}

void TableView::OnFocus() {
    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    lastPosition_[0] = lastPosition_[1] = lastPosition_[2] = 0;
    updateCursor(0, 0);
};

void TableView::cutPosition() {

    clipboard_.active_ = true;
    clipboard_.row_ = row_;
    clipboard_.col_ = col_;
    saveRow_ = row_;
    saveCol_ = col_;

    if ((col_ == 0) || (col_ == 2))
        col_ += 1; // This way, A+B on note cuts
                   // the instruments too and parameters get cut with commands
    cutSelection();
};

GUIRect TableView::getSelectionRect() {
    GUIRect r(clipboard_.col_, clipboard_.row_, col_, row_);
    r.Normalize();
    return r;
};

void TableView::fillClipboardData() {

    // Get Current normalized selection rect

    GUIRect selRect = getSelectionRect();

    // Get size & store in clipboard

    clipboard_.width_ = selRect.Width() + 1;
    clipboard_.height_ = selRect.Height() + 1;
    clipboard_.row_ = selRect.Top();
    clipboard_.col_ = selRect.Left();

    // Copy the data

    Table &table =
        TableHolder::GetInstance()->GetTable(viewData_->currentTable_);

    uint *src1 = table.cmd1_;
    uint *dst1 = clipboard_.cmd1_;
    ushort *src2 = table.param1_;
    ushort *dst2 = clipboard_.param1_;
    uint *src3 = table.cmd2_;
    uint *dst3 = clipboard_.cmd2_;
    ushort *src4 = table.param2_;
    ushort *dst4 = clipboard_.param2_;

    for (int i = 0; i < clipboard_.height_; i++) {
        dst1[i] = src1[clipboard_.row_ + i];
        dst2[i] = src2[clipboard_.row_ + i];
        dst3[i] = src3[clipboard_.row_ + i];
        dst4[i] = src4[clipboard_.row_ + i];
    };
    updateCursor(0, 0);
};

void TableView::extendSelection() {
    GUIRect rect = getSelectionRect();
    if (rect.Left() > 0 || rect.Right() < 3) {
        if (col_ < clipboard_.col_) {
            col_ = 0;
            clipboard_.col_ = 3;
        } else {
            col_ = 3;
            clipboard_.col_ = 0;
        }
        isDirty_ = true;
    } else {
        if (row_ < clipboard_.row_) {
            row_ = 0;
            clipboard_.row_ = 15;
        } else {
            clipboard_.row_ = 0;
            row_ = 15;
        }
        isDirty_ = true;
    }
}

/******************************************************
 interpolateSelection:
        expands the lowest value of selection to the highest
 ******************************************************/
void TableView::interpolateSelection() {

    pushTableUndo();

    if (!clipboard_.active_) {
        return;
    }

    GUIRect rect = getSelectionRect();
    
    // Only interpolate if we're in param columns (1, 3)
    int col = rect.Left();
    if (col != rect.Right() || (col != 1 && col != 3)) {
        return;
    }

    int startRow = rect.Top();
    int endRow = rect.Bottom();
    
    // Need at least 2 rows to interpolate
    if (endRow - startRow < 1) {
        return;
    }

    Table &table = TableHolder::GetInstance()->GetTable(viewData_->currentTable_);

    ushort *paramData;
    if (col == 1) {
        paramData = table.param1_;
    } else {
        paramData = table.param2_;
    }

    ushort startParam = paramData[startRow];
    ushort endParam = paramData[endRow];

    int numSteps = endRow - startRow;
    int paramDiff = (int)endParam - (int)startParam;

    for (int step = 0; step <= numSteps; step++) {
        int row = startRow + step;
        int value = startParam + (paramDiff * step) / (numSteps);
        paramData[row] = (ushort)value;
    }

    isDirty_ = true;
}

void TableView::copySelection() {

    // Keep up with row,col of selection coz
    // fillClipboardData will trash it

    fillClipboardData();

    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    row_ = saveRow_;
    col_ = saveCol_;

    isDirty_ = true;
};

void TableView::cutSelection() {

    pushTableUndo();

    // Keep up with row,col of selection coz
    // fillClipboardData will trash it

    fillClipboardData();

    // Loop over selection col, row & clear data inside it

    Table &table =
        TableHolder::GetInstance()->GetTable(viewData_->currentTable_);
    uint *dst1 = table.cmd1_;
    ushort *dst2 = table.param1_;
    uint *dst3 = table.cmd2_;
    ushort *dst4 = table.param2_;

    for (int i = 0; i < clipboard_.width_; i++) {
        for (int j = 0; j < clipboard_.height_; j++) {
            switch (i + clipboard_.col_) {
            case 0:
                dst1[j + clipboard_.row_] = I_CMD_NONE;
                break;
            case 1:
                dst2[j + clipboard_.row_] = 0x0000;
                break;
            case 2:
                dst3[j + clipboard_.row_] = I_CMD_NONE;
                break;
            case 3:
                dst4[j + clipboard_.row_] = 0x0000;
                break;
            }
        }
    }

    // Clear selection, end selection process & reposition cursor

    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    row_ = saveRow_;
    col_ = saveCol_;
    updateCursor(0, 0);
    isDirty_ = true;
};

/******************************************************
 pasteClipboard:
        copies data in the clipboard to the current step
 ******************************************************/

void TableView::pasteClipboard() {

    pushTableUndo();

    // Get number of row to paste

    int height = clipboard_.height_;
    /*    if (row_+height>16) {
            height=16-row_ ;
        }
      */
    Table &table =
        TableHolder::GetInstance()->GetTable(viewData_->currentTable_);

    uint *dst1 = table.cmd1_;
    uint *src1 = clipboard_.cmd1_;
    ushort *dst2 = table.param1_;
    ushort *src2 = clipboard_.param1_;
    uint *dst3 = table.cmd2_;
    uint *src3 = clipboard_.cmd2_;
    ushort *dst4 = table.param2_;
    ushort *src4 = clipboard_.param2_;

    uint *noCmd = (uint *)-1;
    ushort *noPrm = (ushort *)-1;
    uint *srcCmd[4] = {src1, noCmd, src3, noCmd};
    ushort *srcPrm[4] = {noPrm, src2, noPrm, src4};
    uint *dstCmd[4] = {dst1, noCmd, dst3, noCmd};
    ushort *dstPrm[4] = {noPrm, dst2, noPrm, dst4};

    bool wasUpdated = false;

    for (int i = 0; i < clipboard_.width_; i++) {
        for (int j = 0; j < height; j++) {
            switch (i + clipboard_.col_) {
            case 0:
            case 2:
                if ((col_ + i) == 0 || (col_ + i) == 2) { // Don't allow commands in params, etc
                    dstCmd[col_ + i][(row_ + j) % 16] =
                        srcCmd[clipboard_.col_ + i][j];
                    wasUpdated = true;
                }
                break;
            case 1:
            case 3:
                if ((col_ + i) == 1 || (col_ + i) == 3) {
                    dstPrm[col_ + i][(row_ + j) % 16] =
                        srcPrm[clipboard_.col_ + i][j];
                    wasUpdated = true;
                }
                break;
            }
        }
    }
    if (wasUpdated) {
        updateCursor(0x00, ((row_ + height) % 16 - row_));
        isDirty_ = true;
    }
};

void TableView::updateCursor(int dx, int dy) {

    col_ += dx;
    row_ += dy;
    if (col_ > 3)
        col_ = 3;
    if (col_ < 0)
        col_ = 0;
    if (row_ > 15)
        row_ = 15;
    if (row_ < 0)
        row_ = 0;

    Table &table =
        TableHolder::GetInstance()->GetTable(viewData_->currentTable_);

    // RC6: the grid is centered vertically on 30 rows (anchor +3).
    GUIPoint anchor = GetAnchor();
    anchor._y += 3 ;
    GUIPoint p(anchor);
    switch (col_) {
    case 1:
        p._x += TABLE_GRID_BASE_OFFSET + TABLE_COL_PITCH;
        p._y += row_;
        cmdEditField_->SetPosition(p);
        applyCmdEditMode(1);
        cmdEdit_.SetInt(*(table.param1_ + row_));
        break;
    case 3:
        p._x += TABLE_GRID_BASE_OFFSET + 3 * TABLE_COL_PITCH;
        p._y += row_;
        cmdEditField_->SetPosition(p);
        applyCmdEditMode(3);
        cmdEdit_.SetInt(*(table.param2_ + row_));
        break;
    };

    isDirty_ = true;
};

void TableView::warpToNeighbour(int dir) {

    int current = viewData_->currentTable_ + dir;

    if (current >= TABLE_COUNT) {
        current -= TABLE_COUNT;
    }
    if (current < 0) {
        current += TABLE_COUNT;
    }
    viewData_->currentTable_ = current;
    updateCursor(0, 0);
    isDirty_ = true;
}

void TableView::updateCursorValue(int offset) {

    pushTableUndo();

    unsigned char *c = 0;
    unsigned char limit = 0;
    bool wrap = false;
    FourCC *cc;

    Table &table =
        TableHolder::GetInstance()->GetTable(viewData_->currentTable_);

    switch (col_) {
    case 0:
        cc = table.cmd1_ + row_;
        switch (offset) {
        case 0x01:
            *cc = CommandList::GetNext(*cc);
            break;
        case 0x10:
            *cc = CommandList::GetNextAlpha(*cc);
            break;
        case -0x01:
            *cc = CommandList::GetPrev(*cc);
            break;
        case -0x10:
            *cc = CommandList::GetPrevAlpha(*cc);
            break;
        }
        lastCmd_ = *cc;
        break;

    case 1:
        applyCmdEditMode(1);
        switch (offset) {
        case 0x01:
            cmdEditField_->ProcessArrow(EPBM_RIGHT);
            break;
        case 0x10:
            cmdEditField_->ProcessArrow(EPBM_UP);
            break;
        case -0x01:
            cmdEditField_->ProcessArrow(EPBM_LEFT);
            break;
        case -0x10:
            cmdEditField_->ProcessArrow(EPBM_DOWN);
            break;
        }
        *(table.param1_ + row_) = cmdEdit_.GetInt();
        lastParam_ = cmdEdit_.GetInt();
        break;

    case 2:
        cc = table.cmd2_ + row_;
        switch (offset) {
        case 0x01:
            *cc = CommandList::GetNext(*cc);
            break;
        case 0x10:
            *cc = CommandList::GetNextAlpha(*cc);
            break;
        case -0x01:
            *cc = CommandList::GetPrev(*cc);
            break;
        case -0x10:
            *cc = CommandList::GetPrevAlpha(*cc);
            break;
        }
        lastCmd_ = *cc;
        break;
    case 3:
        applyCmdEditMode(3);
        switch (offset) {
        case 0x01:
            cmdEditField_->ProcessArrow(EPBM_RIGHT);
            break;
        case 0x10:
            cmdEditField_->ProcessArrow(EPBM_UP);
            break;
        case -0x01:
            cmdEditField_->ProcessArrow(EPBM_LEFT);
            break;
        case -0x10:
            cmdEditField_->ProcessArrow(EPBM_DOWN);
            break;
        }
        *(table.param2_ + row_) = cmdEdit_.GetInt();
        lastParam_ = cmdEdit_.GetInt();
        break;
    }
    if (c) {
        updateData(c, offset, limit, wrap);
        switch (col_) {
        case 0:
            lastVol_ = *c;
            break;
        case 1:
            lastTick_ = *c;
            break;
        case 2:
            lastTsp_ = *c;
            break;
        }
    }
    isDirty_ = true;
}

bool TableView::isCommandColumn() const {
    return CommandSelectorCommon::isCommandColumn(col_, 0, 2);
}

FourCC *TableView::getCurrentCommandPointer() {
    Table &table =
        TableHolder::GetInstance()->GetTable(viewData_->currentTable_);
    return CommandSelectorCommon::getCommandPointerByCol(
        col_, 0, table.cmd1_ + row_, 2, table.cmd2_ + row_);
}

void TableView::enterCommandSelector() {

    pushTableUndo();

    FourCC *cmdPtr = getCurrentCommandPointer();
    if (!cmdPtr) return;
    commandSelectorModalActive_ = true;
    DoModal(new CommandSelectorModal(*this, cmdPtr, CommandSelectorPreviewCallback),
            CommandSelectorCallback);
}

void TableView::onCommandSelectorResult(ModalView &d) {
    commandSelectorModalActive_ = false;
    CommandSelectorModal &modal = (CommandSelectorModal &)d;
    if (modal.GetReturnCode() == 1) {
        FourCC *cmd = getCurrentCommandPointer();
        if (cmd) {
            lastCmd_ = *cmd;
        }
    }
    isDirty_ = true;
}

void TableView::onCommandSelectorPreview(ModalView &) { isDirty_ = true; }

void TableView::pasteLast() {

    pushTableUndo();

    uint *i = 0;

    Table &table =
        TableHolder::GetInstance()->GetTable(viewData_->currentTable_);

    switch (col_) {
    case 0:
        i = table.cmd1_ + row_;
        if (*i == I_CMD_NONE) {
            *i = lastCmd_;
            isDirty_ = true;
        } else {
            lastCmd_ = *i;
        }
        break;

    case 1:
        break;

    case 2:
        i = table.cmd2_ + row_;
        if (*i == I_CMD_NONE) {
            *i = lastCmd_;
            isDirty_ = true;
        } else {
            lastCmd_ = *i;
        }
        break;

    case 3:
        break;
    }
};

void TableView::ProcessButtonMask(unsigned short mask, bool pressed) {

    if (!pressed) {
        return;
    }
    // TREEFROG_GLOBAL_UNDO_V5 (Bacon 1.1.1 V14): undo snapshots are now
    // captured inside the real edit mutations (updateCursorValue/paste/cut/
    // interpolate/command selector), so navigation no longer pollutes the
    // history.
    if (viewMode_ == VM_SELECTION) {
        if (!clipboard_.active_) {
            clipboard_.active_ = true;
            clipboard_.col_ = col_;
            clipboard_.row_ = row_;
            saveCol_ = col_;
            saveRow_ = row_;
        }
        processSelectionButtonMask(mask);
    } else {
        processNormalButtonMask(mask);
    };
}

// TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): table snapshot undo/redo.
void TableView::pushTableUndo() {
    Table &table = TableHolder::GetInstance()->GetTable(viewData_->currentTable_);
    TableEdit e;
    memcpy(e.cmd1, table.cmd1_, sizeof(e.cmd1));
    memcpy(e.param1, table.param1_, sizeof(e.param1));
    memcpy(e.cmd2, table.cmd2_, sizeof(e.cmd2));
    memcpy(e.param2, table.param2_, sizeof(e.param2));
    memcpy(e.cmd3, table.cmd3_, sizeof(e.cmd3));
    memcpy(e.param3, table.param3_, sizeof(e.param3));
    e.currentTable = (uchar)viewData_->currentTable_;
    e.row = (uchar)row_;
    e.col = (uchar)col_;
    for (int i = kTableHistorySize - 1; i > 0; i--) tableUndo_[i] = tableUndo_[i - 1];
    tableUndo_[0] = e;
    tableUndoCount_++;
    if (tableUndoCount_ > kTableHistorySize) tableUndoCount_ = kTableHistorySize;
    tableRedoCount_ = 0;
}

static void tableUndoRestore(const TableView::TableEdit &e, ViewData *viewData) {
    Table &table = TableHolder::GetInstance()->GetTable(e.currentTable);
    memcpy(table.cmd1_, e.cmd1, sizeof(e.cmd1));
    memcpy(table.param1_, e.param1, sizeof(e.param1));
    memcpy(table.cmd2_, e.cmd2, sizeof(e.cmd2));
    memcpy(table.param2_, e.param2, sizeof(e.param2));
    memcpy(table.cmd3_, e.cmd3, sizeof(e.cmd3));
    memcpy(table.param3_, e.param3, sizeof(e.param3));
    viewData->currentTable_ = e.currentTable;
}

bool TableView::GlobalUndo() {
    if (tableUndoCount_ == 0) return true;
    TableEdit e = tableUndo_[0];
    for (int i = 0; i < tableUndoCount_ - 1; i++) tableUndo_[i] = tableUndo_[i + 1];
    tableUndoCount_--;
    for (int i = kTableHistorySize - 1; i > 0; i--) tableRedo_[i] = tableRedo_[i - 1];
    tableRedo_[0] = e;
    tableRedoCount_++;
    if (tableRedoCount_ > kTableHistorySize) tableRedoCount_ = kTableHistorySize;
    tableUndoRestore(e, viewData_);
    row_ = e.row;
    col_ = e.col;
    isDirty_ = true;
    return true;
}

bool TableView::GlobalRedo() {
    if (tableRedoCount_ == 0) return true;
    TableEdit e = tableRedo_[0];
    for (int i = 0; i < tableRedoCount_ - 1; i++) tableRedo_[i] = tableRedo_[i + 1];
    tableRedoCount_--;
    for (int i = kTableHistorySize - 1; i > 0; i--) tableUndo_[i] = tableUndo_[i - 1];
    tableUndo_[0] = e;
    tableUndoCount_++;
    if (tableUndoCount_ > kTableHistorySize) tableUndoCount_ = kTableHistorySize;
    tableUndoRestore(e, viewData_);
    row_ = e.row;
    col_ = e.col;
    isDirty_ = true;
    return true;
}

void TableView::processNormalButtonMask(unsigned short mask) {

    Player *player = Player::GetInstance();

    if (mask & EPBM_B) {
        if (mask & EPBM_LEFT)
            warpToNeighbour(-1);
        if (mask & EPBM_RIGHT)
            warpToNeighbour(+1);
        if (mask & EPBM_DOWN)
            warpToNeighbour(-16);
        if (mask & EPBM_UP)
            warpToNeighbour(16);
        if (mask & EPBM_A)
            cutPosition();
        if (mask & EPBM_L)
            viewMode_ = VM_SELECTION;

    } else {

        // A modifier

        if (mask & EPBM_A) {
            if (mask & EPBM_DOWN) {
                if (isCommandColumn())
                    enterCommandSelector();
                else
                    updateCursorValue(-0x10);
            }
            if (mask & EPBM_UP) {
                if (isCommandColumn())
                    enterCommandSelector();
                else
                    updateCursorValue(0x10);
            }
            if (mask & EPBM_LEFT)
                updateCursorValue(-0x01);
            if (mask & EPBM_RIGHT)
                updateCursorValue(0x01);
            if (mask == EPBM_A)
                pasteLast();
            if (mask & EPBM_L)
                pasteClipboard();
        } else {

            // R Modifier

            if (mask & EPBM_R) {
                if (mask & EPBM_UP) {
                    ViewType vt =
                        (viewType_ == VT_TABLE ? VT_PHRASE : VT_INSTRUMENT);
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }
                if (mask & EPBM_LEFT) {
                    if (viewType_ == VT_TABLE2) {
                        ViewType vt = VT_TABLE;
                        ViewEvent ve(VET_SWITCH_VIEW, &vt);
                        SetChanged();
                        NotifyObservers(&ve);
                    }
                }
                if (mask & EPBM_RIGHT) {
                    if (viewType_ == VT_TABLE) {
                        ViewType vt = VT_TABLE2;
                        ViewEvent ve(VET_SWITCH_VIEW, &vt);
                        SetChanged();
                        NotifyObservers(&ve);
                    }
                }
                if (mask & EPBM_START) {
                    player->OnStartButton(PM_PHRASE, viewData_->songX_, true,
                                          viewData_->chainRow_);
                }

            } else {
                // L MOdifier
                if (mask & EPBM_R) {
                } else {

                    // X Modifier (TREEFROG_NAV_X_DIR Bacon 1.1.1):
                    // quick page navigation: X+UP/DOWN jumps 4 rows,
                    // X+LEFT/RIGHT jumps 2 columns.

                    if (mask & EPBM_X) {
                        if (mask & EPBM_DOWN)
                            updateCursor(0, 4);
                        if (mask & EPBM_UP)
                            updateCursor(0, -4);
                        if (mask & EPBM_LEFT)
                            updateCursor(-2, 0);
                        if (mask & EPBM_RIGHT)
                            updateCursor(2, 0);
                    } else {

                    // No modifier

                    if (mask & EPBM_DOWN)
                        updateCursor(0, 1);
                    if (mask & EPBM_UP)
                        updateCursor(0, -1);
                    if (mask & EPBM_LEFT)
                        updateCursor(-1, 0);
                    if (mask & EPBM_RIGHT)
                        updateCursor(1, 0);
                    if (mask & EPBM_START) {
                        player->OnStartButton(PM_PHRASE, viewData_->songX_,
                                              false, viewData_->chainRow_);
                    }
                    }
                }
            }
        }
    }
}

void TableView::processSelectionButtonMask(unsigned short mask) {

    Player *player = Player::GetInstance();

    if (mask & EPBM_B) {
        if (mask & EPBM_L) {
            extendSelection();
        } else if (mask & EPBM_R) {
            interpolateSelection();
        } else {
            copySelection();
        }
    } else {

        // A Modifer

        if (mask & EPBM_A) {
            if (mask & EPBM_L)
                cutSelection();
            //		if (mask&EPBM_R) switchSoloMode() ;
        } else {

            // R Modifier

            if (mask & EPBM_R) {
                if (mask & EPBM_UP) {
                    ViewType vt = VT_PHRASE;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }
                if (mask & EPBM_START) {
                    player->OnStartButton(PM_PHRASE, viewData_->songX_, true,
                                          viewData_->chainRow_);
                }
                /*			if (mask&EPBM_L) unMuteAll() ;
                 */
            } else {
                // L Modifier
                if (mask & EPBM_L) {

                } else {
                    // X Modifier (TREEFROG_NAV_X_DIR Bacon 1.1.1):
                    // quick page navigation: X+UP/DOWN jumps 4 rows,
                    // X+LEFT/RIGHT jumps 2 columns.

                    if (mask & EPBM_X) {
                        if (mask & EPBM_DOWN)
                            updateCursor(0, 4);
                        if (mask & EPBM_UP)
                            updateCursor(0, -4);
                        if (mask & EPBM_LEFT)
                            updateCursor(-2, 0);
                        if (mask & EPBM_RIGHT)
                            updateCursor(2, 0);
                    } else {
                    // No modifier

                    if (mask & EPBM_DOWN)
                        updateCursor(0, 1);
                    if (mask & EPBM_UP)
                        updateCursor(0, -1);
                    if (mask & EPBM_LEFT)
                        updateCursor(-1, 0);
                    if (mask & EPBM_RIGHT)
                        updateCursor(1, 0);
                    if (mask & EPBM_START) {
                        player->OnStartButton(PM_PHRASE, viewData_->songX_,
                                              false, viewData_->chainRow_);
                    }
                    }
                }
            }
        }
    }
}

void TableView::setTextProps(GUITextProperties &props, int row, int col,
                             bool restore) {

    bool invert = false;

    if (clipboard_.active_) {
        GUIRect selRect = getSelectionRect();
        if ((row >= selRect.Left()) && (row <= selRect.Right()) &&
            (col >= selRect.Top()) && (col <= selRect.Bottom())) {
            invert = true;
        }
    } else {
        if ((col_ == row) && (row_ == col)) {
            invert = true;
        }
    }

    if (invert) {
        if (restore) {
            SetColor(CD_NORMAL);
            props.invert_ = false;
        } else {
            // TREEFROG_UI_TABLE_SELECTION_SONG_STYLE
            // Selección validada con la misma semántica visual que Song:
            // CD_HILITE2 + invert_.
            SetColor(CD_HILITE2);
            props.invert_ = true;
        }
    }
}

void TableView::DrawView() {

    Clear();

    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();

    Table &table =
        TableHolder::GetInstance()->GetTable(viewData_->currentTable_);

    // Draw title

    char title[20];
    // RC4 P3 (PLAN_RC4): page titles render with the semantic title role.
    SetColor(UiColors::Resolve(UI_COLOR_TITLE));
    sprintf(title, "Table %2.2x", viewData_->currentTable_);
    DrawString(pos._x, pos._y, title, props);

    // Compute song grid location

    // RC6: the grid is centered vertically on 30 rows (anchor +3).
    GUIPoint anchor = GetAnchor();
    anchor._y += 3 ;

    // Column headers, centered over their columns (TREEFROG_TABLE_COLUMNS_V1)

    static const char *tHeaders[TABLE_COL_COUNT] = {"FX1", "P1", "FX2", "P2"};
    static const int tHeaderX[TABLE_COL_COUNT] = {TABLE_HEADER_X1,
                                                  TABLE_HEADER_P1,
                                                  TABLE_HEADER_X2,
                                                  TABLE_HEADER_P2};
    for (int c = 0; c < TABLE_COL_COUNT; c++) {
        (c == col_) ? SetColor(CD_HILITE2) : SetColor(CD_NORMAL);
        DrawString(tHeaderX[c], anchor._y - 1, tHeaders[c], props);
    }
    SetColor(CD_NORMAL);

    // Display row numbers

    char buffer[6];
    pos = anchor;
    pos._x = anchor._x - 1;
    for (int j = 0; j < 16; j++) {
        ((j / altRowNumber_) % 2) ? SetColor(CD_ROW) : SetColor(CD_ROW2);
        hex2char(j, buffer);
        DrawString(pos._x, pos._y, buffer, props);
        pos._y++;
    }

    SetColor(CD_NORMAL);

    // Draw command 1

    pos = anchor;
    pos._x += TABLE_GRID_BASE_OFFSET;

    FourCC *f = table.cmd1_;

    buffer[4] = 0;

    for (int j = 0; j < 16; j++) {
        FourCC command = *f++;
        getCommandDisplayName(command, buffer);
        setTextProps(props, 0, j, false);
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 0, j, true);
        pos._y++;
        if (j == row_ && col_ == 0) {
            printHelpLegend(command, props);
        }
    }

    // Draw commands params 1

    pos = anchor;
    pos._x += TABLE_GRID_BASE_OFFSET + TABLE_COL_PITCH;

    ushort *param = table.param1_;
    FourCC *cf = table.cmd1_;
    buffer[5] = 0;

    for (int j = 0; j < 16; j++) {
        ushort p = *param++;
        FourCC cmd = *cf++;
        setTextProps(props, 1, j, false);
        // TREEFROG_COMMAND_SPECS_V1 (Fase 6): HEX8 for Fase 4 FX commands.
        if (CommandList::GetParamFormat(cmd) == CMD_PARAM_FORMAT_HEX8) {
            hex2char((unsigned char)(p & 0xFF), buffer);
        } else {
            hexshort2char(p, buffer);
        }
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 1, j, true);
        pos._y++;
    }

    // Draw commands 2

    pos = anchor;
    pos._x += TABLE_GRID_BASE_OFFSET + 2 * TABLE_COL_PITCH;

    f = table.cmd2_;

    buffer[4] = 0;

    for (int j = 0; j < 16; j++) {
        FourCC command = *f++;
        getCommandDisplayName(command, buffer);
        setTextProps(props, 2, j, false);
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 2, j, true);
        pos._y++;
        if (j == row_ && col_ == 2) {
            printHelpLegend(command, props);
        }
    }

    // Draw commands params

    pos = anchor;
    pos._x += TABLE_GRID_BASE_OFFSET + 3 * TABLE_COL_PITCH;

    param = table.param2_;
    cf = table.cmd2_;
    buffer[5] = 0;

    for (int j = 0; j < 16; j++) {
        ushort p = *param++;
        FourCC cmd = *cf++;
        setTextProps(props, 3, j, false);
        // TREEFROG_COMMAND_SPECS_V1 (Fase 6): HEX8 for Fase 4 FX commands.
        if (CommandList::GetParamFormat(cmd) == CMD_PARAM_FORMAT_HEX8) {
            hex2char((unsigned char)(p & 0xFF), buffer);
        } else {
            hexshort2char(p, buffer);
        }
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 3, j, true);
        pos._y++;
    }

    if ((viewMode_ != VM_SELECTION) &&
        ((col_ == 1) || (col_ == 3))) {
        cmdEditField_->SetFocus();
        cmdEditField_->Draw(w_);
    };

    drawMap();
    drawNotes();

    Player *player = Player::GetInstance();

    if (player->IsRunning()) {
        OnPlayerUpdate(PET_UPDATE);
    };
}

void TableView::OnPlayerUpdate(PlayerEventType eventType, unsigned int tick) {

    GUITextProperties props;
    // RC6: the grid is centered vertically on 30 rows (anchor +3).
    GUIPoint anchor = GetAnchor();
    anchor._y += 3 ;
    GUIPoint pos;

    pos._x = anchor._x - 1;
    pos._y = anchor._y + lastPosition_[0];
    if (!commandSelectorModalActive_ ||
        !CommandSelectorCommon::popupContainsPoint(anchor, pos._x, pos._y)) {
        DrawString(pos._x, pos._y, " ", props);
    }

    pos._x += 10;
    pos._y = anchor._y + lastPosition_[1];
    if (!commandSelectorModalActive_ ||
        !CommandSelectorCommon::popupContainsPoint(anchor, pos._x, pos._y)) {
        DrawString(pos._x, pos._y, " ", props);
    }

    pos._x += 10;
    pos._y = anchor._y + lastPosition_[2];
    if (!commandSelectorModalActive_ ||
        !CommandSelectorCommon::popupContainsPoint(anchor, pos._x, pos._y)) {
        DrawString(pos._x, pos._y, " ", props);
    }

    TableHolder *th = TableHolder::GetInstance();
    // Get current channel
    int channel = viewData_->songX_;
    // Table associated to the channel playerpb
    TablePlayback &tpb = TablePlayback::GetTablePlayback(channel);
    Table *playbackTable = tpb.GetTable();
    // Table we're viewing
    Table &viewTable = th->GetTable(viewData_->currentTable_);
    if (playbackTable == &viewTable && viewData_->playMode_ != PM_AUDITION) {

        lastPosition_[0] = tpb.GetPlaybackPosition(0);
        lastPosition_[1] = tpb.GetPlaybackPosition(1);
        lastPosition_[2] = tpb.GetPlaybackPosition(2);

        pos._x = anchor._x - 1;
        pos._y = anchor._y + lastPosition_[0];
        if (!commandSelectorModalActive_ ||
            !CommandSelectorCommon::popupContainsPoint(anchor, pos._x,
                                                       pos._y)) {
            SetColor(CD_PLAY);
            DrawString(pos._x, pos._y, ">", props);
        }

        pos._x += 10;
        pos._y = anchor._y + lastPosition_[1];
        if (!commandSelectorModalActive_ ||
            !CommandSelectorCommon::popupContainsPoint(anchor, pos._x,
                                                       pos._y)) {
            DrawString(pos._x, pos._y, ">", props);
        }

        pos._x += 10;
        pos._y = anchor._y + lastPosition_[2];
        if (!commandSelectorModalActive_ ||
            !CommandSelectorCommon::popupContainsPoint(anchor, pos._x,
                                                       pos._y)) {
            DrawString(pos._x, pos._y, ">", props);
        }
    };
    drawNotes();
}

void TableView::printHelpLegend(FourCC command, GUITextProperties props) {
    std::string *cmdStr = getHelpLegend(command);
    DrawString(10, 0, cmdStr[0].c_str(), props);
    DrawString(10, 1, cmdStr[1].c_str(), props);
    DrawString(10, 2, cmdStr[2].c_str(), props);
}
