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
#include "UIController.h"
#include <stdlib.h>
#include <string.h>

// Column layout (8 columns): row# | N | V | P | I | FX1 | P1 | FX2 | P2
// TREEFROG_PHRASE_COLUMNS_V1: FX3 column removed (user decision, H38.5).
// TREEFROG_PHRASE_PITCH_COLUMN_V1 (H38.7): dedicated pitch column (P) between
// volume and instrument. PTCH is removed from FX; pitch lives in its own
// column. X positions in character cells (each cell = 8px). The 40-cell
// screen is centered: row# at cells 6-7, play cursor at cell 5, data columns
// start at cell 9. The FX block (cols 4-7) is separated from the N-V-P-I
// block by a 2-cell gap (I ends at 20, FX1 starts at 23).
const int PhraseView::kColX[kColCount] = {9, 12, 15, 18, 23, 27, 31, 35};

// Header center positions over each data column:
// N(9-11)->10, V(12-14)->13, P(15-17)->16, I(18-20)->19, FX1(23-25)->24,
// P1(27-30)->28, FX2(31-33)->32, P2(35-38)->36. Parameter columns are blank.
const int PhraseView::kColHeaderX[kColCount] = {10, 13, 16, 19, 24, 28, 32, 36};

// Offsets for note(0), volume(1), pitch(2) and instrument(3) value stepping:
// L, R, U, D. TREEFROG_PHRASE_VOL_EDIT_V1: volume steps by 1 in every
// direction (A+UP/DOWN steps by 10 inside updateCursorValue). Pitch follows
// the same scheme.
short PhraseView::offsets_[4][4] = {{-1, 1, 12, -12},
                                    {-1, 1, 1, -1},
                                    {-1, 1, 1, -1},
                                    {-1, 1, 16, -16}};

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
    lastVol_ = 0x64;
    lastInstr_ = 0;
    lastPitch_ = 0;
    lastCmd_ = I_CMD_NONE;
    lastParam_ = 0;
    commandSelectorModalActive_ = false;

    clipboard_.active_ = false;
    clipboard_.width_ = 0;
    clipboard_.height_ = 0;

    for (int i = 0; i < 16; i++) {
        clipboard_.note_[i] = 0xFF;
        clipboard_.pitch_[i] = 0x00;
        clipboard_.instr_[i] = 0;
    };
    View::EnableNotification();
}

PhraseView::~PhraseView() { delete cmdEditField_; };

// TREEFROG_COMMAND_SPECS_V1 (Fase 6): the shared param hex editor adapts its
// digit count to the command in the cell's cmd column (HEX4 for legacy
// commands, HEX8 for the Fase 4 FX-engine commands).  The stored param value
// is a ushort; only the low byte is meaningful for HEX8 commands.
void PhraseView::applyCmdEditModeForCommand(FourCC command) {
	cmdEditField_->SetHexMode(CommandList::GetParamPrecision(command),
	                         CommandList::GetParamFormatString(command),
	                         CommandList::GetParamMin(command),
	                         CommandList::GetParamMax(command),
	                         CommandList::GetParamWrap(command)) ;
}

void PhraseView::applyCmdEditMode(int paramCol) {
	FourCC *cmd = 0 ;
	if (paramCol == 5) {
		cmd = phrase_->cmd1_ + (16 * viewData_->currentPhrase_ + row_) ;
	} else if (paramCol == 7) {
		cmd = phrase_->cmd2_ + (16 * viewData_->currentPhrase_ + row_) ;
	}
	if (cmd) applyCmdEditModeForCommand(*cmd) ;
}

void PhraseView::updateCursor(int dx, int dy) {

    col_ += dx;
    row_ += dy;
    if (col_ > kColCount - 1)
        col_ = kColCount - 1;
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
    GUIPoint p(anchor);
    switch (col_) {
    case 5:
        p._x = kColX[5];
        p._y += row_;
        cmdEditField_->SetPosition(p);
        applyCmdEditMode(5);
        cmdEdit_.SetInt(
            *(phrase_->param1_ + (16 * viewData_->currentPhrase_ + row_)));
        break;
    case 7:
        p._x = kColX[7];
        p._y += row_;
        cmdEditField_->SetPosition(p);
        applyCmdEditMode(7);
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

bool PhraseView::isCommandColumn() const {
    return CommandSelectorCommon::isCommandColumn(col_, 4, 6);
}

FourCC *PhraseView::getCurrentCommandPointer() {
    return CommandSelectorCommon::getCommandPointerByCol(
        col_, 4, phrase_->cmd1_ + (16 * viewData_->currentPhrase_ + row_), 6,
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
                                   int yOffset, int bigStep) {

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
        c = phrase_->vol_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        // TREEFROG_PHRASE_VOL_V3: row volume is 0..100 (0x64), 0xFF = empty.
        // 100 = full scale (100%), linear mapping.
        limit = 0x64;
        wrap = false;
        break;
    case 2:
        c = phrase_->pitch_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        limit = 24;
        wrap = false;
        break;
    case 3:
        c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
        limit = MAX_INSTRUMENT_COUNT - 1;
        wrap = true;
        break;
    case 4:
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
    case 5:
        applyCmdEditMode(5);
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
    case 6:
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
    case 7:
        applyCmdEditMode(7);
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
        if (editCol == 0 && updateChopNoteValueForRow(row_ + yOffset, direction)) {
            lastNote_ = *c;
        } else {
            // TREEFROG_PHRASE_PITCH_COLUMN_V2 (H38.7): the pitch column stores
            // -24..+24 semitones with an offset so that -1 (0xFF) does not
            // collide with the "no pitch" marker. 0x00 = none, values 0x28..0x58
            // map to -24..+24. Steps by 1 per direction, A+UP/DOWN by 10.
            if (editCol == 2) {
                int p = phrasePitchStoredToInt(*c);
                int step = (direction == VUD_UP || direction == VUD_DOWN) && bigStep
                               ? 10
                               : 1;
                int offset = 0;
                switch (direction) {
                case VUD_LEFT:
                    offset = -1;
                    break;
                case VUD_RIGHT:
                    offset = 1;
                    break;
                case VUD_UP:
                    offset = step;
                    break;
                case VUD_DOWN:
                    offset = -step;
                    break;
                }
                p += offset;
                if (p < -24) p = -24;
                if (p > 24) p = 24;
                *c = phrasePitchIntToStored(p);
                lastPitch_ = p;
            } else {
                bool noteWasEmpty = (editCol == 0) && (*c == 0xFF);
                if ((editCol == 1) && (*c == 0xFF)) {
                    *c = 0x64;
                    isDirty_ = true;
                }
                int offset = offsets_[editCol == 3 ? 3 : (editCol == 1 ? 1 : 0)][direction];
                if (editCol == 1 && (direction == VUD_UP || direction == VUD_DOWN)) {
                    int step = bigStep ? 10 : 1;
                    offset = (direction == VUD_UP) ? step : -step;
                }
                // If note column apply the selected musical scale only for normal, non-chopped instruments.
                if (editCol == 0) {
                    int scale = viewData_->project_->GetScale();
                    while (!scaleSteps[scale][(*c + offset) % 12]) {
                        offset > 0 ? offset++ : offset--;
                    }
                }
                updateData(c, offset, limit, wrap);
                if (noteWasEmpty && (*c != 0xFF)) {
                    uchar *vc = phrase_->vol_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
                    if (*vc == 0xFF) {
                        *vc = 0x64;
                        isDirty_ = true;
                    }
                    uchar *pc = phrase_->pitch_ + (16 * viewData_->currentPhrase_ + row_ + yOffset);
                    if (*pc == PITCH_STORED_NONE) {
                        *pc = PITCH_STORED_ZERO;
                        isDirty_ = true;
                    }
                }
                switch (editCol) {
                case 0:
                    lastNote_ = *c;
                    break;
                case 1:
                    lastVol_ = *c;
                    break;
                case 3:
                    lastInstr_ = *c;
                    break;
                }
            }
        }
    }

    Player *player = Player::GetInstance();
    // Phrase FX params are currently not applied to preview
    if (col_ >= 0 && col_ <= 7) {
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
            // Auto-set volume to full scale (100) when placing a note in empty row
            c = phrase_->vol_ + (16 * viewData_->currentPhrase_ + row_);
            if (*c == 0xFF) {
                *c = 0x64;
            }
            // Auto-set pitch to 00 when placing a note in empty row
            c = phrase_->pitch_ + (16 * viewData_->currentPhrase_ + row_);
            if (*c == PITCH_STORED_NONE) {
                *c = PITCH_STORED_ZERO;
            }
            isDirty_ = true;
        } else {
            lastNote_ = *c;
            c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
            lastInstr_ = *c;
        }
        break;
    case 1:
        c = phrase_->vol_ + (16 * viewData_->currentPhrase_ + row_);
        if ((*c == 0xFF)) {
            *c = lastVol_;
            isDirty_ = true;
        } else {
            lastVol_ = *c;
        }
        break;
    case 2:
        c = phrase_->pitch_ + (16 * viewData_->currentPhrase_ + row_);
        if (*c == PITCH_STORED_NONE) {
            *c = phrasePitchIntToStored(lastPitch_);
            isDirty_ = true;
        } else {
            lastPitch_ = phrasePitchStoredToInt(*c);
        }
        break;
    case 3:
        c = phrase_->instr_ + (16 * viewData_->currentPhrase_ + row_);
        if ((*c == 0xFF)) {
            *c = lastInstr_;
            isDirty_ = true;
        } else {
            lastInstr_ = *c;
        }
        break;

    case 4:
        i = phrase_->cmd1_ + (16 * viewData_->currentPhrase_ + row_);
        if (*i == I_CMD_NONE) {
            *i = lastCmd_;
            isDirty_ = true;
        } else {
            lastCmd_ = *i;
        }
        break;

    case 5:
        /*			s=phrase_->param1_+(16*viewData_->currentPhrase_+row_) ;
                    if (*s==0) {
                        *s=lastParam_ ;
                        cmdEdit_.SetInt(lastParam_) ;
                        isDirty_=true ;
                    }
        */
        break;

    case 6:
        i = phrase_->cmd2_ + (16 * viewData_->currentPhrase_ + row_);
        if (*i == I_CMD_NONE) {
            *i = lastCmd_;
            isDirty_ = true;
        } else {
            lastCmd_ = *i;
        }
        break;

    case 7:
        /*			s=phrase_->param2_+(16*viewData_->currentPhrase_+row_) ;
                    if (*s==0) {
                        *s=lastParam_ ;
                        isDirty_=true ;
                        cmdEdit_.SetInt(lastParam_) ;
                    }
        */
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
        col_ = 3; // This way, A+B on note cuts
                  // the volume, pitch and instrument too
    else if (col_ == 4 || col_ == 6)
        col_ += 1; // parameters get cut with their command
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
            case 5:
                cmdEdit_.SetInt(*(phrase_->param1_ +
                                  (16 * viewData_->currentPhrase_ + row_)));
                break;
            case 7:
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
    uchar *src2 =
        viewData_->song_->phrase_->vol_ + 16 * viewData_->currentPhrase_;
    uchar *dst2 = clipboard_.vol_;
    uchar *src3 =
        viewData_->song_->phrase_->pitch_ + 16 * viewData_->currentPhrase_;
    uchar *dst3 = clipboard_.pitch_;
    uchar *src4 =
        viewData_->song_->phrase_->instr_ + 16 * viewData_->currentPhrase_;
    uchar *dst4 = clipboard_.instr_;
    uint *src5 =
        viewData_->song_->phrase_->cmd1_ + 16 * viewData_->currentPhrase_;
    uint *dst5 = clipboard_.cmd1_;
    ushort *src6 =
        viewData_->song_->phrase_->param1_ + 16 * viewData_->currentPhrase_;
    ushort *dst6 = clipboard_.param1_;
    uint *src7 =
        viewData_->song_->phrase_->cmd2_ + 16 * viewData_->currentPhrase_;
    uint *dst7 = clipboard_.cmd2_;
    ushort *src8 =
        viewData_->song_->phrase_->param2_ + 16 * viewData_->currentPhrase_;
    ushort *dst8 = clipboard_.param2_;

    for (int i = 0; i < clipboard_.height_; i++) {
        dst1[i] = src1[clipboard_.row_ + i];
        dst2[i] = src2[clipboard_.row_ + i];
        dst3[i] = src3[clipboard_.row_ + i];
        dst4[i] = src4[clipboard_.row_ + i];
        dst5[i] = src5[clipboard_.row_ + i];
        dst6[i] = src6[clipboard_.row_ + i];
        dst7[i] = src7[clipboard_.row_ + i];
        dst8[i] = src8[clipboard_.row_ + i];
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
            if (col_ + i < 4) {
                updateCursorValue(direction, i, j);
            }
        }
    }
    row_ = saveRow_;
    col_ = saveCol_;
}

void PhraseView::extendSelection() {
    GUIRect rect = getSelectionRect();
    if (rect.Left() > 0 || rect.Right() < 7) {
        if (col_ < clipboard_.col_) {
            col_ = 0;
            clipboard_.col_ = 7;
        } else {
            col_ = 7;
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
    // Only interpolate if we're in note (0), pitch (2) or param (5, 7) columns
    int col = rect.Left();
    if (col != rect.Right() || (col != 0 && col != 2 && col != 5 && col != 7)) {
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
    } else if (col == 2) {
        // Pitch column
        uchar *pitchData = phrase_->pitch_ + (16 * viewData_->currentPhrase_);

        int startPitch = phrasePitchStoredToInt(pitchData[startRow]);
        int endPitch = phrasePitchStoredToInt(pitchData[endRow]);

        int numSteps = endRow - startRow;
        int pitchDiff = endPitch - startPitch;

        for (int step = 0; step <= numSteps; step++) {
            int row = startRow + step;
            int value = startPitch + (pitchDiff * step) / (numSteps);
            if (value < -24) value = -24;
            if (value > 24) value = 24;
            pitchData[row] = phrasePitchIntToStored(value);
        }
    } else {
        // Parameter columns (5 or 7)
        ushort *paramData = (col == 5)
                                ? phrase_->param1_ + (16 * viewData_->currentPhrase_)
                                : phrase_->param2_ + (16 * viewData_->currentPhrase_);

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
    uchar *dst2 =
        viewData_->song_->phrase_->vol_ + 16 * viewData_->currentPhrase_;
    uchar *dst3 =
        viewData_->song_->phrase_->pitch_ + 16 * viewData_->currentPhrase_;
    uchar *dst4 =
        viewData_->song_->phrase_->instr_ + 16 * viewData_->currentPhrase_;
    uint *dst5 =
        viewData_->song_->phrase_->cmd1_ + 16 * viewData_->currentPhrase_;
    ushort *dst6 =
        viewData_->song_->phrase_->param1_ + 16 * viewData_->currentPhrase_;
    uint *dst7 =
        viewData_->song_->phrase_->cmd2_ + 16 * viewData_->currentPhrase_;
    ushort *dst8 =
        viewData_->song_->phrase_->param2_ + 16 * viewData_->currentPhrase_;

    for (int i = 0; i < clipboard_.width_; i++) {
        for (int j = 0; j < clipboard_.height_; j++) {
            switch (i + clipboard_.col_) {
            case 0:
                dst1[j + clipboard_.row_] = 0xFF;
                break;
            case 1:
                dst2[j + clipboard_.row_] = 0xFF;
                break;
            case 2:
                dst3[j + clipboard_.row_] = PITCH_STORED_NONE;
                break;
            case 3:
                dst4[j + clipboard_.row_] = 0xFF;
                break;
            case 4:
                dst5[j + clipboard_.row_] = I_CMD_NONE;
                break;
            case 5:
                dst6[j + clipboard_.row_] = 0x0000;
                break;
            case 6:
                dst7[j + clipboard_.row_] = I_CMD_NONE;
                break;
            case 7:
                dst8[j + clipboard_.row_] = 0x0000;
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
    uchar *dst2 =
        viewData_->song_->phrase_->vol_ + 16 * viewData_->currentPhrase_;
    uchar *src2 = clipboard_.vol_;
    uchar *dst3 =
        viewData_->song_->phrase_->pitch_ + 16 * viewData_->currentPhrase_;
    uchar *src3 = clipboard_.pitch_;
    uchar *dst4 =
        viewData_->song_->phrase_->instr_ + 16 * viewData_->currentPhrase_;
    uchar *src4 = clipboard_.instr_;
    uint *dst5 =
        viewData_->song_->phrase_->cmd1_ + 16 * viewData_->currentPhrase_;
    uint *src5 = clipboard_.cmd1_;
    ushort *dst6 =
        viewData_->song_->phrase_->param1_ + 16 * viewData_->currentPhrase_;
    ushort *src6 = clipboard_.param1_;
    uint *dst7 =
        viewData_->song_->phrase_->cmd2_ + 16 * viewData_->currentPhrase_;
    uint *src7 = clipboard_.cmd2_;
    ushort *dst8 =
        viewData_->song_->phrase_->param2_ + 16 * viewData_->currentPhrase_;
    ushort *src8 = clipboard_.param2_;

    uint *noCmd = (uint *)-1;
    ushort *noPrm = (ushort *)-1;
    uint *srcCmd[8] = {noCmd, noCmd, noCmd, noCmd, src5, noCmd, src7, noCmd};
    ushort *srcPrm[8] = {noPrm, noPrm, noPrm, noPrm, noPrm, src6, noPrm, src8};
    uint *dstCmd[8] = {noCmd, noCmd, noCmd, noCmd, dst5, noCmd, dst7, noCmd};
    ushort *dstPrm[8] = {noPrm, noPrm, noPrm, noPrm, noPrm, dst6, noPrm, dst8};

    bool wasUpdated = false;

    for (int i = 0; i < clipboard_.width_; i++) {
        for (int j = 0; j < height; j++) {
            int pasteCol = col_ + i;
            switch (i + clipboard_.col_) {
            case 0:
                dst1[(j + row_) % 16] = src1[j];
                wasUpdated = true;
                break;
            case 1:
                dst2[(j + row_) % 16] = src2[j];
                wasUpdated = true;
                break;
            case 2:
                dst3[(j + row_) % 16] = src3[j];
                wasUpdated = true;
                break;
            case 3:
                dst4[(j + row_) % 16] = src4[j];
                wasUpdated = true;
                break;
            case 4:
            case 6:
                if (pasteCol == 4 || pasteCol == 6) {
                    // Don't allow commands in notes, etc
                    dstCmd[pasteCol][(row_ + j) % 16] =
                        srcCmd[clipboard_.col_ + i][j];
                    wasUpdated = true;
                }
                break;
            case 5:
            case 7:
                if (pasteCol == 5 || pasteCol == 7) {
                    dstPrm[pasteCol][(row_ + j) % 16] =
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
        if (viewMode_ == VM_MUTEON) {
            if (mask & EPBM_R) {
                toggleMute();
            }
        };
        if (viewMode_ == VM_SOLOON) {
            if (mask & EPBM_R) {
                switchSoloMode();
            }
        };
        return;
    };

    if (viewMode_ == VM_NEW) {
        if (mask == EPBM_A) {

            // If note or I, we request a new instr

            if (col_ == 0 || col_ == 3) {
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
                if ((col_ == 5) &&
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
                if ((col_ == 7) &&
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
            if (col_ == 0 || col_ == 3) {
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
                if ((col_ == 5) &&
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
                if ((col_ == 7) &&
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
    // Stop audition when pressing any button except A
    if (!(mask & EPBM_A)) {
        stopAudition();
    }
    // B Modifier

    Player *player = Player::GetInstance();

    // U2.13: Phrase-side S-note editing for chopped instruments.
    // Rows keep the same source instrument; note values display as S01..S100 only when that instrument has chops.
    // Normal unchopped instruments keep normal C-3/C-4 note behavior; pitch changes belong in PTCH/ARPG/FX.
    if ((mask & EPBM_R2) && ((col_ == 0) || (col_ == 3))) {
        if (mask & EPBM_LEFT) { assignChopFromPhrase(-1, false); return; }
        if (mask & EPBM_RIGHT) { assignChopFromPhrase(1, false); return; }
        if (mask & EPBM_UP) { assignChopFromPhrase(-4, false); return; }
        if (mask & EPBM_DOWN) { assignChopFromPhrase(4, false); return; }
        if (mask & EPBM_A) { assignChopFromPhrase(0, true); return; }
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
            if ((col_ == 0) || (col_ == 3)) { // Preview when pressing A
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
                    updateCursorValue(VUD_DOWN, 0, 0,
                                      ((col_ == 1) || (col_ == 2)) ? 10 : 0);
            }
            if (mask & EPBM_UP) {
                if (isCommandColumn())
                    enterCommandSelector();
                else
                    updateCursorValue(VUD_UP, 0, 0,
                                      ((col_ == 1) || (col_ == 2)) ? 10 : 0);
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
                pasteLast();
                if ((col_ == 3) || (col_ == 5) || (col_ == 7))
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
    if (phrase_->vol_[offset] == 0xFF) {
        phrase_->vol_[offset] = 0x64;
    }
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
    if (phrase_->vol_[offset] == 0xFF) {
        phrase_->vol_[offset] = 0x64;
    }
    lastNote_ = *note;
    lastInstr_ = *instr;
    char status[48];
    snprintf(status, sizeof(status), "S%02d I%02X", chop + 1, sourceInstrument);
    View::SetNotification(status);
    isDirty_ = true;
    return true;
}

bool PhraseView::adjustPtchParamForRow(int row, int paramCol, ViewUpdateDirection direction) {
    (void)row;
    (void)paramCol;
    (void)direction;
    return false;
}

void PhraseView::formatPtchParam(ushort value, char *buffer, int bufferLen) const {
    if (!buffer || bufferLen <= 0) return;
    int pitch = (int)((signed char)(value & 0xFF));
    if (pitch < -24) pitch = -24;
    if (pitch > 24) pitch = 24;
    snprintf(buffer, bufferLen, "P%+03d", pitch);
}

bool PhraseView::isPtchParamCell(int row, int col) const {
    (void)row;
    (void)col;
    return false;
}

int PhraseView::getChopSourceInstrumentForCurrentRow() {
    return getChopSourceInstrumentForRow(row_);
}

bool PhraseView::assignChopFromPhrase(int delta, bool advanceRow) {
    if (col_ != 0 && col_ != 3) {
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

    // Compute song grid location

    GUIPoint anchor = GetAnchor();

    // Column headers, centered over their columns (TREEFROG_PHRASE_COLUMNS_V1)
    static const char *headers[kColCount] = {"N", "V", "P", "I", "FX1",
                                             "P1", "FX2", "P2"};
    for (int c = 0; c < kColCount; c++) {
        (c == col_) ? SetColor(CD_HILITE2) : SetColor(CD_NORMAL);
        DrawString(kColHeaderX[c], anchor._y - 1, headers[c], props);
    }

    // Display row numbers (cells 6-7, grid centered with 6-cell margins)

    char buffer[6];
    pos._x = 6;
    pos._y = anchor._y;
    for (int j = 0; j < 16; j++) {
        ((j / altRowNumber_) % 2) ? SetColor(CD_ROW) : SetColor(CD_ROW2);
        hex2char(j, buffer);
        DrawString(pos._x, pos._y, buffer, props);
        pos._y++;
    }

    SetColor(CD_NORMAL);

    // Display notes

    pos._x = kColX[0];
    pos._y = anchor._y;

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

    // Display volumes

    pos._x = kColX[1];
    pos._y = anchor._y;

    data = phrase_->vol_ + (16 * viewData_->currentPhrase_);

    for (int j = 0; j < 16; j++) {
        unsigned char d = *data++;
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 1, j, false);
        if (d == 0xFF) {
            DrawString(pos._x, pos._y, " --", props);
        } else {
            // TREEFROG_PHRASE_VOL_V1: volumes display as 0..100 decimal.
            sprintf(buffer, "%3d", d);
            DrawString(pos._x, pos._y, buffer, props);
        }
        setTextProps(props, 1, j, true);
        pos._y++;
    }

    // Display pitch

    pos._x = kColX[2];
    pos._y = anchor._y;

    data = phrase_->pitch_ + (16 * viewData_->currentPhrase_);
    buffer[4] = 0;

    for (int j = 0; j < 16; j++) {
        unsigned char d = *data++;
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 2, j, false);
        if (d == PITCH_STORED_NONE) {
            DrawString(pos._x, pos._y, " --", props);
        } else {
            snprintf(buffer, sizeof(buffer), "%+03d",
                     phrasePitchStoredToInt(d));
            DrawString(pos._x, pos._y, buffer, props);
        }
        setTextProps(props, 2, j, true);
        pos._y++;
    }

    // Draw instruments

    pos._x = kColX[3];
    pos._y = anchor._y;

    data = phrase_->instr_ + (16 * viewData_->currentPhrase_);
    buffer[0] = 'I';
    buffer[3] = 0;

    for (int j = 0; j < 16; j++) {
        unsigned char d = *data++;
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 3, j, false);
        if (d == 0xFF) {
            DrawString(pos._x, pos._y, "I", props);
            DrawString(pos._x + 1, pos._y, "--", props);
        } else {
            hex2char(d, buffer + 1);
            DrawString(pos._x, pos._y, buffer, props);
            if (j == row_ && (col_ == 0 || col_ == 3)) {
                SetColor(CD_NORMAL);
                sprintf(buffer, "I%2.2x: ", d);
                std::string instrLine = buffer;
                setTextProps(props, 3, j, true);
                GUIPoint location = GetTitlePosition();
                location._x += 12;
                InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
                I_Instrument *instr = bank->GetInstrument(d);
                instrLine += instr->GetName();
                DrawString(location._x, location._y, instrLine.c_str(), props);
            }
        }
        setTextProps(props, 3, j, true);
        pos._y++;
    }

    // Draw command 1

    pos._x = kColX[4];
    pos._y = anchor._y;

    FourCC *f = phrase_->cmd1_ + (16 * viewData_->currentPhrase_);

    buffer[4] = 0;

    for (int j = 0; j < 16; j++) {
        FourCC command = *f++;
        getCommandDisplayName(command, buffer);
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 4, j, false);
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 4, j, true);
        pos._y++;
        if (j == row_ && col_ == 4) {
            printHelpLegend(command, props);
        }
    }

    // Draw commands params 1

    pos._x = kColX[5];
    pos._y = anchor._y;

    ushort *param = phrase_->param1_ + (16 * viewData_->currentPhrase_);
    FourCC *cf = phrase_->cmd1_ + (16 * viewData_->currentPhrase_);
    buffer[5] = 0;

    for (int j = 0; j < 16; j++) {
        ushort p = *param++;
        FourCC cmd = *cf++;
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                 : SetColor(CD_NORMAL);
        setTextProps(props, 5, j, false);
        // TREEFROG_COMMAND_SPECS_V1 (Fase 6): Fase 4 FX commands only use the
        // low byte, so their grid cell shows 2 hex digits (HEX8) instead of 4.
        if (CommandList::GetParamFormat(cmd) == CMD_PARAM_FORMAT_HEX8) {
            hex2char((unsigned char)(p & 0xFF), buffer);
        } else {
            hexshort2char(p, buffer);
        }
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 5, j, true);
        pos._y++;
    }

    // Draw commands 2

    pos._x = kColX[6];
    pos._y = anchor._y;

    f = phrase_->cmd2_ + (16 * viewData_->currentPhrase_);

    buffer[4] = 0;

    for (int j = 0; j < 16; j++) {
        FourCC command = *f++;
        getCommandDisplayName(command, buffer);
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                : SetColor(CD_NORMAL);
        setTextProps(props, 6, j, false);
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 6, j, true);
        pos._y++;
        if (j == row_ && col_ == 6) {
            printHelpLegend(command, props);
        }
    }

    // Draw commands params 2

    pos._x = kColX[7];
    pos._y = anchor._y;

    param = phrase_->param2_ + (16 * viewData_->currentPhrase_);
    cf = phrase_->cmd2_ + (16 * viewData_->currentPhrase_);
    buffer[5] = 0;

    for (int j = 0; j < 16; j++) {
        ushort p = *param++;
        FourCC cmd = *cf++;
        (0 == j || 4 == j || 8 == j || 12 == j) ? SetColor(CD_MAJORBEAT)
                                                 : SetColor(CD_NORMAL);
        setTextProps(props, 7, j, false);
        // TREEFROG_COMMAND_SPECS_V1 (Fase 6): HEX8 for Fase 4 FX commands.
        if (CommandList::GetParamFormat(cmd) == CMD_PARAM_FORMAT_HEX8) {
            hex2char((unsigned char)(p & 0xFF), buffer);
        } else {
            hexshort2char(p, buffer);
        }
        DrawString(pos._x, pos._y, buffer, props);
        setTextProps(props, 7, j, true);
        pos._y++;
    }

    drawMap();
    drawNotes();

    Player *player = Player::GetInstance();
    if (player->IsRunning()) {
        OnPlayerUpdate(PET_UPDATE);
    };

    if ((viewMode_ != VM_SELECTION) &&
        ((col_ == 5) || (col_ == 7))) {
        cmdEditField_->SetFocus();
        cmdEditField_->Draw(w_);
    };
};

void PhraseView::OnPlayerUpdate(PlayerEventType eventType, unsigned int tick) {

    GUITextProperties props;
    drawNotes();

    GUIPoint anchor = GetAnchor();
    GUIPoint pos = anchor;
    pos._x = 5;

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
