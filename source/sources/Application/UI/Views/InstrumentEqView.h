#ifndef _INSTRUMENT_EQ_VIEW_H_
#define _INSTRUMENT_EQ_VIEW_H_

#include "Application/Audio/SpectrumAnalyzer.h"
#include "Application/UI/Views/BaseClasses/View.h"
#include "Application/Utils/fixed.h"

class I_Instrument;

// BACON_1.5_RENDERED_PEAK (U2.72): pure bar state for testability -- same
// math the renderer uses.  See InstrumentEqView::computeBarState().
struct AnalyzerBarState {
    float rawDb;               // 20*log10(p) before any clamp, -80 if silent
    float unclampedDisplayDb;  // rawDb + tilt? here rawDb as displayDb before clamp
    float displayDbClamped;    // after -36..0 clamp for frac
    float frac;                // 0..1 clamped
    int instantaneousHeight;   // h = frac * canvasH, 2..canvasH
    float heldHeight;          // heldH[i] after hold/release
    int renderedHeight;        // hh actually drawn
    int x;                     // bx
    int barW;                  // 1
    int centerX;               // bx + barW/2
};

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
	void invalidatePeakAfterEqChange() ;
	// BACON_1.5_RENDERED_PEAK (U2.72): public getters for host tests and
	// diagnostic readout (hardware: P BIN/HZ/X/H/dB).
	int GetRenderedPeakBin() const { return renderedPeakBin_; }
	float GetRenderedPeakHz() const { return renderedPeakHz_; }
	int GetRenderedPeakX() const { return renderedPeakX_; }
	int GetRenderedPeakH() const { return renderedPeakH_; }
	float GetRenderedPeakDb() const { return renderedPeakUnclampedDb_; }
	float GetPeakHz() const { return peakHz_; }
	bool GetPeakMarkerOn() const { return peakMarkerOn_; }
	bool GetPeakManual() const { return peakManual_; }
	int GetWindowPeakBin() const { return windowPeakBin_; }
	int GetPublishedPeakBin() const { return publishedPeakBin_; }
	float GetPublishedPeakHz() const { return publishedPeakHz_; }
	int GetPublishedPeakX() const { return publishedPeakX_; }
	// For host tests: simulate the bar-height math without framebuffer.
	// Returns the rendered peak info for the given bins snapshot.
	struct RenderedPeakInfo {
	    int bin;
	    float hz;
	    int x;
	    int h;
	    float displayDb; // unclamped
	    float clampedFrac;
	};
	RenderedPeakInfo ComputeHighestRenderedBarForTest(const fixed *bins, int n);
	// Pure helper: bar state for a single bin index (same math as PostFlushDraw).
	AnalyzerBarState ComputeBarStateForTest(int idx, float pLinear, float heldBefore) const;
	void GetDiagnosticTop3(int &n, char *out, size_t outSz) const;
	// Host-test helper: feed bins directly, compute peak, update rendered* (no draw).
	void UpdateRenderedPeakFromBins(const fixed *bins, int n);

	I_Instrument *instr_ ;
	int selected_ ;
	bool bypass_ ;
// BACON_1.5_RENDERED_PEAK (U2.72, feedback Peak FAIL): L2+R2 marks the
    // HIGHEST RENDERED BAR that is actually drawn on the canvas (hh).
    // The historical peak path is DIAGNOSTIC ONLY and
    // completely disconnected from the marker.
    bool peakMarkerOn_ ;
    float peakHz_ ;
    // BACON_1.5_ANALYZER_PEAKHIST (U2.62): true while the user is stepping
    // the marker manually (kept for compat, now unused -- L2+X moves band).
    bool peakManual_ ;
    // BACON_1.5_RENDERED_PEAK (U2.72): frame-local highest rendered bar.
    // Overwritten every PostFlushDraw -- never historical.
    int renderedPeakBin_ ;
    float renderedPeakHz_ ;
    int renderedPeakX_ ;
    int renderedPeakH_ ;
    float renderedPeakDb_ ; // unclamped displayDb for tie-break
    float renderedPeakUnclampedDb_ ;
    // BACON_1.5_PEAK_WINDOWED (U2.73): windowed peak 2.5s
    static const unsigned long kPeakUpdateIntervalMs = 2500;
    int windowPeakBin_;
    int windowPeakX_;
    int windowPeakH_;
    float windowPeakHz_;
    float windowPeakDb_;
    bool windowPeakValid_;
    int publishedPeakBin_;
    int publishedPeakX_;
    float publishedPeakHz_;
    bool publishedPeakValid_;
    unsigned long peakWindowStartMs_;
    // hold buffer per bar, >140Hz holds, <140Hz instant (moved from static)
    float heldH_[SpectrumAnalyzer::kLogBins] ;
    // diagnostic generation counters (for hardware readout)
    unsigned int renderFrame_ ;
    unsigned int lastSeenGeneration_ ;
    // diagnostic top3 (for hardware readout P BIN/HZ/X/H/dB)
    int diagTopBin_[3];
    int diagTopX_[3];
    int diagTopH_[3];
    float diagTopDb_[3];
    float diagTopHz_[3];
    int diagTopCount_;
    bool bandOn_[8] ;
    int type_[8] ;
    float freqHz_[8] ;
    float gainDb_[8] ;
    float q_[8] ;
    // BACON_1.5_EQ8_SLOPE (U2.62, feedback #14): per-band slope from the
    // instrument (1 = 12 dB/oct, 2 = 24 dB/oct; non-bell types).
    int slope_[8] ;
    char status_[96] ;
} ;

#endif