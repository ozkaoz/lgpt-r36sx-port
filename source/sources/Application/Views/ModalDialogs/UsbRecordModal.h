#ifndef _USB_RECORD_MODAL_H_
#define _USB_RECORD_MODAL_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.h"

class UsbRecordModal : public ModalView {
public:
    UsbRecordModal(View &view, int instrumentIndex);
    virtual ~UsbRecordModal();

    virtual void DrawView();
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    virtual void OnPlayerUpdate(PlayerEventType type, unsigned int currentTick);
    virtual void OnFrameUpdate(unsigned long frameClock);
    virtual void OnFocus();

private:
    enum Item {
        ITEM_MONITOR = 0,
        ITEM_DURATION,
        ITEM_FILE,
        ITEM_RECORD,
        ITEM_PREVIEW,
        ITEM_SAVE,
        ITEM_DISCARD,
        ITEM_EXIT,
        ITEM_COUNT
    };

    enum SessionState {
        SESSION_BOOT = 0,
        SESSION_IDLE,
        SESSION_ARMING,
        SESSION_RECORDING,
        SESSION_FINALIZING,
        SESSION_READY,
        SESSION_PREVIEWING,
        SESSION_ERROR,
        SESSION_CLOSING
    };

    void setStatus(const char *text);
    void setSessionState(SessionState state, const char *status);
    const char *sessionStateName() const;

    void updateInputArming();
    bool physicalInputNeutral() const;
    void requestClose(bool saved);
    void completeCloseWhenNeutral();

    void updateCaptureSnapshot(bool force);
    void applyCaptureSnapshot();
    bool snapshotPathMatchesCurrentTake() const;

    void moveSelection(int delta);
    void cycleDuration(int delta);
    void toggleMonitor();

    void startRecording();
    void stopRecording();
    void previewRecording();
    void stopPreview(const char *status);
    void saveRecording();
    void discardRecording();
    void exitModal();
    bool executeSelectedAction();

    void ensureRecordDirectory();
    void makeTemporaryCapturePath();
    bool promoteCaptureToFinalPath(
        const char *sourcePath,
        const char *destinationPath,
        char *reason,
        int reasonLength);
    bool currentTakeIsTemporary() const;
    void makeNextCapturePath();
    void updatePlannedPathFromStem();
    bool recordNameExistsExact(const char *name) const;
    int fileEditorViewStart(int width) const;
    void beginFileEdit();
    void cancelFileEdit();
    void confirmFileEdit();
    void moveFileCursor(int delta);
    void cycleFileCharacter(int delta);
    void toggleFileCharacterCase();
    void deleteFileCharacter();
    void resetFileEditorInputGuard();
    void rearmMenuInputAfterFileEditor();
    void processFileEditorPhysicalInput();

    bool currentInstrumentIsSample() const;
    bool validateCaptureWav(
        const char *path,
        long *dataBytes,
        int *frames,
        char *reason,
        int reasonLength) const;
    bool captureFileStable(const char *path) const;
    bool preparePreview(const char *path, int *frames, char *reason, int reasonLength);

    static int countBits(unsigned short value);

    int instrumentIndex_;
    int selected_;
    int durationIndex_;
    SessionState sessionState_;
    unsigned long long stateSinceMs_;

    bool monitorRequested_;
    bool monitorBeforeRecord_;
    bool previewing_;
    bool editingFile_;
    bool fileLowercaseMode_;
    bool fileEditorInputArmed_;
    bool inputArmed_;
    bool closePending_;
    bool closeSaved_;

    unsigned short activeInputMask_;
    unsigned int fileEditorPhysicalMask_;
    unsigned int fileEditorNeutralFrames_;
    unsigned int neutralInputFrames_;
    unsigned int frameCounter_;
    unsigned int capturePollDivider_;
    unsigned int drawRefreshDivider_;
    unsigned int takeCounter_;

    int fileCursor_;
    char fileStem_[48];
    char fileEditBackup_[48];
    char plannedPath_[256];
    char plannedName_[96];
    char currentTakePath_[256];
    char validatedPath_[256];
    long validatedDataBytes_;
    int validatedFrames_;

    TreeFrogUsbCaptureSnapshot capture_;
    char usbState_[48];
    char status_[96];
};

#endif
