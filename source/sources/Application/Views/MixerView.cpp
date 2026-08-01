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

MixerView::MixerView(GUIWindow &w,ViewData *viewData):View(w,viewData) {
	clipboard_.active_=false ;
	clipboard_.data_=0 ;
	invertBatt_=false;
	soloMode_=false;
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
	int x=viewData_->mixerCol_ ;
	x+=dx ;
	if (x<0) x=0 ;
	if (x>7) x=7 ;
	viewData_->mixerCol_=x ;
	isDirty_=true;
}

void MixerView::updateVolume(int delta) {
	Mixer::GetInstance()->NudgeChannelVolume(viewData_->mixerCol_,delta) ;
	isDirty_=true ;
}

void MixerView::toggleMute() {
	UIController::GetInstance()->ToggleMute(viewData_->mixerCol_,viewData_->mixerCol_) ;
	isDirty_=true ;
}

void MixerView::switchSoloMode() {
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
	bool selected=(channel==viewData_->mixerCol_) ;
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

	// TREEFROG_VU_METERS_V3 (H38.5): dense contiguous LED segments, one per
	// character cell (2 per row, no gaps), in the style of the USB-C record
	// meter. The bar reacts in real time to the actual channel output
	// (MixerService::GetChannelPeak): the tip lights in accent color above
	// the sustained volume fill.
	int totalCells=2*height ;
	int filledCells=(volume*totalCells+99)/100 ;
	if (filledCells>totalCells) filledCells=totalCells ;
	int peakCells=int(peak*float(totalCells)) ;
	if (peakCells>totalCells) peakCells=totalCells ;
	for (int row=0;row<height;row++) {
		for (int c=0;c<2;c++) {
			int cellFromBottom=totalCells-(2*row+c) ;
			bool on=(cellFromBottom<=filledCells) ;
			bool vu=((!on)&&(cellFromBottom<=peakCells)) ;
			bool tip=((on)&&(cellFromBottom==peakCells)&&(peakCells>0)&&(peakCells<=filledCells)) ;
			if (vu||tip) {
				SetColor(selected?CD_HILITE1:CD_HILITE2) ;
				props.invert_=true ;
			} else if (on) {
				SetColor(selected?CD_HILITE2:CD_NORMAL) ;
				props.invert_=true ;
			} else {
				SetColor(muted?CD_BORDER:CD_HILITE1) ;
				props.invert_=false ;
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

	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	DrawString(x-1,y,"MST",props) ;

	// TREEFROG_VU_METERS_V3 (H38.5): dense contiguous LED segments,
	// master-wide, same style as the record meter (see drawVolumeBar).
	int totalCells=2*height ;
	int filledCells=(volume*totalCells+99)/100 ;
	if (filledCells>totalCells) filledCells=totalCells ;
	int peakCells=int(peak*float(totalCells)) ;
	if (peakCells>totalCells) peakCells=totalCells ;
	for (int row=0;row<height;row++) {
		for (int c=0;c<2;c++) {
			int cellFromBottom=totalCells-(2*row+c) ;
			bool on=(cellFromBottom<=filledCells) ;
			bool vu=((!on)&&(cellFromBottom<=peakCells)) ;
			bool tip=((on)&&(cellFromBottom==peakCells)&&(peakCells>0)&&(peakCells<=filledCells)) ;
			if (vu||tip) {
				SetColor(CD_HILITE2) ;
				props.invert_=true ;
			} else if (on) {
				SetColor(CD_NORMAL) ;
				props.invert_=true ;
			} else {
				SetColor(CD_HILITE1) ;
				props.invert_=false ;
			}
			DrawString(x+c,y+1+row," ",props) ;
		}
	}

	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	sprintf(buffer,"%3d",volume) ;
	DrawString(x-1,y+height+2,buffer,props) ;
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

	const int x0=6 ;
	const int y0=anchor._y+1 ;
	const int height=11 ;
	const int dx=4 ;

	SetColor(CD_NORMAL) ;
	DrawString(0,y0,"CH",props) ;
	DrawString(0,y0+height+2,"VL",props) ;

	for (int i=0;i<8;i++) {
		drawVolumeBar(i,x0+i*dx,y0,height) ;
	}
	drawMasterBar(x0+8*dx,y0,height) ;

	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	DrawString(4,y0+height+4,"A+UP/DN x10  A+L/R x1",props) ;
	DrawString(4,y0+height+5,"L/R channel  R1+B mute",props) ;
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
