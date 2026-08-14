#ifndef _AUDIO_DRIVER_MODAL_H_
#define _AUDIO_DRIVER_MODAL_H_

#include "Application/UI/Views/BaseClasses/ModalView.h"

class AudioDriverModal : public ModalView {
public:
    AudioDriverModal(View &view);
    virtual ~AudioDriverModal();

    virtual void DrawView();
    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    virtual void OnPlayerUpdate(PlayerEventType type, unsigned int currentTick);
    virtual void OnFocus();

private:
    int selected_;
    bool waitForRelease_;
};

void AudioDriverModalApplyCallback(View &view, ModalView &dialog);

#endif
