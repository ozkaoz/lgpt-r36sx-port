#ifndef _SAMPLE_CHOPPER_MODAL_H_
#define _SAMPLE_CHOPPER_MODAL_H_

#include "Application/Views/BaseClasses/ModalView.h"
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
bool LGPTChopperGlobalUndoRedo(ViewData *viewData, char *status, int statusLen);

class SampleChopperModal: public ModalView {
public:
    SampleChopperModal(View &view,
                       int instrumentIndex,
                       int sampleIndex,
                       const char *sampleName,
                       int sampleSize,
                       int initialChop = -1,
                       bool openPitchEnvelope = false,
                       bool openUsbCapture = false);
    virtual ~SampleChopperModal();

    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
    virtual void OnFocus();
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    virtual bool BlocksUnderlyingDraw() { return true; } // AU10Y_RECORD_MODAL_BLOCKS_UNDERLYING

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
        MAX_CHOPS = 100
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
    bool previewActive_;
    int previewStartFrame_;
    int previewEndFrame_;
    bool operationActive_;
    bool usbCaptureRecording_;
    bool usbCaptureMenuActive_;
    int usbCaptureMenuSelection_;
    bool usbCaptureMonitor_;
    bool usbCaptureFilenameEdit_;
    int usbCaptureFilenameCursor_;
    int usbCaptureLastLevel_;
    int usbCaptureLastLevelL_;
    int usbCaptureLastLevelR_;
    int usbCaptureLastElapsed_;
    bool usbCaptureRequireRelease_;
    unsigned short usbCaptureLastMask_;
    int usbCaptureOpenGuard_;
    int usbCaptureFilePrefixIndex_;
    int usbCaptureFileTake_;
    char usbCapturePendingName_[96];
    int operationPercent_;
    char operationMessage_[64];
    char usbCaptureLastName_[96];
    char usbCaptureLastPath_[256];
    bool chopsInitialized_;
    bool trimMode_;
    bool pitchMode_;
    int pitchSemitones_;
    int pitchEditParam_;
    int pitchAttackMs_;
    int pitchSustainPercent_;
    int pitchReleaseMs_;
    int pitchScope_;
    int normalizeTarget_;
    int pitchLengthPercent_;
    int initialChopRequest_;
    bool openPitchEnvelopeRequest_;
    bool openUsbCaptureRequest_;
    int chopReleaseMs_;
    int selectedChop_;
    int boundaryCount_;
    int boundaries_[MAX_CHOP_BOUNDARIES];
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
    int snapFrameToZeroCrossing(int frame, int minFrame, int maxFrame) const;
    int snapFrameToTransientZeroCrossing(int frame, int minFrame, int maxFrame) const;
    void nudgeChopReleaseMs(int deltaMs);
    void cropToSelectedRange();
    bool destructiveCropToSelectedRange();
    bool destructiveDeleteSelectedRange();
    bool undoLastDestructiveCrop();
    bool destructivePitchSample(int semitones);
    bool destructiveNormalizeSample();
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
    void toggleUsbCapture();
    void startUsbCapture();
    void stopUsbCapture();
    void importLastUsbCapture();
    void openUsbCaptureMenu();
    void closeUsbCaptureMenu();
    void exitUsbCaptureToInstrument();
    void moveUsbCaptureMenuSelection(int delta);
    void activateUsbCaptureMenuItem();
    void drawUsbCaptureMenu(GUITextProperties &props);
    void startUsbCaptureFromMenu();
    void stopUsbCaptureFromMenu();
    void saveUsbCaptureFromMenu();
    void discardUsbCaptureFromMenu();
    void toggleUsbCaptureMonitor();
    void rebuildUsbCapturePendingName();
    void nudgeUsbCaptureTake(int delta);
    void cycleUsbCapturePrefix();
    void enterUsbCaptureFilenameEdit();
    void exitUsbCaptureFilenameEdit(bool accept);
    void editUsbCaptureFilenameChar(int delta);
    void moveUsbCaptureFilenameCursor(int delta);
    void resetUsbCaptureFilenameDefault();
    void deleteUsbCaptureFilenameChar();
    void toggleUsbCaptureFilenameCase();
    void sanitizeUsbCapturePendingName();
    void previewUsbCaptureFromMenu();
};

#endif


