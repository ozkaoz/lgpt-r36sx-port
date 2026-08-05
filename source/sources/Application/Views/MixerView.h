#ifndef _MIXER_VIEW_H_
#define _MIXER_VIEW_H_

#include "BaseClasses/View.h"
#include "ViewData.h"
#include "Application/Model/Song.h"

// TREEFROG_FX_PAGES_V3 (PLAN_FX_REDESIGN_ES.md, Fase 9):
// MixerView page system for the master FX engine.  SELECT cycles
// MIX -> DELAY -> REVERB -> EQ -> COMP -> MIX.  MIX keeps the per-channel
// bars plus an FX RETURNS readout (master DLY/RVB return levels).  The
// per-track DLY/RVB send readouts were removed in Fase 9: sends are
// per-instrument now (edited in InstrumentView), and the per-track sends
// survive only as the Fase 7 inheritance/compatibility layer.  R2 alone
// cycles the MIX-page edit target VOL -> DLY RET -> RVB RET.  DELAY/REVERB/
// EQ/COMP are parameter pages: UP/DOWN moves the row cursor, LEFT/RIGHT
// edits the value, A+UP/DOWN coarse.  EQ exposes the 3-band parametric EQ,
// COMP the compressor (Fase 6 splits the old single MASTER page in two so
// each page fits the 8-line mixer screen without scrolling).
enum FxPage {
    FX_PAGE_MIX = 0,
    FX_PAGE_DELAY,
    FX_PAGE_REVERB,
    FX_PAGE_EQ,
    FX_PAGE_COMP,
    FX_PAGE_COUNT
};

// Parameter rows available on the DELAY/REVERB/EQ/COMP pages (see
// MixerView.cpp kFxParams_ and fxGet/fxSet).  Also used to size the cursor.
// Fase 6: the global SEND/RET rows were removed from the DELAY/REVERB pages
// (sends are now per-track / per-instrument; returns are fixed 0.5 helpers).
enum FxParamId {
    // DELAY
    FX_P_DLY_TIME = 0,
    FX_P_DLY_FBK,
    FX_P_DLY_MIX,
    FX_P_DLY_WID,
    FX_P_DLY_PP,
    FX_P_DLY_SAT,
    FX_P_DLY_BYP,
    // REVERB
    // RC2 (point 3.1): the legacy RVB MIX row was removed from the UI.  The
    // reverb is a true wet-only send/return processor now: RVB MIX no longer
    // acts as a dry/wet control (the engine keeps reading/persisting it but it
    // is inert), and the audible level is set by the instrument send + the
    // Mixer REVERB RETURN.  The page shows PRE/DEC/SIZ/DMP/WID/MODE/BYP.
    FX_P_RVB_PRE,
    FX_P_RVB_DEC,
    FX_P_RVB_SIZ,
    FX_P_RVB_DMP,
    FX_P_RVB_WID,
    FX_P_RVB_MODE,
    FX_P_RVB_BYP,
    // EQ (3 bands, dedicated banded menu - Fase 12: bypass + enable/freq/
    // gain/Q each; EN is first so UP/DOWN walks the band in the same visual
    // order the EQ menu draws)
    FX_P_EQ_BYP,
    FX_P_EQ_LOW_EN,
    FX_P_EQ_LOW_FRQ,
    FX_P_EQ_LOW_GAI,
    FX_P_EQ_LOW_Q,
    FX_P_EQ_MID_EN,
    FX_P_EQ_MID_FRQ,
    FX_P_EQ_MID_GAI,
    FX_P_EQ_MID_Q,
    FX_P_EQ_HI_EN,
    FX_P_EQ_HI_FRQ,
    FX_P_EQ_HI_GAI,
    FX_P_EQ_HI_Q,
    // COMP
    // COMP (dedicated menu - Fase 13: BYP first so it is never off-screen;
    // THR/RAT/KNE/ATK/REL/MKU/LNK/SC follow in the same order the COMP menu
    // draws them, with the GR meter below)
    FX_P_CMP_BYP,
    FX_P_CMP_THR,
    FX_P_CMP_RAT,
    FX_P_CMP_KNE,
    FX_P_CMP_ATK,
    FX_P_CMP_REL,
    FX_P_CMP_MKU,
    FX_P_CMP_LINK,
    FX_P_CMP_SC,
    FX_PARAM_COUNT
};

class MixerView: public View {
public:
	MixerView(GUIWindow &w,ViewData *viewData) ;
	~MixerView() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int tick=0) ;
	virtual void OnFrameUpdate(unsigned long frameClock) ;
	virtual void OnFocus() ;
	// TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): L1+X / R1+X undo/redo of the
	// MIX-page and FX-page edits, A+B resets the hovered option to default
	// (pan to C, channel volume 127, master volume 100, FX row to vdef).
	virtual bool GlobalUndo() ;
	virtual bool GlobalRedo() ;
	virtual bool GlobalResetOption() ;
	// TREEFROG_MIXER_ACTION_MENU_V1 (Bacon 1.1.1 V13): L1+A menu jumps
	// straight to a master FX page (DELAY/REVERB/EQ/COMP).
	void JumpToFxPage(FxPage page) ;
	// TREEFROG_MIXER_HALF_CELL_BARS_V1 (Bacon 1.1.1): repaints the L/R
	// half-cell meter bars on top of the character screen every Flush.
	virtual void PostFlushDraw() ;
protected:
	void processNormalButtonMask(unsigned int mask) ;
    void processSelectionButtonMask(unsigned int mask) ;
	void onStart() ;
	void onStop() ;
	void updateCursor(int dx,int dy)  ;
	void updateVolume(int delta) ;
	void adjustMasterVolume(int delta) ;
	void toggleMute() ;
	void switchSoloMode() ;
	void drawVolumeBar(int channel,int x,int y,int height) ;
	void drawMasterBar(int x,int y,int height) ;
	// TREEFROG_MIXER_HALF_CELL_BARS_V1 (Bacon 1.1.1):
	// The two bars of a meter now share ONE cell column (8 px): the left bar
	// paints px 0..2, a 2-px dark seam px 3..4, the right bar px 5..7, so the
	// L/R split is visible at every pan.  drawMeterBar(side) records the
	// side's fill into the meter record (side 0 keeps painting the cell track
	// so the label column stays coherent; side 1 only records); the real
	// pixels are drawn by PostFlushDraw() after the character screen is
	// flushed.  drawVolumeBar/drawMasterBar pass both sides at the SAME x.
	struct MeterRecord {
		bool valid ;
		int xCell ;
		int yCell ;
		int height ;
		// TREEFROG_MIXER_COMPACT_BARS_V1 (Bacon 1.1.1 V13): normalized
		// level (0..1) of each side after volume scaling; the pixel layer
		// derives the compact 2-px levels from it.  Replaces the old
		// cell-count fill/over fields.
		float levelL ;
		float levelR ;
		bool selected ;
		bool muted ;
		ColorDefinition onColor ;
	} ;
	void drawMeterBar(int x,int y,int height,float peak,int volume,
	                  bool selected,bool muted,GUITextProperties &props,
	                  ColorDefinition onColor,int side,MeterRecord &rec) ;
	void showInstrumentFxMenu() ;

	// TREEFROG_FX_PAGES_V1 (Fase 4.3)
	void cycleFxPage() ;
	void fxProcessPageButtonMask(unsigned int mask) ;
	void fxMoveRow(int delta) ;
	void fxEditRow(int delta,bool coarse) ;
	// TREEFROG_FX_EDIT_CURVE_V1 (PLAN_FX_REDESIGN_ES.md, Fase 14): musical/log
	// curve editing (semitones / octaves) used by fxEditRow on wide-range
	// proportional params (EQ freqs, delay time, reverb pre/decay, comp
	// attack/release/ratio).
	void fxEditCurve(int id,int delta,bool coarse) ;
	void fxResetRow() ;
	void drawFxPages() ;
	void drawFxParamPage(FxPage page) ;
	void drawFxParamRow(int id,int x,int y,int col) ;
	// TREEFROG_FX_MASTER_PAGES_RC2 (PLAN_FX_REDESIGN_ES.md, RC2 point 4):
	// dedicated DELAY MASTER / REVERB MASTER pages with a clear visual
	// hierarchy (title in CD_HILITE1, row label in CD_NORMAL, value in
	// CD_HILITE1, edited row inverted in CD_HILITE2).  drawMasterFxRow draws
	// one two-column row; drawDelayPage/drawReverbPage lay the pages out.
	void drawMasterFxRow(const char *label,const char *value,bool selected,
	                     int x,int y,int valueX) ;
	void drawDelayPage(const char *title) ;
	void drawReverbPage(const char *title) ;
	// TREEFROG_EQ_MENU_V1 (PLAN_FX_REDESIGN_ES.md, Fase 12): dedicated EQ
	// menu (exclusive page) with banded LOW/MID/HIGH layout, ON/OFF, Hz and
	// signed-dB values.  drawEqRow draws one row; drawEqPage is dispatched by
	// drawFxParamPage.
	void drawEqPage(const char *title) ;
	void drawEqRow(int id,int labelX,int valueX,int y) ;
	// TREEFROG_COMP_MENU_V1 (PLAN_FX_REDESIGN_ES.md, Fase 13): dedicated COMP
	// menu (exclusive page) with BYP first, centered labeled rows with units,
	// ratio as x:1, booleans as ON/OFF, and the GR meter always visible below.
	void drawCompPage(const char *title) ;
	// TREEFROG_FX_PAGES_V3 (Fase 9): master FX returns on the MIX page.
	void nudgeDelayReturn(int delta) ;
	void nudgeReverbReturn(int delta) ;
	void drawMixReturns(int y) ;
	// TREEFROG_GLOBAL_UNDO_V1: MIX/FX edit history (L1+X undo, R1+X redo).
	// kind: ME_VOL/ME_PAN/ME_MASTERVOL/ME_DLYRET/ME_RVBRET/ME_FX/ME_MUTE/
	// ME_SOLO.  channel: mixer channel (or -1 for master); ME_FX stores the
	// param id.  value: old int value (vol/pan/mastervol), old percent
	// (returns), old float param (ME_FX), old mute state / mute mask
	// (ME_MUTE/ME_SOLO).  TREEFROG_GLOBAL_UNDO_V7 (Bacon 1.1.1 V16):
	// newValue: post-edit value captured at push time so REDO restores the
	// exact edited state (R1+X), not the state before the edit again.
	struct MixEdit {
		int kind ;
		int channel ;
		float value ;
		float newValue ;
	} ;
	static const int MIX_HISTORY_SIZE=16 ;
	void pushMixUndo(int kind,int channel,float value,float newValue=-1.0f) ;
	void restoreMixEdit(const MixEdit &edit) ;
	MixEdit mixUndo_[MIX_HISTORY_SIZE] ;
	int mixUndoCount_ ;
	MixEdit mixRedo_[MIX_HISTORY_SIZE] ;
	int mixRedoCount_ ;
	float fxGet(int id) const ;
	void fxSet(int id,float v) ;
	int fxRowForId(int id) const ;
	// TREEFROG_MASTER_BYPASS_FIRST_V1 (PLAN_RC3... point 7): ordered row
	// helpers keep BYPASS as the first row on the four master pages.
	int fxBypassId(FxPage page) const ;
	int fxCountOnPage(FxPage page) const ;
	int fxIdForRow(int row) const ;
	bool fxIdOnPage(int id,FxPage page) const ;
	// TREEFROG_MIXER_ACTION_MENU_V1 (Bacon 1.1.1 V13): L1+A master/track
	// action menu (defined in MixerView.cpp); it reads the private mixer
	// state and records softclip edits in the mix undo history.
	friend class MixerActionMenuModal ;
private:
	const char *song_ ;

	struct {                      // .Clipboard structure
        bool active_ ;            // .If currently making a selection
        unsigned char *data_ ;    // .Null if clipboard empty
        int x_ ;                  // .Current selection positions
        int y_ ;                  // .
        int offset_ ;             // .
        int width_ ;              // .Size of selection
        int height_ ;             // .
    } clipboard_ ;

	int saveX_ ;
	int saveY_ ;
	int saveOffset_ ;
	bool invertBatt_ ;
	bool soloMode_ ;
	bool masterSelected_ ;
	int frameRefreshDivider_ ;
	// TREEFROG_MIXER_VU_SMOOTH_V1 (H38.7):
	// Per-channel display level that attacks instantly on a note and falls
	// with a smooth per-frame exponential release, so the bars never jump
	// from full to empty in a single frame (which looked like screen flicker).
	float vuDisplay_[SONG_CHANNEL_COUNT] ;
	// TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1): per-side display levels
	// (L/R), smoothed the same way as vuDisplay_.
	float vuDisplayL_[SONG_CHANNEL_COUNT] ;
	float vuDisplayR_[SONG_CHANNEL_COUNT] ;
	// TREEFROG_FX_PAGES_V1 (Fase 4.3)
	int fxPage_ ;                 // current FxPage
	int fxRow_ ;                  // row cursor within the current page
	// TREEFROG_FX_PAGES_V3 (Fase 9): 0=VOL 1=DLY RET 2=RVB RET on the MIX page
	int fxEditTarget_ ;
	// TREEFROG_MIXER_HALF_CELL_BARS_V1: per-meter records (channels 0..7 +
	// master at SONG_CHANNEL_COUNT) refreshed by DrawView, painted by
	// PostFlushDraw.
	MeterRecord meterRecords_[SONG_CHANNEL_COUNT+1] ;
} ;
#endif
