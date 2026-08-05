
#ifndef _PHRASE_VIEW_H_
#define _PHRASE_VIEW_H_

#include "BaseClasses/UIBigHexVarField.h"
#include "BaseClasses/View.h"
#include "ViewData.h"

#define FCC_EDIT MAKE_FOURCC('V', 'O', 'L', 'M')

class PhraseView : public View {

  public:
    PhraseView(GUIWindow &w, ViewData *viewData);
    ~PhraseView();
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int tick = 0);
    virtual void OnFocus();

    // TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): snapshot of the edited phrase
    // for L1+X (undo) / R1+X (redo).  Captured on every pressed event before
    // the mask is dispatched.
    virtual bool GlobalUndo();
    virtual bool GlobalRedo();
    void onCommandSelectorResult(ModalView &d);
    void onCommandSelectorPreview(ModalView &d);

  protected:
    void updateCursor(int dx, int dy);
    void stopAudition();
    void updateCursorValue(ViewUpdateDirection offset, int xOffset = 0,
                           int yOffset = 0, int bigStep = 0);
    bool isCommandColumn() const;
    FourCC *getCurrentCommandPointer();
    void updateSelectionValue(ViewUpdateDirection direction);
    void warpToNeighbour(int offset);
    void warpInChain(int offset);
    void cutPosition();
    void pasteLast();

    void extendSelection();

    GUIRect getSelectionRect();
    void fillClipboardData();
    void interpolateSelection();
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
    int row_;
    int col_;
    int lastNote_;
    int lastVol_;
    int lastInstr_;
    int lastPitch_;
    int lastCmd_;
    int lastParam_;
    bool commandSelectorModalActive_;
    Phrase *phrase_;
    int lastPlayingPos_;
    Variable cmdEdit_;
    UIBigHexVarField *cmdEditField_;
    void printHelpLegend(FourCC command, GUITextProperties props);
    void enterCommandSelector();
    // TREEFROG_COMMAND_SPECS_V1 (Fase 6): configures the shared hex editor
    // (precision/range) from the command stored in the cell's cmd column.
    void applyCmdEditMode(int paramCol);
    void applyCmdEditModeForCommand(FourCC command);
    int findClosestInstrumentFor(int);
    int getChopSourceInstrumentForCurrentRow();
    int getChopSourceInstrumentForRow(int row);
    int getSavedChopCountForRow(int row, int *sourceInstrument);
    bool updateChopNoteValueForRow(int row, ViewUpdateDirection direction);
    bool pasteDefaultChopForRow(int row);
    bool adjustPtchParamForRow(int row, int paramCol, ViewUpdateDirection direction);
    void formatPtchParam(ushort value, char *buffer, int bufferLen) const;
    bool isPtchParamCell(int row, int col) const;
    bool assignChopFromPhrase(int delta, bool advanceRow);
    struct clipboard {
        bool active_;
        int col_;
        int row_;
        int width_;
        int height_;
        uchar note_[16];
        uchar instr_[16];
        uchar vol_[16];
        uchar pitch_[16];
        uint cmd1_[16];
        ushort param1_[16];
        uint cmd2_[16];
        ushort param2_[16];
        uint cmd3_[16];
        ushort param3_[16];
    } clipboard_;

    // TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): undo/redo history.  A PhraseEdit
    // snapshots all 16 steps of the edited phrase plus the editor cursor.
  public:
    static const int kPhraseHistorySize = 16;
    // TREEFROG_GLOBAL_UNDO_V8 (Bacon 1.1.1 V16): the snapshot no longer
    // carries the cursor; undo/redo must not move the cursor back to the
    // edit site (that reads as "undo did nothing" after navigating away).
    struct PhraseEdit {
        uchar note[16];
        uchar instr[16];
        uchar vol[16];
        uchar pitch[16];
        FourCC cmd1[16];
        ushort param1[16];
        FourCC cmd2[16];
        ushort param2[16];
        FourCC cmd3[16];
        ushort param3[16];
        uchar currentPhrase;
    };
    PhraseEdit phraseUndo_[kPhraseHistorySize];
    int phraseUndoCount_;
    PhraseEdit phraseRedo_[kPhraseHistorySize];
    int phraseRedoCount_;
    void pushPhraseUndo();

  private:
    int saveCol_;
    int saveRow_;

    static short offsets_[4][4];
    static const int kColCount = 8;
    static const int kColX[kColCount];
    // Header center X per column (headers centered over their column).
    static const int kColHeaderX[kColCount];
};

#endif
