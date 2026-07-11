// TREEFROG_V42_NO_WHITE_BOX_UI
#include "MixerView.h"
#include "Application/Model/Mixer.h"
#include "Application/Model/Project.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Views/UIController.h"
#include "Application/Utils/char.h"
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>

MixerView::MixerView(GUIWindow &w,ViewData *viewData):View(w,viewData) {
	clipboard_.active_=false ;
	clipboard_.data_=0 ;
	invertBatt_=false;
	soloMode_=false;
	tempoFlashFrames_=0;
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

void MixerView::updatePan(int delta) {
	Mixer::GetInstance()->NudgeChannelPan(viewData_->mixerCol_,delta) ;
	isDirty_=true ;
}

void MixerView::centerPan() {
	Mixer::GetInstance()->SetChannelPan(viewData_->mixerCol_,0) ;
	isDirty_=true ;
}

void MixerView::updateTempo(int delta) {
	if (!viewData_ || !viewData_->project_) return ;
	Variable *v=viewData_->project_->FindVariable(VAR_TEMPO) ;
	if (!v) return ;
	int value=v->GetInt()+delta ;
	if (value<40) value=40 ;
	if (value>400) value=400 ;
	v->SetInt(value) ;
	SyncMaster::GetInstance()->SetTempo(viewData_->project_->GetTempo()) ;
	tempoFlashFrames_=12 ;
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

	// Y modifier: edit project tempo without leaving Mixer.
	if (mask&EPBM_Y) {
		if (mask&EPBM_UP) updateTempo(1) ;
		if (mask&EPBM_DOWN) updateTempo(-1) ;
		if (mask&EPBM_RIGHT) updateTempo(10) ;
		if (mask&EPBM_LEFT) updateTempo(-10) ;
		return ;
	}

	// R modifier: keep the port-wide convention.
	// R+B toggles mute on the selected channel.
	// R+A toggles solo on the selected channel.
	if (mask&EPBM_R) {
		if (mask&EPBM_B) {
			toggleMute() ;
			return ;
		}
		if (mask&EPBM_A) {
			switchSoloMode() ;
			return ;
		}
		if (mask&EPBM_LEFT) {
			updatePan(-5) ;
			return ;
		}
		if (mask&EPBM_RIGHT) {
			updatePan(5) ;
			return ;
		}
		if (mask&EPBM_DOWN) {
			centerPan() ;
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

void MixerView::formatPan(int pan,char *buffer) {
	if (pan==0) {
		sprintf(buffer," C ") ;
	} else if (pan<0) {
		sprintf(buffer,"L%02d",-pan) ;
	} else {
		sprintf(buffer,"R%02d",pan) ;
	}
}

void MixerView::drawMeterBar(int peak,bool clipped,int x,int y,int height,bool selected) {
	GUITextProperties props ;
	int filled=(peak*height+99)/100 ;
	if (filled<0) filled=0 ;
	if (filled>height) filled=height ;
	for (int row=0;row<height;row++) {
		bool on=((height-row)<=filled) ;
		if (on) {
			SetColor(clipped?CD_HILITE2:(selected?CD_HILITE1:CD_NORMAL)) ;
			props.invert_=true ;
		} else {
			SetColor(selected?CD_BORDER:CD_HILITE1) ;
			props.invert_=false ;
		}
		DrawString(x,y+row,"  ",props) ;
	}
	props.invert_=false ;
}

void MixerView::drawMasterMeter(int x,int y,int height) {
	Player *player=Player::GetInstance() ;
	GUITextProperties props ;
	int l=player->GetMasterPeakLeft() ;
	int r=player->GetMasterPeakRight() ;
	bool clipped=player->Clipped() || (l>=100) || (r>=100) ;

	SetColor(clipped?CD_HILITE2:CD_NORMAL) ;
	DrawString(x,y-1,"ML MR",props) ;
	drawMeterBar(l,clipped,x,y,height,false) ;
	drawMeterBar(r,clipped,x+3,y,height,false) ;
	SetColor(clipped?CD_HILITE2:CD_NORMAL) ;
	DrawString(x,y+height,clipped?"!  !":"L  R",props) ;
	SetColor(CD_NORMAL) ;
}

void MixerView::drawTempoLabel(int x,int y) {
	GUITextProperties props ;
	char tempoStr[8] ;
	sprintf(tempoStr,"T%03d",viewData_->project_->GetTempo()) ;
	if (tempoFlashFrames_>0) {
		SetColor(CD_HILITE2) ;
		props.invert_=true ;
	} else {
		SetColor(CD_NORMAL) ;
		props.invert_=false ;
	}
	DrawString(x,y,tempoStr,props) ;
	props.invert_=false ;
	SetColor(CD_NORMAL) ;
}

void MixerView::drawVolumeBar(int channel,int x,int y,int height) {
	Mixer *mixer=Mixer::GetInstance() ;
	Player *player=Player::GetInstance() ;
	int volume=mixer->GetChannelVolume(channel) ;
	int peak=player->GetChannelPeak(channel) ;
	int pan=mixer->GetChannelPan(channel) ;
	bool clipped=player->IsChannelClipped(channel) || (peak>=100) ;
	bool selected=(channel==viewData_->mixerCol_) ;
	bool muted=player->IsChannelMuted(channel) ;
	GUITextProperties props ;
	char buffer[8] ;
	char hex[3] ;
	char panStr[4] ;

	hex2char(channel,hex) ;
	if (clipped) {
		SetColor(CD_HILITE2) ;
		props.invert_=true ;
	} else if (selected) {
		SetColor(CD_HILITE1) ;
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

	// U2.50: channel strips are stable volume faders.  The master meter
	// remains dynamic, which makes the mixer easier to read on the small LCD.
	// Clip state still comes from the real channel peak and is shown explicitly.
	drawMeterBar(volume,clipped,x,y+1,height,selected) ;

	SetColor(clipped?CD_HILITE2:(selected?CD_HILITE1:(muted?CD_BORDER:CD_NORMAL))) ;
	props.invert_=selected ;
	sprintf(buffer,"%3d",volume) ;
	DrawString(x-1,y+height+2,buffer,props) ;
	props.invert_=false ;

	formatPan(pan,panStr) ;
	SetColor((pan==0)?(selected?CD_HILITE1:CD_NORMAL):CD_HILITE2) ;
	props.invert_=selected ;
	DrawString(x-1,y+height+3,panStr,props) ;
	props.invert_=false ;

	SetColor(clipped?CD_HILITE2:(muted?CD_BORDER:((pan!=0)?CD_HILITE2:CD_NORMAL))) ;
	if (clipped) {
		DrawString(x,y+height+4,"!",props) ;
	} else if (muted) {
		DrawString(x,y+height+4,"M",props) ;
	} else if (pan<0) {
		DrawString(x,y+height+4,"<",props) ;
	} else if (pan>0) {
		DrawString(x,y+height+4,">",props) ;
	} else {
		DrawString(x,y+height+4," ",props) ;
	}
	SetColor(CD_NORMAL) ;
}

void MixerView::DrawView() {

	Clear() ;

	GUITextProperties props ;
	GUIPoint pos=GetTitlePosition() ;

	SetColor(CD_NORMAL) ;
	DrawString(pos._x,pos._y,"Mixer",props) ;
	DrawString(7,pos._y,"R+UP Song",props) ;

	Player *player=Player::GetInstance() ;
	DrawString(21,pos._y,(player->GetSequencerMode()==SM_SONG)?"Song":"Live",props) ;
	drawTempoLabel(25,pos._y) ;

	// U2.50: put operation hints above the strips, then use the lower area
	// for larger, calmer faders.
	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	DrawString(1,2,"A+UD/LR Vol   Y+UD/LR Tempo",props) ;
	DrawString(1,3,"R+L/R Pan    R+DN Center",props) ;
	DrawString(1,4,"START Play   R+B Mute R+A Solo",props) ;

	const int x0=7 ;
	const int y0=6 ;
	const int height=10 ;
	const int dx=4 ;

	SetColor(CD_NORMAL) ;
	DrawString(0,y0,"MS",props) ;
	drawMasterMeter(0,y0+2,height-1) ;
	DrawString(0,y0+height+2,"VL",props) ;
	DrawString(0,y0+height+3,"PN",props) ;
	DrawString(x0-3,y0,"CH",props) ;

	for (int i=0;i<8;i++) {
		drawVolumeBar(i,x0+i*dx,y0,height) ;
	}

	drawNotes() ;
    
	if (player->IsRunning()) {
		OnPlayerUpdate(PET_UPDATE) ;
	} ;
}

void MixerView::OnPlayerUpdate(PlayerEventType ,unsigned int tick) {
	(void)tick ;

	Player *player=Player::GetInstance() ;

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
	GUIPoint anchor=GetAnchor() ;
	const int x0=7 ;
	const int y0=6 ;
	const int height=10 ;
	const int dx=4 ;
	for (int i=0;i<8;i++) {
		drawVolumeBar(i,x0+i*dx,y0,height) ;
	}
	drawMasterMeter(0,y0+2,height-1) ;
	drawTempoLabel(25,0) ;
	if (tempoFlashFrames_>0) tempoFlashFrames_-- ;

} ;
