// TREEFROG_V42_NO_WHITE_BOX_UI
#include "MixerView.h"
#include "Application/Model/Mixer.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Project.h"
#include "Application/Views/UIController.h"
#include "Application/Views/ModalDialogs/InstrumentFxModal.h"
#include "Application/Audio/FxEngine/FxEngine.h"
#include "Application/Utils/fixed.h"
#include "Application/Utils/char.h"
#include "Application/AppWindow.h"
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <math.h>

// TREEFROG_FX_PAGES_PARAMS_V1 (PLAN_FX_REDESIGN_ES.md, Fase 4.3):
// Parameter table for the DELAY / REVERB / MASTER pages.  Each row exposes a
// master-bus parameter as a float in natural units (ms, %, dB, Hz, s, ratio).
// The UI edits the float and the setter clamps to the documented range; the
// DSP modules clamp again, so the page is always consistent.
struct FxParamSpec {
    const char *label ;        // short name shown on the page
    FxPage page ;              // which page owns this row
    float vmin ;               // minimum (natural units)
    float vmax ;               // maximum (natural units)
    const char *fmt ;          // printf format for the value
} ;

static const FxParamSpec kFxParams_[FX_PARAM_COUNT] = {
    // DELAY page
    { "DLY SND", FX_PAGE_DELAY,    0.0f,   1.0f,    "%5.2f" },  // FX_P_DLY_SEND
    { "DLY RET", FX_PAGE_DELAY,    0.0f,   1.0f,    "%5.2f" },  // FX_P_DLY_RET
    { "DLY TIM", FX_PAGE_DELAY,   10.0f, 2000.0f,   "%5.0f" },  // FX_P_DLY_TIME (ms)
    { "DLY FBK", FX_PAGE_DELAY,    0.0f,   0.98f,   "%5.2f" },  // FX_P_DLY_FBK
    { "DLY MIX", FX_PAGE_DELAY,    0.0f,   1.0f,    "%5.2f" },  // FX_P_DLY_MIX
    { "DLY WID", FX_PAGE_DELAY,    0.0f,   1.0f,    "%5.2f" },  // FX_P_DLY_WID
    { "DLY P/P", FX_PAGE_DELAY,    0.0f,   1.0f,    "%5.0f" },  // FX_P_DLY_PP (0/1)
    { "DLY SAT", FX_PAGE_DELAY,    0.0f,   1.0f,    "%5.0f" },  // FX_P_DLY_SAT (0/1)
    { "DLY BYP", FX_PAGE_DELAY,    0.0f,   1.0f,    "%5.0f" },  // FX_P_DLY_BYP (0/1)
    // REVERB page
    { "RVB SND", FX_PAGE_REVERB,   0.0f,   1.0f,    "%5.2f" },  // FX_P_RVB_SEND
    { "RVB RET", FX_PAGE_REVERB,   0.0f,   1.0f,    "%5.2f" },  // FX_P_RVB_RET
    { "RVB PRE", FX_PAGE_REVERB,   0.0f, 100.0f,    "%5.0f" },  // FX_P_RVB_PRE (ms)
    { "RVB DEC", FX_PAGE_REVERB,   0.2f,   8.0f,    "%5.2f" },  // FX_P_RVB_DEC (s)
    { "RVB SIZ", FX_PAGE_REVERB,   0.5f,   1.5f,    "%5.2f" },  // FX_P_RVB_SIZ
    { "RVB DMP", FX_PAGE_REVERB,   0.0f,   1.0f,    "%5.2f" },  // FX_P_RVB_DMP
    { "RVB WID", FX_PAGE_REVERB,   0.0f,   1.0f,    "%5.2f" },  // FX_P_RVB_WID
    { "RVB MOD", FX_PAGE_REVERB,   0.0f,   1.0f,    "%5.0f" },  // FX_P_RVB_MODE (0/1)
    { "RVB MIX", FX_PAGE_REVERB,   0.0f,   1.0f,    "%5.2f" },  // FX_P_RVB_MIX
    { "RVB BYP", FX_PAGE_REVERB,   0.0f,   1.0f,    "%5.0f" },  // FX_P_RVB_BYP (0/1)
    // MASTER page - EQ (bands 0..2)
    { "EQ  BYP", FX_PAGE_MASTER,   0.0f,   1.0f,    "%5.0f" },  // FX_P_EQ_BYP
    { "LO  FRQ", FX_PAGE_MASTER,  20.0f, 20000.0f,  "%5.0f" },  // FX_P_EQ_LOW_FRQ
    { "LO  GAI", FX_PAGE_MASTER, -12.0f,  12.0f,    "%5.1f" },  // FX_P_EQ_LOW_GAI
    { "LO  Q",   FX_PAGE_MASTER,   0.1f,  10.0f,    "%5.2f" },  // FX_P_EQ_LOW_Q
    { "LO  EN",  FX_PAGE_MASTER,   0.0f,   1.0f,    "%5.0f" },  // FX_P_EQ_LOW_EN
    { "MID FRQ", FX_PAGE_MASTER,  20.0f, 20000.0f,  "%5.0f" },  // FX_P_EQ_MID_FRQ
    { "MID GAI", FX_PAGE_MASTER, -12.0f,  12.0f,    "%5.1f" },  // FX_P_EQ_MID_GAI
    { "MID Q",   FX_PAGE_MASTER,   0.1f,  10.0f,    "%5.2f" },  // FX_P_EQ_MID_Q
    { "MID EN",  FX_PAGE_MASTER,   0.0f,   1.0f,    "%5.0f" },  // FX_P_EQ_MID_EN
    { "HI  FRQ", FX_PAGE_MASTER,  20.0f, 20000.0f,  "%5.0f" },  // FX_P_EQ_HI_FRQ
    { "HI  GAI", FX_PAGE_MASTER, -12.0f,  12.0f,    "%5.1f" },  // FX_P_EQ_HI_GAI
    { "HI  Q",   FX_PAGE_MASTER,   0.1f,  10.0f,    "%5.2f" },  // FX_P_EQ_HI_Q
    { "HI  EN",  FX_PAGE_MASTER,   0.0f,   1.0f,    "%5.0f" },  // FX_P_EQ_HI_EN
    // MASTER page - Compressor
    { "CMP THR", FX_PAGE_MASTER, -60.0f,   0.0f,    "%5.1f" },  // FX_P_CMP_THR (dB)
    { "CMP RAT", FX_PAGE_MASTER,   1.0f,  20.0f,    "%5.1f" },  // FX_P_CMP_RAT
    { "CMP KNE", FX_PAGE_MASTER,   0.0f,  12.0f,    "%5.1f" },  // FX_P_CMP_KNE (dB)
    { "CMP ATK", FX_PAGE_MASTER,   0.1f, 500.0f,    "%5.1f" },  // FX_P_CMP_ATK (ms)
    { "CMP REL", FX_PAGE_MASTER,   1.0f, 2000.0f,   "%5.0f" },  // FX_P_CMP_REL (ms)
    { "CMP MKU", FX_PAGE_MASTER,   0.0f,  24.0f,    "%5.1f" },  // FX_P_CMP_MKU (dB)
    { "CMP LNK", FX_PAGE_MASTER,   0.0f,   1.0f,    "%5.0f" },  // FX_P_CMP_LINK
    { "CMP SCL", FX_PAGE_MASTER,   0.0f,   1.0f,    "%5.0f" },  // FX_P_CMP_SC (softclip)
    { "CMP BYP", FX_PAGE_MASTER,   0.0f,   1.0f,    "%5.0f" },  // FX_P_CMP_BYP
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
	// TREEFROG_FX_PAGES_V1 (Fase 4.3): on the MIX page the R2-cycled edit
	// target selects whether UP/DOWN edits the channel volume or one of the
	// per-track FX sends.
	if (fxPage_==FX_PAGE_MIX) {
		if (fxEditTarget_==1) {
			Mixer::GetInstance()->NudgeChannelDelaySend(viewData_->mixerCol_,delta) ;
			isDirty_=true ;
			return ;
		}
		if (fxEditTarget_==2) {
			Mixer::GetInstance()->NudgeChannelReverbSend(viewData_->mixerCol_,delta) ;
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
		// R2 alone cycles the per-channel edit target on the MIX page:
		// VOL -> DLY -> RVB -> VOL (TREEFROG_FX_PAGES_V1, Fase 4.3).
		if (fxPage_==FX_PAGE_MIX) {
			fxEditTarget_=(fxEditTarget_+1)%3 ;
			isDirty_=true ;
			((AppWindow &)w_).SetDirty() ;
		}
		return ;
	}

	// Parameter pages (DELAY/REVERB/MASTER): UP/DOWN row, LEFT/RIGHT edit.
	if (fxPage_!=FX_PAGE_MIX) {
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
	InstrumentFxModal *modal=new InstrumentFxModal(*this,viewData_->mixerCol_) ;
	DoModal(modal) ;
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

int MixerView::fxRowForId(int id) const {
	int row=0 ;
	for (int i=0;i<id;i++) {
		if (kFxParams_[i].page==kFxParams_[id].page) row++ ;
	}
	return row ;
}

void MixerView::fxMoveRow(int delta) {
	int count=0 ;
	for (int i=0;i<FX_PARAM_COUNT;i++) {
		if (fxIdOnPage(i,(FxPage)fxPage_)) count++ ;
	}
	if (count<=0) return ;
	fxRow_+=delta ;
	if (fxRow_<0) fxRow_=count-1 ;
	if (fxRow_>=count) fxRow_=0 ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

void MixerView::fxEditRow(int delta,bool coarse) {
	int count=0 ;
	int targetId=-1 ;
	for (int i=0;i<FX_PARAM_COUNT;i++) {
		if (fxIdOnPage(i,(FxPage)fxPage_)) {
			if (count==fxRow_) targetId=i ;
			count++ ;
		}
	}
	if (targetId<0) return ;
	const FxParamSpec &spec=kFxParams_[targetId] ;
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

void MixerView::fxEditChannelTarget(int delta) {
	(void)delta ;
	// Currently a toggle through VOL/DLY/RVB; delta ignored (kept for symmetry).
	fxEditTarget_=(fxEditTarget_+1)%3 ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

float MixerView::fxGet(int id) const {
	FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
	switch(id) {
	case FX_P_DLY_SEND: return fp2fl(fx.GetDelaySend()) ;
	case FX_P_DLY_RET:  return fp2fl(fx.GetDelayReturn()) ;
	case FX_P_DLY_TIME: return fp2fl(fx.GetDelayTimeMs()) ;
	case FX_P_DLY_FBK:  return fp2fl(fx.GetDelayFeedback()) ;
	case FX_P_DLY_MIX:  return fp2fl(fx.GetDelayMix()) ;
	case FX_P_DLY_WID:  return fp2fl(fx.GetDelayWidth()) ;
	case FX_P_DLY_PP:   return fx.GetDelayPingPong()?1.0f:0.0f ;
	case FX_P_DLY_SAT:  return fx.GetDelaySaturation()?1.0f:0.0f ;
	case FX_P_DLY_BYP:  return fx.GetDelayBypass()?1.0f:0.0f ;
	case FX_P_RVB_SEND: return fp2fl(fx.GetReverbSend()) ;
	case FX_P_RVB_RET:  return fp2fl(fx.GetReverbReturn()) ;
	case FX_P_RVB_PRE:  return fp2fl(fx.GetReverbPredelayMs()) ;
	case FX_P_RVB_DEC:  return fp2fl(fx.GetReverbDecay()) ;
	case FX_P_RVB_SIZ:  return fp2fl(fx.GetReverbSize()) ;
	case FX_P_RVB_DMP:  return fp2fl(fx.GetReverbDamping()) ;
	case FX_P_RVB_WID:  return fp2fl(fx.GetReverbWidth()) ;
	case FX_P_RVB_MODE: return (float)fx.GetReverbMode() ;
	case FX_P_RVB_MIX:  return fp2fl(fx.GetReverbMix()) ;
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
	case FX_P_DLY_SEND: fx.SetDelaySend(fl2fp(v)) ; break ;
	case FX_P_DLY_RET:  fx.SetDelayReturn(fl2fp(v)) ; break ;
	case FX_P_DLY_TIME: fx.SetDelayTimeMs(fl2fp(v)) ; break ;
	case FX_P_DLY_FBK:  fx.SetDelayFeedback(fl2fp(v)) ; break ;
	case FX_P_DLY_MIX:  fx.SetDelayMix(fl2fp(v)) ; break ;
	case FX_P_DLY_WID:  fx.SetDelayWidth(fl2fp(v)) ; break ;
	case FX_P_DLY_PP:   fx.SetDelayPingPong(v>=0.5f) ; break ;
	case FX_P_DLY_SAT:  fx.SetDelaySaturation(v>=0.5f) ; break ;
	case FX_P_DLY_BYP:  fx.SetDelayBypass(v>=0.5f) ; break ;
	case FX_P_RVB_SEND: fx.SetReverbSend(fl2fp(v)) ; break ;
	case FX_P_RVB_RET:  fx.SetReverbReturn(fl2fp(v)) ; break ;
	case FX_P_RVB_PRE:  fx.SetReverbPredelayMs(fl2fp(v)) ; break ;
	case FX_P_RVB_DEC:  fx.SetReverbDecay(fl2fp(v)) ; break ;
	case FX_P_RVB_SIZ:  fx.SetReverbSize(fl2fp(v)) ; break ;
	case FX_P_RVB_DMP:  fx.SetReverbDamping(fl2fp(v)) ; break ;
	case FX_P_RVB_WID:  fx.SetReverbWidth(fl2fp(v)) ; break ;
	case FX_P_RVB_MODE: fx.SetReverbMode((int)v) ; break ;
	case FX_P_RVB_MIX:  fx.SetReverbMix(fl2fp(v)) ; break ;
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
	SetColor(CD_NORMAL) ;
	GUITextProperties props ;
	DrawString(1,1,page==FX_PAGE_DELAY?"DELAY MASTER":
	           page==FX_PAGE_REVERB?"REVERB MASTER":"MASTER FX",props) ;
	for (int i=0;i<FX_PARAM_COUNT;i++) {
		if (fxIdOnPage(i,page)) {
			drawFxParamRow(i,1,y,0) ;
			y++ ;
		}
	}
	// GR meter (compressor) shown on the MASTER page.
	if (page==FX_PAGE_MASTER) {
		float gr=fp2fl(FxEngine::FxEngine::GetInstance().GetCompGainReductionDb()) ;
		SetColor(CD_NORMAL) ;
		props.invert_=false ;
		DrawString(22,2,"GR",props) ;
		char buffer[16] ;
		sprintf(buffer,"%5.1f dB",gr) ;
		DrawString(22,3,buffer,props) ;
		DrawString(22,4,"A+UP/DN x10",props) ;
		DrawString(22,5,"A+L/R x1",props) ;
	}
	DrawString(1,22,"UP/DN row  L/R edit  A coarse",props) ;
	DrawString(1,23,"SELECT page  START play",props) ;
}

void MixerView::drawMixSends() {
	// TREEFROG_FX_PAGES_V1 (Fase 4.3): under each channel bar show the DLY and
	// RVB send values; the row being edited by UP/DOWN is marked.
	const int x0=8 ;
	const int dx=4 ;
	GUITextProperties props ;
	char buffer[8] ;
	Mixer *mixer=Mixer::GetInstance() ;
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		int dly=mixer->GetChannelDelaySend(i) ;
		int rvb=mixer->GetChannelReverbSend(i) ;
		int x=x0+i*dx ;
		bool selected=(!masterSelected_ && i==viewData_->mixerCol_) ;
		// DLY row (under the volume number)
		SetColor((selected&&fxEditTarget_==1)?CD_HILITE2:CD_NORMAL) ;
		props.invert_=(selected&&fxEditTarget_==1) ;
		sprintf(buffer,"D%2d",dly) ;
		DrawString(x-1,16,buffer,props) ;
		// RVB row
		SetColor((selected&&fxEditTarget_==2)?CD_HILITE2:CD_NORMAL) ;
		props.invert_=(selected&&fxEditTarget_==2) ;
		sprintf(buffer,"R%2d",rvb) ;
		DrawString(x-1,17,buffer,props) ;
		props.invert_=false ;
	}
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
		drawMixSends() ;
		DrawString(4,19,"A+UP/DN x10  A+L/R x1",props) ;
		DrawString(4,20,"L/R ch  L->MST  R1+B mute",props) ;
		DrawString(4,21,"START play  R1+A solo  R2+A FX",props) ;
		DrawString(4,23,"R2 edit VOL/DLY/RVB  SELECT FX pages",props) ;
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
