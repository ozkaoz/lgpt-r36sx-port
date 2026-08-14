
#ifndef _SONG_VIEW_H_
#define _SONG_VIEW_H_

#include "BaseClasses/View.h"
#include "Application/Model/Song.h"

class SongView;

class SongView : public View {
  public:
    SongView(GUIWindow &w, ViewData *viewData, const char *song);
    ~SongView();

    // View implementation
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int tick = 0);
    virtual void OnFocus();

    // TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): whole-song snapshot history for
    // L1+X (undo) / R1+X (redo).
    // TREEFROG_GLOBAL_UNDO_V5 (Bacon 1.1.1 V14): snapshots are captured at the
    // real edit sites (chain set/clear/cut, paste, offsets, clone), not on
    // every pressed event, so L1+X reverts the last edit with Ctrl+Z
    // semantics.  The snapshot covers the whole song (8 channels x 256 rows
    // = 2048 bytes) plus the cursor; the previous 256-byte copy only covered
    // rows 0..31, which made undo look dead for edits deeper in the song.
    virtual bool GlobalUndo();
    virtual bool GlobalRedo();

  protected:
    void processNormalButtonMask(unsigned int mask);
    void processSelectionButtonMask(unsigned int mask);

    void extendSelection();
    void updateChain(int offset);
    void updateSongOffset(int offset);
    void updateCursor(int dx, int dy);
    void setChain(unsigned char);
    void cutPosition();
    void clearPosition();
    void clonePosition();
    void deepClonePosition();
    void pasteLast();
    void fillClipboardData();
    GUIRect getSelectionRect();
    void copySelection();
    void pasteClipboard();
    void cutSelection();

    void unMuteAll();
    void toggleMute();
    void switchSoloMode();

    void onStart();
    void startCurrentRow();
    void startImmediate();
    void onStop();

    void jumpToNextSection(int dir);

  private:
    bool updatingChain_; // .Flag that tells we're updating chain
                         //  so we don't allocate chains while
                         //  doing multiple A+ARROWS

    int updateX_; // . Position where update is happening
    int updateY_; //

    unsigned char lastChain_; // .Last chain clipboard

    int lastPlayedPosition_[8]; // .Last position played for song
                                //  used for drawing purpose

    int lastQueuedPosition_[8]; // .Last live queued position for song
                                //  used for drawing purpose

    struct {                  // .Clipboard structure
        bool active_;         // .If currently making a selection
        unsigned char *data_; // .Null if clipboard empty
        int x_;               // .Current selection positions
        int y_;               // .
        int offset_;          // .
        int width_;           // .Size of selection
        int height_;          // .
    } clipboard_;

    int saveX_;
    int saveY_;
    int saveOffset_;
    std::string songname_;
    bool invertBatt_;
    bool needClear_;
    bool canDeepClone_;
    bool soloToggleActive_;
    bool rAComboLatched_;
    bool rBComboLatched_;
    void nudgeTempo(int direction);
    uint8_t jumpLength_; // When jumping columns with B

    // TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): undo/redo history.  A SongEdit
    // snapshots the whole song (8 channels x 256 rows).  TREEFROG_GLOBAL_UNDO_V8
    // (Bacon 1.1.1 V16): the cursor is NOT part of the snapshot anymore --
    // undo/redo must revert the last action, and moving the cursor back to
    // the edit site reads as "undo did nothing" after navigating away.
    static const int kSongHistorySize = 16;
    struct SongEdit {
        unsigned char data[SONG_CHANNEL_COUNT * SONG_ROW_COUNT];
    };
    SongEdit songUndo_[kSongHistorySize];
    int songUndoCount_;
    SongEdit songRedo_[kSongHistorySize];
    int songRedoCount_;
    void pushSongUndo();
};

#endif
