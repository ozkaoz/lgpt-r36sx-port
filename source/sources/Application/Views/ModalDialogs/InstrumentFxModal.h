#ifndef _INSTRUMENT_FX_MODAL_H_
#define _INSTRUMENT_FX_MODAL_H_

#include "Application/Views/BaseClasses/ModalView.h"
#include "Application/Views/BaseClasses/View.h"
#include "Application/Instruments/I_Instrument.h"

// TREEFROG_MIXER_FX_MENU_V1:
// Mixer R2+A menu: adjusts the FX of the whole instrument attached to the
// hovered channel bar. Mirrors the six beatmaking phrase FX families at
// instrument level (reverb mix, filter cutoff, EQ resonance, compression,
// pitch detune; delay is note-level only in this engine). Also exposes the
// channel solo/mute toggles.
class InstrumentFxModal : public ModalView {
public:
    InstrumentFxModal(View &parentView, int channel);
    virtual ~InstrumentFxModal();

    virtual void ProcessButtonMask(unsigned short mask, bool pressed);
    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int tick = 0);
    virtual void OnFocus();

private:
    void applyRow(int row);
    void refreshValues();
    int channel_;
    int row_;
    int values_[6];
    bool solo_;
    bool muted_;
    I_Instrument *instrument_;
    static const int kRowCount = 8;
};

#endif
