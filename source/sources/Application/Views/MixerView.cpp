// TREEFROG_V42_NO_WHITE_BOX_UI
#include "MixerView.h"
#include "Application/Model/Mixer.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Project.h"
#include "Application/Views/UIController.h"
#include "Application/Views/ModalDialogs/InstrumentFxModal.h"
#include "Application/Utils/char.h"
#include "Application/AppWindow.h"
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <math.h>

// TREEFROG_MIXER_VU_DB_SCALE_V1 (H38.7):
// Map a linear output level (0..1) to a perceptual (dBFS) bar fill so the
// bars track the real audible volume: 0dB=full, and the fill drops as the
// volume falls (50dB display range). A linear map makes anything above a few
// dB stay "full white", which is what made the bars look static before.
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
	MixerService *ms=MixerService::GetInstance() ;
	float peak=ms->GetChannelPeak(channel) ;
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
	int filledCells=int(mixVULevel(peak)*float(totalCells)) ;
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
	MixerService *ms=MixerService::GetInstance() ;
	float peak=ms->GetMasterPeak() ;
	GUITextProperties props ;
	char buffer[8] ;

	// TREEFROG_MIXER_MASTER_BAR_V1 (H38.7):
	// Master (MST) bar drawn live on the left of the channel bars, in cyan so
	// it stands out from the white channel fills. When selected it lights
	// purple like a selected channel.
	SetColor(masterSelected_?CD_HILITE2:CD_PLAY) ;
	props.invert_=false ;
	DrawString(x-1,y,"MST",props) ;

	int totalCells=2*height ;
	int filledCells=int(mixVULevel(peak)*float(totalCells)) ;
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

	const int x0=8 ;
	const int y0=anchor._y+1 ;
	const int height=11 ;
	const int dx=4 ;

	SetColor(CD_NORMAL) ;
	DrawString(0,y0,"CH",props) ;
	DrawString(0,y0+height+2,"VL",props) ;

	drawMasterBar(x0-dx,y0,height) ;
	for (int i=0;i<8;i++) {
		drawVolumeBar(i,x0+i*dx,y0,height) ;
	}

	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	DrawString(4,y0+height+4,"A+UP/DN x10  A+L/R x1",props) ;
	DrawString(4,y0+height+5,"L/R ch  L->MST  R1+B mute",props) ;
	DrawString(4,y0+height+6,"START play  R1+A solo  R2+A FX",props) ;

	drawNotes() ;
	drawMap() ;
    
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

	// TREEFROG_MIXER_LIVE_VU_V2 (H38.6):
	// Frame updates are independent of Player transport (same as the USB-C
	// record meter): request a redraw every few frames so the VU bars keep
	// moving even while the player is stopped (recent buffer levels decay).
	++frameRefreshDivider_ ;
	if (frameRefreshDivider_ >= 3) {
		frameRefreshDivider_ = 0 ;
		isDirty_ = true ;
		((AppWindow &)w_).SetDirty() ;
	}
} ;
