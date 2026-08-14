#ifndef _INSTRUMENT_EQ_MODAL_H_
#define _INSTRUMENT_EQ_MODAL_H_

#include "Application/UI/Views/BaseClasses/ModalView.h"
#include "UIFramework/Framework/GUITextProperties.h"

class SampleInstrument ;

/*
 * InstrumentEqModal -- graphical 8-band equalizer for the current sample
 * instrument.
 *
 * Controls (R36SX pad):
 *   LEFT / RIGHT      select band (0..7)
 *   A                toggle the selected band (curve on/off)
 *   X + LEFT/RIGHT   move the band's frequency (horizontal)
 *   X + UP/DOWN      move the band's gain (vertical, -24..+24 dB)
 *   Y + LEFT/RIGHT   change the band's Q (curve shape / width)
 *   Y + UP/DOWN      change the whole curve intensity (+/- 1 dB all bands)
 *   B                cycle the band's filter type (bell / low shelf /
 *                     high shelf / low pass / high pass / notch)
 *   SELECT           toggle the master EQ bypass
 *   R1+B             close the editor
 *
 * Layout: the modal window draws the instrument name + status text; the
 * framebuffer overlay (AppWindow flush hook, PLATFORM_TREEFROG) paints the
 * log-frequency curve canvas and the live spectrum bars.  Band edits write the
 * instrument PARAMs, which SampleInstrument::Render reads next buffer (the
 * changes are audible immediately while a voice is playing).
 */

class InstrumentEqModal : public ModalView {
  public:
    InstrumentEqModal(View &view, int instrumentIndex);
    virtual ~InstrumentEqModal();

    virtual void DrawView();
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick);
    virtual void OnFocus();
    virtual void OnSuspend();
    virtual void OnRestore();

    virtual void ProcessButtonMask(unsigned short mask, bool pressed);

  private:
    void loadFromInstrument();
    void publishToOverlay();
    void clearOverlay();
    void refreshDraw();
    void cycleBandType();
    void setStatus(const char *msg);
    float freqFromIndex(int idx) const;
    int indexFromFreq(float hz) const;

    int instrumentIndex_ ;
    class SampleInstrument *instr_ ;
    int selected_ ;
    bool bypass_ ;
    float freqHz_[8] ;
    float gainDb_[8] ;
    float q_[8] ;
    int type_[8] ;
    bool bandOn_[8] ;
    bool suspended_ ;
    char status_[80] ;
};

#endif