
#include "GrooveView.h"
#include "Application/Model/Groove.h"
#include "Application/Utils/char.h"
#include "Application/Views/BaseClasses/UiColors.h"
#include <string.h>

GrooveView::GrooveView(GUIWindow &w,ViewData *viewData):View(w,viewData) {
	position_=0 ;
	lastPosition_=0 ;
	grooveUndoCount_=0 ;
	grooveRedoCount_=0 ;
	memset(grooveUndo_,0,sizeof(grooveUndo_)) ;
	memset(grooveRedo_,0,sizeof(grooveRedo_)) ;
}

GrooveView::~GrooveView() {
} 

void GrooveView::updateCursor(int dir) {
	position_+=dir ;
	if (position_<0) position_+=16 ;
	if (position_>15) position_-=16 ;
	isDirty_=true;
} ;

void GrooveView::updateCursorValue(int val,bool sync) {
	pushGrooveUndo() ;
	unsigned char *grooveData=Groove::GetInstance()->GetGrooveData(viewData_->currentGroove_) ;
	int value=grooveData[position_] ;
	val+=value ;
	if (val<1) val=1 ;
	if (val>0xF) val=0xF ;
	grooveData[position_]=val ;
	isDirty_=true; 
} ;

// TREEFROG_GLOBAL_UNDO_GROOVE (Bacon 1.1.1): full 16-byte groove snapshots
// so that L1+X / R1+X act as a real global undo/redo in the groove editor.
void GrooveView::pushGrooveUndo() {
	Groove *gr=Groove::GetInstance() ;
	unsigned char *grooveData=gr->GetGrooveData(viewData_->currentGroove_) ;
	if (grooveUndoCount_>0) {
		bool unchanged=true ;
		for (int i=0;i<16;i++) {
			if (grooveUndo_[grooveUndoCount_-1].data[i]!=grooveData[i]) { unchanged=false ; break ; }
		}
		if (unchanged) return ;
	}
	if (grooveUndoCount_==kGrooveHistorySize) {
		for (int i=0;i<kGrooveHistorySize-1;i++) {
			memcpy(grooveUndo_[i].data,grooveUndo_[i+1].data,16) ;
		}
		grooveUndoCount_-- ;
	}
	memcpy(grooveUndo_[grooveUndoCount_].data,grooveData,16) ;
	grooveUndoCount_++ ;
}

bool GrooveView::GlobalUndo() {
	if (grooveUndoCount_==0) return false ;
	Groove *gr=Groove::GetInstance() ;
	unsigned char *grooveData=gr->GetGrooveData(viewData_->currentGroove_) ;
	grooveUndoCount_-- ;
	if (grooveRedoCount_==kGrooveHistorySize) {
		for (int i=0;i<kGrooveHistorySize-1;i++) {
			memcpy(grooveRedo_[i].data,grooveRedo_[i+1].data,16) ;
		}
		grooveRedoCount_-- ;
	}
	memcpy(grooveRedo_[grooveRedoCount_].data,grooveData,16) ;
	grooveRedoCount_++ ;
	memcpy(grooveData,grooveUndo_[grooveUndoCount_].data,16) ;
	isDirty_=true ;
	return true ;
}

bool GrooveView::GlobalRedo() {
	if (grooveRedoCount_==0) return false ;
	Groove *gr=Groove::GetInstance() ;
	unsigned char *grooveData=gr->GetGrooveData(viewData_->currentGroove_) ;
	if (grooveUndoCount_==kGrooveHistorySize) {
		for (int i=0;i<kGrooveHistorySize-1;i++) {
			memcpy(grooveUndo_[i].data,grooveUndo_[i+1].data,16) ;
		}
		grooveUndoCount_-- ;
	}
	memcpy(grooveUndo_[grooveUndoCount_].data,grooveData,16) ;
	grooveUndoCount_++ ;
	grooveRedoCount_-- ;
	memcpy(grooveData,grooveRedo_[grooveRedoCount_].data,16) ;
	isDirty_=true ;
	return true ;
}

void GrooveView::warpGroove(int dir) {
	int current=viewData_->currentGroove_ ;
	current+=dir ;
	if (current>=MAX_GROOVES) {
		current-=MAX_GROOVES ;
	} ;
	if (current<0) {
		current+=MAX_GROOVES ;
	} ;
	viewData_->currentGroove_=current ;
	isDirty_=true ;
} ;

void GrooveView::initCursorValue() {
	pushGrooveUndo() ;
	unsigned char *grooveData=Groove::GetInstance()->GetGrooveData(viewData_->currentGroove_) ;
	if (grooveData[position_]==NO_GROOVE_DATA) {
		grooveData[position_]=1 ;
	} ;
	isDirty_=true ;
} ;

void GrooveView::clearCursorValue() {
	pushGrooveUndo() ;
	unsigned char *grooveData=Groove::GetInstance()->GetGrooveData(viewData_->currentGroove_) ;
	grooveData[position_]=NO_GROOVE_DATA ;
	isDirty_=true ;
}	

void GrooveView::ProcessButtonMask(unsigned short mask,bool pressed) {

	if (!pressed) return ;
	
	Player *player=Player::GetInstance() ;

	if (mask&EPBM_B) {         
			if (mask&EPBM_LEFT) {
				warpGroove(-1) ;
			}
			if (mask&EPBM_RIGHT) {
				warpGroove(1) ;
			}
			if (mask&EPBM_DOWN) {
				warpGroove(-0x10) ;
			}
			if (mask&EPBM_UP) {
				warpGroove(0x10) ;
			}
			if (mask&EPBM_A) {
				clearCursorValue() ;
			} ;
	} else {

	  // A modifier
	  if (mask&EPBM_A) {         
			if (mask&EPBM_LEFT) {
				updateCursorValue(-1) ;
			}
			if (mask&EPBM_RIGHT) {
				updateCursorValue(1) ;
			}
			if (mask&EPBM_DOWN) {
				updateCursorValue(-1,true) ;
			}
			if (mask&EPBM_UP) {
				updateCursorValue(1,true) ;
			}
			if (mask==EPBM_A) {
				initCursorValue() ;
			} ;
	  } else {
		  // X Modifier (TREEFROG_NAV_X_DIR Bacon 1.1.1): quick navigation.
		  // X+UP/DOWN: page jump (4 cells), X+LEFT/RIGHT: big groove jump.
		  if (mask&EPBM_X) {
			  if (mask&EPBM_DOWN) updateCursor(4) ;
			  if (mask&EPBM_UP) updateCursor(-4) ;
			  if (mask&EPBM_LEFT) warpGroove(-0x10) ;
			  if (mask&EPBM_RIGHT) warpGroove(0x10) ;
		  } else {
		  // R Modifier

          	if (mask&EPBM_R) {
				if (mask&EPBM_DOWN) {
					ViewType vt=VT_PHRASE;
					ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
					SetChanged();
					NotifyObservers(&ve) ;
				}
				if (mask&EPBM_START) {
					player->OnStartButton(PM_PHRASE,viewData_->songX_,true,viewData_->chainRow_) ;
    			}			

	    	} else {
                // No modifier
    			if (mask&EPBM_DOWN) updateCursor(1) ;
    			if (mask&EPBM_UP) updateCursor(-1) ;
    			if (mask&EPBM_START) {
					player->OnStartButton(PM_PHRASE,viewData_->songX_,false,viewData_->chainRow_) ;
    			}
		    }
	  }
	  } 
	    
	}
} ;

void GrooveView::DrawView() {

	Clear() ;

	GUITextProperties props ;
	GUIPoint pos=GetTitlePosition() ;

// Draw title

	char title[40] ;

	// RC4 P3 (PLAN_RC4): page titles render with the semantic title role.
	SetColor(UiColors::Resolve(UI_COLOR_TITLE)) ;

	sprintf(title,"Groove: %2.2x",viewData_->currentGroove_) ;
	DrawString(pos._x,pos._y,title,props) ;

// Compute song grid location

	GUIPoint anchor=GetAnchor() ;
	
// Display row numbers

	char buffer[6] ;
	pos=anchor ;
	pos._x-=3 ;
	for (int j=0;j<16;j++) {
		((j/altRowNumber_)%2)?SetColor(CD_ROW):SetColor(CD_ROW2);
		hex2char(j,buffer) ;
		DrawString(pos._x,pos._y,buffer,props) ;
		pos._y++ ;
	}


// Display current groove

	pos=anchor ;

	SetColor(CD_NORMAL) ;

	unsigned char *grooveData=Groove::GetInstance()->GetGrooveData(viewData_->currentGroove_) ;
	for (int j=0;j<16;j++) {
		if (grooveData[j]!=NO_GROOVE_DATA) {
			hex2char(grooveData[j],buffer) ;
			buffer[3]=0 ;
		} else {
			strcpy(buffer,"--") ;
		} ;
		// TREEFROG_UI_GROOVE_SELECTION_SONG_STYLE
		// Usar la misma semántica visual validada en Song:
		// CD_HILITE2 + invert_ para la celda activa.
		if (j==position_) {
		    SetColor(CD_HILITE2);
		    props.invert_ = true;
		} else {
		    SetColor(CD_NORMAL);
		    props.invert_ = false;
		}
		DrawString(pos._x,pos._y,buffer,props) ;
		props.invert_ = false;
		SetColor(CD_NORMAL);
		pos._y++ ;
	}

	drawMap() ;
	drawNotes() ;
} ;

void GrooveView::OnPlayerUpdate(PlayerEventType ,unsigned int tick) {

	GUITextProperties props ;
	GUIPoint anchor=GetAnchor() ;
	GUIPoint pos ;

	pos._x=anchor._x-1 ;
	pos._y=anchor._y+lastPosition_ ;
	DrawString(pos._x,pos._y," ",props) ;
		
	Groove *gr=Groove::GetInstance() ;
	// Get current channel
	int channel=viewData_->songX_ ;

	int groove ;
	int groovepos ;

	gr->GetChannelData(channel,&groove,&groovepos) ;

	if (groove==viewData_->currentGroove_ &&
		viewData_->playMode_ != PM_AUDITION) {
		lastPosition_=groovepos ;
		pos._x=anchor._x-1 ;
		pos._y=anchor._y+lastPosition_ ;
        SetColor(CD_PLAY);
        DrawString(pos._x,pos._y,">",props);
        SetColor(CD_NORMAL);
	} ;

    drawNotes() ;
} ;

void GrooveView::OnFocus() {
} ;
