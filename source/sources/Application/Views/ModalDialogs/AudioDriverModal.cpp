#include "AudioDriverModal.h"
#include "Application/Persistency/PersistencyService.h"
#include "Application/Mixer/MixerService.h"
#include <unistd.h>

#include "Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.h"
#include <stdio.h>

AudioDriverModal::AudioDriverModal(View &view)
    : ModalView(view),
      selected_(TreeFrogUac2Bridge_GetDriverMode()),
      waitForRelease_(true) {
    const int count = TreeFrogUac2Bridge_GetDriverModeCount();
    if (selected_ < 0 || selected_ >= count) selected_ = 0;
}

AudioDriverModal::~AudioDriverModal() {}

void AudioDriverModal::DrawView() {
    SetWindow(36, 19);
    GUITextProperties props;
    char line[64];

    SetColor(CD_HILITE1);
    props.invert_ = true;
    DrawString(0, 0, "           AUDIO DRIVER             ", props);
    props.invert_ = false;

    SetColor(CD_NORMAL);
    snprintf(line, sizeof(line), "Dev: %-27.27s",
             TreeFrogUac2Bridge_GetUsbDeviceText());
    DrawString(1, 2, line, props);
    snprintf(line, sizeof(line), "USB: %-28.28s",
             TreeFrogUac2Bridge_GetUsbStateText());
    DrawString(1, 3, line, props);

    const int count = TreeFrogUac2Bridge_GetDriverModeCount();
    int samplerIndex = -1;
    for (int i = 0; i < count; ++i) {
        props.invert_ = (i == selected_);
        SetColor(i == selected_ ? CD_HILITE2 : CD_NORMAL);
        const char *name = TreeFrogUac2Bridge_GetDriverModeNameByIndex(i);
        if (strcmp(name, "Sampler") == 0) {
            samplerIndex = i;
            /* U2.52.5 SAMPLER_OUT_ONLY: the direction toggle is removed. */
            snprintf(line, sizeof(line), "%c %-24.24s%s",
                     i == selected_ ? '>' : ' ', name, "[OUT]");
        } else {
            snprintf(line, sizeof(line), "%c %-30.30s",
                     i == selected_ ? '>' : ' ', name);
        }
        DrawString(1, 5 + i * 2, line, props);
        props.invert_ = false;
        SetColor(CD_NORMAL);
        const char *desc =
            TreeFrogUac2Bridge_GetDriverModeDescriptionByIndex(i);
        if (i == samplerIndex) {
            desc = "OUT: console->sampler (play)";
        }
        DrawString(3, 6 + i * 2, desc, props);
    }

    SetColor(CD_NORMAL);
    if (waitForRelease_) {
        DrawString(1, 16, "Release SELECT/R2 or opening keys", props);
    } else {
        DrawString(1, 16, "A apply+restart   B cancel", props);
    }
    DrawString(1, 17, "Restart is automatic; stay in LGPT", props);
    DrawString(1, 18, "Sampler: Instrument R1 + RIGHT", props);
}

void AudioDriverModal::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (waitForRelease_) {
        if (!pressed && mask == 0) {
            waitForRelease_ = false;
            isDirty_ = true;
        }
        return;
    }

    if (!pressed) return;

    const int count = TreeFrogUac2Bridge_GetDriverModeCount();
    if (mask & EPBM_UP) {
        selected_ = (selected_ + count - 1) % count;
        isDirty_ = true;
    } else if (mask & EPBM_DOWN) {
        selected_ = (selected_ + 1) % count;
        isDirty_ = true;
    } else if (mask & EPBM_A) {
        EndModal(selected_);
    } else if (mask & EPBM_B) {
        EndModal(-1);
    }
}

void AudioDriverModal::OnPlayerUpdate(PlayerEventType, unsigned int) {}
void AudioDriverModal::OnFocus() { isDirty_ = true; }

void AudioDriverModalApplyCallback(View &view, ModalView &dialog) {
    int mode = dialog.GetReturnCode();
    if (mode < 0) return;

    const int activeMode = TreeFrogUac2Bridge_GetDriverMode();
    if (mode != activeMode) {
        MixerService::GetInstance()->SetRenderMode(0);
        PersistencyService::GetInstance()->Save();
        sync();
        view.SetNotification("Project saved; changing Audio Driver");
    }
    const char *name = TreeFrogUac2Bridge_SetDriverMode(mode);
    char message[96];
    snprintf(message, sizeof(message), "Saved; Audio Driver: %s", name ? name : "");
    view.SetNotification(message);
}
