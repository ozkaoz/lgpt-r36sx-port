#ifndef _MIXER_VIEW_H_
#define _MIXER_VIEW_H_

#include "BaseClasses/View.h"
#include "ViewData.h"
#include "Application/Model/Song.h"

// TREEFROG_FX_PAGES_V1 (PLAN_FX_REDESIGN_ES.md, Fase 4.3):
// MixerView gains a page system for the master FX engine.  SELECT cycles
// MIX -> DELAY -> REVERB -> MASTER -> MIX.  MIX keeps the per-channel bars
// and adds per-track DLY/RVB send readouts/edits (R2 alone cycles the edit
// target VOL/DLY/RVB on the hovered channel).  DELAY/REVERB/MASTER are
// parameter pages: UP/DOWN moves the row cursor, LEFT/RIGHT edits the value,
// A+UP/DOWN coarse.  MASTER exposes the full 3-band EQ + compressor.
enum FxPage {
    FX_PAGE_MIX = 0,
    FX_PAGE_DELAY,
    FX_PAGE_REVERB,
    FX_PAGE_MASTER,
    FX_PAGE_COUNT
};

// Parameter rows available on the DELAY/REVERB/MASTER pages (see
// MixerView.cpp kFxParams_ and fxGet/fxSet).  Also used to size the cursor.
enum FxParamId {
    // DELAY
    FX_P_DLY_SEND = 0,
    FX_P_DLY_RET,
    FX_P_DLY_TIME,
    FX_P_DLY_FBK,
    FX_P_DLY_MIX,
    FX_P_DLY_WID,
    FX_P_DLY_PP,
    FX_P_DLY_SAT,
    FX_P_DLY_BYP,
    // REVERB
    FX_P_RVB_SEND,
    FX_P_RVB_RET,
    FX_P_RVB_PRE,
    FX_P_RVB_DEC,
    FX_P_RVB_SIZ,
    FX_P_RVB_DMP,
    FX_P_RVB_WID,
    FX_P_RVB_MODE,
    FX_P_RVB_MIX,
    FX_P_RVB_BYP,
    // MASTER EQ
    FX_P_EQ_BYP,
    FX_P_EQ_LOW_FRQ,
    FX_P_EQ_LOW_GAI,
    FX_P_EQ_LOW_Q,
    FX_P_EQ_LOW_EN,
    FX_P_EQ_MID_FRQ,
    FX_P_EQ_MID_GAI,
    FX_P_EQ_MID_Q,
    FX_P_EQ_MID_EN,
    FX_P_EQ_HI_FRQ,
    FX_P_EQ_HI_GAI,
    FX_P_EQ_HI_Q,
    FX_P_EQ_HI_EN,
    // MASTER COMP
    FX_P_CMP_THR,
    FX_P_CMP_RAT,
    FX_P_CMP_KNE,
    FX_P_CMP_ATK,
    FX_P_CMP_REL,
    FX_P_CMP_MKU,
    FX_P_CMP_LINK,
    FX_P_CMP_SC,
    FX_P_CMP_BYP,
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
	void showInstrumentFxMenu() ;

	// TREEFROG_FX_PAGES_V1 (Fase 4.3)
	void cycleFxPage() ;
	void fxProcessPageButtonMask(unsigned int mask) ;
	void fxMoveRow(int delta) ;
	void fxEditRow(int delta,bool coarse) ;
	void fxEditChannelTarget(int delta) ;
	void drawFxPages() ;
	void drawFxParamPage(FxPage page) ;
	void drawFxParamRow(int id,int x,int y,int col) ;
	void drawMixSends() ;
	float fxGet(int id) const ;
	void fxSet(int id,float v) ;
	int fxRowForId(int id) const ;
	bool fxIdOnPage(int id,FxPage page) const ;
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
	// TREEFROG_FX_PAGES_V1 (Fase 4.3)
	int fxPage_ ;                 // current FxPage
	int fxRow_ ;                  // row cursor within the current page
	int fxEditTarget_ ;           // 0=VOL 1=DLY 2=RVB on the MIX page
} ;
#endif
