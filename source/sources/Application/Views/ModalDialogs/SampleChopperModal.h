#ifndef _SAMPLE_CHOPPER_MODAL_H_
#define _SAMPLE_CHOPPER_MODAL_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "Application/Views/ModalDialogs/ChopModel.h"
#include "Application/Views/ModalDialogs/SampleEditHistory.h"
#include "Application/Views/ModalDialogs/PitchEnvelopeTool.h"
#include "Application/Views/ModalDialogs/PreviewService.h"
#include "Application/Views/ModalDialogs/ChopperView.h"
#include "Application/Views/ModalDialogs/ChopperController.h"
#include "UIFramework/Framework/GUITextProperties.h"
#include <string>

class ViewData;

bool LGPTChopperAssignSavedChopToPhraseRow(ViewData *viewData,
                                           int phraseIndex,
                                           int row,
                                           int sourceInstrumentIndex,
                                           int requestedChop,
                                           int delta,
                                           bool advanceSessionCursor,
                                           char *status,
                                           int statusLen);
int LGPTChopperGetSavedChopCountForInstrument(ViewData *viewData,
                                               int sourceInstrumentIndex);
bool LGPTChopperIsChopNoteForInstrument(ViewData *viewData,
                                          int sourceInstrumentIndex,
                                          int noteValue,
                                          int *displayChopNumber);
bool LGPTChopperGetChopRangeForSampleIndex(int sampleIndex,
                                           int chopIndex,
                                           int *startFrame,
                                           int *endFrameExclusive);

class SampleChopperModal: public ModalView {
public:
    SampleChopperModal(View &view,
                       int instrumentIndex,
                       int sampleIndex,
                       const char *sampleName,
                       int sampleSize);
    virtual ~SampleChopperModal();

    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
    virtual void OnFocus();
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    // TREEFROG_CHOPPER_HELP_V1 (Bacon 1.1.1 V13): the chopper reports its
    // own view type so SELECT+R1 opens the CHOPPER help section instead of
    // the Instrument section of the view underneath.
    virtual ViewType GetViewType() const {
        return pitchMode_ ? VT_CHOPPITCH : VT_CHOPPER;
    }
    // TREEFROG_CHOPPER_HELP_V1: while Help (or another modal) is pushed on
    // top, the waveform pixel overlay must not draw over it; it is cleared on
    // suspend and re-published when the chopper regains focus.  TREEFROG_CHOPPER_HELP_V2
    // (Bacon 1.1.1 V16): DrawView() runs every frame while suspended (the
    // base Redraw always repaints a modal-less view) and publishOverlayState()
    // would re-enable g_chopperOverlayActive right back over the Help box;
    // the suspended_ flag keeps the overlay off until OnRestore().
    virtual void OnSuspend() { suspended_ = true; clearOverlayState(); }
    virtual void OnRestore() { suspended_ = false; publishOverlayState(); isDirty_ = true; }

private:
    enum {
        SCREEN_W = 40,
        SCREEN_H = 30,
        WAVE_X = 16,
        WAVE_Y = 72,
        WAVE_W = 288,
        WAVE_H = 88,
        MAX_COLUMNS = 288,
        MIN_ZOOM_PERCENT = 5,
        MAX_ZOOM_PERCENT = 100,
        MAX_CHOP_BOUNDARIES = 101,
        MAX_CHOPS = 100,
        MAX_LOGICAL_HISTORY = 24
    };

    struct LogicalHistoryState {
        int sampleIndex;
        int sourceSize;
        int selectedChop;
        int boundaryCount;
        int cursorFrame;
        int viewStartFrame;
        int zoomPercent;
        bool trimMode;
        bool pitchMode;
        int pitchSemitones;
        int pitchEditParam;
        int pitchAttackMs;
        int pitchSustainPercent;
        int pitchReleaseMs;
        int pitchScope;
        char samplePath[256];
        char action[40];
        int boundaries[MAX_CHOP_BOUNDARIES];

        LogicalHistoryState()
            : sampleIndex(-1),
              sourceSize(0),
              selectedChop(0),
              boundaryCount(0),
              cursorFrame(0),
              viewStartFrame(0),
              zoomPercent(MIN_ZOOM_PERCENT),
              trimMode(false),
              pitchMode(false),
              pitchSemitones(0),
              pitchEditParam(0),
              pitchAttackMs(0),
              pitchSustainPercent(100),
              pitchReleaseMs(0),
              pitchScope(0) {
            samplePath[0] = 0;
            action[0] = 0;
            for (int i = 0; i < MAX_CHOP_BOUNDARIES; ++i)
                boundaries[i] = 0;
        }
    };

    int instrumentIndex_;
    int sampleIndex_;
    int sampleSize_;
    int sourceSize_;
    int sourceChannels_;
    int sourceRate_;
    int cursorFrame_;
    int viewStartFrame_;
    int zoomPercent_;
    bool hasWaveform_;
    bool playbackTriggered_;
    // F3-3a (docs/F3_ARCHITECTURE_ES.md): rango de playback (active /
    // start / end) extraido a PreviewService (capa pura); la vista
    // conserva audio, mensajes y overlay via los adapters delegados
    // (setPreviewPlaybackRange / clearPreviewPlaybackRange /
    // stopSamplePreview / play* siguen aqui).
    PreviewService preview_;
    bool operationActive_;
    int operationPercent_;
    char operationMessage_[64];
    char operationComboLabel_[16];
    bool chopsInitialized_;
    bool trimMode_;
    bool pitchMode_;
    bool suspended_;
    // F3-2 (docs/F3_ARCHITECTURE_ES.md): parametros pitch/env y DSP puro
    // extraidos a PitchEnvelopeTool; el historial logico undo/redo
    // (LogicalHistoryState) a SampleEditHistory.  La vista conserva
    // mensajes, preview, preview del pitch, y el orden de las escrituras
    // golden via los metodos delegados (capture/restore/push/undo/redo
    // siguen aqui como adapter de la capa pura).
    PitchEnvelopeTool pitchEnvTool_;
    SampleEditHistory<LogicalHistoryState> editHistory_;
    // F3-1 (docs/F3_ARCHITECTURE_ES.md): estado de cortes (boundaries,
    // count, seleccion) extraido a ChopModel con los algoritmos golden;
    // la vista conserva mensajes, preview, historia y dibujo.
    ChopModel chopModel_;
    std::string sampleName_;
    std::string samplePath_;
    char statusMessage_[64];
    int minColumn_[MAX_COLUMNS];
    int maxColumn_[MAX_COLUMNS];

    bool hasAssignedSample() const;
    int clampInt(int value, int minValue, int maxValue) const;
    int getZoomFactor() const;
    int getViewFrameCount() const;
    int getCursorFrame() const;
    int frameToPixel(int frame) const;
    int pixelToFrame(int px) const;
    void clampViewStart();
    void centerViewOnCursor();
    void ensureCursorVisible();
    void nudgeCursorPixels(int deltaPx);
    void nudgeZoomPercent(int deltaPercent);
    void resetChopState();
    bool restoreChopStateForCurrentSample();
    void saveChopStateForCurrentSample();
    void loadSampleByIndex(int index, const char *reason);
    void selectSample(int delta);
    void prepareWaveformPreview();
    void initializeChopsIfNeeded();
    void addChopAtCursor();
    void deleteSelectedChop();
    void sortBoundaries();
    int findBoundaryIndex(int frame) const;
    void selectChop(int delta);
    bool hasUserChops() const;
    bool hasActiveSliceRange() const;
    int selectedChopStartFrame() const;
    int selectedChopEndFrame() const;
    int getFrameStepForEdit() const;
    void toggleTrimMode();
    void nudgeSelectedStart(int deltaFrames);
    void nudgeSelectedEnd(int deltaFrames);
    void cropToSelectedRange();
    void captureLogicalState(LogicalHistoryState &state,
                             const char *action) const;
    void restoreLogicalState(const LogicalHistoryState &state);
    void pushLogicalUndo(const char *action);
    bool undoLogicalEdit();
    bool redoLogicalEdit();
    void clearLogicalRedo();
    void clearLogicalHistory();
    bool undoLastChopperEdit();
    bool redoLastChopperEdit();
    bool destructiveCropToSelectedRange();
    bool destructiveDeleteSelectedRange();
    bool restoreLastDestructiveEdit(bool redo);
    bool destructivePitchSample(int semitones);
    bool normalizeSample();
    void previewPitchSetting();
    bool writePreviewPitchWav(short *samples, int frames, int channels, int rate, std::string &logicalPath);
    bool buildPitchedBuffer(int semitones, short **outSamples, int *outFrames, int *outChannels, int *outRate);
    bool buildPitchEnvelopeBufferFromRange(int startFrame, int endFrame, int semitones, short **outSamples, int *outFrames, int *outChannels, int *outRate);
    bool preparePitchEnvelopePreviewBuffer(short **outSamples, int *outFrames, int *outChannels, int *outRate);
    bool hasPitchEnvelopeChange() const;
    void resetPitchEnvelopeSettings();
    void applyEnvelopeToBuffer(short *samples, int frames, int channels, int rate, int attackMs, int sustainPercent, int releaseMs);
    void drawPitchScreen(GUITextProperties &props);
    void selectPitchEditParam(int delta);
    void nudgePitchEnvelopeValue(int delta);
    void nudgePitchSemitones(int delta);
    void selectPitchTargetSample(int delta);
    void refreshCurrentInstrumentAfterSampleEdit(int newSize);
    void togglePitchMode();
    void previewTrimStart();
    void previewTrimEnd();
    void showOperationProgress(const char *message, int percent);
    void clearOperationProgress();
    void drawOperationOverlay(GUITextProperties &props);
    void publishOverlayState();
    void clearOverlayState();
    void playFullSample();
    void playFromFrame(int frame, const char *label);
    void playFrameRange(int startFrame, int endFrame, const char *label);
    void playSelectedChop();
    void setPreviewPlaybackRange(int startFrame, int endFrame);
    void clearPreviewPlaybackRange();
    void assignSelectedChopToPhrase();
    void exportChopsToPhrase();
    bool ensureCurrentPhraseSlot();
    bool configureChopInstrument(int instrumentIndex, int startFrame, int endFrame);
    void stopSamplePreview();
    void setStatus(const char *message);
    void drawStringAbs(int x, int y, const char *txt, GUITextProperties &props);
    void clearTextScreen();
    void drawTopBar(GUITextProperties &props);
    void drawFrame(GUITextProperties &props);
    void drawSampleInfo(GUITextProperties &props);
    void drawEmptyWaveformText(GUITextProperties &props);
    void drawControls(GUITextProperties &props);
    // F3-3b (docs/F3_ARCHITECTURE_ES.md): el dibujo textual (celdas,
    // invert, color) vive en ChopperView (capa pura); aqui solo el drenado
    // a la pantalla (SetColor/DrawString reales).
    void drainChopperGrid(const ChopperGrid &grid, GUITextProperties &props);
    // TREEFROG_U2_39_CHOPPER_SPLIT_ZERO (Bacon 1.1.1): split whole sample in
    // N equal parts (L1+B cycles 4/8/16/32) and zero-cross snapping for the
    // selected chop start/end (L1+A / L1+B in trim mode).
    void splitSampleIntoEqualParts(int parts);
    void snapSelectedBoundaryToZeroCross(bool isStart);
    void cycleSplitParts();
    void clearAllChops();
    void setOperationCombo(const char *combo);
    int splitParts_;
    // F3-3c (docs/F3_ARCHITECTURE_ES.md): logica de edicion (12 flujos
    // golden) extraida a ChopperController (capa pura header-only).  El
    // adapter ChopperHostAdapter traduce los efectos de vista; la vista
    // conserva audio, destructivos, undo/redo fisico y logico, persistencia
    // por sample, preview y overlay (los metodos delegados siguen aqui con
    // la API publica intacta).
    struct ChopperHostAdapter : public ChopperController::Host {
        explicit ChopperHostAdapter(SampleChopperModal &owner);
        void SetStatus(const char *message);
        void SetOperationCombo(const char *combo);
        void PushLogicalUndo(const char *action);
        void SaveChopState();
        void EnsureCursorVisible();
        void PrepareWaveformPreview();
        void PublishOverlayState();
        void MarkDirty();
        bool SampleLoaded();
        int QuerySnapBuffer(short **samples, int *channels);
        bool LiveStreamingPosition(int *frame);
        SampleChopperModal &owner_;
    };
    ChopperHostAdapter chopperHost_;
    ChopperController chopperController_;
};

#endif


