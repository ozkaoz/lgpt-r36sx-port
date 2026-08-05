// TREEFROG_V42_NO_WHITE_BOX_UI
#include "View.h"
#include "System/Console/Trace.h"
#include "Application/Player/Player.h"
#include "Application/Utils/char.h"
#include "Application/AppWindow.h"
#include "Application/Model/Config.h"
#include "ModalView.h"

bool View::initPrivate_=false ;

int View::margin_=0 ;
int View::songRowCount_; //=21 sets screen height among other things
bool View::miniLayout_=false ;
int View::altRowNumber_ = 4;

View::View(GUIWindow &w,ViewData *viewData):
	w_(w),
	hasFocus_(false),
	modalView_(0),
	modalViewCallback_(0),
	suspendedModal_(0),
	suspendedModalCallback_(0)
{
  if (!initPrivate_) 
  {
	   GUIRect rect=w.GetRect() ;
     miniLayout_=(rect.Width()<320);
	   View::margin_=0 ;
		songRowCount_ = miniLayout_ ? 16:22; // 22 is row display count among other things

		const char *altRowStr = Config::GetInstance()->GetValue("ALTROWNUMBER");
		if (altRowStr) {
			altRowNumber_ = atoi(altRowStr);
		}

     initPrivate_=true ;
  }
	mask_=0 ;
	inputEventWhen_=0 ;
	viewMode_=VM_NORMAL ;
	locked_=false ;
	viewData_=viewData;
	NOTIFICATION_TIMEOUT = 1000;
	displayNotification_ = "";
} ;

GUIPoint View::GetAnchor() {
	int width=40 ;
	int height=30 ;
	return GUIPoint((width-SONG_CHANNEL_COUNT*3)/2+2,(height-View::songRowCount_)/2) ;
}

GUIPoint View::GetTitlePosition() {
#ifndef PLATFORM_CAANOO
	return GUIPoint(0,0) ;
#else
	return GUIPoint(0,1) ;
#endif
} ;

bool View::Lock() {
	if (locked_) return false ;
	locked_=true ;
	return true ;
} ;

void View::WaitForObject() {
	while (locked_) {} ;
}

void View::Unlock() {
	locked_=false ;
}

void View::drawMap() {
    if (!miniLayout_) {
        static const char *tf_v30_sidebar_marker = "TREEFROG_V32_SIDEBAR_VISUAL_TABLE_SHIFT";
        (void)tf_v30_sidebar_marker;

        GUIPoint anchor = GetAnchor();
        (void)anchor;
        // TREEFROG_UI_MAP_LAYOUT_V3:
        // Sidebar moved to the bottom-left corner (x=1-4, y=27-29) with a
        // two-column margin so it is not glued to the edge. The area is
        // collision-free on every view: drawNotes() occupies x=10-33 at
        // y=27-29, all grids stay inside the anchor area (x=7-33, y<=25),
        // the Song status block sits at the top-right and MixerView keeps
        // its channel bars and legend above y=26.
        const int baseX = 2;
        const int baseY = 27;

        GUITextProperties props;
        props.invert_ = true;

        // TREEFROG_UI_MAP_LAYOUT:
        // Visual layout validated on R36SX: P G / SCPI / M TT.
        // M sits below S to advertise Song -> Mixer with R+DOWN.
        // This only changes the sidebar drawing; navigation remains in each view.
        SetColor(CD_HILITE1);
        DrawString(baseX,     baseY,     "P G ", props);
        DrawString(baseX,     baseY + 1, "SCPI", props);
        DrawString(baseX,     baseY + 2, "M TT", props);

        int ax = baseX;
        int ay = baseY + 1;
        const char *glyph = "S";

        switch (viewType_) {
            case VT_PROJECT:
                ax = baseX;
                ay = baseY;
                glyph = "P";
                break;
            case VT_GROOVE:
                ax = baseX + 2;
                ay = baseY;
                glyph = "G";
                break;
            case VT_SONG:
                ax = baseX;
                ay = baseY + 1;
                glyph = "S";
                break;
            case VT_CHAIN:
                ax = baseX + 1;
                ay = baseY + 1;
                glyph = "C";
                break;
            case VT_PHRASE:
                ax = baseX + 2;
                ay = baseY + 1;
                glyph = "P";
                break;
            case VT_INSTRUMENT:
                ax = baseX + 3;
                ay = baseY + 1;
                glyph = "I";
                break;
            case VT_MIXER:
                ax = baseX;
                ay = baseY + 2;
                glyph = "M";
                break;
            case VT_TABLE:
                ax = baseX + 2;
                ay = baseY + 2;
                glyph = "T";
                break;
            case VT_TABLE2:
                ax = baseX + 3;
                ay = baseY + 2;
                glyph = "T";
                break;
            default:
                ax = baseX;
                ay = baseY + 1;
                glyph = "S";
                break;
        }

        // TREEFROG_UI_MAP_FOCUS:
        // Celda activa validada con semántica clásica: CD_HILITE2 + invert_.
        // Mantener equivalente visual a la barra inferior seleccionada.
        SetColor(CD_HILITE2);
        props.invert_ = true;
        DrawString(ax, ay, glyph, props);
        props.invert_ = false;
    }
}

void View::drawNotes() {

    if (!miniLayout_) {

		GUIPoint anchor=GetAnchor() ;
		int initialX = View::margin_+10 ;
		int initialY = anchor._y+23 ;
		GUIPoint pos(initialX,initialY) ;
		GUITextProperties props ;

        Player *player=Player::GetInstance() ;
		
		//column banger refactor
		props.invert_ = true;
        for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			if (i==viewData_->songX_) {
				SetColor(CD_HILITE2) ;
			} else {
				SetColor(CD_HILITE1) ;
			}
			if (player->IsRunning() && viewData_->playMode_ != PM_AUDITION) {
				DrawString(pos._x,pos._y,player->GetPlayedNote(i),props) ; //row for the note values
				pos._y++ ;
				DrawString(pos._x,pos._y,player->GetPlayedOctive(i),props) ; //row for the octive values
				pos._y++ ;
				DrawString(pos._x,pos._y,player->GetPlayedInstrument(i),props) ; //draw instrument number
			} else {
				DrawString(pos._x,pos._y,"  ",props) ; //row for the note values
				pos._y++ ;
				DrawString(pos._x,pos._y,"  ",props) ; //row for the octive values
				pos._y++ ;
				DrawString(pos._x,pos._y,"  ",props) ; //draw instrument number
			}
			pos._y = initialY ;
			pos._x+= 3;
		}
     }
}

void View::DoModal(ModalView *view,ModalViewCallback cb) {
	modalView_=view ;
	modalView_->OnFocus() ;
	modalViewCallback_=cb ;
	isDirty_=true ;
} ;

void View::ReplaceModal(ModalView *view,ModalViewCallback cb) {
    if (modalView_) {
        SAFE_DELETE(modalView_);
    }
    modalView_ = view;
    modalViewCallback_ = cb;
    if (modalView_) modalView_->OnFocus();
    isDirty_ = true;
};

bool View::HasModal() const {
    return modalView_ != 0;
};

// RC4 P1 (PLAN_RC4 section 11.3): push a modal on top of the current one.
// The active modal is suspended (kept alive, hidden) and restored by
// RestoreSuspendedModal when the pushed modal finishes.  Returns true if a
// modal was actually suspended (i.e. Help opened over an open dialog).
bool View::PushModal(ModalView *view, ModalViewCallback cb) {
    bool hadModal = (modalView_ != 0);
    if (hadModal) {
        suspendedModal_ = modalView_;
        suspendedModalCallback_ = modalViewCallback_;
        // TREEFROG_CHOPPER_HELP_V1 (Bacon 1.1.1 V13): let the suspended
        // modal stop drawing its private overlay (chopper waveform) while
        // the pushed window is shown on top.
        if (suspendedModal_) suspendedModal_->OnSuspend();
    }
    modalView_ = view;
    modalViewCallback_ = cb;
    if (modalView_) modalView_->OnFocus();
    isDirty_ = true;
    return hadModal;
};

void View::RestoreSuspendedModal() {
    if (suspendedModal_) {
        modalView_ = suspendedModal_;
        modalViewCallback_ = suspendedModalCallback_;
        suspendedModal_ = 0;
        suspendedModalCallback_ = 0;
        if (modalView_) {
            modalView_->OnRestore();
            modalView_->OnFocus();
        }
        isDirty_ = true;
    }
}


void View::Redraw() {
	if (modalView_) {
		if (isDirty_) {
			DrawView() ;
		}
		modalView_->Redraw() ;
	} else {
		DrawView() ;
	}
	isDirty_=false ;
} ;

void View::SetDirty(bool isDirty) {
	isDirty_=true ;
} ;

void View::ProcessButton(unsigned short mask, bool pressed, long eventWhen) {
	inputEventWhen_=eventWhen ;
	isDirty_=false ;
	if (modalView_) {
		modalView_->ProcessButton(mask,pressed,eventWhen);
		if (modalView_->isDirty_) {
			isDirty_=true;
		}
		if (modalView_->IsFinished()) {
			// process callback sending the modal dialog
			if (modalViewCallback_) {
				modalViewCallback_(*this,*modalView_) ;
			}
			SAFE_DELETE(modalView_) ;
			// RC4 P1 (PLAN_RC4 section 11.3): if Help was pushed over a
			// dialog, restore the suspended modal now that Help is done.
			RestoreSuspendedModal() ;
			isDirty_=true ;
		}
	} else {
		// TREEFROG_GLOBAL_UNDO_V2 (Bacon 1.1.1): global combos are offered to
		// the view first.  Pure L1+X / R1+X / A+B only; any extra button falls
		// through to the view's own handling.  A view claims a combo by
		// returning true from GlobalUndo()/GlobalRedo()/GlobalResetOption();
		// when it returns false the mask reaches ProcessButtonMask so legacy
		// behaviors (Song A+B clear, Phrase/Table A+B cut, ...) keep working.
		// TREEFROG_GLOBAL_UNDO_V3 (Bacon 1.1.1): the runtime can keep extra
		// bits latched into the atomic mask (same note as the Song Y+X
		// handling), so the combos are matched with a mask instead of exact
		// equality.  The other shoulder button is still excluded so L+X and
		// R+X cannot both match when both shoulders are held; any other
		// latched bit (dpad rocker, ...) no longer kills the combo.
		if (pressed) {
			if ((mask & (EPBM_L | EPBM_X)) == (EPBM_L | EPBM_X) &&
			    (mask & EPBM_R) == 0) {
				if (GlobalUndo()) return;
			} else if ((mask & (EPBM_R | EPBM_X)) == (EPBM_R | EPBM_X) &&
			           (mask & EPBM_L) == 0) {
				if (GlobalRedo()) return;
			} else if ((mask & (EPBM_A | EPBM_B)) == (EPBM_A | EPBM_B)) {
				if (GlobalResetOption()) return;
			}
		}
		ProcessButtonMask(mask, pressed);
	}
	if (isDirty_) ((AppWindow &)w_).SetDirty() ;
} ;

void View::UpdateActiveModal(PlayerEventType type,
                             unsigned int currentTick) {
    if (!modalView_) return;

    modalView_->OnPlayerUpdate(type, currentTick);

    if (modalView_->isDirty_) {
        isDirty_ = true;
        ((AppWindow &)w_).SetDirty();
    }
}

void View::UpdateActiveModalFrame(unsigned long frameClock) {
    if (!modalView_) return;

    modalView_->OnFrameUpdate(frameClock);

    if (modalView_->isDirty_) {
        isDirty_ = true;
        ((AppWindow &)w_).SetDirty();
    }

    /*
     * A sampler exit can now be committed by the frame-local raw-input FSM,
     * outside ProcessButton(). Finalize the modal through the same callback
     * path used by ordinary queued events.
     */
    if (modalView_->IsFinished()) {
        if (modalViewCallback_)
            modalViewCallback_(*this, *modalView_);

        SAFE_DELETE(modalView_);
        // RC4 P1 (PLAN_RC4 section 11.3): restore the modal suspended under
        // a pushed Help overlay.
        RestoreSuspendedModal();
        isDirty_ = true;
        ((AppWindow &)w_).SetDirty();
    }
}

void View::Clear() {
	((AppWindow &)w_).Clear() ;
}

void View::SetColor(ColorDefinition cd) {
	((AppWindow &)w_).SetColor(cd) ;
} ;

void View::ClearRect(int x,int y,int w,int h) {
	GUIRect rect(x,y,(x+w),(y+h)) ;
	w_.ClearRect(rect) ;
} ;

void View::DrawString(int x,int y,const char *txt,GUITextProperties &props) {
	GUIPoint pos(x,y) ;
	w_.DrawString(txt,pos,props) ;
} ;

/*
	Displays the saved notification for 1 second
*/
void View::EnableNotification() {
	if ((SDL_GetTicks() - notificationTime_) <= NOTIFICATION_TIMEOUT) {
		SetColor(CD_NORMAL);
		GUITextProperties props;
        int xOffset = 4;
        DrawString(xOffset, notiDistY_, displayNotification_.c_str(), props);
    } else {
		displayNotification_ = "";
	}
}

/*
    Set displayed notification
    Saves the current time
    Optionally set display y offset if not in a project (default == 2)
    Allows negative offsets, use with care!
*/
void View::SetNotification(const char *notification, int offset) {
    notificationTime_ = SDL_GetTicks();
    displayNotification_ = notification;
    notiDistY_ = offset;
    isDirty_ = true;
}
