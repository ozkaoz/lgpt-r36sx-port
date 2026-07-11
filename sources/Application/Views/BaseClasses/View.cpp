// TREEFROG_V42_NO_WHITE_BOX_UI
#include "View.h"
#include "System/Console/Trace.h"
#include "Application/Player/Player.h"
#include "Application/Utils/char.h"
#include "Application/AppWindow.h"
#include "Application/Model/Config.h"
#include "ModalView.h"
#include "Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.h"
#include <stdio.h>

#define TREEFROG_AU9V_BRIDGE_DECLS_IN_CPP 1

#ifndef TREEFROG_UAC2_BRIDGE_H
extern "C" {
int TreeFrogUac2Bridge_GetDriverMode(void);
const char *TreeFrogUac2Bridge_SetDriverMode(int mode);
int TreeFrogUac2Bridge_GetDriverModeCount(void);
const char *TreeFrogUac2Bridge_GetDriverModeNameByIndex(int mode);
const char *TreeFrogUac2Bridge_GetDriverModeDescriptionByIndex(int mode);
int TreeFrogUac2Bridge_IsDriverModeSelectable(int mode);
const char *TreeFrogUac2Bridge_GetUsbStateText(void);
}
#endif


bool View::initPrivate_=false ;


class TreeFrogGlobalAudioDriverModal : public ModalView {
public:
    TreeFrogGlobalAudioDriverModal(View &view) : ModalView(view), selected_(TreeFrogUac2Bridge_GetDriverMode()) {
        if (selected_ < 0 || selected_ >= TreeFrogUac2Bridge_GetDriverModeCount()) selected_ = 1;
    }
    virtual ~TreeFrogGlobalAudioDriverModal() {}
    virtual void DrawView() {
        View::Clear();
        GUITextProperties props;
        props.invert_ = false;
        SetColor(CD_HILITE2);
        props.invert_ = true;
        View::DrawString(0, 0, " Audio Driver                           ", props);
        props.invert_ = false;
        SetColor(CD_NORMAL);
        View::DrawString(1, 2, TreeFrogUac2Bridge_GetUsbStateText(), props);
        View::DrawString(1, 4, "R2+SELECT: close/open", props);
        for (int i = 0; i < TreeFrogUac2Bridge_GetDriverModeCount(); ++i) {
            props.invert_ = (i == selected_);
            SetColor((i == selected_) ? CD_HILITE2 : CD_NORMAL);
            char line[41];
            snprintf(line, sizeof(line), "%c %-17s %s",
                     (i == selected_) ? '>' : ' ',
                     TreeFrogUac2Bridge_GetDriverModeNameByIndex(i),
                     TreeFrogUac2Bridge_IsDriverModeSelectable(i) ? "" : "[AU10 locked]");
            View::DrawString(1, 7 + i * 3, line, props);
            props.invert_ = false;
            SetColor(CD_NORMAL);
            View::DrawString(3, 8 + i * 3, TreeFrogUac2Bridge_GetDriverModeDescriptionByIndex(i), props);
        }
        props.invert_ = false;
        SetColor(CD_NORMAL);
        View::DrawString(1, 25, "A select | B back | UP/DOWN move", props);
        View::DrawString(1, 27, "Default: USB_OUT_AUTO_MUTE", props);
    }
    virtual void ProcessButtonMask(unsigned short mask, bool pressed) {
        if (!pressed) return;
        if ((mask & EPBM_R2) && (mask & EPBM_SELECT)) { EndModal(-1); return; }
        if (mask & EPBM_UP) { selected_--; if (selected_ < 0) selected_ = TreeFrogUac2Bridge_GetDriverModeCount() - 1; isDirty_ = true; }
        else if (mask & EPBM_DOWN) { selected_++; if (selected_ >= TreeFrogUac2Bridge_GetDriverModeCount()) selected_ = 0; isDirty_ = true; }
        else if (mask & EPBM_A) { EndModal(selected_); }
        else if (mask & EPBM_B) { EndModal(-1); }
    }
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int) {}
    virtual void OnFocus() { isDirty_ = true; }
    virtual bool BlocksUnderlyingDraw() { return true; }
private:
    int selected_;
};

static void TreeFrogGlobalAudioDriverCallback(View &v, ModalView &dialog) {
    int rc = dialog.GetReturnCode();
    if (rc < 0) return;
    if (!TreeFrogUac2Bridge_IsDriverModeSelectable(rc)) { v.SetNotification("USB capture needs AU10"); return; }
    const char *mode = TreeFrogUac2Bridge_SetDriverMode(rc);
    char msg[96]; snprintf(msg, sizeof(msg), "Audio Driver: %s", mode); v.SetNotification(msg);
}

int View::margin_=0 ;
int View::songRowCount_; //=21 sets screen height among other things
bool View::miniLayout_=false ;
int View::altRowNumber_ = 4;

View::View(GUIWindow &w,ViewData *viewData):
	w_(w),
	modalView_(0),
	modalViewCallback_(0),
	hasFocus_(false)
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
        const int baseX = View::margin_;
        const int baseY = anchor._y;

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


void View::PlayerUpdate(PlayerEventType type, unsigned int currentTick) {
    if (modalView_) {
        modalView_->OnPlayerUpdate(type, currentTick);
        if (modalView_->isDirty_) {
            isDirty_ = true;
            ((AppWindow &)w_).SetDirty();
        }
        return;
    }
    OnPlayerUpdate(type, currentTick);
}

void View::Redraw() {
	if (modalView_) {
		if (isDirty_ && !modalView_->BlocksUnderlyingDraw()) {
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

void View::ProcessButton(unsigned short mask, bool pressed) {
	isDirty_=false ;
    // global audio driver hotkey AU9V
    if (pressed && (mask & EPBM_R2) && (mask & EPBM_SELECT)) {
        if (modalView_) {
            SAFE_DELETE(modalView_);
            modalViewCallback_ = 0;
            isDirty_ = true;
            ((AppWindow &)w_).SetDirty();
            return;
        } else {
            DoModal(new TreeFrogGlobalAudioDriverModal(*this), TreeFrogGlobalAudioDriverCallback);
            ((AppWindow &)w_).SetDirty();
            return;
        }
    }
	if (modalView_) {
		modalView_->ProcessButton(mask,pressed);
		if (modalView_->isDirty_) {
			isDirty_=true;
		}
		if (modalView_->IsFinished()) {
			// process callback sending the modal dialog
			if (modalViewCallback_) {
				modalViewCallback_(*this,*modalView_) ;
			}
			SAFE_DELETE(modalView_) ;
			isDirty_=true ;
		}
	} else {
		ProcessButtonMask(mask,pressed);
	}
	if (isDirty_) ((AppWindow &)w_).SetDirty() ;
} ;

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
