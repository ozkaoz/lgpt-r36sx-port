#ifndef _SAMPLE_MANAGER_DIALOG_H_
#define _SAMPLE_MANAGER_DIALOG_H_

#include "Application/Views/BaseClasses/ModalView.h"

class SampleManagerDialog: public ModalView {
public:
    SampleManagerDialog(View &view);
    virtual ~SampleManagerDialog();

    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
    virtual void OnFocus();
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);

protected:
    void clampSelection();
    void setStatus(const char *fmt, ...);
    int getUseCount(int sampleIndex);
    bool hasChops(int sampleIndex);
    bool canDeleteSample(int sampleIndex, char *reason, int reasonLen);
    int unassignSampleFromInstruments(int sampleIndex);
    void deleteSelectedSample();
    void forceDeleteSelectedSample();
    void purgeUnusedSamples();
    void deleteSidecarForName(const char *name);
    void notifyChopperDelete(int deletedIndex);

private:
    int selected_;
    int topIndex_;
    int forceConfirmIndex_;
    char status_[64];
};

#endif
