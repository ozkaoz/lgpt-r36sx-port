
#ifndef _CHAIN_VIEW_H_
#define _CHAIN_VIEW_H_

#include "BaseClasses/View.h"
#include "ViewData.h"

class ChainView : public View {
  public:
    ChainView(GUIWindow &w, ViewData *data);
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    virtual void DrawView();
    virtual void OnFocus();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int tick = 0);

    // TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): snapshot of the edited chain
    // for L1+X (undo) / R1+X (redo).  Captured on every pressed event before
    // the mask is dispatched.
    virtual bool GlobalUndo();
    virtual bool GlobalRedo();

  protected:
    void updateCursor(int dx, int dy);
    void updateCursorValue(int offset, int dx = 0, int dy = 0);
    void updateSelectionValue(int offset);
    void setPhrase(unsigned char value);
    void warpToNeighbour(int offset);
    void warpInColumn(int offset);
    void cutPosition();
    void clonePosition();
    void pasteLastPhrase();
    void extendSelection();

    GUIRect getSelectionRect();
    void fillClipboardData();
    void copySelection();
    void cutSelection();
    void pasteClipboard();

    void unMuteAll();
    void toggleMute();
    void switchSoloMode();

    void processNormalButtonMask(unsigned short mask);
    void processSelectionButtonMask(unsigned short mask);
    void setTextProps(GUITextProperties &props, int row, int col, bool restore);

  private:
    bool updatingPhrase_;      // .Tells if we're in the middle
                               //  of updating a phrase to avoid
                               //  allocation of unused phrases
    int updateRow_;            // .Tells which row is being updated
    unsigned char lastPhrase_; // .Clipboard for phrase
    int lastPlayingPos_;
    int lastQueuedPos_;

    struct ChainClip {
        bool active_;
        int col_;
        int row_;
        int width_;
        int height_;
        //		int saverow_ ;
        //		int savecol_ ;
        unsigned char phrase_[16];
        unsigned char transpose_[16];
    } clipboard_;

    // TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): undo/redo history.  A ChainEdit
    // snapshots the 16 steps of the edited chain plus the editor cursor.
  public:
    static const int kChainHistorySize = 16;
    struct ChainEdit {
        unsigned char data[16];
        unsigned char transpose[16];
        unsigned char currentChain;
        unsigned char chainRow;
        unsigned char chainCol;
    };
    ChainEdit chainUndo_[kChainHistorySize];
    int chainUndoCount_;
    ChainEdit chainRedo_[kChainHistorySize];
    int chainRedoCount_;
    void pushChainUndo();

  private:
    int saveRow_;
    int saveCol_;
};

#endif
