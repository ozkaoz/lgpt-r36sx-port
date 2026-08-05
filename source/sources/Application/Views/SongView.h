
#ifndef _SONG_VIEW_H_
#define _SONG_VIEW_H_

#include "BaseClasses/View.h"

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
    // L1+X (undo) / R1+X (redo).  Snapshots are captured on every pressed
    // event before the mask is dispatched, so any edit (chain set/clear/cut,
    // paste, offsets) and the cursor position revert as a unit.
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
    // snapshots the 256 song rows plus the editor cursor.
    static const int kSongHistorySize = 16;
    struct SongEdit {
        unsigned char data[256];
        unsigned char songX;
        unsigned char chainRow;
    };
    SongEdit songUndo_[kSongHistorySize];
    int songUndoCount_;
    SongEdit songRedo_[kSongHistorySize];
    int songRedoCount_;
    void pushSongUndo();
};

#endif
