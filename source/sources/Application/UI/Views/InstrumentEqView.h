#ifndef _INSTRUMENT_EQ_VIEW_H_
#define _INSTRUMENT_EQ_VIEW_H_

#include "Application/UI/Views/BaseClasses/View.h"
#include "Application/Utils/fixed.h"

class I_Instrument;

/*
 * BACON_1.5_EQ8_VIEW (bacon-1.5, items 3/5/6): fullscreen graphic 8-band
 * instrument EQ editor (320x240 pixel canvas + char overlay).
 *
 * Replaces the old InstrumentEqModal + TreeFrogInstrumentEqOverlayDraw hook:
 *   - Fullscreen View switched via ViewEvent(VET_SWITCH_VIEW,
 *     VT_INSTRUMENT_EQ), like every other screen.
 *   - The composite response curve is drawn from the SAME coefficients the
 *     real DSP applies (I_Instrument::GetInstrumentEq() ->
 *     GetBandCoeffs/GetSampleRate).  No duplicated RBJ math in the UI, so
 *     the picture always matches the sound (item 6).
 *   - Edits write the instrument EQ variables directly; syncInstrumentEq()
 *     in the audio path picks them up on the next buffer (fingerprint cache
 *     + coefficient smoothing), so changes are LIVE while the phrase plays
 *     (item 3).
 *   - START toggles playback with the exact Player::OnStartButton() contract
 *     used by InstrumentView; R+START stops (item 3).
 *   - 8 draggable nodes, 7 filter types, log frequency scale 20 Hz-20 kHz,
 *     live spectrum from the common targeted analyzer tap drawn as thin
 *     3-px bars (Pro-Q style, 24 log bins) BEHIND the curve canvas, on the
 *     true 0..1 audio scale (item 7, feedback (F) + #6).
 *   - The canvas fills the whole screen under the char header (feedback
 *     #6); the frequency axis labels (40..20k) sit in the last 8 px.
 *   - SELECT+R1 opens its own EQ8 help section (feedback (E)); the canvas
 *     is skipped while a modal is open so the help stays on top (#6).
 *   - The instrument id is shown WITHOUT the old 0x3F mask (item 6).
 */

class InstrumentEqView: public View {
public:
	InstrumentEqView(GUIWindow &w, ViewData *data);
	virtual ~InstrumentEqView() ;

	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType, unsigned int currentTick) ;
	virtual void OnFocus() ;
	virtual void OnFrameUpdate(unsigned long frameClock) ;
	// Disarms the analyzer when leaving the view.
	virtual void LooseFocus() ;
	virtual void PostFlushDraw() ;

protected:
	virtual void ProcessButtonMask(unsigned short mask, bool pressed) ;

private:
	void loadFromInstrument() ;
	void writeToInstrument() ;
	void setStatus(const char *msg) ;
	void cycleBandType() ;
	void refreshDraw() ;
	// BACON_1.5_EQ8_PIXEL_HEADER (U2.53, feedback #7): builds the three
	// header strings (title / selected-band line / status) from the view
	// state.  DrawView() sends them to the char screen and PostFlushDraw()
	// re-renders the same strings in pixels on top, so both layers always
	// agree.
	void buildHeader(char *title, size_t titleSz, char *line, size_t lineSz,
	                 char *status, size_t statusSz) const ;
	float freqFromIndex(int idx) const ;
	int indexFromFreq(float hz) const ;

	I_Instrument *instr_ ;
	int selected_ ;
	bool bypass_ ;
	bool bandOn_[8] ;
	int type_[8] ;
	float freqHz_[8] ;
	float gainDb_[8] ;
	float q_[8] ;
	char status_[96] ;
} ;

#endif