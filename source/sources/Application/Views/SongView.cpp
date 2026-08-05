// TREEFROG_V42_NO_WHITE_BOX_UI
#include "SongView.h"
#include "Application/Commands/ApplicationCommandDispatcher.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/ProjectDatas.h"
#include "Application/Player/Player.h"
#include "Application/Utils/char.h"
#include "System/Console/Trace.h"
#include "System/System/System.h"
#include "UIController.h"
#include "Application/Views/BaseClasses/UiColors.h"
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <string.h>

extern "C" const char *TreeFrogU2430SongComboBuildMarker(void) {
    return "U2430_SONG_RA_RB_EDGE_TOGGLE";
}


// TREEFROG_SONG_INPUT_DEBUG_HELPERS_V1
static void treefrog_song_debug_log(const char *where,
                                    unsigned int mask,
                                    bool pressed,
                                    int viewMode,
                                    bool dirty,
                                    int songX,
                                    int songY,
                                    int songOffset,
                                    int currentValue) {
#if TREEFROG_INPUT_DEBUG
    FILE *f = fopen("/tmp/r36sx_lgpt_logs/song_debug.log", "a");
    if (!f) return;

    fprintf(f,
            "%lu %s pressed=%d mask=0x%04x"
            " A=%d B=%d X=%d Y=%d L=%d R=%d START=%d"
            " viewMode=%d dirty=%d x=%d y=%d off=%d val=0x%02x\n",
            (unsigned long)System::GetInstance()->GetClock(),
            where ? where : "song",
            pressed ? 1 : 0,
            mask,
            (mask & EPBM_A) ? 1 : 0,
            (mask & EPBM_B) ? 1 : 0,
            (mask & EPBM_X) ? 1 : 0,
            (mask & EPBM_Y) ? 1 : 0,
            (mask & EPBM_L) ? 1 : 0,
            (mask & EPBM_R) ? 1 : 0,
            (mask & EPBM_START) ? 1 : 0,
            viewMode,
            dirty ? 1 : 0,
            songX,
            songY,
            songOffset,
            currentValue & 0xff);

    fclose(f);
#else
    (void)where;
    (void)mask;
    (void)pressed;
    (void)viewMode;
    (void)dirty;
    (void)songX;
    (void)songY;
    (void)songOffset;
    (void)currentValue;
#endif
}

// TREEFROG_SONG_DEBUG_PROTECTED_ACCESS_FIX_V1
// No acceder a View::viewMode_ ni View::isDirty_ desde función libre.
void treefrog_song_debug_snapshot(SongView *self,
                                  const char *where,
                                  unsigned int mask,
                                  bool pressed) {
#if TREEFROG_INPUT_DEBUG
    if (!self || !self->viewData_) {
        treefrog_song_debug_log(where, mask, pressed, -1, false, -1, -1, -1, -1);
        return;
    }

    int val = -1;
    if (self->viewData_->song_) {
        unsigned char *p = self->viewData_->GetCurrentSongPointer();
        if (p) val = *p;
    }

    treefrog_song_debug_log(where,
                            mask,
                            pressed,
                            -1,
                            false,
                            self->viewData_->songX_,
                            self->viewData_->songY_,
                            self->viewData_->songOffset_,
                            val);
#else
    (void)self;
    (void)where;
    (void)mask;
    (void)pressed;
#endif
}


/****************
 Constructor
 ****************/

SongView::SongView(GUIWindow &w, ViewData *viewData, const char *song)
    : View(w, viewData) {

    updatingChain_ = false;
    lastChain_ = 0;
    songname_ = song;

    for (int i = 0; i < 8; i++) {
        this->lastPlayedPosition_[i] = 0;
        this->lastQueuedPosition_[i] = 0;
    }
    clipboard_.active_ = false;
    clipboard_.data_ = 0;
    invertBatt_ = false;
    canDeepClone_ = false;
    soloToggleActive_ = false;
    rAComboLatched_ = false;
    rBComboLatched_ = false;
    jumpLength_ = 0x10; // B-jump 16 rows like LSDJ
}

/****************
 Destructor
 ****************/

SongView::~SongView() {
    if (clipboard_.data_ != 0)
        SYS_FREE((void *)clipboard_.data_);
};

/******************************************************
 updateChain:
        update current chain value by adding offset
        parameter
 ******************************************************/

void SongView::updateChain(int offset) {

    unsigned int chain = viewData_->UpdateSongChain(offset);
    updatingChain_ = true;
    lastChain_ = chain;
    updateX_ = viewData_->songX_;
    updateY_ = viewData_->songY_;
    isDirty_ = true;
    canDeepClone_ = false;
}

/******************************************************
 updateChain:
        set current chain value to value parameter
 ******************************************************/

void SongView::setChain(unsigned char value) {
    viewData_->SetSongChain(value);
    lastChain_ = value;
    isDirty_ = true;
}

/******************************************************
 updateSongOffset:
        Jump from the current position up or down
    by [offset] rows
 ******************************************************/

void SongView::updateSongOffset(int offset) {
    viewData_->UpdateSongOffset(offset);
    isDirty_ = true;
    canDeepClone_ = false;
}

/******************************************************
 updateCursor:
        modify location of cursor in view by
        adding dx & dy parameters
 ******************************************************/

void SongView::updateCursor(int dx, int dy) {
    viewData_->UpdateSongCursor(dx, dy);
    isDirty_ = true;
    canDeepClone_ = false;
}

/******************************************************
 cutPosition:
        copy current position content to clipboard &
        erase current position value
 ******************************************************/

void SongView::cutPosition() {
    // TREEFROG_SONG_CUT_ENTER_DEBUG_V1
    treefrog_song_debug_snapshot(this, "cutPosition.enter", 0, true);


    // prepare selection data
    clipboard_.x_ = viewData_->songX_;
    clipboard_.y_ = viewData_->songY_;
    clipboard_.offset_ = viewData_->songOffset_;

    saveX_ = viewData_->songX_;
    saveY_ = viewData_->songY_;
    saveOffset_ = viewData_->songOffset_;

    // cut selection
    cutSelection();

    // TREEFROG_SONG_CUT_FORCE_DIRTY_V2
    isDirty_ = true;
    SetDirty(true);

    // TREEFROG_SONG_CUT_EXIT_DEBUG_V1
    isDirty_ = true;
    SetDirty(true);
    treefrog_song_debug_snapshot(this, "cutPosition.exit.forceDirty", 0, true);
}

// TREEFROG_INPUT_SONG_AB_CLEAR_YX_CUT
// Clear de una celda Song sin compactar filas.
// Deja la posición actual como -- usando el mismo sentinel 0xFF que ya usa SongView.
void SongView::clearPosition() {
    // TREEFROG_SONG_CLEAR_ENTER_DEBUG_V1
    treefrog_song_debug_snapshot(this, "clearPosition.enter", 0, true);

    int col = (int)viewData_->songX_;
    int row = (int)viewData_->songY_ + (int)viewData_->songOffset_;

    if (col < 0 || col >= SONG_CHANNEL_COUNT) return;
    if (row < 0 || row >= SONG_ROW_COUNT) return;

    unsigned char *cell = viewData_->song_->data_ + row * SONG_CHANNEL_COUNT + col;
    *cell = 0xFF;

    // TREEFROG_SONG_CLEAR_FORCE_DIRTY_V2
    isDirty_ = true;
    SetDirty(true);

    // TREEFROG_SONG_CLEAR_EXIT_DEBUG_V1
    isDirty_ = true;
    SetDirty(true);
    treefrog_song_debug_snapshot(this, "clearPosition.exit.forceDirty", 0, true);
};

/******************************************************
 pastePosition:
        set current position to last chain value if
        current step is empty
 ******************************************************/

void SongView::pasteLast() {

    // If we're on an empty spot, we past the last chain
    // otherwise we take the current chain as last

    unsigned char *c = viewData_->GetCurrentSongPointer();
    if (*c == 0xFF) {
        *c = lastChain_;
        viewData_->song_->chain_->SetUsed(*c);
        isDirty_ = true;
    } else {
        lastChain_ = *c;
    }
};

/******************************************************
 clonePosition:
        slim clone current position
 ******************************************************/

void SongView::clonePosition() {

    unsigned char *pos = viewData_->GetCurrentSongPointer();
    unsigned char current = *pos;
    if (current == 255)
        return;

    unsigned short next = viewData_->song_->chain_->GetNext();
    if (next == NO_MORE_CHAIN)
        return;

    unsigned char *src = viewData_->song_->chain_->data_ + 16 * current;
    unsigned char *dst = viewData_->song_->chain_->data_ + 16 * next;

    for (int i = 0; i < 16; i++) {
        *dst++ = *src++;
    };

    src = viewData_->song_->chain_->transpose_ + 16 * current;
    dst = viewData_->song_->chain_->transpose_ + 16 * next;

    for (int i = 0; i < 16; i++) {
        *dst++ = *src++;
    };
    setChain((unsigned char)next);
    isDirty_ = true;
};

/******************************************************
 deepClonePosition:
        deep clone chain and all phrases within
        made by koisignal (https://github.com/koi-ikeno)
 ******************************************************/

void SongView::deepClonePosition() {
    Phrase *ph = viewData_->song_->phrase_;
    Chain *ch = viewData_->song_->chain_;
    unsigned char *pos = viewData_->GetCurrentSongPointer();
    unsigned char curChainNum = *pos;

    if (curChainNum == CHAIN_COUNT) {
        View::SetNotification("no more chains!");
        return;
    }

    unsigned char *srcChain = ch->data_ + 16 * curChainNum;
    unsigned char *dstChain = ch->data_ + 16 * curChainNum;
    unsigned short srcPhrases[16];
    unsigned short dstPhrases[16];

    // Init outside valid range
    for (int i = 0; i < 16; i++) {
        srcPhrases[i] = NO_MORE_CHAIN;
        dstPhrases[i] = NO_MORE_CHAIN;
    }

    for (int i = 0; i < 16; i++) {
        unsigned short srcPhraseNum = *srcChain;

        // skip when "--"
        if (srcPhraseNum == CHAIN_COUNT) {
            srcChain++;
            dstChain++;
            continue;
        }

        unsigned short newPhraseNum = NO_MORE_CHAIN;

        for (int j = 0; j < 16; j++) {
            if (srcPhrases[j] == srcPhraseNum) {
                newPhraseNum = dstPhrases[j];
                break;
            }
        }

        if (newPhraseNum == NO_MORE_CHAIN) {
            newPhraseNum = ph->GetNext();
            if (newPhraseNum == NO_MORE_PHRASE) {
                View::SetNotification("no more phrases!");
                return;
            }
            for (int k = 0; k < 16; k++) {
                *(ph->note_ + 16 * newPhraseNum + k) =
                    *(ph->note_ + 16 * srcPhraseNum + k);
                *(ph->instr_ + 16 * newPhraseNum + k) =
                    *(ph->instr_ + 16 * srcPhraseNum + k);
                *(ph->cmd1_ + 16 * newPhraseNum + k) =
                    *(ph->cmd1_ + 16 * srcPhraseNum + k);
                *(ph->cmd2_ + 16 * newPhraseNum + k) =
                    *(ph->cmd2_ + 16 * srcPhraseNum + k);
                *(ph->param1_ + 16 * newPhraseNum + k) =
                    *(ph->param1_ + 16 * srcPhraseNum + k);
                *(ph->param2_ + 16 * newPhraseNum + k) =
                    *(ph->param2_ + 16 * srcPhraseNum + k);
            }
        }
        srcPhrases[i] = srcPhraseNum;
        dstPhrases[i] = newPhraseNum;
        *dstChain = newPhraseNum;
        srcChain++;
        dstChain++;
    }
    View::SetNotification("deep clone");

    setChain((unsigned char)curChainNum);
}

void SongView::extendSelection() {
    GUIRect rect = getSelectionRect();
    if (rect.Left() > 0 || rect.Right() < 7) {
        if (viewData_->songX_ < clipboard_.x_) {
            viewData_->songX_ = 0;
            clipboard_.x_ = 7;
        } else {
            viewData_->songX_ = 7;
            clipboard_.x_ = 0;
        }
        isDirty_ = true;
    } else {
        if (viewData_->songY_ < clipboard_.y_) {
            viewData_->songY_ = 0;
            clipboard_.y_ = 0x17;
        } else {
            clipboard_.y_ = 0;
            viewData_->songY_ = 0x17;
        }
        isDirty_ = true;
    }
}

/******************************************************
 OnFocus:
        called when current view is becoming active
 ******************************************************/

void SongView::OnFocus() { clipboard_.active_ = false; };

GUIRect SongView::getSelectionRect() {

    GUIRect selRect(clipboard_.x_, clipboard_.y_ + clipboard_.offset_,
                    viewData_->songX_,
                    viewData_->songY_ + viewData_->songOffset_);

    selRect.Normalize();
    return selRect;
}

/******************************************************
 fillClipboard:
        fill clipboard with current selection value
 ******************************************************/

void SongView::fillClipboardData() {

    // Clear current selection data

    if (!clipboard_.data_)
        SYS_FREE((void *)clipboard_.data_);

    // Prepare selection related information

    GUIRect selRect = getSelectionRect();

    // Set current selection  data

    clipboard_.width_ = selRect.Width() + 1;
    clipboard_.height_ = selRect.Height() + 1;

    clipboard_.data_ =
        (unsigned char *)SYS_MALLOC(clipboard_.width_ * clipboard_.height_);

    unsigned char *src = viewData_->song_->data_ + selRect.Left() +
                         SONG_CHANNEL_COUNT * selRect.Top();
    unsigned char *dst = clipboard_.data_;

    for (int j = 0; j < clipboard_.height_; j++) {
        for (int i = 0; i < clipboard_.width_; i++) {
            *dst++ = *src++;
        }
        src += (SONG_CHANNEL_COUNT - clipboard_.width_);
    }
};

/******************************************************
 copySelection:
        copy current selection to clipboard
 ******************************************************/

void SongView::copySelection() {

    fillClipboardData();
    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    viewData_->songX_ = saveX_;
    viewData_->songY_ = saveY_;
    viewData_->songOffset_ = saveOffset_;
    View::SetNotification("copied selection");
}

/******************************************************
 cutSelection:
        cut current selection to clipboard
 ******************************************************/

void SongView::cutSelection() {

    // first copy the data to clipboard

    fillClipboardData();
    GUIRect selRect = getSelectionRect();

    // now move all rows up for cut

    unsigned char *dst = viewData_->song_->data_ + selRect.Left() +
                         SONG_CHANNEL_COUNT * (selRect.Top());
    unsigned char *src = dst + SONG_CHANNEL_COUNT * clipboard_.height_;

    int rowCount = SONG_ROW_COUNT - selRect.Bottom() - 1;

    for (int j = 0; j < rowCount; j++) {

        for (int i = 0; i < clipboard_.width_; i++) {
            *dst++ = *src++;
        }
        src += (SONG_CHANNEL_COUNT - clipboard_.width_);
        dst += (SONG_CHANNEL_COUNT - clipboard_.width_);
    }

    // TREEFROG_U2_23_SONG_CUT_TAIL_CLEAR_FIX
    for (int j = 0; j < clipboard_.height_; j++) {
        for (int i = 0; i < clipboard_.width_; i++) {
            *dst++ = 0xFF;
        }
        dst += (SONG_CHANNEL_COUNT - clipboard_.width_);
    };

    clipboard_.active_ = false;
    viewMode_ = VM_NORMAL;
    viewData_->songX_ = saveX_;
    viewData_->songY_ = saveY_;
    viewData_->songOffset_ = saveOffset_;

    isDirty_ = true;
}

/******************************************************
 pasteSelection:
        paste clipboard content to song
 ******************************************************/

void SongView::pasteClipboard() {

    if (!clipboard_.data_)
        return;

    // Check we're not out of scope

    int width = clipboard_.width_;
    int height = clipboard_.height_;

    if (viewData_->songX_ + width > SONG_CHANNEL_COUNT) {
        width = SONG_CHANNEL_COUNT - viewData_->songX_;
    }
    if (viewData_->songY_ + viewData_->songOffset_ + height > SONG_ROW_COUNT) {
        height = SONG_ROW_COUNT - viewData_->songY_ - viewData_->songOffset_;
    } else {

        // Move down from insert point

        unsigned char *dst = viewData_->song_->data_ + viewData_->songX_ +
                             (SONG_ROW_COUNT - 1) * SONG_CHANNEL_COUNT;
        unsigned char *src = dst - height * SONG_CHANNEL_COUNT;

        int rowCount =
            SONG_ROW_COUNT - (viewData_->songY_ + viewData_->songOffset_);

        for (int j = 0; j < rowCount; j++) {
            for (int i = 0; i < width; i++) {
                *dst++ = *src++;
            }
            dst -= (SONG_CHANNEL_COUNT + width);
            src -= (SONG_CHANNEL_COUNT + width);
        }
    }

    // Prepare copy pointer

    unsigned char *dst = viewData_->GetCurrentSongPointer();
    unsigned char *src = clipboard_.data_;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            *dst++ = *src++;
        }
        dst += (SONG_CHANNEL_COUNT - width);
        src += (clipboard_.width_ - width);
    }

    updateCursor(0, height);
}

void SongView::unMuteAll() {

    UIController *controller = UIController::GetInstance();
    controller->UnMuteAll();
    soloToggleActive_ = false;
    viewMode_ = VM_NORMAL;
};

void SongView::toggleMute() {

    UIController *controller = UIController::GetInstance();

    int from = viewData_->songX_;
    int to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    };
    controller->ToggleMute(from, to);
    viewMode_ = (viewMode_ != VM_MUTEON) ? VM_MUTEON : VM_NORMAL;
};

void SongView::switchSoloMode() {

    UIController *controller = UIController::GetInstance();
    int from = viewData_->songX_;
    int to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    };

    /*
     * U2.43.0 SONG_RA_EDGE_TOGGLE
     *
     * viewMode_ is transient and can be reset by the initial R press before A
     * arrives.  Keep the actual solo toggle state independently so a second
     * complete R+A chord restores the pre-solo mute mask.
     */
    const bool enableSolo = !soloToggleActive_;
    controller->SwitchSoloMode(from, to, enableSolo);
    soloToggleActive_ = enableSolo;
    viewMode_ = enableSolo ? VM_SOLOON : VM_NORMAL;
    isDirty_ = true;
};

void SongView::onStart() {
    // Always play with zero offset in chains when in SongView
    viewData_->chainRow_ = 0;
    Player *player = Player::GetInstance();
    unsigned char from = viewData_->songX_;
    unsigned char to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    }
    int renderMode = viewData_->renderMode_;
    if (renderMode > 0 && !player->IsRunning()) {
        viewData_->isRendering_ = true;
        View::SetNotification("Rendering started!");
    } else if (viewData_->isRendering_ && player->IsRunning()) {
        viewData_->isRendering_ = false;
        View::SetNotification("Rendering done!");
    }
    player->OnSongStartButton(from, to, false, false);
};

void SongView::startCurrentRow() {
    Player *player = Player::GetInstance();
    player->SetSequencerMode(SM_LIVE);
    player->OnSongStartButton(0, 7, false, false);
}

void SongView::startImmediate() {
    Player *player = Player::GetInstance();

    unsigned char from = viewData_->songX_;
    unsigned char to = from;
    player->OnSongStartButton(from, to, false, true);
}

void SongView::onStop() {
    // Always play with zero offset in chains when in SongView
    viewData_->chainRow_ = 0;
    Player *player = Player::GetInstance();
    unsigned char from = viewData_->songX_;
    unsigned char to = from;
    if (clipboard_.active_) {
        GUIRect r = getSelectionRect();
        from = r.Left();
        to = r.Right();
    }

    player->OnSongStartButton(from, to, true, false);
};

void SongView::jumpToNextSection(int direction) {

    int current = viewData_->songY_ + viewData_->songOffset_;
    bool foundGap = false;
    for (int i = 0; i < SONG_ROW_COUNT; i++) {
        unsigned char *start = viewData_->song_->data_ + viewData_->songX_ +
                               SONG_CHANNEL_COUNT * current;
        if (foundGap && (*start != 0xFF)) {
            break;
        } else {
            if (*start == 0xFF) {
                foundGap = true;
            }
        }
        current += direction;
        if (current < 0) {
            current += SONG_ROW_COUNT;
        }
        if (current >= SONG_ROW_COUNT) {
            current -= SONG_ROW_COUNT;
        }
    }
    // If we go backwards, we stil have to go to the beginning of the block

    if (direction < 0) {
        while (current > 0) {
            unsigned char *start = viewData_->song_->data_ + viewData_->songX_ +
                                   SONG_CHANNEL_COUNT * current;
            if (*start == 0xFF) {
                current++;
                break;
            };
            current--;
        };
    }

    // Update viewdata position from current

    if ((current - viewData_->songOffset_ > 0x17) ||
        (current - viewData_->songOffset_ < 0)) {
        viewData_->songOffset_ = current - 4;
        if (viewData_->songOffset_ < 0) {
            viewData_->songOffset_ = 0;
        }
    }
    viewData_->songY_ = current - viewData_->songOffset_;
    isDirty_ = true;
}

/******************************************************
 ProcessButtonMask:
        process button mask even coming from the main
        application window
 ******************************************************/

void SongView::ProcessButtonMask(unsigned short mask, bool pressed) {
    // TREEFROG_SONG_PROCESS_BUTTON_ENTER_DEBUG_V1
    treefrog_song_debug_snapshot(this, "ProcessButtonMask.enter", mask, pressed);

    /*
     * U2.43.0 SONG_LOCAL_COMBO_LATCH
     *
     * R+A and R+B are edge-triggered toggles.  They execute once per complete
     * chord and are rearmed only after both chord buttons have been released.
     * No action is executed on release.
     */
    const unsigned short soloChord = EPBM_R | EPBM_A;
    const unsigned short muteChord = EPBM_R | EPBM_B;

    if (!pressed) {
        if ((mask & soloChord) == 0) rAComboLatched_ = false;
        if ((mask & muteChord) == 0) rBComboLatched_ = false;
        return;
    }

    // TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): snapshot the song + cursor
    // before every pressed event so L1+X can revert any edit and the
    // cursor movement that went with it.
    pushSongUndo();

    if (mask == soloChord) {
        if (!rAComboLatched_) {
            rAComboLatched_ = true;
            switchSoloMode();
        }
        return;
    }

    if (mask == muteChord) {
        if (!rBComboLatched_) {
            rBComboLatched_ = true;
            toggleMute();
        }
        return;
    }

    /*
     * Preserve the active solo visual state while the user begins the second
     * R+A chord.  The subsequent exact chord will disable solo.
     */
    if (mask == EPBM_R && soloToggleActive_) return;

    if (viewMode_ == VM_NEW) {
        if (mask == EPBM_A) {
            unsigned short next = viewData_->song_->chain_->GetNext();
            if (next != NO_MORE_CHAIN) {
                setChain((unsigned char)next);
                isDirty_ = true;
            }
            mask &= (0xFFFF - EPBM_A);
        }
    }

    if (viewMode_ == VM_CLONE) {
        if ((mask & EPBM_A) && (mask & EPBM_L)) {
            clonePosition();
            mask &= (0xFFFF - (EPBM_A | EPBM_L));
            canDeepClone_ = true;
        } else {
            viewMode_ = VM_SELECTION;
        }
    };

    if (canDeepClone_ && (mask & EPBM_A) && (mask & EPBM_L)) {
        deepClonePosition();
        mask &= (0xFFFF - (EPBM_A | EPBM_L));
        canDeepClone_ = false;
    }
    if (clipboard_.active_) {
        viewMode_ = VM_SELECTION;
    };
    // Process selection related keys

    if (viewMode_ == VM_SELECTION) {
        if (clipboard_.active_ == false) {
            clipboard_.active_ = true;
            clipboard_.x_ = viewData_->songX_;
            clipboard_.y_ = viewData_->songY_;
            clipboard_.offset_ = viewData_->songOffset_;
            saveX_ = clipboard_.x_;
            saveY_ = clipboard_.y_;
            saveOffset_ = clipboard_.offset_;
        }
        processSelectionButtonMask(mask);
    } else {

        // Switch back to normal mode

        viewMode_ = VM_NORMAL;
        processNormalButtonMask(mask);
    }

    // TREEFROG_SONG_PROCESS_BUTTON_EXIT_DEBUG_V1
    treefrog_song_debug_snapshot(this, "ProcessButtonMask.exit", mask, pressed);
}

// TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): whole-song snapshot undo/redo.
void SongView::pushSongUndo() {
    SongEdit e;
    memcpy(e.data, viewData_->song_->data_, 256);
    e.songX = (unsigned char)viewData_->songX_;
    e.chainRow = (unsigned char)viewData_->chainRow_;
    for (int i = kSongHistorySize - 1; i > 0; i--) songUndo_[i] = songUndo_[i - 1];
    songUndo_[0] = e;
    songUndoCount_++;
    if (songUndoCount_ > kSongHistorySize) songUndoCount_ = kSongHistorySize;
    songRedoCount_ = 0;
}

bool SongView::GlobalUndo() {
    if (songUndoCount_ == 0) return true;
    SongEdit e = songUndo_[0];
    for (int i = 0; i < songUndoCount_ - 1; i++) songUndo_[i] = songUndo_[i + 1];
    songUndoCount_--;
    for (int i = kSongHistorySize - 1; i > 0; i--) songRedo_[i] = songRedo_[i - 1];
    songRedo_[0] = e;
    songRedoCount_++;
    if (songRedoCount_ > kSongHistorySize) songRedoCount_ = kSongHistorySize;
    memcpy(viewData_->song_->data_, e.data, 256);
    viewData_->songX_ = e.songX;
    viewData_->chainRow_ = e.chainRow;
    isDirty_ = true;
    return true;
}

bool SongView::GlobalRedo() {
    if (songRedoCount_ == 0) return true;
    SongEdit e = songRedo_[0];
    for (int i = 0; i < songRedoCount_ - 1; i++) songRedo_[i] = songRedo_[i + 1];
    songRedoCount_--;
    for (int i = kSongHistorySize - 1; i > 0; i--) songUndo_[i] = songUndo_[i - 1];
    songUndo_[0] = e;
    songUndoCount_++;
    if (songUndoCount_ > kSongHistorySize) songUndoCount_ = kSongHistorySize;
    memcpy(viewData_->song_->data_, e.data, 256);
    viewData_->songX_ = e.songX;
    viewData_->chainRow_ = e.chainRow;
    isDirty_ = true;
    return true;
}

/******************************************************
 processNormalButtonMask:
        process button mask in the case there is no
        selection active
 ******************************************************/

void SongView::processNormalButtonMask(unsigned int mask) {
    // TREEFROG_SONG_NORMAL_ENTER_DEBUG_V1
    treefrog_song_debug_snapshot(this, "processNormal.enter", mask, true);
    if ((mask & EPBM_B) && (mask & EPBM_A)) {
        treefrog_song_debug_snapshot(this, "processNormal.AB_SEEN", mask, true);
    }
    if ((mask & EPBM_X) || (mask & EPBM_Y)) {
        treefrog_song_debug_snapshot(this, "processNormal.XY_SEEN", mask, true);
    }

    // TREEFROG_INPUT_SONG_YX_CUT_NORMAL_MODE_V2
    // R36SX TreeFrog:
    // Y+X ejecuta el cut/compact histórico de Song.
    // No exigimos máscara exacta porque el runtime puede mantener otros bits latcheados.
    const unsigned int treefrogSongYXMask = EPBM_Y | EPBM_X;
    if ((mask & treefrogSongYXMask) == treefrogSongYXMask) {
        cutPosition();
        isDirty_ = true;
        SetDirty(true);
        return;
    }

    // B Modifier

    if (mask & EPBM_B) {

        if (mask & EPBM_DOWN)
            updateSongOffset(SongView::jumpLength_);
        if (mask & EPBM_UP)
            updateSongOffset(-SongView::jumpLength_);
        if (mask & (EPBM_RIGHT | EPBM_LEFT)) {
            Player *player = Player::GetInstance();
            switch (player->GetSequencerMode()) {
            case SM_SONG:
                player->SetSequencerMode(SM_LIVE);
                break;
            case SM_LIVE:
                player->SetSequencerMode(SM_SONG);
                break;
            }
            isDirty_ = true;
        }
        if ((mask & EPBM_A) && (!(mask & EPBM_R)))
            // TREEFROG_INPUT_SONG_AB_CLEAR_YX_CUT
            // A+B ahora limpia la celda actual sin compactar filas.
            clearPosition();
        if (mask & EPBM_L) {

            viewMode_ = VM_CLONE;
        };
        if (mask & EPBM_R) {
            toggleMute();
        };
        if (mask & EPBM_START) {
            startImmediate();
        }
    } else {

        // A modifier

        if (mask & EPBM_A) {

            if (mask & EPBM_DOWN)
                updateChain(-0x10);
            if (mask & EPBM_UP)
                updateChain(0x10);
            if (mask & EPBM_LEFT)
                updateChain(-0x01);
            if (mask & EPBM_RIGHT)
                updateChain(0x01);
            if (mask & EPBM_L && !canDeepClone_) {
                pasteClipboard();
            }
            if (mask == EPBM_A) {

                pasteLast();
                viewMode_ = VM_NEW;
            }
            if (mask & EPBM_R) {
                switchSoloMode();
            };
        } else {

            // R Modifier

            if (mask & EPBM_R) {

                if (mask & EPBM_L) {
                    unMuteAll();
                }

                if (mask & EPBM_RIGHT) {
                    unsigned char *data = viewData_->GetCurrentSongPointer();
                    if (*data != 0xFF) {
                        ViewType vt = VT_CHAIN;
                        ViewEvent ve(VET_SWITCH_VIEW, &vt);
                        viewData_->currentChain_ = *data;
                        SetChanged();
                        NotifyObservers(&ve);
                    }
                }

                if (mask & EPBM_UP) {
                    ViewType vt = VT_PROJECT;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if (mask & EPBM_DOWN) {
                    ViewType vt = VT_MIXER;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if (mask & EPBM_START) {
                    onStop();
                }

            } else {

                // L Modifier

                if (mask & EPBM_L) {
                    if (mask & EPBM_DOWN)
                        jumpToNextSection(1);
                    if (mask & EPBM_UP)
                        jumpToNextSection(-1);
                    if (mask & EPBM_START)
                        startCurrentRow();
                    if (mask & EPBM_LEFT)
                        nudgeTempo(-1);
                    if (mask & EPBM_RIGHT)
                        nudgeTempo(1);
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
                        onStart();
                    }
                }
            }
        }
    }

    if ((!(mask & EPBM_A)) && updatingChain_) {
        unsigned char *c = viewData_->song_->data_ + updateX_ +
                           8 * (viewData_->songOffset_ + updateY_);
        viewData_->song_->chain_->SetUsed(*c);
        updatingChain_ = false;
    }
};

/******************************************************
 processSelectionButtonMask:
        process button mask in the case there is a
        selection active
 ******************************************************/

void SongView::processSelectionButtonMask(unsigned int mask) {

    // B Modifier

    if (mask & EPBM_B) {
        if (mask & EPBM_R) {
            toggleMute();
        };
        if (mask & EPBM_L) {
            extendSelection();
        };
        if (mask == EPBM_B) {
            copySelection();
        }

    } else {

        // A modifier

        if (mask & EPBM_A) {
            if (mask & EPBM_L) {
                cutSelection();
            }
            if (mask & EPBM_R) {
                switchSoloMode();
            };
        } else {

            // R Modifier

            if (mask & EPBM_R) {

                if (mask & EPBM_L) {
                    unMuteAll();
                }

                if (mask & EPBM_RIGHT) {
                    unsigned char *data = viewData_->GetCurrentSongPointer();
                    if (*data != 0xFF) {
                        ViewType vt = VT_CHAIN;
                        ViewEvent ve(VET_SWITCH_VIEW, &vt);
                        viewData_->currentChain_ = *data;
                        SetChanged();
                        NotifyObservers(&ve);
                    }
                }

                if (mask & EPBM_UP) {
                    ViewType vt = VT_PROJECT;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if (mask & EPBM_DOWN) {
                    ViewType vt = VT_MIXER;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if (mask & EPBM_START) {
                    onStop();
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
                    onStart();
                }
            }
        }
    }
}

/******************************************************
 Redraw:
        redraw completely the song view
 ******************************************************/

void SongView::DrawView() {

    Clear();
    View::EnableNotification();

    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();

    // Prepare selection related information

    GUIRect selRect;
    if (clipboard_.active_) {
        selRect = GUIRect(clipboard_.x_, clipboard_.y_ + clipboard_.offset_,
                          viewData_->songX_,
                          viewData_->songY_ + viewData_->songOffset_);

        selRect.Normalize();
    }

    // Draw title

    // RC4 P3 (PLAN_RC4): page titles render with the semantic title role.
    SetColor(UiColors::Resolve(UI_COLOR_TITLE));

    Player *player = Player::GetInstance();

    std::ostringstream os;

    os << ((player->GetSequencerMode() == SM_SONG) ? "Song" : "Live");
    os << ": ";

    if (songname_.substr(0, 5) == "lgpt_") {
        os << songname_.substr(5);
    }

    std::string buffer(os.str());

    DrawString(pos._x, pos._y, buffer.c_str(), props);

    // Compute song grid location

    GUIPoint anchor = GetAnchor();

    // Display row numbers

    char row[3];
    pos = anchor;
    pos._x -= 3;
    for (int j = 0; j < View::songRowCount_; j++) {
        char p = j + viewData_->songOffset_;
        ((p / altRowNumber_) % 2) ? SetColor(CD_ROW) : SetColor(CD_ROW2);
        hex2char(p, row);
        DrawString(pos._x, pos._y, row, props);
        pos._y += 1;
    }

    SetColor(CD_NORMAL);

    pos = anchor;
    unsigned char *data =
        viewData_->song_->data_ + (SONG_CHANNEL_COUNT * viewData_->songOffset_);
    short dx = 3;
    short dy = 1;
    for (int j = 0; j < View::songRowCount_; j++) {

        pos._x = anchor._x;

        for (int i = 0; i < 8; i++) {

            bool invert = false;

            // see if we need to invert current step
            // if there's a selection or we are at cursor position

            if (clipboard_.active_) {
                if ((i >= selRect.Left()) && (i <= selRect.Right()) &&
                    (j + viewData_->songOffset_ >= selRect.Top()) &&
                    (j + viewData_->songOffset_ <= selRect.Bottom())) {
                    invert = true;
                }
            } else {
                if (i == viewData_->songX_ && j == viewData_->songY_) {
                    invert = true;
                }
            }

            // draw current step

            unsigned char d = *data++;

            if (d == 0xFE) {
                SetColor(CD_SONGVIEWFE);
            } else if (d == 0x00) {
                SetColor(CD_SONGVIEW00);
            } else {
                SetColor(CD_NORMAL);
            }

            if (invert) {
                SetColor(CD_HILITE2);
                props.invert_ = true;
            }

            if (d == 0xFF) {
                DrawString(pos._x, pos._y, "--", props);
            } else {
                hex2char(d, row);
                DrawString(pos._x, pos._y, row, props);
            }

            // Put back drawing state

            if (invert) {
                SetColor(CD_NORMAL);
                props.invert_ = false;
            }

            // Next step

            pos._x += dx;
        }
        pos._y += dy;
    }
    SetColor(CD_NORMAL);

    drawMap();
    drawNotes();

    if (player->IsRunning()) {
        OnPlayerUpdate(PET_UPDATE);
    };
};

/******************************************************
 OnPlayerUpdate:
        Called when positions in player change. Should
        provide visual feedback of currently played
        position
 ******************************************************/

void SongView::OnPlayerUpdate(PlayerEventType eventType, unsigned int tick) {

    Player *player = Player::GetInstance();

    GUIPoint anchor = GetAnchor();
    GUIPoint pos = anchor;
    pos._x -= 1;

    GUITextProperties props;
    SetColor(CD_CURSOR);

    // Loop on all channels

    for (int i = 0; i < SONG_CHANNEL_COUNT; i++) {

        // Clear all current positions

        int y = lastPlayedPosition_[i] - viewData_->songOffset_;
        if (y >= 0 && y < View::songRowCount_ &&
            viewData_->playMode_ != PM_AUDITION) {
            pos._y = anchor._y + y;
            DrawString(pos._x, pos._y, " ", props);
        }

        // Clear all last queued positions

        y = lastQueuedPosition_[i] - viewData_->songOffset_;
        if (y >= 0 && y < View::songRowCount_) {
            pos._y = anchor._y + y;
            DrawString(pos._x, pos._y, " ", props);
        }

        // For each playing position, draw current location

        if (player->IsChannelPlaying(i)) {
            if (eventType != PET_STOP) {
                if (viewData_->currentPlayChain_[i] != 0xFF) {
                    int y = viewData_->songPlayPos_[i] - viewData_->songOffset_;
                    if (y >= 0 && y < View::songRowCount_) {
                        pos._y = anchor._y + y;
                        if (!player->IsChannelMuted(i)) {
                            SetColor(CD_PLAY);
                            DrawString(pos._x, pos._y, ">", props);
                        } else {
                            SetColor(CD_MUTE);
                            DrawString(pos._x, pos._y, "-", props);
                        }
                        SetColor(CD_CURSOR);
                        lastPlayedPosition_[i] = viewData_->songPlayPos_[i];
                    }
                }
            }
        }

        // If in live mode, update queued position

        if (player->GetSequencerMode() == SM_LIVE) {
            if (player->GetQueueingMode(i) != QM_NONE) {

                if (eventType != PET_STOP) {
                    int y =
                        player->GetQueuePosition(i) - viewData_->songOffset_;
                    if (y >= 0 && y < View::songRowCount_) {
                        pos._y = anchor._y + y;
                        char *indicator = player->GetLiveIndicator(i);
                        DrawString(pos._x, pos._y, indicator, props);
                        lastQueuedPosition_[i] = player->GetQueuePosition(i);
                    }
                }
            };
        }
        pos._x += 3;
    }

    SetColor(CD_NORMAL);

    // Draw clipping indicator & CPU usage

    if (View::miniLayout_) {
        pos._y = 0;
        pos._x = 25;
    } else {
        pos = anchor;
        pos._x += 25;
    }

    if (player->Clipped()) {
        DrawString(pos._x, pos._y, "clip", props);
    } else {
        DrawString(pos._x, pos._y, "----", props);
    }

    char strbuffer[10];
    pos._y += 1;
    sprintf(strbuffer, "%3.3d%%", player->GetPlayedBufferPercentage());
    DrawString(pos._x, pos._y, strbuffer, props);

    System *sys = System::GetInstance();
    int batt = sys->GetBatteryLevel();
    if (batt >= 0) {
        if (batt < 90) {
            SetColor(CD_HILITE2);
            invertBatt_ = !invertBatt_;
        } else {
            invertBatt_ = false;
        };
        props.invert_ = invertBatt_;

        pos._y += 1;
        sprintf(strbuffer, "%3.3d", batt);
        DrawString(pos._x, pos._y, strbuffer, props);
    }

    if (eventType != PET_STOP) {
        SetColor(CD_NORMAL);
        props.invert_ = false;
        int time = int(player->GetPlayTime());
        int mi = time / 60;
        int se = time - mi * 60;
        sprintf(strbuffer, "%2.2d:%2.2d", mi, se);
        pos._y += 1;
        DrawString(pos._x, pos._y, strbuffer, props);
    }
    drawNotes();
};

void SongView::nudgeTempo(int direction) {
    ApplicationCommandDispatcher *dispatcher =
        ApplicationCommandDispatcher::GetInstance();
    switch (direction) {
    case -1:
        dispatcher->OnNudgeDown();
        break;
    case 1:
        dispatcher->OnNudgeUp();
        break;
    }
}
