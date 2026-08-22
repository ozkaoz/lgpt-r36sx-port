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
	modalViewCallback_(0),
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
	// F2: golden modalView_ = view; modalView_->OnFocus() -- el
	// NavigationController actualiza el tope del stack y da el foco.
	nav_.Open(view) ;
	modalViewCallback_=cb ;
	isDirty_=true ;
} ;

void View::ReplaceModal(ModalView *view,ModalViewCallback cb) {
	nav_.Replace(view);
	modalViewCallback_ = cb;
	isDirty_ = true;
};

bool View::HasModal() const {
    return nav_.HasModal();
};

// RC4 P1 (PLAN_RC4 section 11.3): push a modal on top of the current one.
// The active modal is suspended (kept alive, hidden) and restored by
// RestoreSuspendedModal when the pushed modal finishes.  Returns true if a
// modal was actually suspended (i.e. Help opened over an open dialog).
bool View::PushModal(ModalView *view, ModalViewCallback cb) {
	// F2: golden suspendedModal_ = modalView_; -- el NavigationController
	// apila el activo como suspendido, le avisa (OnSuspend) y da el foco al
	// nuevo (OnFocus). El callback tipado acompaña al modal aqui.
	bool hadModal = nav_.Push(view);
	if (hadModal) {
		suspendedModalCallback_ = modalViewCallback_;
	}
	modalViewCallback_ = cb;
	isDirty_ = true;
	return hadModal;
};

void View::RestoreSuspendedModal() {
	if (!nav_.Suspended()) return;
	// F2: golden modalView_ = suspendedModal_; -- el controller restaura el
	// tope (OnRestore + OnFocus) y aqui se recupera el callback suspendido.
	nav_.RestoreSuspended() ;
	modalViewCallback_ = suspendedModalCallback_;
	suspendedModalCallback_ = 0;
	isDirty_ = true;
}


void View::Redraw() {
	if (nav_.HasModal()) {
		// TREEFROG_HELP_OVER_SUSPENDED_MODAL_V1 (Bacon 1.1.1 V15): draw the
		// suspended modal (the chopper) under the pushed overlay (Help) so
		// the user still sees the screen they left instead of the bare base
		// view underneath.  It is painted after the base view (which clears
		// the screen) and before the top modal.  The suspended modal stays
		// input-dead; only the top modal receives buttons.
		if (isDirty_) {
			DrawView() ;
		}
		if (nav_.Suspended()) SuspendedModal()->Redraw() ;
		ActiveModal()->Redraw() ;
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
	if (nav_.HasModal()) {
		ModalView *mv = ActiveModal();
		mv->ProcessButton(mask,pressed,eventWhen);
		if (mv->isDirty_) {
			isDirty_=true;
		}
		if (mv->IsFinished()) {
			// process callback sending the modal dialog
			if (modalViewCallback_) {
				modalViewCallback_(*this,*mv) ;
			}
			// F2: golden SAFE_DELETE(modalView_) + RestoreSuspendedModal()
			// (RC4 P1: si Help fue empujado sobre un dialogo, restaurar el
			// suspendido ahora que Help termino).
			nav_.CloseActive() ;
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
				// TREEFROG_GLOBAL_UNDO_V4 (Bacon 1.1.1): the claimed combo
				// returns early and would otherwise skip the SetDirty at the
				// bottom of this method, so the undo/redo change was never
				// repainted until the next unrelated input event.  Request a
				// redraw explicitly before returning.
				if (GlobalUndo()) {
					((AppWindow &)w_).SetDirty();
					return;
				}
			} else if ((mask & (EPBM_R | EPBM_X)) == (EPBM_R | EPBM_X) &&
			           (mask & EPBM_L) == 0) {
				if (GlobalRedo()) {
					((AppWindow &)w_).SetDirty();
					return;
				}
			} else if ((mask & (EPBM_A | EPBM_B)) == (EPBM_A | EPBM_B)) {
				if (GlobalResetOption()) {
					((AppWindow &)w_).SetDirty();
					return;
				}
			}
		}
		ProcessButtonMask(mask, pressed);
	}
	if (isDirty_) ((AppWindow &)w_).SetDirty() ;
} ;

void View::UpdateActiveModal(PlayerEventType type,
                             unsigned int currentTick) {
    if (!nav_.HasModal()) return;

    ModalView *mv = ActiveModal();
    mv->OnPlayerUpdate(type, currentTick);

    if (mv->isDirty_) {
        isDirty_ = true;
        ((AppWindow &)w_).SetDirty();
    }
}

void View::UpdateActiveModalFrame(unsigned long frameClock) {
    if (!nav_.HasModal()) return;

    ModalView *mv = ActiveModal();
    mv->OnFrameUpdate(frameClock);

    if (mv->isDirty_) {
        isDirty_ = true;
        ((AppWindow &)w_).SetDirty();
    }

    /*
     * A sampler exit can now be committed by the frame-local raw-input FSM,
     * outside ProcessButton(). Finalize the modal through the same callback
     * path used by ordinary queued events.
     */
    if (mv->IsFinished()) {
        if (modalViewCallback_)
            modalViewCallback_(*this, *mv);

        // F2: golden SAFE_DELETE(modalView_) + RestoreSuspendedModal()
        // (RC4 P1: restore the modal suspended under a pushed Help overlay).
        nav_.CloseActive();
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
	int x0 = x; int y0 = y; int x1 = x+w; int y1 = y+h;
	if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
	if (x1 > 40) x1 = 40; if (y1 > 30) y1 = 30;
	if (x0 >= x1 || y0 >= y1) return;
	GUIRect rect(x0,y0,x1,y1) ;
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
