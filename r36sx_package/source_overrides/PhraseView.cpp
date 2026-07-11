// TREEFROG_V42_NO_WHITE_BOX_UI
#include "PhraseView.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Model/Scale.h"
#include "Application/Model/Table.h"
#include "Application/Utils/HelpLegend.h"
#include "Application/Utils/char.h"
#include "Application/Views/CommandSelectorCommon.h"
#include "ModalDialogs/SampleChopperModal.h"
#include "Application/Views/ModalDialogs/CommandSelectorModal.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/InstrumentBank.h"
#include "UIController.h"
#include "Services/Time/TimeService.h"
#include "Application/Player/Player.h"
#include <stdlib.h>
#include <string.h>

short PhraseView::offsets_[7][4] = {
    {-1, 1, 12, -12},   // note
    {-1, 1, 16, -16},   // volume
    {-1, 1, 16, -16},   // instrument
    {0, 0, 0, 0},       // command 1
    {0, 0, 0, 0},       // parameter 1
    {0, 0, 0, 0},       // command 2
    {0, 0, 0, 0}        // parameter 2
};

static void CommandSelectorCallback(View &v, ModalView &d) {
    ((PhraseView &)v).onCommandSelectorResult(d);
}

static void CommandSelectorPreviewCallback(View &v, ModalView &d) {
    ((PhraseView &)v).onCommandSelectorPreview(d);
}

PhraseView::PhraseView(GUIWindow &w, ViewData *viewData)
    : View(w, viewData), cmdEdit_("edit", FCC_EDIT, 0) {
    phrase_ = viewData_->song_->phrase_;
    lastPlayingPos_ = 0;
    GUIPoint pos(0, 10);
    cmdEditField_ =
        new UIBigHexVarField(pos, cmdEdit_, 4, "%4.4X", 0, 0xFFFF, 16, true);
    row_ = 0;
    viewData->phraseCurPos_ = 0;
    col_ = 0;
    lastNote_ = 60;
    lastVol_ = 0xFF;
    lastInstr_ = 0;
    lastCmd_ = I_CMD_NONE;
    lastParam_ = 0;
    commandSelectorModalActive_ = false;
    lastPlainATime_ = 0;
    lastPlainARow_ = -1;
    lastPlainAPhrase_ = -1;

    clipboard_.active_ = false;
    clipboard_.width_ = 0;
    clipboard_.height_ = 0;

    for (int i = 0; i < 16; i++) {
        clipboard_.note_[i] = 0xFF;
        clipboard_.volume_[i] = 0xFF;
        clipboard_.instr_[i] = 0;
    };
    View::EnableNotification();
}

PhraseView::~PhraseView() { delete cmdEditField_; };

void PhraseView::updateCursor(int dx, int dy) {

    col_ += dx;
    row_ += dy;
    if (col_ > 6)
        col_ = 6;
    if (col_ < 0)
        col_ = 0;
    if (row_ > 15) {
        // Try to see if the current chain has a phrase after this one

        if ((viewMode_ != VM_SELECTION) && (viewData_->chainRow_ < 15)) {
            viewData_->chainRow_++;
            unsigned char *p = viewData_->GetCurrentChainPointer();
            if (*p != 0xFF) {
                viewData_->currentPhrase_ = *p;
                row_ = 0;
            } else { // rollback
                viewData_->chainRow_--;
                row_ = 15;
            }
        } else {
            row_ = 15;
        }
    }
    if (row_ < 0) {

        // Try to see if the current chain has a phrase before this one

        if ((viewMode_ != VM_SELECTION) && (viewData_->chainRow_ > 0)) {
            viewData_->chainRow_--;
            unsigned char *p = viewData_->GetCurrentChainPointer();
            if (*p != 0xFF) {
                viewData_->currentPhrase_ = *p;
                row_ = 15;
            } else { // rollback
                viewData_->chainRow_++;
                row_ = 0;
            }
        } else {
            row_ = 0;
        }
    }
    GUIPoint anchor = GetAnchor();
    anchor._x -= 3;
    GUIPoint p(anchor);
    switch (col_) {
    case 4:
        p._x += 17;
        p._y += row_;
        cmdEditField_->SetPosition(p);
        cmdEdit_.SetInt(
            *(phrase_->param1_ + (16 * viewData_->currentPhrase_ + row_)));
        break;
    case 6:
        p._x += 27;
        p._y += row_;
        cmdEditField_->SetPosition(p);
        cmdEdit_.SetInt(
            *(phrase_->param2_ + (16 * viewData_->currentPhrase_ + row_)));
        break;
    };
    viewData_->phraseCurPos_ = row_;
    isDirty_ = true;
}

void PhraseView::stopAudition() {
    Player *player = Player::GetInstance();
    if (viewData_->playMode_ == PM_AUDITION)
        player->Stop();
}

bool PhraseView::isCommandColumn() const { return col_ == 3 || col_ == 5; }

FourCC *PhraseView::getCurrentCommandPointer() {
    return CommandSelectorCommon::getCommandPointerByCol(
        col_, 3, phrase_->cmd1_ + (16 * viewData_->currentPhrase_ + row_), 5,
        phrase_->cmd2_ + (16 * viewData_->currentPhrase_ + row_));
}

void PhraseView::enterCommandSelector() {
    FourCC *cmdPtr = getCurrentCommandPointer();
    if (!cmdPtr) return;
    commandSelectorModalActive_ = true;
    DoModal(new CommandSelectorModal(*this, cmdPtr, CommandSelectorPreviewCallback),
            CommandSelectorCallback);
}

void PhraseView::onCommandSelectorResult(ModalView &d) {
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

void PhraseView::onCommandSelectorPreview(ModalView &) {
    isDirty_ = true;
    Player *player = Player::GetInstance();
    // Don't audition when in playback, allow when browsing around
    if (!player->IsRunning() || viewData_->playMode_ == PM_AUDITION) {
        #if !defined(TREEFROG_DISABLE_PHRASE_AUDITION) || !TREEFROG_DISABLE_PHRASE_AUDITION
        player->OnStartButton(PM_AUDITION, viewData_->songX_, false,
                              viewData_->chainRow_);
#endif /* TREEFROG_DISABLE_PHRASE_AUDITION_V133 */
    }
}

void PhraseView::updateCursorValue(ViewUpdateDirection direction, int xOffset,
                                   int yOffset) {

    unsigned char *c = 0;
    unsigned char limit = 0;
    bool wrap = false;
    FourCC *cc;

    switch (col_ + xOffset) {
    case 0:
        c = phrase_->note_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        limit = 119;
        wrap = true;
        break;
    case 1:
        c = phrase_->volume_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        limit = 0xFE;
        wrap = false;
        break;
    case 2:
        c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        limit = MAX_INSTRUMENT_COUNT - 1;
        wrap = true;
        break;
    case 3:
        cc = phrase_->cmd1_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        switch (direction) {
        case VUD_RIGHT:
            *cc = CommandList::GetNext(*cc);
            break;
        case VUD_UP:
            *cc = CommandList::GetNextAlpha(*cc);
            break;
        case VUD_LEFT:
            *cc = CommandList::GetPrev(*cc);
            break;
        case VUD_DOWN:
            *cc = CommandList::GetPrevAlpha(*cc);
            break;
        }
        lastCmd_ = *cc;
        break;
    case 4:
        if (adjustPtchParamForRow(row_ + yOffset, 4, direction)) break;
        switch (direction) {
        case VUD_RIGHT:
            cmdEditField_->ProcessArrow(EPBM_RIGHT);
            break;
        case VUD_UP:
            cmdEditField_->ProcessArrow(EPBM_UP);
            break;
        case VUD_LEFT:
            cmdEditField_->ProcessArrow(EPBM_LEFT);
            break;
        case VUD_DOWN:
            cmdEditField_->ProcessArrow(EPBM_DOWN);
            break;
        }
        *(phrase_->param1_ + (16 * viewData_->currentPhrase_ + row_ +
                              yOffset)) = cmdEdit_.GetInt();
        lastParam_ = cmdEdit_.GetInt();
        break;
    case 5:
        cc = phrase_->cmd2_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        switch (direction) {
        case VUD_RIGHT:
            *cc = CommandList::GetNext(*cc);
            break;
        case VUD_UP:
            *cc = CommandList::GetNextAlpha(*cc);
            break;
        case VUD_LEFT:
            *cc = CommandList::GetPrev(*cc);
            break;
        case VUD_DOWN:
            *cc = CommandList::GetPrevAlpha(*cc);
            break;
        }
        lastCmd_ = *cc;
        break;
    case 6:
        if (adjustPtchParamForRow(row_ + yOffset, 6, direction)) break;
        switch (direction) {
        case VUD_RIGHT:
            cmdEditField_->ProcessArrow(EPBM_RIGHT);
            break;
        case VUD_UP:
            cmdEditField_->ProcessArrow(EPBM_UP);
            break;
        case VUD_LEFT:
            cmdEditField_->ProcessArrow(EPBM_LEFT);
            break;
        case VUD_DOWN:
            cmdEditField_->ProcessArrow(EPBM_DOWN);
            break;
        }
        *(phrase_->param2_ + (16 * viewData_->currentPhrase_ + row_ +
                              yOffset)) = cmdEdit_.GetInt();
        lastParam_ = cmdEdit_.GetInt();
        break;
    }
    if (c) {
        int editCol = col_ + xOffset;
        if (editCol == 1 && *c == 0xFF) *c = 0x80;
        if (*c == 0xFF) return;
        if (editCol == 0 && updateChopNoteValueForRow(row_ + yOffset, direction)) {
            lastNote_ = *c;
        } else {
            int offset = offsets_[editCol][direction];
            // If note column apply the selected musical scale only for normal, non-chopped instruments.
            if (editCol == 0) {
                int scale = viewData_->project_->GetScale();
                while (!scaleSteps[scale][(*c + offset) % 12]) {
                    offset > 0 ? offset++ : offset--;
                }
            }
            updateData(c, offset, limit, wrap);
            switch (editCol) {
            case 0:
                lastNote_ = *c;
                break;
            case 1:
                lastVol_ = *c;
                break;
            case 2:
                lastInstr_ = *c;
                break;
            }
        }
    }

    Player *player = Player::GetInstance();
    // Phrase FX params are currently not applied to preview
    if (col_ == 0 || col_ == 1 || col_ == 2 || col_ == 3 || col_ == 4 ||
        col_ == 5 || col_ == 6) {
        if (player->IsRunning()) {
            if ((viewData_->playMode_ == PM_AUDITION)) {
                player->Stop();
                #if !defined(TREEFROG_DISABLE_PHRASE_AUDITION) || !TREEFROG_DISABLE_PHRASE_AUDITION
        player->OnStartButton(PM_AUDITION, viewData_->songX_, false,
                                      viewData_->chainRow_);
#endif /* TREEFROG_DISABLE_PHRASE_AUDITION_V133 */
            }
        } else {
            #if !defined(TREEFROG_DISABLE_PHRASE_AUDITION) || !TREEFROG_DISABLE_PHRASE_AUDITION
        player->OnStartButton(PM_AUDITION, viewData_->songX_, false,
                                  viewData_->chainRow_);
#endif /* TREEFROG_DISABLE_PHRASE_AUDITION_V133 */
        }
    }
    isDirty_ = true;
}

// If we're on an empty spot, we past the last element
// otherwise we take the current phrase as last

void PhraseView::pasteLast() {

    uchar *c = 0;
    uint *i = 0;

    switch (col_) {
    case 0:
        c = phrase_->note_ + (16 * viewData_->currentPhrase_ + row_);
        if ((*c == 0xFF)) {
            if (pasteDefaultChopForRow(row_)) break;
            *c = lastNote_;
            c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
            *c = lastInstr_;
            isDirty_ = true;
        } else {
            lastNote_ = *c;
            c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
            lastInstr_ = *c;
        }
        break;
    case 1:
        c = phrase_->volume_ + (16 * viewData_->currentPhrase_ + row_);
        if ((*c == 0xFF)) {
            *c = (lastVol_ == 0xFF) ? 0x80 : lastVol_;
            isDirty_ = true;
        } else {
            lastVol_ = *c;
        }
        break;
    case 2:
        c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
        if ((*c == 0xFF)) {
            *c = lastInstr_;
            isDirty_ = true;
        } else {
            lastInstr_ = *c;
        }
        break;
    case 3:
        i = phrase_->cmd1_ + (16 * viewData_->currentPhrase_ + row_);
        if (*i == I_CMD_NONE) {
            *i = lastCmd_;
            isDirty_ = true;
        } else {
            lastCmd_ = *i;
        }
        break;
    case 4:
        break;
    case 5:
        i = phrase_->cmd2_ + (16 * viewData_->currentPhrase_ + row_);
        if (*i == I_CMD_NONE) {
            *i = lastCmd_;
            isDirty_ = true;
        } else {
            lastCmd_ = *i;
        }
        break;
    case 6:
        break;
    }
}

void PhraseView::cutPosition() {

    clipboard_.active_ = true;
    clipboard_.row_ = row_;
    clipboard_.col_ = col_;
    saveRow_ = row_;
    saveCol_ = col_;

    if (col_ == 0)
        col_ = 2;  // note cut also clears volume + instrument
    else if (col_ == 3)
        col_ = 4;  // command cut also clears param
    else if (col_ == 5)
        col_ = 6;
    cutSelection();
};

void PhraseView::warpInChain(int offset) {

    int currentRow = viewData_->chainRow_;
    viewData_->chainRow_ += offset;
    if ((viewData_->chainRow_ < 16) && (viewData_->chainRow_ >= 0)) {
        unsigned char *p = viewData_->GetCurrentChainPointer();
        if (*p != 0xFF) {
            viewData_->currentPhrase_ = *p;
            switch (col_) {
            case 4:
                cmdEdit_.SetInt(*(phrase_->param1_ +
                                  (16 * viewData_->currentPhrase_ + row_)));
                break;
            case 6:
                cmdEdit_.SetInt(*(phrase_->param2_ +
                                  (16 * viewData_->currentPhrase_ + row_)));
                break;
            };
        } else { // rollback
            viewData_->chainRow_ = currentRow;
        }
    } else { // rollback
        viewData_->chainRow_ = currentRow;
    }
    isDirty_ = true;
}

void PhraseView::warpToNeighbour(int offset) {
    // save current data
    int saveX = viewData_->songX_;
    int saveOffset = viewData_->songOffset_;
    int newPos = saveX + offset;

    while ((newPos > -1) && (newPos < SONG_CHANNEL_COUNT)) {
        // Go to neighbout song channel
        viewData_->songX_ = newPos;
        unsigned char *c = viewData_->GetCurrentSongPointer();
        // is there a chain ?
        unsigned char oldChain = viewData_->currentChain_;
        if (*c != 0xFF) {
            // go to chain
            viewData_->currentChain_ = *c;
            // get phrase at location
            unsigned char *p = viewData_->GetCurrentChainPointer();
            // is there a phrase ?
            if (*p != 0xFF) {
                viewData_->currentPhrase_ = *p;
                updateCursor(0, 0);
                isDirty_ = true;
                return;
            } else {
                viewData_->currentPhrase_ = 0xFE;
                viewData_->currentChain_ = *c;
                updateCursor(0, 0);
                isDirty_ = true;
                return;
            }
        } else {
            // no chain, to neighbour song channel
            newPos += offset;
        }
    }
    // restore song
    viewData_->songX_ = saveX;
    viewData_->songOffset_ = saveOffset;
}

/******************************************************
 getSelectionRect:
        gets the normalized rectangle of the current
        selection. Valid only while selection is drawn
 ******************************************************/

GUIRect PhraseView::getSelectionRect() {
    GUIRect r(clipboard_.col_, clipboard_.row_, col_, row_);
    r.Normalize();
    return r;
};

/******************************************************
 fillClipboardData:

        copies the necessary information from the
        current selection to the clipboard for future
        paste. We're copying data all across the row
        because we"re too lazy to try to figure a better
        procedure
 ******************************************************/

void PhraseView::fillClipboardData() {

    // Get Current normalized selection rect

    GUIRect selRect = getSelectionRect();

    // Get size & store in clipboard

    clipboard_.width_ = selRect.Width() + 1;
    clipboard_.height_ = selRect.Height() + 1;
    clipboard_.row_ = selRect.Top();
    clipboard_.col_ = selRect.Left();

    // Copy the data

    uchar *src1 =
        viewData_->song_->phrase_->note_ + 16 * viewData_->currentPhrase_;
    uchar *dst1 = clipboard_.note_;
    uchar *srcVol =
        viewData_->song_->phrase_->volume_ + 16 * viewData_->currentPhrase_;
    uchar *dstVol = clipboard_.volume_;
    uchar *src2 =
        viewData_->song_->phrase_->instr_ + 16 * viewData_->currentPhrase_;
    uchar *dst2 = clipboard_.instr_;
    uint *src3 =
        viewData_->song_->phrase_->cmd1_ + 16 * viewData_->currentPhrase_;
    uint *dst3 = clipboard_.cmd1_;
    ushort *src4 =
        viewData_->song_->phrase_->param1_ + 16 * viewData_->currentPhrase_;
    ushort *dst4 = clipboard_.param1_;
    uint *src5 =
        viewData_->song_->phrase_->cmd2_ + 16 * viewData_->currentPhrase_;
    uint *dst5 = clipboard_.cmd2_;
    ushort *src6 =
        viewData_->song_->phrase_->param2_ + 16 * viewData_->currentPhrase_;
    ushort *dst6 = clipboard_.param2_;

    for (int i = 0; i < clipboard_.height_; i++) {
        dst1[i] = src1[clipboard_.row_ + i];
        dstVol[i] = srcVol[clipboard_.row_ + i];
        dst2[i] = src2[clipboard_.row_ + i];
        dst3[i] = src3[clipboard_.row_ + i];
        dst4[i] = src4[clipboard_.row_ + i];
        dst5[i] = src5[clipboard_.row_ + i];
        dst6[i] = src6[clipboard_.row_ + i];
    };
    updateCursor(0, 0);
};

void PhraseView::updateSelectionValue(ViewUpdateDirection direction) { // HERE

    saveRow_ = row_;
    saveCol_ = col_;

    GUIRect r = getSelectionRect();
    col_ = r.Left();
    row_ = r.Top();

    for (int i = 0; i <= r.Width(); i++) {
        for (int j = 0; j <= r.Height(); j++) {
            if (col_ + i < 3) {
                updateCursorValue(direction, i, j);
            }
        }
    }
    row_ = saveRow_;
    col_ = saveCol_;
}

void PhraseView::extendSelection() {
    GUIRect rect = getSelectionRect();
    if (rect.Left() > 0 || rect.Right() < 6) {
        if (col_ < clipboard_.col_) {
            col_ = 0;
            clipboard_.col_ = 6;
        } else {
            col_ = 6;
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
void PhraseView::interpolateSelection() {
    if (!clipboard_.active_) {
        return;
    }

    GUIRect rect = getSelectionRect();
    // Only interpolate if we're in note (0), volume (1), or param (4, 6) columns
    int col = rect.Left();
    if (col != rect.Right() || (col != 0 && col != 1 && col != 4 && col != 6)) {
        return;
    }

    int startRow = rect.Top();
    int endRow = rect.Bottom();
    // Need at least 2 rows to interpolate
    if (endRow - startRow < 1) {
        return;
    }

    // Select the appropriate data array based on column
    if (col == 0) {
        // Note column
        uchar *noteData = phrase_->note_ + (16 * viewData_->currentPhrase_);

        uchar startNote = noteData[startRow];
        uchar endNote = noteData[endRow];

        if (startNote == 0xFF || endNote == 0xFF) {
            View::SetNotification("No note info");
            return;
        }

        int numSteps = endRow - startRow;
        int noteDiff = (int)endNote - (int)startNote;

        for (int step = 0; step <= numSteps; step++) {
            int row = startRow + step;
            int value = startNote + (noteDiff * step) / (numSteps);
            noteData[row] = (uchar)value;
        }
    } else if (col == 1) {
        // Volume column
        uchar *volData = phrase_->volume_ + (16 * viewData_->currentPhrase_);
        uchar startVol = volData[startRow];
        uchar endVol = volData[endRow];
        if (startVol == 0xFF || endVol == 0xFF) {
            View::SetNotification("No volume info");
            return;
        }
        int numSteps = endRow - startRow;
        int volDiff = (int)endVol - (int)startVol;
        for (int step = 0; step <= numSteps; step++) {
            int row = startRow + step;
            int value = startVol + (volDiff * step) / (numSteps);
            if (value < 0) value = 0;
            if (value > 0xFE) value = 0xFE;
            volData[row] = (uchar)value;
        }
    } else {
        // Parameter columns (4 or 6)
        ushort *paramData = (col == 4) ? 
            phrase_->param1_ + (16 * viewData_->currentPhrase_) :
            phrase_->param2_ + (16 * viewData_->currentPhrase_);

        ushort startParam = paramData[startRow];
        ushort endParam = paramData[endRow];

        int numSteps = endRow - startRow;
        int paramDiff = (int)endParam - (int)startParam;

        for (int step = 0; step <= numSteps; step++) {
            int row = startRow + step;
            int value = startParam + (paramDiff * step) / (numSteps);
            paramData[row] = (ushort)value;
        }
    }
    isDirty_ = true;
}

/******************************************************
 copySelection:
        copies data in the current selection to the
        clipboard & end selection process
 ******************************************************/

void PhraseView::copySelection() {

    // Keep up with row,col of selection coz
    // fillClipboardData will trash it

    fillClipboardData();

    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    row_ = saveRow_;
    col_ = saveCol_;

    View::SetNotification("Copied selection");
};

/******************************************************
 cut:  copies data in the current selection to the
       clipboard, clear selection content & end selection
       process
 ******************************************************/

void PhraseView::cutSelection() {

    // Keep up with row,col of selection coz
    // fillClipboardData will trash it

    fillClipboardData();

    // Loop over selection col, row & clear data inside it

    uchar *dst1 =
        viewData_->song_->phrase_->note_ + 16 * viewData_->currentPhrase_;
    uchar *dstVol =
        viewData_->song_->phrase_->volume_ + 16 * viewData_->currentPhrase_;
    uchar *dst2 =
        viewData_->song_->phrase_->instr_ + 16 * viewData_->currentPhrase_;
    uint *dst3 =
        viewData_->song_->phrase_->cmd1_ + 16 * viewData_->currentPhrase_;
    ushort *dst4 =
        viewData_->song_->phrase_->param1_ + 16 * viewData_->currentPhrase_;
    uint *dst5 =
        viewData_->song_->phrase_->cmd2_ + 16 * viewData_->currentPhrase_;
    ushort *dst6 =
        viewData_->song_->phrase_->param2_ + 16 * viewData_->currentPhrase_;

    for (int i = 0; i < clipboard_.width_; i++) {
        for (int j = 0; j < clipboard_.height_; j++) {
            switch (i + clipboard_.col_) {
            case 0:
                dst1[j + clipboard_.row_] = 0xFF;
                break;
            case 1:
                dstVol[j + clipboard_.row_] = 0xFF;
                break;
            case 2:
                dst2[j + clipboard_.row_] = 0xFF;
                break;
            case 3:
                dst3[j + clipboard_.row_] = I_CMD_NONE;
                break;
            case 4:
                dst4[j + clipboard_.row_] = 0x0000;
                break;
            case 5:
                dst5[j + clipboard_.row_] = I_CMD_NONE;
                break;
            case 6:
                dst6[j + clipboard_.row_] = 0x0000;
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

void PhraseView::pasteClipboard() {

    // Get number of row to paste

    int height = clipboard_.height_;
    /*    if (row_+height>16) {
            height=16-row_ ;
        }
      */
    uchar *dst1 =
        viewData_->song_->phrase_->note_ + 16 * viewData_->currentPhrase_;
    uchar *src1 = clipboard_.note_;
    uchar *dstVol =
        viewData_->song_->phrase_->volume_ + 16 * viewData_->currentPhrase_;
    uchar *srcVol = clipboard_.volume_;
    uchar *dst2 =
        viewData_->song_->phrase_->instr_ + 16 * viewData_->currentPhrase_;
    uchar *src2 = clipboard_.instr_;
    uint *dst3 =
        viewData_->song_->phrase_->cmd1_ + 16 * viewData_->currentPhrase_;
    uint *src3 = clipboard_.cmd1_;
    ushort *dst4 =
        viewData_->song_->phrase_->param1_ + 16 * viewData_->currentPhrase_;
    ushort *src4 = clipboard_.param1_;
    uint *dst5 =
        viewData_->song_->phrase_->cmd2_ + 16 * viewData_->currentPhrase_;
    uint *src5 = clipboard_.cmd2_;
    ushort *dst6 =
        viewData_->song_->phrase_->param2_ + 16 * viewData_->currentPhrase_;
    ushort *src6 = clipboard_.param2_;

    uint *noCmd = (uint *)-1;
    ushort *noPrm = (ushort *)-1;
    uint *srcCmd[7] = {noCmd, noCmd, noCmd, src3, noCmd, src5, noCmd};
    ushort *srcPrm[7] = {noPrm, noPrm, noPrm, noPrm, src4, noPrm, src6};
    uint *dstCmd[7] = {noCmd, noCmd, noCmd, dst3, noCmd, dst5, noCmd};
    ushort *dstPrm[7] = {noPrm, noPrm, noPrm, noPrm, dst4, noPrm, dst6};

    bool wasUpdated = false;

    for (int i = 0; i < clipboard_.width_; i++) {
        for (int j = 0; j < height; j++) {
            switch (i + clipboard_.col_) {
            case 0:
                dst1[(j + row_) % 16] = src1[j];
                wasUpdated = true;
                break;
            case 1:
                dstVol[(j + row_) % 16] = srcVol[j];
                wasUpdated = true;
                break;
            case 2:
                dst2[(j + row_) % 16] = src2[j];
                wasUpdated = true;
                break;
            case 3:
            case 5:
                if ((col_ + i) == 3 ||
                    (col_ + i) == 5) { // Don't allow commands in notes, etc
                    dstCmd[col_ + i][(row_ + j) % 16] =
                        srcCmd[clipboard_.col_ + i][j];
                    wasUpdated = true;
                }
                break;
            case 4:
            case 6:
                if ((col_ + i) == 4 || (col_ + i) == 6) {
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

void PhraseView::unMuteAll() {

    UIController *controller = UIController::GetInstance();
    controller->UnMuteAll();
};

void PhraseView::toggleMute() {

    UIController *controller = UIController::GetInstance();
    controller->ToggleMute(viewData_->songX_, viewData_->songX_);
    viewMode_ = (viewMode_ != VM_MUTEON) ? VM_MUTEON : VM_NORMAL;
};

void PhraseView::switchSoloMode() {

    UIController *controller = UIController::GetInstance();
    controller->SwitchSoloMode(viewData_->songX_, viewData_->songX_,
                               (viewMode_ == VM_NORMAL));
    viewMode_ = (viewMode_ != VM_SOLOON) ? VM_SOLOON : VM_NORMAL;
    isDirty_ = true;
};

void PhraseView::OnFocus() {
    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    updateCursor(0, 0);
};

void PhraseView::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) {
        return;
    };

    if ((mask & EPBM_R) && (mask & EPBM_X)) {
        phraseUndoRedo();
        return;
    }

    if (viewMode_ == VM_NEW) {
        if (mask == EPBM_A) {

            // If note or I, we request a new instr

            if (col_ < 3) {
                InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
                unsigned short next = bank->GetNext();
                if (next != NO_MORE_INSTRUMENT) {
                    unsigned char *c = phrase_->instr_ +
                                       (16 * viewData_->currentPhrase_ + row_);
                    *c = (unsigned char)next;
                    lastInstr_ = next;
                    isDirty_ = true;
                }
                mask &= (0xFFFF - EPBM_A);
            } else {
                if ((col_ == 4) &&
                    (*(phrase_->cmd1_ + (16 * viewData_->currentPhrase_ +
                                         row_))) == I_CMD_TABL) {
                    TableHolder *th = TableHolder::GetInstance();
                    unsigned short next = th->GetNext();
                    if (next != NO_MORE_TABLE) {
                        ushort *c = phrase_->param1_ +
                                    (16 * viewData_->currentPhrase_ + row_);
                        *c = next;
                        isDirty_ = true;
                        mask &= (0xFFFF - EPBM_A);
                        cmdEdit_.SetInt(next);
                    }
                }
                if ((col_ == 6) &&
                    (*(phrase_->cmd2_ + (16 * viewData_->currentPhrase_ +
                                         row_))) == I_CMD_TABL) {
                    TableHolder *th = TableHolder::GetInstance();
                    unsigned short next = th->GetNext();
                    if (next != NO_MORE_TABLE) {
                        ushort *c = phrase_->param2_ +
                                    (16 * viewData_->currentPhrase_ + row_);
                        *c = next;
                        isDirty_ = true;
                        mask &= (0xFFFF - EPBM_A);
                        cmdEdit_.SetInt(next);
                    }
                }
            };
        }
    }

    if (viewMode_ == VM_CLONE) {
        if ((mask & EPBM_A) && (mask & EPBM_L)) {
            if (col_ < 3) {
                InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
                unsigned char *c =
                    phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
                if (*c != 0xFF) {
                    unsigned short next = bank->Clone(*c);
                    if (next != NO_MORE_INSTRUMENT) {
                        *c = (unsigned char)next;
                        lastInstr_ = next;
                        isDirty_ = true;
                    } else {
                        View::SetNotification("No more instruments");
                    }
                }
            } else {
                if ((col_ == 4) &&
                    (*(phrase_->cmd1_ + (16 * viewData_->currentPhrase_ +
                                         row_))) == I_CMD_TABL) {
                    TableHolder *th = TableHolder::GetInstance();
                    int current = *(phrase_->param1_ +
                                    (16 * viewData_->currentPhrase_ + row_));
                    if (current != -1) {
                        unsigned short next = th->Clone(current);
                        if (next != NO_MORE_TABLE) {
                            ushort *c = phrase_->param1_ +
                                        (16 * viewData_->currentPhrase_ + row_);
                            *c = next;
                            isDirty_ = true;
                            cmdEdit_.SetInt(next);
                        } else {
                            View::SetNotification("No more tables");
                        }
                    }
                }
                if ((col_ == 6) &&
                    (*(phrase_->cmd2_ + (16 * viewData_->currentPhrase_ +
                                         row_))) == I_CMD_TABL) {
                    TableHolder *th = TableHolder::GetInstance();
                    unsigned short next =
                        th->Clone(*(phrase_->param2_ +
                                    (16 * viewData_->currentPhrase_ + row_)));
                    if (next != NO_MORE_TABLE) {
                        ushort *c = phrase_->param2_ +
                                    (16 * viewData_->currentPhrase_ + row_);
                        *c = next;
                        isDirty_ = true;
                        cmdEdit_.SetInt(next);
                    } else {
                        View::SetNotification("No more tables");
                    }
                }
            };
            mask &= (0xFFFF - (EPBM_A | EPBM_L));
        } else {
            viewMode_ = VM_SELECTION;
        }
    };

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
        viewMode_ = VM_NORMAL;
        processNormalButtonMask(mask);
    };
}

void PhraseView::processNormalButtonMask(unsigned short mask) {
    // U2.42: match Mixer shortcuts globally. R+B mutes, R+A solos.
    if ((mask & EPBM_R) && (mask & EPBM_B)) {
        toggleMute();
        return;
    }
    if ((mask & EPBM_R) && (mask & EPBM_A)) {
        switchSoloMode();
        return;
    }

    // Stop audition when pressing any button except A
    if (!(mask & EPBM_A)) {
        stopAudition();
    }
    // B Modifier

    Player *player = Player::GetInstance();

    // U2.13: Phrase-side S-note editing for chopped instruments.
    // Rows keep the same source instrument; note values display as S01..S100 only when that instrument has chops.
    // Normal unchopped instruments keep normal C-3/C-4 note behavior; pitch changes belong in PTCH/ARPG/FX.
    if ((mask & EPBM_R2) && ((col_ == 0) || (col_ == 2))) {
        if (mask & EPBM_LEFT) { assignChopFromPhrase(-1, false); return; }
        if (mask & EPBM_RIGHT) { assignChopFromPhrase(1, false); return; }
        if (mask & EPBM_UP) { assignChopFromPhrase(-4, false); return; }
        if (mask & EPBM_DOWN) { assignChopFromPhrase(4, false); return; }
        if (mask & EPBM_A) { assignChopFromPhrase(0, true); return; }
    }

    if (mask == EPBM_Y) {
        previewCurrentPhraseRow();
        return;
    }

    if ((mask & EPBM_L2) && (mask & (EPBM_LEFT | EPBM_RIGHT | EPBM_UP | EPBM_DOWN)) &&
        !(mask & (EPBM_A | EPBM_B | EPBM_X | EPBM_Y | EPBM_L | EPBM_R | EPBM_R2 | EPBM_START | EPBM_SELECT))) {
        /* U2.46 TEST:
           L2+LEFT/RIGHT walks previous/next used phrase directly.
           L2+UP/DOWN walks phrase assignments in the current Song channel,
           so the user can jump across Song-assigned phrases without returning
           to Song/Chain first. L2+DOWN creates and links a new phrase when
           there is no later assignment in the current Song channel. */
        if (mask & EPBM_LEFT) { navigatePhraseList(-1); return; }
        if (mask & EPBM_RIGHT) { navigatePhraseList(1); return; }
        if (mask & EPBM_UP) { navigateSongAssignment(-1); return; }
        if (mask & EPBM_DOWN) { navigateSongAssignment(1); return; }
    }

    if (mask & EPBM_B) {
        if (mask & EPBM_LEFT)
            warpToNeighbour(-1);
        if (mask & EPBM_RIGHT)
            warpToNeighbour(1);
        if (mask & EPBM_UP)
            warpInChain(-1);
        if (mask & EPBM_DOWN)
            warpInChain(1);
        if (mask & EPBM_A) {
            cutPosition();
        }
        if (mask & EPBM_L) {
            viewMode_ = VM_CLONE;
        };
        if (mask & EPBM_R)
            toggleMute();
    } else {

        // A Modifer

        if (mask & EPBM_A) {
            if ((col_ == 0) || (col_ == 2)) { // Preview when pressing A
                Player *player = Player::GetInstance();
                if (!player->IsRunning()) {
                    #if !defined(TREEFROG_DISABLE_PHRASE_AUDITION) || !TREEFROG_DISABLE_PHRASE_AUDITION
        player->OnStartButton(PM_AUDITION, viewData_->songX_, false,
                                          viewData_->chainRow_);
#endif /* TREEFROG_DISABLE_PHRASE_AUDITION_V133 */
                }
            }

            if (mask & EPBM_DOWN) {
                if (isCommandColumn())
                    enterCommandSelector();
                else
                    updateCursorValue(VUD_DOWN);
            }
            if (mask & EPBM_UP) {
                if (isCommandColumn())
                    enterCommandSelector();
                else
                    updateCursorValue(VUD_UP);
            }
            if (mask & EPBM_LEFT)
                updateCursorValue(VUD_LEFT);
            if (mask & EPBM_RIGHT)
                updateCursorValue(VUD_RIGHT);
            if (mask & EPBM_L)
                pasteClipboard();
            if (mask & EPBM_R)
                switchSoloMode();
            if (mask == EPBM_A) {
                if (handlePlainADoubleTap()) return;
                pasteLast();
                if ((col_ == 1) || (col_ == 2) || (col_ == 4) || (col_ == 6))
                    viewMode_ = VM_NEW;
            }

        } else {

            // R Modifier

            if (mask & EPBM_R) {
                if (mask & EPBM_LEFT) {
                    ViewType vt = VT_CHAIN;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }
                if (mask & EPBM_RIGHT) {
                    unsigned char *c = phrase_->instr_ +
                                       (16 * viewData_->currentPhrase_ + row_);
                    if (*c != 0xFF) {
                        viewData_->currentInstrument_ = *c;
                    } else {
                        int nearest = findClosestInstrumentFor(row_);
                        if (nearest >= 0) {
                            viewData_->currentInstrument_ = nearest;
                        } else viewData_->currentInstrument_= lastInstr_;
                    }
                    if (viewData_->currentInstrument_ != 0xFF) {
                        /* AU11M_ALLOW_INSTRUMENT_VIEW_WHILE_PLAYING */
                        ViewType vt = VT_INSTRUMENT;
                        ViewEvent ve(VET_SWITCH_VIEW, &vt);
                        SetChanged();
                        NotifyObservers(&ve);
                    }
                }
                if (mask & EPBM_DOWN) {

                    // Go to table view

                    ViewType vt = VT_TABLE;

                    FourCC *cmd = phrase_->cmd1_ +
                                  (16 * viewData_->currentPhrase_ + row_);
                    ushort *param = phrase_->param1_ +
                                    (16 * viewData_->currentPhrase_ + row_);

                    if (*cmd != I_CMD_TABL) {
                        cmd = phrase_->cmd2_ +
                              (16 * viewData_->currentPhrase_ + row_);
                        param = phrase_->param2_ +
                                (16 * viewData_->currentPhrase_ + row_);
                    }
                    if (*cmd == I_CMD_TABL) {
                        viewData_->currentTable_ = (*param) & 0x7F;
                    }
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if (mask & EPBM_UP) {

                    // Go to groove view

                    ViewType vt = VT_GROOVE;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if (mask & EPBM_START) {
                    player->OnStartButton(PM_PHRASE, viewData_->songX_, true,
                                          viewData_->chainRow_);
                }
                if (mask & EPBM_L)
                    unMuteAll();

            } else {
                // L Modifier
                if (mask & EPBM_L) {
                    if (mask & EPBM_X) {
                        clipboard_.active_ = true;
                        clipboard_.col_ = col_;
                        clipboard_.row_ = row_;
                        saveCol_ = col_;
                        saveRow_ = row_;
                        viewMode_ = VM_SELECTION;
                        View::SetNotification("Selection started");
                        isDirty_ = true;
                        return;
                    }
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
};

/*
 * For currently selected row, find nearest instrument from the top
 */


int PhraseView::getChopSourceInstrumentForRow(int row) {
    if (row < 0 || row > 15) row = row_;
    int offset = 16 * viewData_->currentPhrase_ + row;
    unsigned char rowInstr = phrase_->instr_[offset];
    if (rowInstr != 0xFF) return rowInstr;
    if (viewData_->currentInstrument_ != 0xFF) return viewData_->currentInstrument_;
    if (lastInstr_ != 0xFF) return lastInstr_;
    return -1;
}

int PhraseView::getSavedChopCountForRow(int row, int *sourceInstrument) {
    int instr = getChopSourceInstrumentForRow(row);
    if (sourceInstrument) *sourceInstrument = instr;
    if (instr < 0) return 0;
    int count = LGPTChopperGetSavedChopCountForInstrument(viewData_, instr);
    if (count < 0) count = 0;
    if (count > 100) count = 100;
    return count;
}

bool PhraseView::updateChopNoteValueForRow(int row, ViewUpdateDirection direction) {
    int sourceInstrument = -1;
    int count = getSavedChopCountForRow(row, &sourceInstrument);
    if (count <= 0 || sourceInstrument < 0) return false;

    int offset = 16 * viewData_->currentPhrase_ + row;
    unsigned char *note = phrase_->note_ + offset;
    unsigned char *instr = phrase_->instr_ + offset;

    int current = (*note == 0xFF || *note >= count) ? 0 : *note;
    int delta = 0;
    switch (direction) {
    case VUD_LEFT:  delta = -1; break;
    case VUD_RIGHT: delta = 1; break;
    case VUD_UP:    delta = 4; break;
    case VUD_DOWN:  delta = -4; break;
    }
    current += delta;
    while (current < 0) current += count;
    current %= count;

    *note = (unsigned char)current;
    *instr = (unsigned char)sourceInstrument;
    lastNote_ = *note;
    lastInstr_ = *instr;
    char status[48];
    snprintf(status, sizeof(status), "S%02d I%02X", current + 1, sourceInstrument);
    View::SetNotification(status);
    isDirty_ = true;
    return true;
}

bool PhraseView::pasteDefaultChopForRow(int row) {
    int sourceInstrument = -1;
    int count = getSavedChopCountForRow(row, &sourceInstrument);
    if (count <= 0 || sourceInstrument < 0) return false;

    int offset = 16 * viewData_->currentPhrase_ + row;
    unsigned char *note = phrase_->note_ + offset;
    unsigned char *instr = phrase_->instr_ + offset;
    int chop = (lastNote_ >= 0 && lastNote_ < count) ? lastNote_ : 0;
    *note = (unsigned char)chop;
    *instr = (unsigned char)sourceInstrument;
    lastNote_ = *note;
    lastInstr_ = *instr;
    char status[48];
    snprintf(status, sizeof(status), "S%02d I%02X", chop + 1, sourceInstrument);
    View::SetNotification(status);
    isDirty_ = true;
    return true;
}

bool PhraseView::adjustPtchParamForRow(int row, int paramCol, ViewUpdateDirection direction) {
    if (row < 0 || row > 15) return false;
    int offset = 16 * viewData_->currentPhrase_ + row;
    FourCC cmd = I_CMD_NONE;
    ushort *param = 0;
    if (paramCol == 4) {
        cmd = phrase_->cmd1_[offset];
        param = phrase_->param1_ + offset;
    } else if (paramCol == 6) {
        cmd = phrase_->cmd2_[offset];
        param = phrase_->param2_ + offset;
    }
    if (cmd != I_CMD_PTCH || !param) return false;

    int pitch = (int)((signed char)(*param & 0xFF));
    int delta = 0;
    switch (direction) {
    case VUD_LEFT:
        delta = -1;
        break;
    case VUD_RIGHT:
        delta = 1;
        break;
    case VUD_DOWN:
        delta = -10;
        break;
    case VUD_UP:
        delta = 10;
        break;
    }
    pitch += delta;
    if (pitch < -24) pitch = -24;
    if (pitch > 24) pitch = 24;

    *param = (ushort)((*param & 0xFF00) | ((unsigned char)(pitch & 0xFF)));
    cmdEdit_.SetInt(*param);
    lastParam_ = *param;
    char status[48];
    snprintf(status, sizeof(status), "PTCH P%+03d", pitch);
    View::SetNotification(status);
    isDirty_ = true;
    return true;
}

void PhraseView::formatPtchParam(ushort value, char *buffer, int bufferLen) const {
    if (!buffer || bufferLen <= 0) return;
    int pitch = (int)((signed char)(value & 0xFF));
    if (pitch < -24) pitch = -24;
    if (pitch > 24) pitch = 24;
    snprintf(buffer, bufferLen, "P%+03d", pitch);
}

bool PhraseView::isPtchParamCell(int row, int col) const {
    if (row < 0 || row > 15) return false;
    int offset = 16 * viewData_->currentPhrase_ + row;
    if (col == 4) return phrase_->cmd1_[offset] == I_CMD_PTCH;
    if (col == 6) return phrase_->cmd2_[offset] == I_CMD_PTCH;
    return false;
}

int PhraseView::getChopSourceInstrumentForCurrentRow() {
    return getChopSourceInstrumentForRow(row_);
}

bool PhraseView::assignChopFromPhrase(int delta, bool advanceRow) {
    if (col_ != 0 && col_ != 2) {
        View::SetNotification("Chop assign: use note/instr column");
        return false;
    }
    int sourceInstrument = getChopSourceInstrumentForCurrentRow();
    char status[64];
    bool ok = LGPTChopperAssignSavedChopToPhraseRow(viewData_, viewData_->currentPhrase_, row_,
                                                    sourceInstrument, -1, delta, false,
                                                    status, sizeof(status));
    if (status[0]) View::SetNotification(status);
    if (ok) {
        int offset = 16 * viewData_->currentPhrase_ + row_;
        lastNote_ = phrase_->note_[offset];
        lastInstr_ = phrase_->instr_[offset];
        if (advanceRow) updateCursor(0, 1);
        else updateCursor(0, 0);
        isDirty_ = true;
    }
    return ok;
}


bool PhraseView::previewCurrentPhraseRow() {
    int offset = 16 * viewData_->currentPhrase_ + row_;
    if (phrase_->note_[offset] == 0xFF && phrase_->instr_[offset] == 0xFF) {
        View::SetNotification("No row to preview");
        return false;
    }
    Player *player = Player::GetInstance();
    if (player->IsRunning()) player->Stop();
#if !defined(TREEFROG_DISABLE_PHRASE_AUDITION) || !TREEFROG_DISABLE_PHRASE_AUDITION
    player->OnStartButton(PM_AUDITION, viewData_->songX_, false, viewData_->chainRow_);
#endif
    View::SetNotification("Preview row");
    return true;
}

bool PhraseView::openAssignedChopPitchEnvelope() {
    /* U2.45 TEST: allow double-A access even while playback is running.
       The Pitch/Envelope modal can be opened for inspection/editing without
       forcing the user to stop the project first. */
    int offset = 16 * viewData_->currentPhrase_ + row_;
    int instrIndex = phrase_->instr_[offset];
    int noteValue = phrase_->note_[offset];
    if (instrIndex == 0xFF || noteValue == 0xFF) {
        View::SetNotification("No assigned chop");
        return false;
    }
    int chopCount = LGPTChopperGetSavedChopCountForInstrument(viewData_, instrIndex);
    if (chopCount <= 0 || noteValue < 0 || noteValue >= chopCount) {
        View::SetNotification("No assigned chop");
        return false;
    }
    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
    if (!bank) return false;
    I_Instrument *instr = bank->GetInstrument(instrIndex);
    if (!instr || instr->GetType() != IT_SAMPLE) {
        View::SetNotification("Not a sample instrument");
        return false;
    }
    SampleInstrument *sampleInstr = (SampleInstrument *)instr;
    Variable *sampleVar = sampleInstr->FindVariable(SIP_SAMPLE);
    int sampleIndex = sampleVar ? sampleVar->GetInt() : NO_SAMPLE;
    SampleChopperModal *scm = new SampleChopperModal(*this,
        instrIndex,
        sampleIndex,
        (sampleIndex == NO_SAMPLE) ? "" : sampleInstr->GetFileName(),
        (sampleIndex == NO_SAMPLE) ? 0 : sampleInstr->GetSampleSize(),
        noteValue,
        true);
    DoModal(scm);
    isDirty_ = true;
    return true;
}


bool PhraseView::linkCreatedPhraseToNextChainSlot(int phraseIndex) {
    if (!viewData_ || !viewData_->song_ || !viewData_->song_->chain_) return false;
    if (phraseIndex < 0 || phraseIndex >= PHRASE_COUNT) return false;
    int chain = viewData_->currentChain_;
    if (chain < 0 || chain >= CHAIN_COUNT) return false;

    unsigned char *chainData = viewData_->song_->chain_->data_ + (16 * chain);
    int startRow = viewData_->chainRow_;
    if (startRow < 0) startRow = 0;
    if (startRow > 15) startRow = 15;

    for (int pass = 0; pass < 2; pass++) {
        int from = (pass == 0) ? startRow + 1 : 0;
        int to = (pass == 0) ? 15 : startRow;
        for (int r = from; r <= to; r++) {
            if (chainData[r] == 0xFF) {
                chainData[r] = (unsigned char)phraseIndex;
                viewData_->chainRow_ = r;
                return true;
            }
        }
    }
    return false;
}

bool PhraseView::navigatePhraseList(int delta) {
    if (!viewData_ || !viewData_->song_ || !viewData_->song_->phrase_) {
        View::SetNotification("No phrase data");
        return false;
    }
    if (delta == 0) return false;

    Phrase *phrases = viewData_->song_->phrase_;
    int current = viewData_->currentPhrase_;
    if (current < 0 || current >= PHRASE_COUNT) current = 0;

    int next = -1;
    if (delta > 0) {
        for (int p = current + 1; p < PHRASE_COUNT; p++) {
            if (phrases->IsUsed((uchar)p)) { next = p; break; }
        }
    } else {
        for (int p = current - 1; p >= 0; p--) {
            if (phrases->IsUsed((uchar)p)) { next = p; break; }
        }
    }

    if (next < 0 || next >= PHRASE_COUNT) {
        View::SetNotification(delta > 0 ? "No next phrase" : "No previous phrase");
        return false;
    }

    viewData_->currentPhrase_ = next;
    row_ = 0;
    updateCursor(0, 0);
    char msg[48];
    snprintf(msg, sizeof(msg), "Phrase %02X", next);
    View::SetNotification(msg);
    isDirty_ = true;
    return true;
}

bool PhraseView::navigateSongAssignment(int delta) {
    if (!viewData_ || !viewData_->song_ || !viewData_->song_->data_ ||
        !viewData_->song_->chain_ || !viewData_->song_->chain_->data_) {
        View::SetNotification("No song data");
        return false;
    }
    if (delta == 0) return false;

    int channel = viewData_->songX_;
    if (channel < 0) channel = 0;
    if (channel >= SONG_CHANNEL_COUNT) channel = SONG_CHANNEL_COUNT - 1;

    struct AssignmentRef {
        int songRow;
        int chain;
        int chainRow;
        int phrase;
    };

    AssignmentRef refs[SONG_ROW_COUNT * 16];
    int count = 0;
    int currentIndex = -1;
    int currentSongRow = viewData_->songOffset_ + viewData_->songY_;

    for (int songRow = 0; songRow < SONG_ROW_COUNT; songRow++) {
        unsigned char chainIndex = viewData_->song_->data_[songRow * SONG_CHANNEL_COUNT + channel];
        if (chainIndex == 0xFF || chainIndex >= CHAIN_COUNT) continue;
        for (int chainRow = 0; chainRow < 16; chainRow++) {
            unsigned char phraseIndex = viewData_->song_->chain_->data_[chainIndex * 16 + chainRow];
            if (phraseIndex == 0xFF || phraseIndex >= PHRASE_COUNT) continue;
            refs[count].songRow = songRow;
            refs[count].chain = chainIndex;
            refs[count].chainRow = chainRow;
            refs[count].phrase = phraseIndex;
            if (phraseIndex == viewData_->currentPhrase_ &&
                chainIndex == viewData_->currentChain_ &&
                chainRow == viewData_->chainRow_ &&
                songRow == currentSongRow) {
                currentIndex = count;
            }
            count++;
        }
    }

    if (count <= 0) {
        if (delta > 0 && viewData_->song_->phrase_) {
            unsigned short created = viewData_->song_->phrase_->GetNext();
            if (created != NO_MORE_PHRASE && created < PHRASE_COUNT) {
                bool linked = linkCreatedPhraseToNextChainSlot((int)created);
                viewData_->currentPhrase_ = (int)created;
                row_ = 0;
                updateCursor(0, 0);
                char msg[64];
                snprintf(msg, sizeof(msg), linked ? "New song phrase %02X" : "New phrase %02X", created);
                View::SetNotification(msg);
                isDirty_ = true;
                return true;
            }
        }
        View::SetNotification("No song assignments");
        return false;
    }

    if (currentIndex < 0) {
        int bestScore = 999999;
        for (int i = 0; i < count; i++) {
            int score = 0;
            if (refs[i].phrase != viewData_->currentPhrase_) score += 10000;
            if (refs[i].chain != viewData_->currentChain_) score += 1000;
            if (refs[i].chainRow != viewData_->chainRow_) score += 100;
            int d = refs[i].songRow - currentSongRow;
            if (d < 0) d = -d;
            score += d;
            if (score < bestScore) { bestScore = score; currentIndex = i; }
        }
    }

    int nextIndex = currentIndex + (delta > 0 ? 1 : -1);
    if (nextIndex >= count && delta > 0 && viewData_->song_->phrase_) {
        unsigned short created = viewData_->song_->phrase_->GetNext();
        if (created != NO_MORE_PHRASE && created < PHRASE_COUNT) {
            bool linked = linkCreatedPhraseToNextChainSlot((int)created);
            viewData_->currentPhrase_ = (int)created;
            row_ = 0;
            updateCursor(0, 0);
            char msg[64];
            snprintf(msg, sizeof(msg), linked ? "New song phrase %02X" : "New phrase %02X", created);
            View::SetNotification(msg);
            isDirty_ = true;
            return true;
        }
        View::SetNotification("No free phrase");
        return false;
    }
    if (nextIndex < 0) nextIndex = count - 1;
    if (nextIndex >= count) nextIndex = 0;

    AssignmentRef &dst = refs[nextIndex];
    viewData_->currentChain_ = dst.chain;
    viewData_->currentPhrase_ = dst.phrase;
    viewData_->chainRow_ = dst.chainRow;
    viewData_->songX_ = channel;
    if (dst.songRow < 0) dst.songRow = 0;
    if (dst.songRow >= SONG_ROW_COUNT) dst.songRow = SONG_ROW_COUNT - 1;
    if (dst.songRow < viewData_->songOffset_ ||
        dst.songRow >= viewData_->songOffset_ + View::songRowCount_) {
        int off = dst.songRow - (View::songRowCount_ / 2);
        if (off < 0) off = 0;
        if (off > 232) off = 232;
        viewData_->songOffset_ = off;
    }
    viewData_->songY_ = dst.songRow - viewData_->songOffset_;
    if (viewData_->songY_ < 0) viewData_->songY_ = 0;
    if (viewData_->songY_ >= View::songRowCount_) viewData_->songY_ = View::songRowCount_ - 1;

    row_ = 0;
    updateCursor(0, 0);
    char msg[80];
    snprintf(msg, sizeof(msg), "Song %02X Ch%02X P%02X", dst.songRow, channel, dst.phrase);
    View::SetNotification(msg);
    isDirty_ = true;
    return true;
}

bool PhraseView::handlePlainADoubleTap() {
    unsigned long now = System::GetInstance()->GetClock();
    bool sameTarget = (lastPlainAPhrase_ == viewData_->currentPhrase_ && lastPlainARow_ == row_);
    if (sameTarget && lastPlainATime_ != 0 && (now - lastPlainATime_) <= 350) {
        lastPlainATime_ = 0;
        lastPlainARow_ = -1;
        lastPlainAPhrase_ = -1;
        return openAssignedChopPitchEnvelope();
    }
    lastPlainATime_ = now;
    lastPlainARow_ = row_;
    lastPlainAPhrase_ = viewData_->currentPhrase_;
    return false;
}

void PhraseView::phraseUndoRedo() {
    char status[64];
    if (LGPTChopperGlobalUndoRedo(viewData_, status, sizeof(status))) {
        View::SetNotification(status[0] ? status : "Undo/redo");
    } else {
        View::SetNotification(status[0] ? status : "Nothing to undo");
    }
    isDirty_ = true;
}

int PhraseView::findClosestInstrumentFor(int row) {
    unsigned char *instr = phrase_->instr_ + (16 * viewData_->currentPhrase_);
    if (instr[row] != 0xFF) return instr[row];
    for (int d = 1; d < 16; ++d) {
        int up = row - d;
        int down = row + d;
        if (up >= 0 && instr[up] != 0xFF) return instr[up];
        if (down < 16 && instr[down] != 0xFF) return instr[down];
    }
    return -1; // none found in phrase
}

void PhraseView::processSelectionButtonMask(unsigned short mask) {

    Player *player = Player::GetInstance();

    if ((mask & EPBM_X) && !(mask & (EPBM_L | EPBM_R | EPBM_A | EPBM_B | EPBM_Y | EPBM_L2 | EPBM_R2 | EPBM_START | EPBM_SELECT | EPBM_LEFT | EPBM_RIGHT | EPBM_UP | EPBM_DOWN))) {
        cutSelection();
        return;
    }
    if ((mask & EPBM_L) && (mask & EPBM_X)) {
        View::SetNotification("Selection active: X cuts");
        return;
    }

    // B modifier

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

            if (mask & EPBM_DOWN)
                updateSelectionValue(VUD_DOWN);
            if (mask & EPBM_UP)
                updateSelectionValue(VUD_UP);
            if (mask & EPBM_LEFT)
                updateSelectionValue(VUD_LEFT);
            if (mask & EPBM_RIGHT)
                updateSelectionValue(VUD_RIGHT);

            if (mask & EPBM_L)
                cutSelection();
            if (mask & EPBM_R)
                switchSoloMode();
        } else {

            // R Modifier

            if (mask & EPBM_R) {
                if (mask & EPBM_LEFT) {
                    ViewType vt = VT_CHAIN;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }
                if (mask & EPBM_RIGHT) {
                    unsigned char *c = phrase_->instr_ +
                                       (16 * viewData_->currentPhrase_ + row_);
                    if (*c != 0xFF) {
                        viewData_->currentInstrument_ = *c;
                    } else {
                        int nearest = findClosestInstrumentFor(row_);
                        if (nearest >= 0) {
                            viewData_->currentInstrument_ = nearest;
                        } else viewData_->currentInstrument_= lastInstr_;
                    }
                    /* AU11M_ALLOW_INSTRUMENT_VIEW_WHILE_PLAYING */
                    ViewType vt = VT_INSTRUMENT;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }
                if (mask & EPBM_START) {
                    player->OnStartButton(PM_PHRASE, viewData_->songX_, true,
                                          viewData_->chainRow_);
                }
                if (mask & EPBM_L)
                    unMuteAll();

            } else {
                // L Modifier
                if (mask & EPBM_L) {
                    if (mask & EPBM_X) {
                        clipboard_.active_ = true;
                        clipboard_.col_ = col_;
                        clipboard_.row_ = row_;
                        saveCol_ = col_;
                        saveRow_ = row_;
                        viewMode_ = VM_SELECTION;
                        View::SetNotification("Selection started");
                        isDirty_ = true;
                        return;
                    }
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
};

void PhraseView::setTextProps(GUITextProperties &props, int row, int col,
                              bool restore) {

    // TREEFROG_V1_3_PHRASE_TEXT_FOCUS_ORDER:
    // este port evita props.invert_ para no generar barras/recuadros en TreeFrog;
    // por eso DrawView debe aplicar el color base de fila antes de setTextProps().
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
            // TREEFROG_UI_PHRASE_SELECTION_SONG_STYLE
            // Selección validada con la misma semántica visual que Song:
            // CD_HILITE2 + invert_.
            SetColor(CD_HILITE2);
            props.invert_ = true;
        }
    }
};

void PhraseView::DrawView() {

    Clear();
    View::EnableNotification();

    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();

    // Draw title

    char title[20];

    SetColor(CD_NORMAL);
    sprintf(title, "Phrase %2.2x", viewData_->currentPhrase_);
    DrawString(pos._x, pos._y, title, props);

    // Compute phrase grid location.
    // U2.48: center the widened Note/Vol/Inst/Cmd layout inside 40 columns.

    GUIPoint anchor = GetAnchor();
    anchor._x -= 3;

    // Display row numbers

    char buffer[6];
    pos = anchor;
    pos._x -= 3;
    for (int j = 0; j < 16; j++) {
        ((j / altRowNumber_) % 2) ? SetColor(CD_ROW) : SetColor(CD_ROW2);
        hex2char(j, buffer);
        DrawString(pos._x, pos._y, buffer, props);
        pos._y++;
    }

    SetColor(CD_NORMAL);

    pos = anchor;

    // Display notes

    unsigned char *data = phrase_->note_ + (16 * viewData_->currentPhrase_);

    buffer[4] = 0;
    for (int j = 0; j < 16; j++) {
        unsigned char d = *data++;
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 0, j, false);
        if (d == 0xFF) {
            DrawString(pos._x, pos._y, "----", props);
        } else {
            int phraseOffset = 16 * viewData_->currentPhrase_ + j;
            int rowInstr = phrase_->instr_[phraseOffset];
            int chopCount = LGPTChopperGetSavedChopCountForInstrument(viewData_, rowInstr);
            int chopNumber = 0;
            if (chopCount > 0) {
                if (LGPTChopperIsChopNoteForInstrument(viewData_, rowInstr, d, &chopNumber)) {
                    snprintf(buffer, sizeof(buffer), "S%02d", chopNumber);
                } else {
                    snprintf(buffer, sizeof(buffer), "S--");
                }
                DrawString(pos._x, pos._y, buffer, props);
            } else {
                note2char(d, buffer);
                DrawString(pos._x, pos._y, buffer, props);
            }
        }
        setTextProps(props, 0, j, true);
        pos._y++;
    }

    // Draw volume

    pos = anchor;
    pos._x += 4;

    data = phrase_->volume_ + (16 * viewData_->currentPhrase_);
    for (int j = 0; j < 16; j++) {
        unsigned char d = *data++;
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 1, j, false);
        if (d == 0xFF) {
            DrawString(pos._x, pos._y, "V--", props);
        } else {
            buffer[0] = 'V';
            hex2char(d, buffer + 1);
            buffer[3] = 0;
            DrawString(pos._x, pos._y, buffer, props);
        }
        setTextProps(props, 1, j, true);
        pos._y++;
    }

    // Draw instruments

    pos = anchor;
    pos._x += 8;

    data = phrase_->instr_ + (16 * viewData_->currentPhrase_);
    buffer[0] = 'I';
    buffer[3] = 0;

    for (int j = 0; j < 16; j++) {
        unsigned char d = *data++;
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 2, j, false);
        if (d == 0xFF) {
            DrawString(pos._x, pos._y, "I", props);
            DrawString(pos._x + 1, pos._y, "--", props);
        } else {
            hex2char(d, buffer + 1);
            DrawString(pos._x, pos._y, buffer, props);
            if (j == row_ && (col_ == 0 || col_ == 2)) {
                SetColor(CD_NORMAL);
                sprintf(buffer, "I%2.2x: ", d);
                std::string instrLine = buffer;
                setTextProps(props, 2, j, true);
                GUIPoint location = GetTitlePosition();
                location._x += 12;
                InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
                I_Instrument *instr = bank->GetInstrument(d);
                instrLine += instr->GetName();
                DrawString(location._x, location._y, instrLine.c_str(), props);
            }
        }
        setTextProps(props, 2, j, true);
        pos._y++;
    }

    // Draw command 1

    pos = anchor;
    pos._x += 12;

    FourCC *f = phrase_->cmd1_ + (16 * viewData_->currentPhrase_);

    buffer[4] = 0;

    for (int j = 0; j < 16; j++) {
        FourCC command = *f++;
        fourCC2char(command, buffer);
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 3, j, false);
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 3, j, true);
        pos._y++;
        if (j == row_ && col_ == 3) {
            printHelpLegend(command, props);
        }
    }

    // Draw commands params 1

    pos = anchor;
    pos._x += 17;

    ushort *param = phrase_->param1_ + (16 * viewData_->currentPhrase_);
    buffer[5] = 0;

    for (int j = 0; j < 16; j++) {
        ushort p = *param++;
        /*		if (p==0xFFFF) {
                    DrawString(pos._x,pos._y,"----",props) ;
                } else {
        */ (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                   : SetColor(CD_NORMAL);
        setTextProps(props, 4, j, false);
        if (phrase_->cmd1_[16 * viewData_->currentPhrase_ + j] == I_CMD_PTCH) {
            formatPtchParam(p, buffer, sizeof(buffer));
        } else {
            hexshort2char(p, buffer);
        }
        DrawString(pos._x, pos._y, buffer, props);
        /*		}
         */
        setTextProps(props, 4, j, true);
        pos._y++;
    }

    // Draw commands 2

    pos = anchor;
    pos._x += 22;

    f = phrase_->cmd2_ + (16 * viewData_->currentPhrase_);

    buffer[4] = 0;

    for (int j = 0; j < 16; j++) {
        FourCC command = *f++;
        fourCC2char(command, buffer);
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 5, j, false);
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 5, j, true);
        pos._y++;
        if (j == row_ && col_ == 5) {
            printHelpLegend(command, props);
        }
    }

    // Draw commands params

    pos = anchor;
    pos._x += 27;

    param = phrase_->param2_ + (16 * viewData_->currentPhrase_);
    buffer[5] = 0;

    for (int j = 0; j < 16; j++) {
        ushort p = *param++;
        /*		if (p==0xFFFF) {
                    DrawString(pos._x,pos._y,"----",props) ;
                } else {
        */ (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                   : SetColor(CD_NORMAL);
        setTextProps(props, 6, j, false);
        if (phrase_->cmd2_[16 * viewData_->currentPhrase_ + j] == I_CMD_PTCH) {
            formatPtchParam(p, buffer, sizeof(buffer));
        } else {
            hexshort2char(p, buffer);
        }
        DrawString(pos._x, pos._y, buffer, props);
        /*		}
         */
        setTextProps(props, 6, j, true);
        pos._y++;
    }

    drawMap();
    drawNotes();

    Player *player = Player::GetInstance();
    if (player->IsRunning()) {
        OnPlayerUpdate(PET_UPDATE);
    };

    if ((viewMode_ != VM_SELECTION) && ((col_ == 4) || (col_ == 6))) {
        if (!isPtchParamCell(row_, col_)) {
            cmdEditField_->SetFocus();
            cmdEditField_->Draw(w_);
        }
    };
};

void PhraseView::OnPlayerUpdate(PlayerEventType eventType, unsigned int tick) {

    GUITextProperties props;
    drawNotes();

    GUIPoint anchor = GetAnchor();
    anchor._x -= 3;
    GUIPoint pos = anchor;
    pos._x -= 1;

    SetColor(CD_NORMAL);

    pos._y = anchor._y + lastPlayingPos_;
    if (!commandSelectorModalActive_ ||
        !CommandSelectorCommon::popupContainsPoint(anchor, pos._x, pos._y)) {
        DrawString(pos._x, pos._y, " ", props);
    }

    Player *player = Player::GetInstance();

    if (eventType != PET_STOP) {

        // Clear current position if needed

        for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
            if (player->IsChannelPlaying(i)) {
                if (viewData_->currentPlayPhrase_[i] ==
                        viewData_->currentPhrase_ &&
                    viewData_->playMode_ != PM_AUDITION) {
                    pos._y = anchor._y + viewData_->phrasePlayPos_[i];
                    if (!commandSelectorModalActive_ ||
                        !CommandSelectorCommon::popupContainsPoint(
                            anchor, pos._x, pos._y)) {
                        if (!player->IsChannelMuted(i)) {
                            SetColor(CD_PLAY);
                            DrawString(pos._x, pos._y, ">", props);
                        } else {
                            SetColor(CD_MUTE);
                            DrawString(pos._x, pos._y, "-", props);
                        }
                    }
                    SetColor(CD_NORMAL);
                    lastPlayingPos_ = viewData_->phrasePlayPos_[i];
                    break;
                }
            }
        }

        // clear any live indicator
        pos._y = anchor._y;

        // Loop on all channels to see if one has queued current chain
        if (player->GetSequencerMode() == SM_LIVE) {

            for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {
                // is anything queued ?
                if (player->GetQueueingMode(i) != QM_NONE) {
                    // find the chain queued in channel
                    unsigned char songPos = player->GetQueuePosition(i);
                    unsigned char *chain =
                        viewData_->song_->data_ + i + 8 * songPos;
                    if (*chain == viewData_->currentChain_) {
                        char *indicator = player->GetLiveIndicator(i);
                        if (!commandSelectorModalActive_ ||
                            !CommandSelectorCommon::popupContainsPoint(
                                anchor, pos._x, pos._y)) {
                            DrawString(pos._x, pos._y, indicator, props);
                        }
                        break;
                    }
                }
            }
        }
    }

    pos = anchor;
    pos._x += 200;

    /*	if (player->Clipped()) {
               w_.DrawString("clip",pos,props);
        } else {
               w_.DrawString("----",pos,props);
        }
    */
};

void PhraseView::printHelpLegend(FourCC command, GUITextProperties props) {
    SetColor(CD_NORMAL);
    std::string *cmdStr = getHelpLegend(command);
    DrawString(10, 0, cmdStr[0].c_str(), props);
    DrawString(10, 1, cmdStr[1].c_str(), props);
    DrawString(10, 2, cmdStr[2].c_str(), props);
}
