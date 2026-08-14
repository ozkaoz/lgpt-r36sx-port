
#ifndef _PHRASE_VIEW_H_
#define _PHRASE_VIEW_H_

#include "BaseClasses/UIBigHexVarField.h"
#include "BaseClasses/View.h"
#include "ViewData.h"
#include "Application/Phrase/PhraseGridEdit.h"
#include "Application/Phrase/PhraseUndo.h"

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
    // F3-5a: el portapapeles es el struct puro de PhraseGridEdit.h (mismo
    // layout golden que el struct clipboard original de la vista).
    PhraseClipboard clipboard_;

    // TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): undo/redo history.  F3-5b: los
    // snapshots, la captura push y el paso undo/redo viven en la capa pura
    // PhraseUndo.h; la vista conserva los arrays de historia y la politica
    // de push (que acciones capturan el estado pre-edit).
  public:
    // F3-5b: el tamano golden (16) es la constante de la capa pura; se
    // conserva kPhraseHistorySize como alias publico de compatibilidad.
    static const int kPhraseHistorySize = kPhraseUndoHistorySize;
    // F3-5b: el snapshot es el tipo puro de PhraseUndo.h (mismo layout que
    // el PhraseEdit original: 10 arrays de 16 pasos + currentPhrase).
    typedef PhraseUndoSnapshot PhraseEdit;
    PhraseUndoSnapshot phraseUndo_[kPhraseUndoHistorySize];
    int phraseUndoCount_;
    PhraseUndoSnapshot phraseRedo_[kPhraseUndoHistorySize];
    int phraseRedoCount_;
    void pushPhraseUndo();

  private:
    int saveCol_;
    int saveRow_;

    static const int kColCount = 8;
    static const int kColX[kColCount];
    // Header center X per column (headers centered over their column).
    static const int kColHeaderX[kColCount];
};

#endif
