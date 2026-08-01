// TREEFROG_MIXER_FX_MENU_V1
#include "InstrumentFxModal.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Player/Player.h"
#include "Application/Model/Project.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Views/UIController.h"
#include "Application/Utils/char.h"
#include <stdio.h>
#include <string.h>

static const FourCC FX_NONE = MAKE_FOURCC('-', '-', '-', '-');

static FourCC kFxVariables[6] = {
    SIP_FBMIX,      // RVB reverb (feedback mix)
    FX_NONE,        // DLY delay: note-level only in this engine
    SIP_FILTCUTOFF, // FLT filter cutoff
    SIP_FILTRESO,   // EQ resonance (tone/Q)
    SIP_CRUSH,      // CMP compression/crush
    SIP_FINETUNE    // PIT pitch detune
};

InstrumentFxModal::InstrumentFxModal(View &parentView, int channel)
    : ModalView(parentView),
      channel_(channel),
      row_(0),
      solo_(false),
      muted_(false),
      instrument_(0) {
    Player *player = Player::GetInstance();
    instrument_ = player->GetChannelInstrument(channel_);
    if (!instrument_) {
        InstrumentBank *bank = parentView.viewData_->project_->GetInstrumentBank();
        instrument_ = bank->GetInstrument(parentView.viewData_->currentInstrument_);
    }
    muted_ = player->IsChannelMuted(channel_);
    refreshValues();
}

InstrumentFxModal::~InstrumentFxModal() {}

void InstrumentFxModal::refreshValues() {
    for (int i = 0; i < 6; i++) {
        values_[i] = 0;
        if (kFxVariables[i] != FX_NONE && instrument_) {
            Variable *v = instrument_->FindVariable(kFxVariables[i]);
            if (v) {
                values_[i] = v->GetInt();
            }
        }
    }
}

void InstrumentFxModal::applyRow(int row) {
    if (row < 0 || row >= 6) return;
    if (kFxVariables[row] == FX_NONE || !instrument_) return;
    Variable *v = instrument_->FindVariable(kFxVariables[row]);
    if (v) {
        v->SetInt(values_[row]);
    }
}

void InstrumentFxModal::ProcessButtonMask(unsigned short mask, bool pressed) {
    if (!pressed) return;

    if (mask & EPBM_DOWN) {
        row_ = (row_ + 1) % kRowCount;
        isDirty_ = true;
        return;
    }
    if (mask & EPBM_UP) {
        row_ = (row_ + kRowCount - 1) % kRowCount;
        isDirty_ = true;
        return;
    }
    if (mask & EPBM_A) {
        if (mask & EPBM_RIGHT) {
            if (row_ < 6) {
                values_[row_] = (values_[row_] + 0x10) & 0xFF;
                applyRow(row_);
                isDirty_ = true;
            }
            return;
        }
        if (mask & EPBM_LEFT) {
            if (row_ < 6) {
                values_[row_] = (values_[row_] - 0x10) & 0xFF;
                applyRow(row_);
                isDirty_ = true;
            }
            return;
        }
        EndModal(1);
        return;
    }
    if (mask & EPBM_RIGHT) {
        if (row_ < 6) {
            values_[row_] = (values_[row_] + 1) & 0xFF;
            applyRow(row_);
        }
        isDirty_ = true;
        return;
    }
    if (mask & EPBM_LEFT) {
        if (row_ < 6) {
            values_[row_] = (values_[row_] - 1) & 0xFF;
            applyRow(row_);
        }
        isDirty_ = true;
        return;
    }
    if (mask & EPBM_B) {
        EndModal(0);
        return;
    }
    if (mask & (EPBM_X | EPBM_Y)) {
        if (row_ == 6) {
            solo_ = !solo_;
            UIController::GetInstance()->SwitchSoloMode(channel_, channel_, solo_);
            isDirty_ = true;
        } else if (row_ == 7) {
            muted_ = !muted_;
            UIController::GetInstance()->ToggleMute(channel_, channel_);
            muted_ = Player::GetInstance()->IsChannelMuted(channel_);
            isDirty_ = true;
        }
        return;
    }
}

void InstrumentFxModal::DrawView() {
    SetWindow(26, 11);

    GUITextProperties props;
    char line[32];

    SetColor(CD_NORMAL);
    props.invert_ = false;
    sprintf(line, "INSTRUMENT FX  ch:%1X", channel_);
    DrawString(1, 0, line, props);

    static const char *names[6] = {
        "RVB reverb", "DLY delay", "FLT cutoff",
        "EQ  reso  ", "CMP crush ", "PIT detune"};
    for (int i = 0; i < 6; i++) {
        bool selected = (row_ == i);
        bool available = (kFxVariables[i] != FX_NONE) && instrument_;
        SetColor(selected ? CD_HILITE2 : CD_NORMAL);
        props.invert_ = selected;
        if (i == 1) {
            sprintf(line, "%-10s phrase only", names[i]);
        } else if (available) {
            sprintf(line, "%-10s %2.2X", names[i], values_[i]);
        } else {
            sprintf(line, "%-10s n/a", names[i]);
        }
        DrawString(1, 1 + i, line, props);
    }
    props.invert_ = false;

    SetColor(row_ == 6 ? CD_HILITE2 : CD_NORMAL);
    props.invert_ = (row_ == 6);
    sprintf(line, "SOLO        %s", solo_ ? "ON " : "OFF");
    DrawString(1, 7, line, props);
    props.invert_ = false;

    SetColor(row_ == 7 ? CD_HILITE2 : CD_NORMAL);
    props.invert_ = (row_ == 7);
    sprintf(line, "MUTE        %s", muted_ ? "ON " : "OFF");
    DrawString(1, 8, line, props);
    props.invert_ = false;

    SetColor(CD_NORMAL);
    DrawString(1, 10, "L/R val  A+L/R x16  X/Y toggle", props);
    DrawString(1, 11, "A ok  B close", props);
}

void InstrumentFxModal::OnPlayerUpdate(PlayerEventType, unsigned int) {
    isDirty_ = true;
}

void InstrumentFxModal::OnFocus() { isDirty_ = true; }
