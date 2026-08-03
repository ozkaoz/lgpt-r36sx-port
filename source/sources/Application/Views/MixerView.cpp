// TREEFROG_V42_NO_WHITE_BOX_UI
#include "MixerView.h"
#include "Application/Model/Mixer.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Project.h"
#include "Application/Views/UIController.h"
#include "Application/Views/BaseClasses/UiDraw.h"
#include "Application/Audio/FxEngine/FxEngine.h"
#include "Application/Utils/fixed.h"
#include "Application/Utils/char.h"
#include "Application/AppWindow.h"
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <math.h>

// TREEFROG_FX_PAGES_PARAMS_V2 (PLAN_FX_REDESIGN_ES.md, Fase 6):
// Parameter table for the DELAY / REVERB / EQ / COMP pages.  Each row exposes
// a master-bus parameter as a float in natural units (ms, %, dB, Hz, s,
// ratio).  The UI edits the float and the setter clamps to the documented
// range; the DSP modules clamp again, so the page is always consistent.
// Fase 6: the global SEND/RET rows were removed (sends are per-track /
// per-instrument; returns are fixed 0.5 helpers), and the old MASTER page
// was split into EQ and COMP so each fits the 8-line mixer screen.
struct FxParamSpec {
    const char *label ;        // short name shown on the page
    FxPage page ;              // which page owns this row
    float vmin ;               // minimum (natural units)
    float vmax ;               // maximum (natural units)
    float vdef ;               // legacy default (natural units), A+B restores
    const char *fmt ;          // printf format for the value
} ;

static const FxParamSpec kFxParams_[FX_PARAM_COUNT] = {
    // DELAY page
    { "DLY TIM", FX_PAGE_DELAY,   10.0f, 2000.0f,   0.0f,   "%5.0f" },  // FX_P_DLY_TIME (ms)
    { "DLY FBK", FX_PAGE_DELAY,    0.0f,   0.98f,   0.0f,   "%5.2f" },  // FX_P_DLY_FBK
    { "DLY MIX", FX_PAGE_DELAY,    0.0f,   1.0f,    1.0f,   "%5.2f" },  // FX_P_DLY_MIX
    { "DLY WID", FX_PAGE_DELAY,    0.0f,   1.0f,    1.0f,   "%5.2f" },  // FX_P_DLY_WID
    { "DLY P/P", FX_PAGE_DELAY,    0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_DLY_PP (0/1)
    { "DLY SAT", FX_PAGE_DELAY,    0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_DLY_SAT (0/1)
    { "DLY BYP", FX_PAGE_DELAY,    0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_DLY_BYP (0/1)
    // REVERB page
    // RC2 (point 3.1): the RVB MIX row was removed.  The reverb is wet-only
    // (no dry/wet crossfade); the audible level is the instrument send + the
    // Mixer REVERB RETURN (MIX page FX RETURNS), not an internal mix.
    { "RVB PRE", FX_PAGE_REVERB,   0.0f, 100.0f,    0.0f,   "%5.0f" },  // FX_P_RVB_PRE (ms)
    { "RVB DEC", FX_PAGE_REVERB,   0.2f,   8.0f,    1.0f,   "%5.2f" },  // FX_P_RVB_DEC (s)
    { "RVB SIZ", FX_PAGE_REVERB,   0.5f,   1.5f,    1.0f,   "%5.2f" },  // FX_P_RVB_SIZ
    { "RVB DMP", FX_PAGE_REVERB,   0.0f,   1.0f,    0.5f,   "%5.2f" },  // FX_P_RVB_DMP
    { "RVB WID", FX_PAGE_REVERB,   0.0f,   1.0f,    1.0f,   "%5.2f" },  // FX_P_RVB_WID
    { "RVB MOD", FX_PAGE_REVERB,   0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_RVB_MODE (0/1)
    { "RVB BYP", FX_PAGE_REVERB,   0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_RVB_BYP (0/1)
    // EQ page (3 bands, dedicated banded menu - Fase 12).  Per band the rows
    // are EN / FRQ / GAI / Q so UP/DOWN walks the band in the same visual
    // order drawEqPage() renders.  Frequencies default to 100/1000/10000 Hz.
    { "EQ  BYP", FX_PAGE_EQ,       0.0f,   1.0f,    1.0f,   "%5.0f" },  // FX_P_EQ_BYP
    { "LO  EN",  FX_PAGE_EQ,       0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_EQ_LOW_EN
    { "LO  FRQ", FX_PAGE_EQ,      20.0f, 20000.0f,100.0f,  "%5.0f" },  // FX_P_EQ_LOW_FRQ
    { "LO  GAI", FX_PAGE_EQ,      -12.0f,  12.0f,   0.0f,   "%5.1f" },  // FX_P_EQ_LOW_GAI
    { "LO  Q",   FX_PAGE_EQ,       0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQ_LOW_Q
    { "MID EN",  FX_PAGE_EQ,       0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_EQ_MID_EN
    { "MID FRQ", FX_PAGE_EQ,      20.0f, 20000.0f,1000.0f, "%5.0f" },  // FX_P_EQ_MID_FRQ
    { "MID GAI", FX_PAGE_EQ,      -12.0f,  12.0f,   0.0f,   "%5.1f" },  // FX_P_EQ_MID_GAI
    { "MID Q",   FX_PAGE_EQ,       0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQ_MID_Q
    { "HI  EN",  FX_PAGE_EQ,       0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_EQ_HI_EN
    { "HI  FRQ", FX_PAGE_EQ,      20.0f, 20000.0f,10000.0f,"%5.0f" },  // FX_P_EQ_HI_FRQ
    { "HI  GAI", FX_PAGE_EQ,      -12.0f,  12.0f,   0.0f,   "%5.1f" },  // FX_P_EQ_HI_GAI
    { "HI  Q",   FX_PAGE_EQ,       0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQ_HI_Q
    // COMP page (dedicated menu - Fase 13: BYP first so it is never
    // off-screen, then THR/RAT/KNE/ATK/REL/MKU/LNK/SC in the order
    // drawCompPage() renders; the GR meter sits below the parameters).
    { "CMP BYP", FX_PAGE_COMP,     0.0f,   1.0f,    1.0f,   "%5.0f" },  // FX_P_CMP_BYP
    { "CMP THR", FX_PAGE_COMP,   -60.0f,   0.0f,  -24.0f,  "%5.1f" },  // FX_P_CMP_THR (dB)
    { "CMP RAT", FX_PAGE_COMP,     1.0f,  20.0f,    4.0f,   "%5.1f" },  // FX_P_CMP_RAT
    { "CMP KNE", FX_PAGE_COMP,     0.0f,  12.0f,    6.0f,   "%5.1f" },  // FX_P_CMP_KNE (dB)
    { "CMP ATK", FX_PAGE_COMP,     0.1f, 500.0f,   15.0f,   "%5.1f" },  // FX_P_CMP_ATK (ms)
    { "CMP REL", FX_PAGE_COMP,     1.0f, 2000.0f, 200.0f,   "%5.0f" },  // FX_P_CMP_REL (ms)
    { "CMP MKU", FX_PAGE_COMP,     0.0f,  24.0f,    0.0f,   "%5.1f" },  // FX_P_CMP_MKU (dB)
    { "CMP LNK", FX_PAGE_COMP,     0.0f,   1.0f,    1.0f,   "%5.0f" },  // FX_P_CMP_LINK
    { "CMP SCL", FX_PAGE_COMP,     0.0f,   1.0f,    1.0f,   "%5.0f" },  // FX_P_CMP_SC (softclip)
} ;

// TREEFROG_MIXER_VU_DB_SCALE_V2 (H38.6):
// The bar fill is scaled by BOTH the volume setting and the live output
// level: fill = (volume/100) * dB-scaled(peak). This keeps the bars honest
// on hardware where the measured peak itself does not follow the volume:
// volume 0 -> empty, volume 1 -> near empty, 50 -> half, 100 -> full, while
// the dB-scaled peak still makes the bar bounce with the music.
static float mixVULevel(float peak) {
	if (peak <= 0.0f) return 0.0f ;
	float db = 20.0f * log10f(peak) ;
	float level = (db + 50.0f) / 50.0f ;
	if (level < 0.0f) level = 0.0f ;
	if (level > 1.0f) level = 1.0f ;
	return level ;
}

// TREEFROG_FX_PAGES_V3 (PLAN_FX_REDESIGN_ES.md, Fase 9):
// Master FX returns are stored as fixed (Q15) 0..1 levels in FxEngine.
// These helpers convert to/from the integer percent (0..100) the MIX page
// edits, clamped so the value always round-trips.
static int fxReturnPercent(fixed ret) {
	float f=fp2fl(ret) ;
	if (f<0.0f) f=0.0f ;
	if (f>1.0f) f=1.0f ;
	return (int)(f*100.0f+0.5f) ;
}
static fixed fxReturnFromPercent(int p) {
	if (p<0) p=0 ;
	if (p>100) p=100 ;
	return fl2fp((float)p*0.01f) ;
}
void MixerView::nudgeDelayReturn(int delta) {
	FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
	fx.SetDelayReturn(fxReturnFromPercent(fxReturnPercent(fx.GetDelayReturn())+delta)) ;
}
void MixerView::nudgeReverbReturn(int delta) {
	FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
	fx.SetReverbReturn(fxReturnFromPercent(fxReturnPercent(fx.GetReverbReturn())+delta)) ;
}

MixerView::MixerView(GUIWindow &w,ViewData *viewData):View(w,viewData) {
	clipboard_.active_=false ;
	clipboard_.data_=0 ;
	invertBatt_=false;
	soloMode_=false;
	masterSelected_=false;
	fxPage_=FX_PAGE_MIX ;
	fxRow_=0 ;
	fxEditTarget_=0 ;
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		vuDisplay_[i]=0.0f ;
	}
}

MixerView::~MixerView() {
} 


void MixerView::onStart() {
	Player *player=Player::GetInstance() ;
	unsigned char from=viewData_->songX_ ;
	unsigned char to=from ;
	player->OnStartButton(PM_SONG,from,false,to) ;
} ;

void MixerView::onStop() {
	Player *player=Player::GetInstance() ;
	unsigned char from=viewData_->songX_ ;
	unsigned char to=from ;
	player->OnStartButton(PM_SONG,from,true,to) ;
} ;

void MixerView::OnFocus() {
} ;

void MixerView::updateCursor(int dx,int dy) {
	(void)dy ;
	// TREEFROG_MIXER_MASTER_BAR_V1:
	// The master bar sits to the LEFT of channel 0. Moving left from channel 0
	// selects the master; moving right from the master selects channel 0.
	if (masterSelected_) {
		if (dx>0) {
			masterSelected_=false ;
			viewData_->mixerCol_=0 ;
		}
	} else {
		int x=viewData_->mixerCol_ ;
		x+=dx ;
		if (x<0) {
			x=0 ;
			masterSelected_=true ;
		}
		if (x>7) x=7 ;
		viewData_->mixerCol_=x ;
	}
	isDirty_=true;
}

void MixerView::updateVolume(int delta) {
	if (masterSelected_) {
		adjustMasterVolume(delta) ;
		return ;
	}
	// TREEFROG_FX_PAGES_V3 (Fase 9): on the MIX page the R2-cycled edit
	// target selects whether UP/DOWN edits the channel volume or one of the
	// master FX returns (sends are per-instrument now, edited in Instrument).
	if (fxPage_==FX_PAGE_MIX) {
		if (fxEditTarget_==1) {
			nudgeDelayReturn(delta) ;
			isDirty_=true ;
			return ;
		}
		if (fxEditTarget_==2) {
			nudgeReverbReturn(delta) ;
			isDirty_=true ;
			return ;
		}
	}
	Mixer::GetInstance()->NudgeChannelVolume(viewData_->mixerCol_,delta) ;
	isDirty_=true ;
}

void MixerView::adjustMasterVolume(int delta) {
	Project *project=viewData_->project_ ;
	if (!project) return ;
	int v=project->GetMasterVolume() ;
	v+=delta ;
	if (v<10) v=10 ;
	if (v>100) v=100 ;
	Variable *var=project->FindVariable(VAR_MASTERVOL) ;
	if (var) var->SetInt(v,false) ;
	MixerService::GetInstance()->SetMasterVolume(v) ;
	isDirty_=true ;
}

void MixerView::toggleMute() {
	if (masterSelected_) return ;
	UIController::GetInstance()->ToggleMute(viewData_->mixerCol_,viewData_->mixerCol_) ;
	isDirty_=true ;
}

void MixerView::switchSoloMode() {
	if (masterSelected_) return ;
	UIController::GetInstance()->SwitchSoloMode(viewData_->mixerCol_,viewData_->mixerCol_,!soloMode_) ;
	soloMode_=!soloMode_ ;
	isDirty_=true ;
}

void MixerView::ProcessButtonMask(unsigned short mask,bool pressed) {
	if (!pressed) return ;

	if (clipboard_.active_) {
		viewMode_=VM_SELECTION ;
	} ;
	
	if (viewMode_==VM_SELECTION) {
        if (clipboard_.active_==false) {
            clipboard_.active_=true ;
            clipboard_.x_=viewData_->songX_ ;
            clipboard_.y_=viewData_->songY_ ;
            clipboard_.offset_=viewData_->songOffset_ ;
			saveX_=clipboard_.x_ ;
			saveY_=clipboard_.y_ ;
			saveOffset_=clipboard_.offset_ ;
        }
        processSelectionButtonMask(mask) ;
    } else {
        viewMode_=VM_NORMAL ;
        processNormalButtonMask(mask) ;
    }
} ;


void MixerView::processNormalButtonMask(unsigned int mask) {

	// TREEFROG_FX_PAGES_V1 (Fase 4.3):
	// SELECT cycles MIX -> DELAY -> REVERB -> MASTER -> MIX.  On the
	// parameter pages UP/DOWN moves the row cursor and LEFT/RIGHT edits the
	// value; on the MIX page the classic channel/master behaviour applies.
	if (mask&EPBM_SELECT) {
		cycleFxPage() ;
		return ;
	}

	// R1 modifier: keep the port-wide convention.
	// R1+B toggles mute on the selected channel.
	// R1+A toggles Solo on the selected channel
	// (TREEFROG_MIXER_SOLO_V1: solo follows the hovered channel bar).
	if (mask&EPBM_R) {
		if (mask&EPBM_B) {
			toggleMute() ;
			return ;
		}
		if (mask&EPBM_A) {
			switchSoloMode() ;
			return ;
		}
		if (mask&EPBM_UP) {
			ViewType vt=VT_SONG;
			ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
			SetChanged();
			NotifyObservers(&ve) ;
		}
		if (mask&EPBM_START) {
			onStop() ;
		}
		return ;
	}

	// R2 modifier: opens the instrument FX menu for the hovered channel bar
	// (TREEFROG_MIXER_FX_MENU_V2: FX apply to the whole instrument).
	if (mask&EPBM_R2) {
		if (mask&EPBM_A) {
			showInstrumentFxMenu() ;
			return ;
		}
		// R2 alone cycles the MIX-page edit target:
		// VOL -> DLY RET -> RVB RET (TREEFROG_FX_PAGES_V3, Fase 9).
		if (fxPage_==FX_PAGE_MIX) {
			fxEditTarget_=(fxEditTarget_+1)%3 ;
			isDirty_=true ;
			((AppWindow &)w_).SetDirty() ;
		}
		return ;
	}

	// Parameter pages (DELAY/REVERB/EQ/COMP): UP/DOWN row, LEFT/RIGHT edit.
	if (fxPage_!=FX_PAGE_MIX) {
		// TREEFROG_FX_NAV_A_B_DEFAULT_V1: A+B restores the hovered row to its
		// legacy default (checked before the arrow edits so it wins the mask).
		if ((mask&EPBM_A) && (mask&EPBM_B) &&
		    !(mask & (EPBM_LEFT | EPBM_RIGHT | EPBM_UP | EPBM_DOWN |
		              EPBM_L | EPBM_R | EPBM_L2 | EPBM_R2 |
		              EPBM_X | EPBM_Y | EPBM_SELECT | EPBM_START))) {
			fxResetRow() ;
			return ;
		}
		if (mask&EPBM_A) {
			if (mask&EPBM_UP) fxEditRow(1,true) ;
			if (mask&EPBM_DOWN) fxEditRow(-1,true) ;
			if (mask&EPBM_LEFT) fxEditRow(-1,false) ;
			if (mask&EPBM_RIGHT) fxEditRow(1,false) ;
			return ;
		}
		if (mask&EPBM_UP) { fxMoveRow(-1) ; return ; }
		if (mask&EPBM_DOWN) { fxMoveRow(1) ; return ; }
		if (mask&EPBM_LEFT) { fxEditRow(-1,false) ; return ; }
		if (mask&EPBM_RIGHT) { fxEditRow(1,false) ; return ; }
		if (mask&EPBM_START) onStart() ;
		return ;
	}

	// A modifier: value edits, matching the port editing convention.
	// A+UP/DOWN changes by 10, A+LEFT/RIGHT changes by 1.
	if (mask&EPBM_A) {
		if (mask&EPBM_UP) updateVolume(10) ;
		if (mask&EPBM_DOWN) updateVolume(-10) ;
		if (mask&EPBM_LEFT) updateVolume(-1) ;
		if (mask&EPBM_RIGHT) updateVolume(1) ;
		return ;
	}

	// L modifier remains a secondary coarse edit path for compatibility.
	if (mask&EPBM_L) {
		if (mask&EPBM_UP) updateVolume(10) ;
		if (mask&EPBM_DOWN) updateVolume(-10) ;
		if (mask&EPBM_LEFT) updateCursor(-1,0) ;
		if (mask&EPBM_RIGHT) updateCursor(1,0) ;
		return ;
	}

	// No modifier.
	if (mask&EPBM_START) {
		onStart() ;
	}
	if (mask&EPBM_LEFT) updateCursor(-1,0) ;
	if (mask&EPBM_RIGHT) updateCursor(1,0) ;
	if (mask&EPBM_UP) updateVolume(1) ;
	if (mask&EPBM_DOWN) updateVolume(-1) ;
} ;


void MixerView::processSelectionButtonMask(unsigned int mask) {
	if (mask&EPBM_R) {
		if (mask&EPBM_START) {
			onStop() ;
		}
	} else {
		if (mask&EPBM_START) {
			onStart() ;
		}
	}
}

void MixerView::showInstrumentFxMenu() {
	// TREEFROG_MIXER_FX_MENU_V3 (PLAN_FX_REDESIGN_ES.md, Fase 6):
	// R2+A on a channel bar now switches to the Instrument view for the
	// instrument attached to that channel (channels 0..7 map to the first
	// 8 sample instruments).  The old InstrumentFxModal is removed: the
	// per-instrument FX sends (DRY/DLY/RVB) and the legacy comb/offline FX
	// live directly in InstrumentView now.
	int channel=viewData_->mixerCol_ ;
	if (channel>=0 && channel<SONG_CHANNEL_COUNT) {
		viewData_->currentInstrument_=channel ;
	}
	ViewType vt=VT_INSTRUMENT ;
	ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
	SetChanged() ;
	NotifyObservers(&ve) ;
}

void MixerView::drawVolumeBar(int channel,int x,int y,int height) {
	Mixer *mixer=Mixer::GetInstance() ;
	Player *player=Player::GetInstance() ;
	int volume=mixer->GetChannelVolume(channel) ;
	// TREEFROG_MIXER_PER_CHANNEL_VU_V1 (H38.7):
	// Read the channel's own produced-audio level (instrument buffer activity)
	// instead of the saturated mixed bus level, so the bar bounces with the
	// instrument in real time and falls quickly on mute/stop.
	// TREEFROG_MIXER_VU_SMOOTH_V1 (H38.7): the drawn level is the per-frame
	// smoothed display value (instant attack, smooth release), never the raw
	// audio peak, so the bar cannot jump full->empty in one frame.
	float peak=vuDisplay_[channel] ;
	bool selected=(!masterSelected_ && channel==viewData_->mixerCol_) ;
	bool muted=player->IsChannelMuted(channel) ;
	GUITextProperties props ;
	char buffer[8] ;
	char hex[3] ;

	hex2char(channel,hex) ;
	if (selected) {
		SetColor(CD_HILITE2) ;
		props.invert_=true ;
	} else if (muted) {
		SetColor(CD_BORDER) ;
		props.invert_=false ;
	} else {
		SetColor(CD_NORMAL) ;
		props.invert_=false ;
	}
	DrawString(x,y,hex,props) ;
	props.invert_=false ;

	// TREEFROG_MIXER_LIVE_BAR_V4 (H38.7):
	// The bar fill follows the real-time output level (GetChannelPeak), so a
	// low volume reads as a small wave and a loud volume as a big one. The
	// channel volume setting is drawn as an accent marker cell plus the
	// numeric value below. Selected bars stay purple, muted bars dim.
	int totalCells=2*height ;
	int filledCells=int(mixVULevel(peak)*float(volume)*0.01f*float(totalCells)) ;
	if (filledCells>totalCells) filledCells=totalCells ;
	int volMarker=(volume*totalCells+99)/100 ;
	if (volMarker>totalCells) volMarker=totalCells ;
	for (int row=0;row<height;row++) {
		for (int c=0;c<2;c++) {
			int cellFromBottom=totalCells-(2*row+c) ;
			bool on=(cellFromBottom<=filledCells) ;
			bool marker=((!on)&&(cellFromBottom==volMarker)) ;
			if (selected) {
				SetColor(on?CD_HILITE2:CD_HILITE1) ;
				props.invert_=on ;
			} else if (muted) {
				SetColor(CD_BORDER) ;
				props.invert_=false ;
			} else {
				if (on) {
					SetColor(CD_NORMAL) ;
					props.invert_=true ;
				} else if (marker) {
					SetColor(CD_HILITE2) ;
					props.invert_=true ;
				} else {
					SetColor(CD_HILITE1) ;
					props.invert_=false ;
				}
			}
			DrawString(x+c,y+1+row," ",props) ;
		}
	}

	SetColor(selected?CD_HILITE2:(muted?CD_BORDER:CD_NORMAL)) ;
	props.invert_=selected ;
	sprintf(buffer,"%3d",volume) ;
	DrawString(x-1,y+height+2,buffer,props) ;
	props.invert_=false ;

	if (muted) {
		SetColor(CD_HILITE2) ;
		DrawString(x,y+height+3,"M",props) ;
	}
}

void MixerView::drawMasterBar(int x,int y,int height) {
	Project *project=viewData_->project_ ;
	int volume=project?project->GetMasterVolume():100 ;
	Mixer *mixer=Mixer::GetInstance() ;
	GUITextProperties props ;
	char buffer[8] ;

	// TREEFROG_MIXER_MASTER_SUM_V3 (H38.7):
	// The master bar is the DYNAMIC SUM of the per-channel DISPLAY levels,
	// i.e. exactly the same dB-scaled level each channel bar shows
	// (mixVULevel(display) * volume/100). This keeps the master consistent
	// with the channel bars and adds headroom so a single quiet element reads
	// near-empty instead of pinning the meter to 100% (half the sum scale is
	// reserved, so two full-loudness channels reach full scale).
	float sum=0.0f ;
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		sum += mixVULevel(vuDisplay_[i]) * (float)mixer->GetChannelVolume(i) * 0.01f ;
	}
	float masterLevel=sum*0.5f ;
	if (masterLevel>1.0f) masterLevel=1.0f ;

	// TREEFROG_MIXER_MASTER_BAR_V1 (H38.7):
	// Master (MST) bar drawn live on the left of the channel bars, in cyan so
	// it stands out from the white channel fills. When selected it lights
	// purple like a selected channel.
	SetColor(masterSelected_?CD_HILITE2:CD_PLAY) ;
	props.invert_=false ;
	DrawString(x-1,y,"MST",props) ;

	int totalCells=2*height ;
	int filledCells=int(masterLevel*float(volume)*0.01f*float(totalCells)) ;
	if (filledCells>totalCells) filledCells=totalCells ;
	int volMarker=(volume*totalCells+99)/100 ;
	if (volMarker>totalCells) volMarker=totalCells ;
	for (int row=0;row<height;row++) {
		for (int c=0;c<2;c++) {
			int cellFromBottom=totalCells-(2*row+c) ;
			bool on=(cellFromBottom<=filledCells) ;
			bool marker=((!on)&&(cellFromBottom==volMarker)) ;
			if (masterSelected_) {
				SetColor(on?CD_HILITE2:CD_HILITE1) ;
				props.invert_=on ;
			} else {
				if (on) {
					SetColor(CD_PLAY) ;
					props.invert_=true ;
				} else if (marker) {
					SetColor(CD_HILITE2) ;
					props.invert_=true ;
				} else {
					SetColor(CD_HILITE1) ;
					props.invert_=false ;
				}
			}
			DrawString(x+c,y+1+row," ",props) ;
		}
	}

	SetColor(masterSelected_?CD_HILITE2:CD_PLAY) ;
	props.invert_=masterSelected_ ;
	sprintf(buffer,"%3d",volume) ;
	DrawString(x-1,y+height+2,buffer,props) ;
	props.invert_=false ;
}

// TREEFROG_FX_PAGES_V1 (Fase 4.3) -------------------------------------------

void MixerView::cycleFxPage() {
	fxPage_=(fxPage_+1)%FX_PAGE_COUNT ;
	fxRow_=0 ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

bool MixerView::fxIdOnPage(int id,FxPage page) const {
	return kFxParams_[id].page==page ;
}

// TREEFROG_MASTER_BYPASS_FIRST_V1 (PLAN_RC3_MODERNIZACION_VISUAL_ES.md, point 7):
// On DELAY/REVERB/EQ/COMP the BYPASS parameter is the FIRST visual and logical
// row.  These helpers put the page Bypass at row 0 for navigation and drawing.
// The underlying kFxParams_ table and FxParam enum stay byte-identical
// (bit-identical persistence); only the row order changes.
int MixerView::fxBypassId(FxPage page) const {
	switch (page) {
	case FX_PAGE_DELAY:  return FX_P_DLY_BYP ;
	case FX_PAGE_REVERB: return FX_P_RVB_BYP ;
	case FX_PAGE_EQ:     return FX_P_EQ_BYP ;
	case FX_PAGE_COMP:   return FX_P_CMP_BYP ;
	default:             return -1 ;
	}
}

int MixerView::fxCountOnPage(FxPage page) const {
	int count=0 ;
	for (int i=0;i<FX_PARAM_COUNT;i++) {
		if (kFxParams_[i].page==page) count++ ;
	}
	return count ;
}

// Ordered row position of a param on its page, with the page Bypass first.
int MixerView::fxRowForId(int id) const {
	FxPage page=kFxParams_[id].page ;
	int byp=fxBypassId(page) ;
	if (id==byp) return 0 ;
	int row=1 ;
	for (int i=0;i<id;i++) {
		if (kFxParams_[i].page==page && i!=byp) row++ ;
	}
	return row ;
}

// Inverse of fxRowForId: given a logical row (0 = bypass), return the param id.
int MixerView::fxIdForRow(int row) const {
	FxPage page=(FxPage)fxPage_ ;
	int byp=fxBypassId(page) ;
	if (byp>=0 && row==0) return byp ;
	int seen=1 ;
	for (int i=0;i<FX_PARAM_COUNT;i++) {
		if (kFxParams_[i].page!=page || i==byp) continue ;
		if (seen==row) return i ;
		seen++ ;
	}
	return -1 ;
}

void MixerView::fxMoveRow(int delta) {
	int count=fxCountOnPage((FxPage)fxPage_) ;
	if (count<=0) return ;
	fxRow_+=delta ;
	if (fxRow_<0) fxRow_=count-1 ;
	if (fxRow_>=count) fxRow_=0 ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

// TREEFROG_FX_EDIT_CURVE_V1 (PLAN_FX_REDESIGN_ES.md, Fase 12 + Fase 14):
// Wide-range proportional parameters are edited on a musical/log curve, never
// with a linear 1/10 step: fine (L/R) steps by one semitone (x2^(1/12)),
// coarse (A+UP/DOWN) by one octave (x2).  The relative error is constant, so
// the whole range is traversable in a bounded number of presses and editing
// stays musically meaningful.  Applies to EQ frequencies and to every other
// wide-range time/ratio parameter (delay time, reverb pre-delay/decay,
// compressor attack/release/ratio).
static bool fxUsesCurve(int id) {
	switch (id) {
	case FX_P_EQ_LOW_FRQ:
	case FX_P_EQ_MID_FRQ:
	case FX_P_EQ_HI_FRQ:
	case FX_P_DLY_TIME:
	case FX_P_RVB_PRE:
	case FX_P_RVB_DEC:
	case FX_P_CMP_ATK:
	case FX_P_CMP_REL:
	case FX_P_CMP_RAT:
		return true ;
	default:
		return false ;
	}
}
void MixerView::fxEditCurve(int id,int delta,bool coarse) {
	const FxParamSpec &spec=kFxParams_[id] ;
	float v=fxGet(id) ;
	// Values below the floor snap to it so proportional editing never
	// multiplies zero (e.g. DLY TIM defaults to 0 while vmin is 10).  If the
	// floor itself is 0 (e.g. RVB PRE), the first upward edit starts from 1%
	// of the range instead of being stuck at 0.
	if (delta>0) {
		if (v<spec.vmin) v=spec.vmin ;
		else if (v<=0.0f) v=(spec.vmax-spec.vmin)*0.01f ;
	} else if (delta<0 && v>spec.vmax) {
		v=spec.vmax ;
	}
	float factor=coarse?2.0f:1.05946309436f ;  // octave / semitone
	if (delta<0) factor=1.0f/factor ;
	int steps=delta<0?-delta:delta ;
	for (int s=0;s<steps;s++) v*=factor ;
	if (v<spec.vmin) v=spec.vmin ;
	if (v>spec.vmax) v=spec.vmax ;
	fxSet(id,v) ;
}

void MixerView::fxEditRow(int delta,bool coarse) {
	int targetId=fxIdForRow(fxRow_) ;
	if (targetId<0) return ;
	const FxParamSpec &spec=kFxParams_[targetId] ;
	if (fxUsesCurve(targetId)) {
		fxEditCurve(targetId,delta,coarse) ;
		isDirty_=true ;
		((AppWindow &)w_).SetDirty() ;
		return ;
	}
	float step=(coarse?10.0f:1.0f) ;
	// Bool-ish rows (fmt %5.0f, range 0..1) step by 1.
	if (spec.vmax-spec.vmin<=1.5f) step=1.0f ;
	float v=fxGet(targetId)+step*(float)delta ;
	if (v<spec.vmin) v=spec.vmin ;
	if (v>spec.vmax) v=spec.vmax ;
	fxSet(targetId,v) ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

// TREEFROG_FX_NAV_A_B_DEFAULT_V1 (PLAN_FX_REDESIGN_ES.md, Fase 6):
// A+B restores the hovered parameter to its legacy default so the whole page
// can be brought back to the Fase 5 "all defaults" state without hunting.
void MixerView::fxResetRow() {
	int targetId=fxIdForRow(fxRow_) ;
	if (targetId<0) return ;
	fxSet(targetId,kFxParams_[targetId].vdef) ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

// (Fase 9) nudgeDelayReturn/nudgeReverbReturn defined above; see MixerView.h.

float MixerView::fxGet(int id) const {
	FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
	switch(id) {
	case FX_P_DLY_TIME: return fp2fl(fx.GetDelayTimeMs()) ;
	case FX_P_DLY_FBK:  return fp2fl(fx.GetDelayFeedback()) ;
	case FX_P_DLY_MIX:  return fp2fl(fx.GetDelayMix()) ;
	case FX_P_DLY_WID:  return fp2fl(fx.GetDelayWidth()) ;
	case FX_P_DLY_PP:   return fx.GetDelayPingPong()?1.0f:0.0f ;
	case FX_P_DLY_SAT:  return fx.GetDelaySaturation()?1.0f:0.0f ;
	case FX_P_DLY_BYP:  return fx.GetDelayBypass()?1.0f:0.0f ;
	case FX_P_RVB_PRE:  return fp2fl(fx.GetReverbPredelayMs()) ;
	case FX_P_RVB_DEC:  return fp2fl(fx.GetReverbDecay()) ;
	case FX_P_RVB_SIZ:  return fp2fl(fx.GetReverbSize()) ;
	case FX_P_RVB_DMP:  return fp2fl(fx.GetReverbDamping()) ;
	case FX_P_RVB_WID:  return fp2fl(fx.GetReverbWidth()) ;
	case FX_P_RVB_MODE: return (float)fx.GetReverbMode() ;
	case FX_P_RVB_BYP:  return fx.GetReverbBypass()?1.0f:0.0f ;
	case FX_P_EQ_BYP:   return fx.GetEqBypass()?1.0f:0.0f ;
	case FX_P_EQ_LOW_FRQ: return fp2fl(fx.GetEqBandFreq(0)) ;
	case FX_P_EQ_LOW_GAI: return fp2fl(fx.GetEqBandGainDb(0)) ;
	case FX_P_EQ_LOW_Q:   return fp2fl(fx.GetEqBandQ(0)) ;
	case FX_P_EQ_LOW_EN:  return fx.GetEqBandEnabled(0)?1.0f:0.0f ;
	case FX_P_EQ_MID_FRQ: return fp2fl(fx.GetEqBandFreq(1)) ;
	case FX_P_EQ_MID_GAI: return fp2fl(fx.GetEqBandGainDb(1)) ;
	case FX_P_EQ_MID_Q:   return fp2fl(fx.GetEqBandQ(1)) ;
	case FX_P_EQ_MID_EN:  return fx.GetEqBandEnabled(1)?1.0f:0.0f ;
	case FX_P_EQ_HI_FRQ:  return fp2fl(fx.GetEqBandFreq(2)) ;
	case FX_P_EQ_HI_GAI:  return fp2fl(fx.GetEqBandGainDb(2)) ;
	case FX_P_EQ_HI_Q:    return fp2fl(fx.GetEqBandQ(2)) ;
	case FX_P_EQ_HI_EN:   return fx.GetEqBandEnabled(2)?1.0f:0.0f ;
	case FX_P_CMP_THR:    return fp2fl(fx.GetCompThresholdDb()) ;
	case FX_P_CMP_RAT:    return fp2fl(fx.GetCompRatio()) ;
	case FX_P_CMP_KNE:    return fp2fl(fx.GetCompKneeDb()) ;
	case FX_P_CMP_ATK:    return fx.GetCompAttackMs() ;
	case FX_P_CMP_REL:    return fx.GetCompReleaseMs() ;
	case FX_P_CMP_MKU:    return fp2fl(fx.GetCompMakeupDb()) ;
	case FX_P_CMP_LINK:   return fx.GetCompStereoLink()?1.0f:0.0f ;
	case FX_P_CMP_SC:     return fx.GetCompSoftClip()?1.0f:0.0f ;
	case FX_P_CMP_BYP:    return fx.GetCompBypass()?1.0f:0.0f ;
	}
	return 0.0f ;
}

void MixerView::fxSet(int id,float v) {
	FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
	const FxParamSpec &spec=kFxParams_[id] ;
	if (v<spec.vmin) v=spec.vmin ;
	if (v>spec.vmax) v=spec.vmax ;
	switch(id) {
	case FX_P_DLY_TIME: fx.SetDelayTimeMs(fl2fp(v)) ; break ;
	case FX_P_DLY_FBK:  fx.SetDelayFeedback(fl2fp(v)) ; break ;
	case FX_P_DLY_MIX:  fx.SetDelayMix(fl2fp(v)) ; break ;
	case FX_P_DLY_WID:  fx.SetDelayWidth(fl2fp(v)) ; break ;
	case FX_P_DLY_PP:   fx.SetDelayPingPong(v>=0.5f) ; break ;
	case FX_P_DLY_SAT:  fx.SetDelaySaturation(v>=0.5f) ; break ;
	case FX_P_DLY_BYP:  fx.SetDelayBypass(v>=0.5f) ; break ;
	case FX_P_RVB_PRE:  fx.SetReverbPredelayMs(fl2fp(v)) ; break ;
	case FX_P_RVB_DEC:  fx.SetReverbDecay(fl2fp(v)) ; break ;
	case FX_P_RVB_SIZ:  fx.SetReverbSize(fl2fp(v)) ; break ;
	case FX_P_RVB_DMP:  fx.SetReverbDamping(fl2fp(v)) ; break ;
	case FX_P_RVB_WID:  fx.SetReverbWidth(fl2fp(v)) ; break ;
	case FX_P_RVB_MODE: fx.SetReverbMode((int)v) ; break ;
	case FX_P_RVB_BYP:  fx.SetReverbBypass(v>=0.5f) ; break ;
	case FX_P_EQ_BYP:   fx.SetEqBypass(v>=0.5f) ; break ;
	case FX_P_EQ_LOW_FRQ: fx.SetEqBandFreq(0,fl2fp(v)) ; break ;
	case FX_P_EQ_LOW_GAI: fx.SetEqBandGainDb(0,fl2fp(v)) ; break ;
	case FX_P_EQ_LOW_Q:   fx.SetEqBandQ(0,fl2fp(v)) ; break ;
	case FX_P_EQ_LOW_EN:  fx.SetEqBandEnabled(0,v>=0.5f) ; break ;
	case FX_P_EQ_MID_FRQ: fx.SetEqBandFreq(1,fl2fp(v)) ; break ;
	case FX_P_EQ_MID_GAI: fx.SetEqBandGainDb(1,fl2fp(v)) ; break ;
	case FX_P_EQ_MID_Q:   fx.SetEqBandQ(1,fl2fp(v)) ; break ;
	case FX_P_EQ_MID_EN:  fx.SetEqBandEnabled(1,v>=0.5f) ; break ;
	case FX_P_EQ_HI_FRQ:  fx.SetEqBandFreq(2,fl2fp(v)) ; break ;
	case FX_P_EQ_HI_GAI:  fx.SetEqBandGainDb(2,fl2fp(v)) ; break ;
	case FX_P_EQ_HI_Q:    fx.SetEqBandQ(2,fl2fp(v)) ; break ;
	case FX_P_EQ_HI_EN:   fx.SetEqBandEnabled(2,v>=0.5f) ; break ;
	case FX_P_CMP_THR:    fx.SetCompThresholdDb(fl2fp(v)) ; break ;
	case FX_P_CMP_RAT:    fx.SetCompRatio(fl2fp(v)) ; break ;
	case FX_P_CMP_KNE:    fx.SetCompKneeDb(fl2fp(v)) ; break ;
	case FX_P_CMP_ATK:    fx.SetCompAttackMs(fl2fp(v)) ; break ;
	case FX_P_CMP_REL:    fx.SetCompReleaseMs(fl2fp(v)) ; break ;
	case FX_P_CMP_MKU:    fx.SetCompMakeupDb(fl2fp(v)) ; break ;
	case FX_P_CMP_LINK:   fx.SetCompStereoLink(v>=0.5f) ; break ;
	case FX_P_CMP_SC:     fx.SetCompSoftClip(v>=0.5f) ; break ;
	case FX_P_CMP_BYP:    fx.SetCompBypass(v>=0.5f) ; break ;
	}
}

void MixerView::drawFxParamRow(int id,int x,int y,int col) {
	(void)col ;
	GUITextProperties props ;
	char buffer[16] ;
	const FxParamSpec &spec=kFxParams_[id] ;
	bool selected=(fxRowForId(id)==fxRow_) ;
	float v=fxGet(id) ;
	if (spec.vmax-spec.vmin<=1.5f) {
		sprintf(buffer,"%-9s%5.0f",spec.label,v) ;
	} else {
		sprintf(buffer,"%-9s",spec.label) ;
		DrawString(x,y,buffer,props) ;
		sprintf(buffer,spec.fmt,v) ;
		DrawString(x+10,y,buffer,props) ;
		SetColor(selected?CD_HILITE2:CD_NORMAL) ;
		props.invert_=selected ;
		DrawString(x,y,buffer,props) ;
		props.invert_=false ;
		return ;
	}
	SetColor(selected?CD_HILITE2:CD_NORMAL) ;
	props.invert_=selected ;
	DrawString(x,y,buffer,props) ;
	props.invert_=false ;
}

void MixerView::drawFxParamPage(FxPage page) {
	int y=2 ;
	GUITextProperties props ;
	// TREEFROG_FX_PAGES_V3 (PLAN_FX_REDESIGN_ES.md, Fase 11): the title shows
	// the page position [n/5] so the SELECT cycle is always visible.  RC3
	// (PLAN_RC3 point 2/26): the title is centered with UiDraw and the
	// permanent hint lines moved to HelpRegistry (SELECT+R1).
	char pageTitle[24] ;
	int pageNum=(int)page+1 ;
	switch(page) {
	case FX_PAGE_DELAY:  sprintf(pageTitle,"DELAY MASTER [%d/5]",pageNum) ; break ;
	case FX_PAGE_REVERB: sprintf(pageTitle,"REVERB MASTER [%d/5]",pageNum) ; break ;
	case FX_PAGE_EQ:     sprintf(pageTitle,"MASTER EQ [%d/5]",pageNum) ; break ;
	default:             sprintf(pageTitle,"MASTER COMP [%d/5]",pageNum) ; break ;
	}
	UiDraw::DrawCenteredTitleAt(*this,1,pageTitle) ;
	// TREEFROG_FX_MASTER_PAGES_RC2 (PLAN_FX_REDESIGN_ES.md, RC2 point 4): the
	// DELAY and REVERB pages are dedicated two-column menus (label / value)
	// with a clear hierarchy instead of the generic parameter list.
	if (page==FX_PAGE_DELAY) {
		drawDelayPage() ;
		return ;
	}
	if (page==FX_PAGE_REVERB) {
		drawReverbPage() ;
		return ;
	}
	// TREEFROG_EQ_MENU_V1 (PLAN_FX_REDESIGN_ES.md, Fase 12): the EQ page is a
	// dedicated exclusive menu with a banded LOW/MID/HIGH layout (EN/FRQ/GAI/Q
	// per band), so it does not use the generic parameter list.
	if (page==FX_PAGE_EQ) {
		drawEqPage() ;
		return ;
	}
	// TREEFROG_COMP_MENU_V1 (PLAN_FX_REDESIGN_ES.md, Fase 13): the COMP page
	// is a dedicated exclusive menu (BYP first, centered labeled rows, GR
	// meter always visible) instead of the generic parameter list.
	if (page==FX_PAGE_COMP) {
		drawCompPage() ;
		return ;
	}
	for (int i=0;i<FX_PARAM_COUNT;i++) {
		if (fxIdOnPage(i,page)) {
			drawFxParamRow(i,1,y,0) ;
			y++ ;
		}
	}
}

// TREEFROG_FX_MASTER_PAGES_RC2 (PLAN_FX_REDESIGN_ES.md, RC2 point 4):
// One two-column row of a DELAY/REVERB master page.  Hierarchy: the row
// label renders in CD_NORMAL, the value in CD_HILITE1; the edited row
// inverts with CD_HILITE2 so the current target always stands out.
void MixerView::drawMasterFxRow(const char *label,const char *value,
                                bool selected,int x,int y,int valueX) {
	GUITextProperties props ;
	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	DrawString(x,y,label,props) ;
	SetColor(selected?CD_HILITE2:CD_HILITE1) ;
	props.invert_=selected ;
	DrawString(valueX,y,value,props) ;
	props.invert_=false ;
	SetColor(CD_NORMAL) ;
}

void MixerView::drawDelayPage() {
	char buffer[16] ;
	const int x=2 ;
	const int valueX=x+12 ;
	static const char *labels[7]={"BYPASS","TIME","FEEDBACK","MIX",
	                              "WIDTH","PING/PONG","SATURATE"} ;
	static const int ids[7]={FX_P_DLY_BYP,FX_P_DLY_TIME,FX_P_DLY_FBK,
	                         FX_P_DLY_MIX,FX_P_DLY_WID,FX_P_DLY_PP,
	                         FX_P_DLY_SAT} ;
	for (int p=0;p<7;p++) {
		int id=ids[p] ;
		float v=fxGet(id) ;
		bool selected=(fxRowForId(id)==fxRow_) ;
		if (id==FX_P_DLY_BYP) {
			// RC4 P2: unified Bypass row via UiDraw (PLAN_RC4 section 12).
			UiDraw::DrawBypassRow(*this,x,2+p,v>=0.5f,selected) ;
			continue ;
		}
		switch(id) {
		case FX_P_DLY_TIME: sprintf(buffer,"%4.0f ms",v) ; break ;
		case FX_P_DLY_PP:
		case FX_P_DLY_SAT:  sprintf(buffer,"%s",v>=0.5f?"ON":"OFF") ; break ;
		default:            sprintf(buffer,"%.2f",v) ; break ;  // FBK/MIX/WID
		}
		drawMasterFxRow(labels[p],buffer,selected,x,2+p,valueX) ;
	}
}

void MixerView::drawReverbPage() {
	char buffer[16] ;
	const int x=2 ;
	const int valueX=x+12 ;
	static const char *labels[7]={"BYPASS","PREDELAY","DECAY","SIZE",
	                              "DAMPING","WIDTH","MODE"} ;
	static const int ids[7]={FX_P_RVB_BYP,FX_P_RVB_PRE,FX_P_RVB_DEC,
	                         FX_P_RVB_SIZ,FX_P_RVB_DMP,FX_P_RVB_WID,
	                         FX_P_RVB_MODE} ;
	for (int p=0;p<7;p++) {
		int id=ids[p] ;
		float v=fxGet(id) ;
		bool selected=(fxRowForId(id)==fxRow_) ;
		if (id==FX_P_RVB_BYP) {
			UiDraw::DrawBypassRow(*this,x,2+p,v>=0.5f,selected) ;
			continue ;
		}
		switch(id) {
		case FX_P_RVB_PRE:  sprintf(buffer,"%4.0f ms",v) ; break ;
		case FX_P_RVB_DEC:  sprintf(buffer,"%.2f s",v) ; break ;
		case FX_P_RVB_MODE: sprintf(buffer,"%s",v>=0.5f?"NORMAL":"ECO") ; break ;
		default:            sprintf(buffer,"%.2f",v) ; break ;  // SIZ/DMP/WID
		}
		drawMasterFxRow(labels[p],buffer,selected,x,2+p,valueX) ;
	}
}

// TREEFROG_EQ_MENU_V1 (PLAN_FX_REDESIGN_ES.md, Fase 12): dedicated EQ menu.
// The band labels come from the row position inside the band (EN/FRQ/GAIN/Q)
// because the band itself is announced by the LOW/MID/HIGH header row drawn
// by drawEqPage(); every param keeps its own selectable row so the current
// row is always unambiguous.  Values are right-aligned with units: frequencies
// in Hz, gain with an explicit sign in dB, enables as ON/OFF.
static const char *eqParamLabel(int id) {
	if (id==FX_P_EQ_BYP) return "BYPASS" ;
	if (id==FX_P_EQ_LOW_EN||id==FX_P_EQ_MID_EN||id==FX_P_EQ_HI_EN) return "EN" ;
	if (id==FX_P_EQ_LOW_FRQ||id==FX_P_EQ_MID_FRQ||id==FX_P_EQ_HI_FRQ) return "FRQ" ;
	if (id==FX_P_EQ_LOW_GAI||id==FX_P_EQ_MID_GAI||id==FX_P_EQ_HI_GAI) return "GAIN" ;
	return "Q" ;
}
static const char *eqBandName(int id) {
	if (id<=FX_P_EQ_LOW_Q) return "LOW" ;
	if (id<=FX_P_EQ_MID_Q) return "MID" ;
	return "HIGH" ;
}

void MixerView::drawEqRow(int id,int x,int y) {
	GUITextProperties props ;
	char buffer[16] ;
	bool selected=(fxRowForId(id)==fxRow_) ;
	bool on=(fxGet(id)>=0.5f) ;
	// RC4 P2: EQ Bypass renders through the unified row; the band EN toggles
	// keep the "[ ON ]" inline style (they are a per-band enable).
	if (id==FX_P_EQ_BYP) {
		UiDraw::DrawBypassRow(*this,x,y,on,selected) ;
		return ;
	}
	SetColor(selected?CD_HILITE2:CD_NORMAL) ;
	props.invert_=selected ;
	if (id==FX_P_EQ_LOW_EN||id==FX_P_EQ_MID_EN||id==FX_P_EQ_HI_EN) {
		sprintf(buffer,"%-6s[ %s ]",eqParamLabel(id),on?"ON":"OFF") ;
	} else if (fxUsesCurve(id)) {
		sprintf(buffer,"%-6s%6.0f Hz",eqParamLabel(id),fxGet(id)) ;
	} else if (id==FX_P_EQ_LOW_GAI||id==FX_P_EQ_MID_GAI||id==FX_P_EQ_HI_GAI) {
		sprintf(buffer,"%-6s%+5.1f dB",eqParamLabel(id),fxGet(id)) ;
	} else {
		sprintf(buffer,"%-6s%5.2f",eqParamLabel(id),fxGet(id)) ;
	}
	DrawString(x,y,buffer,props) ;
	props.invert_=false ;
	SetColor(CD_NORMAL) ;
}

void MixerView::drawEqPage() {
	// Title (row 1) is drawn by drawFxParamPage(); hints moved to the
	// HelpOverlay (SELECT+R1).  Rows: bypass at y=2, then each band is a
	// header line (y=4/10/16) followed by EN/FRQ/GAIN/Q.  All bands stay on
	// screen so editing never hides the rest of the EQ.
	const int x=13 ;
	GUITextProperties props ;
	drawEqRow(FX_P_EQ_BYP,x,2) ;
	const int bandBase[3]={FX_P_EQ_LOW_EN,FX_P_EQ_MID_EN,FX_P_EQ_HI_EN} ;
	for (int b=0;b<3;b++) {
		int yHeader=4+b*6 ;
		SetColor(CD_NORMAL) ;
		props.invert_=false ;
		DrawString(x,yHeader,eqBandName(bandBase[b]),props) ;
		for (int p=0;p<4;p++) {
			drawEqRow(bandBase[b]+p,x,yHeader+1+p) ;
		}
	}
}

// TREEFROG_COMP_MENU_V1 (PLAN_FX_REDESIGN_ES.md, Fase 13): dedicated COMP
// menu.  BYP is the first row so it is never off-screen; rows are centered
// with a fixed value column, ratio shows as x:1, booleans as ON/OFF, and the
// GR meter (a readout, not a selectable row) stays visible below the
// parameters.  No clipping indicator: the engine exposes no reliable real
// audio clip reading (GetRtViolations is buffer RT telemetry, must stay 0).
void MixerView::drawCompPage() {
	const int labelX=8 ;
	const int valueX=labelX+16 ;
	GUITextProperties props ;
	char buffer[20] ;
	const char *labels[9]={"Bypass","Threshold","Ratio","Knee",
	                       "Attack","Release","Makeup","Stereo Link","Soft Clip"} ;
	for (int p=0;p<9;p++) {
		int id=FX_P_CMP_BYP+p ;  // enum is BYP first, contiguous
		const FxParamSpec &spec=kFxParams_[id] ;
		bool selected=(fxRowForId(id)==fxRow_) ;
		float v=fxGet(id) ;
		// RC4 P2: COMP bypass through the unified row; the rest keep the
		// label/value two-column layout.
		if (p==0) {
			UiDraw::DrawBypassRow(*this,labelX,p+2,v>=0.5f,selected) ;
			props.invert_=false ;
			SetColor(CD_NORMAL) ;
			continue ;
		}
		SetColor(selected?CD_HILITE2:CD_NORMAL) ;
		props.invert_=selected ;
		if (spec.vmax-spec.vmin<=1.5f) {
			sprintf(buffer,"%s",v>=0.5f?"ON":"OFF") ;
		} else {
			switch (id) {
			case FX_P_CMP_RAT: sprintf(buffer,"%3.1f:1",v) ; break ;
			case FX_P_CMP_KNE: sprintf(buffer,"%5.1f dB",v) ; break ;
			case FX_P_CMP_ATK: sprintf(buffer,"%5.1f ms",v) ; break ;
			case FX_P_CMP_REL: sprintf(buffer,"%5.1f ms",v) ; break ;
			default:           sprintf(buffer,"%+5.1f dB",v) ; break ;  // THR, MKU
			}
		}
		DrawString(labelX,p+2,labels[p],props) ;
		DrawString(valueX,p+2,buffer,props) ;
		props.invert_=false ;
		SetColor(CD_NORMAL) ;
	}
	// GR meter: always visible below the parameters.
	float gr=fp2fl(FxEngine::FxEngine::GetInstance().GetCompGainReductionDb()) ;
	float mag=(gr<0.0f)?-gr:gr ;
	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	sprintf(buffer,"-%05.1f dB",mag) ;
	DrawString(labelX,12,"Gain Reduction",props) ;
	DrawString(valueX,12,buffer,props) ;
}

void MixerView::drawMixReturns() {
	// TREEFROG_FX_PAGES_V3 (PLAN_FX_REDESIGN_ES.md, Fase 9): the MIX page
	// shows the master FX returns (wet levels of the delay/reverb into the
	// master bus) instead of the old per-track D/R send readouts, which are
	// now per-instrument (Fase 6/7) and edited in InstrumentView.  The
	// return level being edited by UP/DOWN is highlighted.
	GUITextProperties props ;
	char buffer[16] ;
	FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
	int dly=fxReturnPercent(fx.GetDelayReturn()) ;
	int rvb=fxReturnPercent(fx.GetReverbReturn()) ;
	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	DrawString(0,16,"RET",props) ;
	SetColor((fxEditTarget_==1)?CD_HILITE2:CD_NORMAL) ;
	props.invert_=(fxEditTarget_==1) ;
	sprintf(buffer,"D:%3d",dly) ;
	DrawString(4,16,buffer,props) ;
	SetColor((fxEditTarget_==2)?CD_HILITE2:CD_NORMAL) ;
	props.invert_=(fxEditTarget_==2) ;
	sprintf(buffer,"R:%3d",rvb) ;
	DrawString(11,16,buffer,props) ;
	props.invert_=false ;
	SetColor(CD_NORMAL) ;
	DrawString(18,16,"FX RETURNS",props) ;
}

void MixerView::drawFxPages() {
	GUITextProperties props ;
	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	if (fxPage_==FX_PAGE_MIX) {
		// Classic mixer bars + per-track send readouts.
		const int x0=8 ;
		const int y0=2 ;
		const int height=11 ;
		const int dx=4 ;
		DrawString(0,y0,"CH",props) ;
		DrawString(0,y0+height+2,"VL",props) ;
		drawMasterBar(x0-dx,y0,height) ;
		for (int i=0;i<8;i++) {
			drawVolumeBar(i,x0+i*dx,y0,height) ;
		}
		drawMixReturns() ;
	} else {
		drawFxParamPage((FxPage)fxPage_) ;
	}
	drawNotes() ;
	drawMap() ;
}

void MixerView::DrawView() {

	Clear() ;

	GUITextProperties props ;
	GUIPoint pos=GetTitlePosition() ;
	GUIPoint anchor=GetAnchor() ;

	SetColor(CD_NORMAL) ;
	DrawString(pos._x,pos._y,"Mixer",props) ;
	DrawString(7,pos._y,"R+UP Song",props) ;

	Player *player=Player::GetInstance() ;
	DrawString(21,pos._y,(player->GetSequencerMode()==SM_SONG)?"Song":"Live",props) ;

	drawFxPages() ;
    
	if (player->IsRunning()) {
		OnPlayerUpdate(PET_UPDATE) ;
	} ;
} ;

void MixerView::OnPlayerUpdate(PlayerEventType ,unsigned int tick) {
	(void)tick ;

	Player *player=Player::GetInstance() ;

	// TREEFROG_MIXER_LIVE_VU_V1:
	// While the player is running, every transport tick requests a redraw so
	// the VU bars bounce in real time (AppWindow only flushes when dirty).
	if (player->IsRunning()) {
		isDirty_ = true;
		((AppWindow &)w_).SetDirty() ;
	}

	GUITextProperties props ;
	SetColor(CD_NORMAL) ;
	GUIPoint pos(30,0) ;
	
	if (player->Clipped()) {
        DrawString(pos._x,pos._y,"clip",props); 
    } else {
        DrawString(pos._x,pos._y,"----",props); 
    }
	char strbuffer[10] ;

	pos._y+=1 ;
	sprintf(strbuffer,"%3.3d%%",player->GetPlayedBufferPercentage()) ; 
	DrawString(pos._x,pos._y,strbuffer,props) ;

    System *sys=System::GetInstance() ;
    int batt=sys->GetBatteryLevel() ;
    if (batt>=0) {
		if (batt<90) {
			SetColor(CD_HILITE2) ;
			invertBatt_=!invertBatt_ ;
		} else {
			invertBatt_=false ;
		} ;
		props.invert_=invertBatt_ ;

	    pos._y+=1 ;
    	sprintf(strbuffer,"%3.3d",batt) ; 
	    DrawString(pos._x,pos._y,strbuffer,props) ;
    }
	SetColor(CD_NORMAL) ;
	props.invert_=false ;
    int time=int(player->GetPlayTime()) ;
    int mi=time/60 ;
    int se=time-mi*60 ;
	sprintf(strbuffer,"%2.2d:%2.2d",mi,se) ; 
	pos._y+=1 ;	
	DrawString(pos._x,pos._y,strbuffer,props) ;

    drawNotes() ;

} ;

void MixerView::OnFrameUpdate(unsigned long frameClock) {
	(void)frameClock ;

	// TREEFROG_MIXER_VU_SMOOTH_V1 (H38.7):
	// Once per frame, blend the raw audio peak into the display levels used
	// by the bars. Attack is instant (a note pops straight up), release is a
	// smooth per-frame exponential fall (~0.6^12 empties a full bar in about
	// 12 frames) so the bars never jump full->empty in one frame. This runs
	// every frame regardless of transport, so the fall is also smooth when
	// the player is stopped.
	{
		Player *player=Player::GetInstance() ;
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			float measured=player->GetChannelPeak(i) ;
			if (measured>vuDisplay_[i]) {
				vuDisplay_[i]=measured ;
			} else {
				vuDisplay_[i]*=0.6f ;
				if (vuDisplay_[i]<0.001f) vuDisplay_[i]=0.0f ;
			}
		}
	}

	// TREEFROG_MIXER_LIVE_VU_V2 (H38.7):
	// Frame updates are independent of Player transport (same as the USB-C
	// record meter): request a redraw every frame so the VU bars track the
	// channel activity in real time, including quick mute/stop decay.
	++frameRefreshDivider_ ;
	if (frameRefreshDivider_ >= 1) {
		frameRefreshDivider_ = 0 ;
		isDirty_ = true ;
		((AppWindow &)w_).SetDirty() ;
	}
} ;
