// TREEFROG_V42_NO_WHITE_BOX_UI
#include "MixerView.h"
#include "Application/Model/Mixer.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Project.h"
#include "Application/Model/ProjectDatas.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Views/UIController.h"
#include "Application/Views/BaseClasses/UiDraw.h"
#include "Application/Views/BaseClasses/ModalView.h"
#include "Application/Audio/FxEngine/FxEngine.h"
#include "Application/Utils/fixed.h"
#include "Application/Utils/char.h"
#include "Application/AppWindow.h"
#include "Application/UI/Input/ChordResolver.h"
#if defined(PLATFORM_TREEFROG)
#include "Adapters/TREEFROG/GUI/TreeFrogGUIWindowImp.h"
#endif
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <math.h>

// TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): MIX/FX edit history kinds for the
// global L1+X / R1+X combos (MixEdit::kind).
// TREEFROG_MIXER_ACTION_MENU_V1 (Bacon 1.1.1 V13): ME_SOFTCLIP /
// ME_SOFTCLIPGAIN track the master limiter/clip edits of the L1+A menu.
enum { ME_VOL, ME_PAN, ME_MASTERVOL, ME_DLYRET, ME_RVBRET, ME_FX,
       ME_SOFTCLIP, ME_SOFTCLIPGAIN, ME_MUTE, ME_SOLO } ;

// TREEFROG_MIXER_ACTION_MENU_V1 (Bacon 1.1.1 V13):
// L1+A on the mixer opens this menu.  On the master bar it edits the master
// limiter (softclip) and clip gain and jumps to the DELAY/REVERB/EQ/COMP
// pages; on a channel bar it jumps to the FILTER/BITCRUSHER/PLAYBACK/
// FX SENDS/AUTOMATION sections of the instrument editor.  The menu is a
// real modal; pendingAction_ is read by MixerActionMenuApplyCallback after
// the modal closes (the action runs once the menu is gone, so the switch to
// the Instrument view cannot leave a stale modal behind).
class MixerActionMenuModal : public ModalView {
  public:
    MixerActionMenuModal(MixerView &view)
        : ModalView(view), mixer_(view),
          pendingAction_(0), item_(0),
          masterMenu_(view.masterSelected_) {}
    virtual ~MixerActionMenuModal() {}

    virtual void DrawView() ;
    virtual void ProcessButtonMask(unsigned short mask, bool pressed) ;
    virtual void OnFocus() { isDirty_ = true; }
    virtual void OnPlayerUpdate(PlayerEventType, unsigned int) {}

    MixerView &mixer_ ;
    int pendingAction_ ;

  private:
    int item_ ;
    bool masterMenu_ ;
} ;

void MixerActionMenuApplyCallback(View &view, ModalView &dialog) ;

void MixerActionMenuModal::DrawView() {
    GUITextProperties props ;
    props.invert_ = false ;
    int rowCount = masterMenu_ ? 6 : 5 ;
    SetWindow(26, rowCount + 3) ;

    char title[32] ;
    if (masterMenu_) {
        snprintf(title, sizeof(title), "MASTER MENU") ;
    } else {
        snprintf(title, sizeof(title), "TRACK %02X MENU",
                 mixer_.viewData_->mixerCol_) ;
    }
    SetColor(CD_HILITE1) ;
    int tx = (GetWindowWidth() - (int)strlen(title)) / 2 ;
    if (tx < 0) tx = 0 ;
    DrawString(tx, 0, title, props) ;

    Project *project = mixer_.viewData_->project_ ;
    int channel = mixer_.viewData_->mixerCol_ ;
    const int labelX = 1 ;
    const int valueX = 14 ;
    for (int i = 0; i < rowCount; i++) {
        int row = 1 + i ;
        bool selected = (i == item_) ;
        char label[16] ;
        char value[16] ;
        if (masterMenu_) {
            switch (i) {
            case 0:
                snprintf(label, sizeof(label), "LIMITER") ;
                if (project) {
                    int idx = project->GetSoftclip() ;
                    if (idx < 0) idx = 0 ;
                    if (idx > 4) idx = 4 ;
                    snprintf(value, sizeof(value), "%s", softclipStates[idx]) ;
                } else snprintf(value, sizeof(value), "-") ;
                break ;
            case 1:
                snprintf(label, sizeof(label), "CLIP GAIN") ;
                if (project) {
                    int idx = project->GetSoftclipGain() ;
                    if (idx < 0) idx = 0 ;
                    if (idx > 1) idx = 1 ;
                    snprintf(value, sizeof(value), "%s",
                             softclipGainStates[idx]) ;
                } else snprintf(value, sizeof(value), "-") ;
                break ;
            default:
                snprintf(label, sizeof(label), "FX %s",
                         i == 2 ? "DELAY" :
                         i == 3 ? "REVERB" :
                         i == 4 ? "EQ" : "COMP") ;
                snprintf(value, sizeof(value), "A: open") ;
                break ;
            }
        } else {
            static const char *sections[5] = {
                "FILTER", "BITCRUSHER", "PLAYBACK", "FX SENDS",
                "AUTOMATION" } ;
            snprintf(label, sizeof(label), "%s", sections[i]) ;
            snprintf(value, sizeof(value), "A: open") ;
        }
        SetColor(CD_NORMAL) ;
        props.invert_ = false ;
        DrawString(labelX, row, label, props) ;
        SetColor(selected ? CD_HILITE2 : CD_HILITE1) ;
        props.invert_ = selected ;
        DrawString(valueX, row, value, props) ;
        props.invert_ = false ;
        SetColor(CD_NORMAL) ;
    }

    (void)channel ;
    SetColor(CD_NORMAL) ;
    props.invert_ = false ;
    DrawString(1, rowCount + 1, "UP/DN move  A open", props) ;
    DrawString(1, rowCount + 2,
               masterMenu_ ? "L/R edit  B close" : "B close", props) ;
}

void MixerActionMenuModal::ProcessButtonMask(unsigned short mask,
                                             bool pressed) {
    if (!pressed) return ;
    if (mask & EPBM_B) {
        EndModal(0) ;
        return ;
    }
    int rowCount = masterMenu_ ? 6 : 5 ;
    if (mask & EPBM_UP) {
        item_-- ;
        if (item_ < 0) item_ = rowCount - 1 ;
        return ;
    }
    if (mask & EPBM_DOWN) {
        item_++ ;
        if (item_ >= rowCount) item_ = 0 ;
        return ;
    }
    // L/R edits the value rows of the master menu (limiter / clip gain);
    // every edit is recorded in the mix undo history (L1+X restores).
    if (masterMenu_ && item_ <= 1 &&
        (mask & (EPBM_LEFT | EPBM_RIGHT)) != 0) {
        Project *project = mixer_.viewData_->project_ ;
        if (!project) return ;
        int delta = (mask & EPBM_RIGHT) ? 1 : -1 ;
        if (item_ == 0) {
            int idx = project->GetSoftclip() + delta ;
            if (idx < 0) idx = 0 ;
            if (idx > 4) idx = 4 ;
            if (idx == project->GetSoftclip()) return ;
            mixer_.pushMixUndo(ME_SOFTCLIP, -1,
                               (float)project->GetSoftclip(),
                               (float)idx) ;
            Variable *var = project->FindVariable(VAR_SOFTCLIP) ;
            if (var) var->SetInt(idx, false) ;
            MixerService::GetInstance()->SetSoftclip(
                idx, project->GetSoftclipGain()) ;
        } else {
            int idx = project->GetSoftclipGain() + delta ;
            if (idx < 0) idx = 0 ;
            if (idx > 1) idx = 1 ;
            if (idx == project->GetSoftclipGain()) return ;
            mixer_.pushMixUndo(ME_SOFTCLIPGAIN, -1,
                               (float)project->GetSoftclipGain(),
                               (float)idx) ;
            Variable *var = project->FindVariable(VAR_SOFTCLIP_GAIN) ;
            if (var) var->SetInt(idx, false) ;
            MixerService::GetInstance()->SetSoftclip(
                project->GetSoftclip(), idx) ;
        }
        isDirty_ = true ;
        return ;
    }
    if (mask & EPBM_A) {
        // 1..4 = master FX pages (DELAY..COMP); 101+ = track sections.
        if (masterMenu_ && item_ >= 2) {
            pendingAction_ = item_ - 1 ;
        } else if (!masterMenu_) {
            pendingAction_ = 101 + item_ ;
        }
        EndModal(pendingAction_) ;
        return ;
    }
}

void MixerView::JumpToFxPage(FxPage page) {
    if (page < FX_PAGE_MIX || page >= FX_PAGE_COUNT) return ;
    fxPage_ = page ;
    fxRow_ = 0 ;
    isDirty_ = true ;
    ((AppWindow &)w_).SetDirty() ;
}

void MixerActionMenuApplyCallback(View &view, ModalView &dialog) {
    MixerActionMenuModal *menu = (MixerActionMenuModal *)&dialog ;
    MixerView &mixer = menu->mixer_ ;
    int action = menu->pendingAction_ ;
    menu->pendingAction_ = 0 ;
    if (action >= 101) {
        // Track section: open the Instrument view for the hovered channel
        // and land on the requested section (FILTER/BITCRUSHER/PLAYBACK/
        // FX SENDS/AUTOMATION).
        int section = action - 101 ;
        if (section < 0 || section > 4) return ;
        static const unsigned int hintIds[5] = {
            SIP_FILTMIX, SIP_CRUSH, SIP_INTERPOLATION, SIP_DRY,
            SIP_TABLEAUTO } ;
        int channel = mixer.viewData_->mixerCol_ ;
        if (channel >= 0 && channel < SONG_CHANNEL_COUNT) {
            mixer.viewData_->currentInstrument_ = channel ;
            mixer.viewData_->instrumentFocusHint_ = hintIds[section] ;
            ViewType vt = VT_INSTRUMENT ;
            ViewEvent ve(VET_SWITCH_VIEW, &vt) ;
            mixer.SetChanged() ;
            mixer.NotifyObservers(&ve) ;
        }
    } else if (action >= 1 && action <= 4) {
        mixer.JumpToFxPage((FxPage)(FX_PAGE_DELAY + action - 1)) ;
    }
}

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
    { "LO  GAI", FX_PAGE_EQ,      -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQ_LOW_GAI
    { "LO  Q",   FX_PAGE_EQ,       0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQ_LOW_Q
    { "MID EN",  FX_PAGE_EQ,       0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_EQ_MID_EN
    { "MID FRQ", FX_PAGE_EQ,      20.0f, 20000.0f,1000.0f, "%5.0f" },  // FX_P_EQ_MID_FRQ
    { "MID GAI", FX_PAGE_EQ,      -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQ_MID_GAI
    { "MID Q",   FX_PAGE_EQ,       0.1f,  10.0f,    1.0f,   "%5.2f" },  // FX_P_EQ_MID_Q
    { "HI  EN",  FX_PAGE_EQ,       0.0f,   1.0f,    0.0f,   "%5.0f" },  // FX_P_EQ_HI_EN
    { "HI  FRQ", FX_PAGE_EQ,      20.0f, 20000.0f,10000.0f,"%5.0f" },  // FX_P_EQ_HI_FRQ
    { "HI  GAI", FX_PAGE_EQ,      -24.0f,  24.0f,   0.0f,   "%5.1f" },  // FX_P_EQ_HI_GAI
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

// TREEFROG_MIXER_VU_DB_SCALE_V5 (Bacon 1.1.1):
// DAW/VU-style rebased scale.  The displayed 0 dB row corresponds to the
// typical loud output at volume 100 (measured ~-12 dBFS real peaks for loud
// material on the normalized 0..1 peaks), so at volume 100 the bar genuinely
// reaches the 0 dB row and strong material pushes into the red +3 dB zone
// above it -- 0 dB is reachable, red means over 0 dB.  Displayed dB = real
// dB + 12 on a -36..+3 span (39 dB): level = (db+36)/39 maps 0 dB to cell
// 11 of 12 and +3 dB to the top cell, which is exactly where the fill turns
// red (filledCells >= totalCells).  The V3 -50..0 scale was honest but made
// red unreachable: loud material read 9/12 cells at volume 100 and the +3
// zone did not exist.
static float mixVULevel(float peak) {
	if (peak <= 0.0f) return 0.0f ;
	float db = 20.0f * log10f(peak) + 12.0f ;
	float level = (db + 36.0f) / 39.0f ;
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
		vuDisplayL_[i]=0.0f ;
		vuDisplayR_[i]=0.0f ;
	}
	// TREEFROG_MIXER_HALF_CELL_BARS_V1 + TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1)
	for (int i=0;i<=SONG_CHANNEL_COUNT;i++) {
		meterRecords_[i].valid=false ;
	}
	mixUndoCount_=0 ;
	mixRedoCount_=0 ;
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
			// TREEFROG_GLOBAL_UNDO_V7: capture old and new percent.
			FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
			int oldRet=fxReturnPercent(fx.GetDelayReturn()) ;
			nudgeDelayReturn(delta) ;
			pushMixUndo(ME_DLYRET,viewData_->mixerCol_,
			            (float)oldRet,
			            (float)fxReturnPercent(fx.GetDelayReturn())) ;
			isDirty_=true ;
			return ;
		}
		if (fxEditTarget_==2) {
			FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
			int oldRet=fxReturnPercent(fx.GetReverbReturn()) ;
			nudgeReverbReturn(delta) ;
			pushMixUndo(ME_RVBRET,viewData_->mixerCol_,
			            (float)oldRet,
			            (float)fxReturnPercent(fx.GetReverbReturn())) ;
			isDirty_=true ;
			return ;
		}
	}
	Mixer *mixer=Mixer::GetInstance() ;
	int oldVol=mixer->GetChannelVolume(viewData_->mixerCol_) ;
	mixer->NudgeChannelVolume(viewData_->mixerCol_,delta) ;
	pushMixUndo(ME_VOL,viewData_->mixerCol_,
	            (float)oldVol,
	            (float)mixer->GetChannelVolume(viewData_->mixerCol_)) ;
	isDirty_=true ;
}

void MixerView::adjustMasterVolume(int delta) {
	Project *project=viewData_->project_ ;
	if (!project) return ;
	int v=project->GetMasterVolume() ;
	int v2=v+delta ;
	if (v2<10) v2=10 ;
	if (v2>100) v2=100 ;
	// TREEFROG_GLOBAL_UNDO_V7: old value for undo, clamped post-edit for redo.
	pushMixUndo(ME_MASTERVOL,-1,(float)v,(float)v2) ;
	v=v2 ;
	Variable *var=project->FindVariable(VAR_MASTERVOL) ;
	if (var) var->SetInt(v,false) ;
	MixerService::GetInstance()->SetMasterVolume(v) ;
	isDirty_=true ;
}

void MixerView::toggleMute() {
	if (masterSelected_) return ;
	// TREEFROG_GLOBAL_UNDO_V7: mute toggles are undoable/redoable (the user
	// expects R1+A on the mixer to be an action L1+X can take back).
	Player *player=Player::GetInstance() ;
	int channel=viewData_->mixerCol_ ;
	bool oldMuted=player->IsChannelMuted(channel) ;
	UIController::GetInstance()->ToggleMute(channel,channel) ;
	pushMixUndo(ME_MUTE,channel,oldMuted?1.0f:0.0f,oldMuted?0.0f:1.0f) ;
	isDirty_=true ;
}

void MixerView::switchSoloMode() {
	if (masterSelected_) return ;
	// TREEFROG_GLOBAL_UNDO_V7: solo toggles are undoable/redoable.  The full
	// 8-bit mute mask is captured before and after the toggle; the undo
	// restores the pre-toggle mask, the redo the post-toggle mask.
	Player *player=Player::GetInstance() ;
	int channel=viewData_->mixerCol_ ;
	unsigned char oldMask=0,newMask=0 ;
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		if (player->IsChannelMuted(i)) oldMask|=(unsigned char)(1<<i) ;
	}
	UIController::GetInstance()->SwitchSoloMode(channel,channel,!soloMode_) ;
	soloMode_=!soloMode_ ;
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		if (player->IsChannelMuted(i)) newMask|=(unsigned char)(1<<i) ;
	}
	pushMixUndo(ME_SOLO,channel,(float)oldMask,(float)newMask) ;
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

	// F1b input policy (REFACTOR_ROADMAP_ES.md): el input se resuelve contra
	// el catalogo dorado (ActionMap.cpp) con el contexto de la pagina actual
	// (MIX vs DELAY/REVERB/EQ/COMP). La tabla transcribe 1:1 el orden y las
	// condiciones de esta funcion en Bacon 1.2.1 (MixerView.cpp:534-697;
	// p.ej. la rama `mask&EPBM_SELECT` del golden es el binding
	// ACTION_CYCLE_FX_PAGE); el dispatch llama a los mismos metodos y
	// respeta las colas multi-fire.
	using namespace UI::Input ;
	const PadMask pad=(PadMask)mask ;  /* los bits EPBM_* espejan PhysicalKey */
	const ContextId ctx=(fxPage_==FX_PAGE_MIX)?CTX_MIXER:CTX_MIXER_FX ;
	const ActionId action=ChordResolver_Resolve(pad,ctx) ;

	switch (action) {
		case ACTION_CYCLE_FX_PAGE:
			cycleFxPage() ;
			return ;
		case ACTION_TOGGLE_MUTE:
			toggleMute() ;
			return ;
		case ACTION_TOGGLE_SOLO:
			switchSoloMode() ;
			return ;
		case ACTION_SWITCH_VIEW_SONG:
			/* MixerView.cpp:558-563, R1+UP VT_SONG; cola +START onStop. */
			{
				ViewType vt=VT_SONG;
				ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
				SetChanged();
				NotifyObservers(&ve) ;
			}
			if (pad&KEY_START) onStop() ;
			return ;
		case ACTION_STOP:
			onStop() ;
			return ;
		case ACTION_OPEN_INSTRUMENT_FX:
			showInstrumentFxMenu() ;
			return ;
		case ACTION_CYCLE_FX_EDIT_TARGET:
			/* MixerView.cpp:579-583: ciclo VOL -> DLY RET -> RVB RET. */
			fxEditTarget_=(fxEditTarget_+1)%3 ;
			isDirty_=true ;
			((AppWindow &)w_).SetDirty() ;
			return ;
		case ACTION_RESET_PAN:
			/* Bloque L2 del golden: masterSelected_ aborta ANTES de todo.
			 * (MixerView.cpp:594; el enumerador mixer_golden_enum verifica
			 * que todas las mascaras L2 resuelvan acciones de pan.) */
			if (masterSelected_) return ;
			{
				Mixer *mixer=Mixer::GetInstance() ;
				int channel=viewData_->mixerCol_ ;
				pushMixUndo(ME_PAN,channel,(float)mixer->GetChannelPan(channel),0.0f) ;
				mixer->SetChannelPan(channel,0) ;
				isDirty_=true ;
				((AppWindow &)w_).SetDirty() ;
			}
			return ;
		case ACTION_PAN_NUDGE_LEFT:
		case ACTION_PAN_NUDGE_LEFT_COARSE: {
			if (masterSelected_) return ;
			Mixer *mixer=Mixer::GetInstance() ;
			int channel=viewData_->mixerCol_ ;
			int oldPan=mixer->GetChannelPan(channel) ;
			int step=(action==ACTION_PAN_NUDGE_LEFT_COARSE)?10:1 ;
			mixer->NudgeChannelPan(channel,-step) ;
			pushMixUndo(ME_PAN,channel,(float)oldPan,
			            (float)mixer->GetChannelPan(channel)) ;
			isDirty_=true ;
			return ;
		}
		case ACTION_PAN_NUDGE_RIGHT:
		case ACTION_PAN_NUDGE_RIGHT_COARSE: {
			if (masterSelected_) return ;
			Mixer *mixer=Mixer::GetInstance() ;
			int channel=viewData_->mixerCol_ ;
			int oldPan=mixer->GetChannelPan(channel) ;
			int step=(action==ACTION_PAN_NUDGE_RIGHT_COARSE)?10:1 ;
			mixer->NudgeChannelPan(channel,step) ;
			pushMixUndo(ME_PAN,channel,(float)oldPan,
			            (float)mixer->GetChannelPan(channel)) ;
			isDirty_=true ;
			return ;
		}
		case ACTION_OPEN_MENU:
			DoModal(new MixerActionMenuModal(*this),
			        MixerActionMenuApplyCallback) ;
			return ;
		case ACTION_RESET_PARAMETER:
			fxResetRow() ;
			return ;
		/* Bloque A de paginas FX (multi-fire U,D,L,R: cuando se resuelve
		 * EDIT_PARAM_* con cola, ni UP/DOWN anteriores estan presentes:
		 * el orden de la tabla U>D>L>R ya los resolvio). */
		case ACTION_EDIT_PARAM_UP:
			fxEditRow(1,true) ;
			if (pad&KEY_DOWN) fxEditRow(-1,true) ;
			if (pad&KEY_LEFT) fxEditRow(-1,false) ;
			if (pad&KEY_RIGHT) fxEditRow(1,false) ;
			return ;
		case ACTION_EDIT_PARAM_DOWN:
			fxEditRow(-1,true) ;
			if (pad&KEY_LEFT) fxEditRow(-1,false) ;
			if (pad&KEY_RIGHT) fxEditRow(1,false) ;
			return ;
		case ACTION_EDIT_PARAM_LEFT:
			if (pad&KEY_A) {
				fxEditRow(-1,false) ;
				if (pad&KEY_RIGHT) fxEditRow(1,false) ;
				return ;
			}
			fxEditRow(-1,false) ;
			return ;
		case ACTION_EDIT_PARAM_RIGHT:
			if (pad&KEY_A) {
				fxEditRow(1,false) ;
				return ;
			}
			fxEditRow(1,false) ;
			return ;
		/* Filas de paginas FX: single-fire (la primera flecha en orden
		 * U>D>L>R retorna; las demas nunca disparan). */
		case ACTION_ROW_UP:
			fxMoveRow(-1) ; return ;
		case ACTION_ROW_DOWN:
			fxMoveRow(1) ; return ;
		case ACTION_PLAY_STOP:
			/* Pagina MIX: START del bloque base (cola L,R,U,D multi-fire).
			 * Paginas FX: START puro (las flechas ya se resolvieron antes,
			 * nunca llegan aqui). */
			onStart() ;
			if (pad&KEY_LEFT) updateCursor(-1,0) ;
			if (pad&KEY_RIGHT) updateCursor(1,0) ;
			if (pad&KEY_UP) updateVolume(1) ;
			if (pad&KEY_DOWN) updateVolume(-1) ;
			return ;
		/* Bloques A y L1 de la pagina MIX (multi-fire U,D,L,R; A gana a
		 * L1: cuando la tabla resuelve VOLUME_COARSE_* con A presente la
		 * rama es la A, si no es la L1). */
		case ACTION_VOLUME_COARSE_UP: {
			updateVolume(10) ;
			if (pad&KEY_DOWN) updateVolume(-10) ;
			if (pad&KEY_A) {
				if (pad&KEY_LEFT) updateVolume(-1) ;
				if (pad&KEY_RIGHT) updateVolume(1) ;
			} else {
				if (pad&KEY_LEFT) updateCursor(-1,0) ;
				if (pad&KEY_RIGHT) updateCursor(1,0) ;
			}
			return ;
		}
		case ACTION_VOLUME_COARSE_DOWN: {
			updateVolume(-10) ;
			if (pad&KEY_A) {
				if (pad&KEY_LEFT) updateVolume(-1) ;
				if (pad&KEY_RIGHT) updateVolume(1) ;
			} else {
				if (pad&KEY_LEFT) updateCursor(-1,0) ;
				if (pad&KEY_RIGHT) updateCursor(1,0) ;
			}
			return ;
		}
		/* A+LEFT/RIGHT: cola LEFT->RIGHT para FINE_DECREASE (el orden de
		 * la tabla U>D>L>R garantiza que ni UP ni DOWN estan presentes). */
		case ACTION_VOLUME_FINE_DECREASE:
			updateVolume(-1) ;
			if (pad&KEY_RIGHT) updateVolume(1) ;
			return ;
		case ACTION_VOLUME_FINE_INCREASE:
			updateVolume(1) ;
			return ;
		/* Bloque base (sin modificador). MIX_CURSOR_* llega de la rama L1
		 * (L1+flecha, cola U/D grueso + R/L) o de la base (flecha sola,
		 * cola R/U/D fino); se distingue por la mascara. */
		case ACTION_MIX_CURSOR_LEFT: {
			if (pad&KEY_L1) {
				if (pad&KEY_UP) updateVolume(10) ;
				if (pad&KEY_DOWN) updateVolume(-10) ;
				updateCursor(-1,0) ;
				if (pad&KEY_RIGHT) updateCursor(1,0) ;
			} else {
				updateCursor(-1,0) ;
				if (pad&KEY_RIGHT) updateCursor(1,0) ;
				if (pad&KEY_UP) updateVolume(1) ;
				if (pad&KEY_DOWN) updateVolume(-1) ;
			}
			return ;
		}
		case ACTION_MIX_CURSOR_RIGHT: {
			if (pad&KEY_L1) {
				if (pad&KEY_UP) updateVolume(10) ;
				if (pad&KEY_DOWN) updateVolume(-10) ;
				if (pad&KEY_LEFT) updateCursor(-1,0) ;
				updateCursor(1,0) ;
			} else {
				if (pad&KEY_LEFT) updateCursor(-1,0) ;
				updateCursor(1,0) ;
				if (pad&KEY_UP) updateVolume(1) ;
				if (pad&KEY_DOWN) updateVolume(-1) ;
			}
			return ;
		}
		case ACTION_VOLUME_FINE_UP:
			updateVolume(1) ;
			if (pad&KEY_DOWN) updateVolume(-1) ;
			return ;
		case ACTION_VOLUME_FINE_DOWN:
			if (pad&KEY_UP) updateVolume(1) ;
			updateVolume(-1) ;
			return ;
		case ACTION_NONE:
			return ;
	} ;
} ;

// TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1):
// MIX/FX edit history for the global L1+X / R1+X combos.
void MixerView::pushMixUndo(int kind,int channel,float value,float newValue) {
	for (int i=MIX_HISTORY_SIZE-1;i>0;i--) {
		mixUndo_[i]=mixUndo_[i-1] ;
	}
	mixUndo_[0].kind=kind ;
	mixUndo_[0].channel=channel ;
	mixUndo_[0].value=value ;
	mixUndo_[0].newValue=newValue ;
	mixUndoCount_++ ;
	if (mixUndoCount_>MIX_HISTORY_SIZE) mixUndoCount_=MIX_HISTORY_SIZE ;
	mixRedoCount_=0 ;
}

void MixerView::restoreMixEdit(const MixEdit &edit) {
	Mixer *mixer=Mixer::GetInstance() ;
	Project *project=viewData_->project_ ;
	// TREEFROG_GLOBAL_UNDO_V7: REDO restores the post-edit value captured at
	// push time; plain restores (UNDO) fall back to the pre-edit value.
	const float val=(edit.newValue>=0.0f)?edit.newValue:edit.value ;
	switch (edit.kind) {
	case ME_VOL:
		mixer->SetChannelVolume(edit.channel,(int)val) ;
		break ;
	case ME_PAN:
		mixer->SetChannelPan(edit.channel,(int)val) ;
		break ;
	case ME_MASTERVOL:
		if (project) {
			Variable *var=project->FindVariable(VAR_MASTERVOL) ;
			if (var) var->SetInt((int)val,false) ;
			MixerService::GetInstance()->SetMasterVolume((int)val) ;
		}
		break ;
	case ME_DLYRET:
		FxEngine::FxEngine::GetInstance().SetDelayReturn(fxReturnFromPercent((int)val)) ;
		break ;
	case ME_RVBRET:
		FxEngine::FxEngine::GetInstance().SetReverbReturn(fxReturnFromPercent((int)val)) ;
		break ;
	case ME_FX:
		fxSet(edit.channel,val) ;
		break ;
	case ME_SOFTCLIP:
		if (project) {
			Variable *var=project->FindVariable(VAR_SOFTCLIP) ;
			if (var) var->SetInt((int)val,false) ;
			MixerService::GetInstance()->SetSoftclip((int)val,
			                                         project->GetSoftclipGain()) ;
		}
		break ;
	case ME_SOFTCLIPGAIN:
		if (project) {
			Variable *var=project->FindVariable(VAR_SOFTCLIP_GAIN) ;
			if (var) var->SetInt((int)val,false) ;
			MixerService::GetInstance()->SetSoftclip(project->GetSoftclip(),
			                                         (int)val) ;
		}
		break ;
	case ME_MUTE:
		// TREEFROG_GLOBAL_UNDO_V7: channel mute toggles are undoable.  The
		// restore value is the mute state to reach (0/1).
		Player::GetInstance()->SetChannelMute(
		    edit.channel, (val>=0.5f)) ;
		break ;
	case ME_SOLO: {
		// TREEFROG_GLOBAL_UNDO_V7: solo toggles are undoable.  The value is
		// the full 8-bit mute mask (old on undo, new on redo); restoring it
		// directly avoids touching UIController::soloMask_ so a later solo
		// off still returns to the pre-solo mask.
		unsigned char mask=(unsigned char)(int)val ;
		Player *player=Player::GetInstance() ;
		int unmuted=0 ;
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			bool muted=((mask&(1<<i))!=0) ;
			player->SetChannelMute(i,muted) ;
			if (!muted) unmuted++ ;
		}
		soloMode_=(unmuted==1) ;
		break ;
	}
	}
}

bool MixerView::GlobalUndo() {
	if (mixUndoCount_==0) return true ;
	MixEdit e=mixUndo_[0] ;
	for (int i=0;i<mixUndoCount_-1;i++) {
		mixUndo_[i]=mixUndo_[i+1] ;
	}
	mixUndoCount_-- ;
	for (int i=MIX_HISTORY_SIZE-1;i>0;i--) {
		mixRedo_[i]=mixRedo_[i-1] ;
	}
	mixRedo_[0]=e ;
	mixRedoCount_++ ;
	if (mixRedoCount_>MIX_HISTORY_SIZE) mixRedoCount_=MIX_HISTORY_SIZE ;
	restoreMixEdit(e) ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
	return true ;
}

bool MixerView::GlobalRedo() {
	if (mixRedoCount_==0) return true ;
	MixEdit e=mixRedo_[0] ;
	for (int i=0;i<mixRedoCount_-1;i++) {
		mixRedo_[i]=mixRedo_[i+1] ;
	}
	mixRedoCount_-- ;
	for (int i=MIX_HISTORY_SIZE-1;i>0;i--) {
		mixUndo_[i]=mixUndo_[i-1] ;
	}
	mixUndo_[0]=e ;
	mixUndoCount_++ ;
	if (mixUndoCount_>MIX_HISTORY_SIZE) mixUndoCount_=MIX_HISTORY_SIZE ;
	restoreMixEdit(e) ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
	return true ;
}

// TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): A+B restores the hovered option to
// its default state.  On the FX pages that is fxResetRow (legacy vdef); on
// the MIX page the hovered channel resets to volume 100 + pan center, the
// master bar to volume 100.
bool MixerView::GlobalResetOption() {
	if (fxPage_!=FX_PAGE_MIX) {
		fxResetRow() ;
		return true ;
	}
	Mixer *mixer=Mixer::GetInstance() ;
	if (masterSelected_) {
		Project *project=viewData_->project_ ;
		if (!project) return true ;
		pushMixUndo(ME_MASTERVOL,-1,(float)project->GetMasterVolume(),100.0f) ;
		Variable *var=project->FindVariable(VAR_MASTERVOL) ;
		if (var) var->SetInt(100,false) ;
		MixerService::GetInstance()->SetMasterVolume(100) ;
	} else {
		int channel=viewData_->mixerCol_ ;
		pushMixUndo(ME_VOL,channel,(float)mixer->GetChannelVolume(channel),100.0f) ;
		pushMixUndo(ME_PAN,channel,(float)mixer->GetChannelPan(channel),0.0f) ;
		mixer->SetChannelVolume(channel,100) ;
		mixer->SetChannelPan(channel,0) ;
	}
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
	return true ;
}


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
	// RC6: the 2-cell channel label is drawn at x-1 so its right cell sits
	// on the 1-cell meter axis (same rule as MST/%3d).
	DrawString(x-1,y,hex,props) ;
	props.invert_=false ;

	// TREEFROG_MIXER_LIVE_BAR_V4 (H38.7) + RC5:
	// The bar fill follows the real-time output level (GetChannelPeak), so a
	// low volume reads as a small wave and a loud volume as a big one. The
	// channel volume setting is drawn as an accent marker cell plus the
	// numeric value below. Selected bars stay purple, muted bars dim.  RC5:
	// each row is a single cell (one-column meter) so the 9 meters of the
	// MIX page fit the centered bank; totalCells == height.
	// TREEFROG_MIXER_ZERO_DB_CLIP_V5 (Bacon 1.1.1):
	// The bar fill = mixVULevel(peak) * volume/100 * cells on the rebased
	// DAW scale shared with the master bar and the CUE column.  At volume
	// 100 the 0 dB row (cell 11 of 12) is the real level of loud material,
	// so it is genuinely reachable; the bar turns red (CD_ERROR) only when
	// the fill passes 0 dB into the +3 zone (the top cell), exactly the
	// condition that produces the clipped sound.  The 4-cell pitch separates
	// the 3-digit volume numbers ("100 100" instead of "100100").
	// TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1) + TREEFROG_MIXER_HALF_CELL_BARS_V2:
	// The channel bar is TWO independent bars sharing ONE cell column (8 px):
	// L at px 0..2, a 2-px dark seam at px 3..4, R at px 5..7.  The old
	// one-cell gap column (x+1) is gone and the bars are narrower, so they
	// fit exactly over the previous single column ("quepan sobre su propia
	// columna anterior").  Each side shows its own post-pan peak, so the pan
	// is visible in the bars themselves: center = both equal, hard left =
	// left full / right empty.  Each side turns red on its own when it
	// passes 0 dB into the +3 zone.  With the 0..127 volume scale (127 =
	// +2.1 dB) the fill can push past 0 dB and reach the red zone.  Side 0
	// records L and paints the cell track; side 1 records R at the same x;
	// the pixels are drawn by PostFlushDraw() after the char flush.
	drawMeterBar(x,y,height,vuDisplayL_[channel],volume,selected,muted,props,CD_NORMAL,0,meterRecords_[channel]) ;
	drawMeterBar(x,y,height,vuDisplayR_[channel],volume,selected,muted,props,CD_NORMAL,1,meterRecords_[channel]) ;

	SetColor(selected?CD_HILITE2:(muted?CD_BORDER:CD_NORMAL)) ;
	props.invert_=selected ;
	sprintf(buffer,"%3d",volume) ;
	DrawString(x-1,y+height+2,buffer,props) ;
	props.invert_=false ;

	// TREEFROG_MIXER_PAN_V1 (Bacon 1.1.1):
	// The row under the volume numbers shows the stereo pan of every
	// channel: "L/R" + the hard-side value (0..100), "C" for center, drawn
	// under the number column (4 cells, so hard pans read "L100"/"R100"
	// without touching the neighbour).  A muted channel shows its "M"
	// marker instead -- its pan is inaudible anyway.  Center pans sit at
	// the same digit column as the L/R values (right-aligned value).
	int pan=mixer->GetChannelPan(channel) ;
	if (muted) {
		SetColor(CD_HILITE2) ;
		DrawString(x,y+height+3,"M",props) ;
	} else {
		SetColor(selected?CD_HILITE2:CD_NORMAL) ;
		props.invert_=selected ;
		if (pan==0) {
			DrawString(x-1,y+height+3,"  C",props) ;
		} else if (pan<0) {
			sprintf(buffer,"L%3d",-pan) ;
			DrawString(x-1,y+height+3,buffer,props) ;
		} else {
			sprintf(buffer,"R%3d",pan) ;
			DrawString(x-1,y+height+3,buffer,props) ;
		}
		props.invert_=false ;
	}
}

void MixerView::drawMasterBar(int x,int y,int height) {
	Project *project=viewData_->project_ ;
	int volume=project?project->GetMasterVolume():100 ;
	GUITextProperties props ;
	char buffer[8] ;

	// TREEFROG_MIXER_MASTER_VU_V5 (Bacon 1.1.1) + TREEFROG_MIXER_STEREO_METERS_V1:
	// Master bars = mixVULevel(master peak) * masterVolume/100 on the rebased
	// DAW scale shared with the channel bars and the CUE column.  At master
	// volume 100 the 0 dB row is the real level of loud output
	// (MixerService::GetMasterPeak, true pre-clip mix sum, can exceed 1.0);
	// lower volumes scale the fill so the bar always reads like the
	// loudness you actually hear.  It turns red (CD_ERROR) only when the
	// fill passes 0 dB into the +3 zone (the top cell), i.e. the output
	// really exceeds the 0 dB row.  The peak is measured pre-clip (Bacon
	// 1.1.1: the mix sum can exceed 0 dB), so the red zone is genuinely
	// reachable.  Two bars are drawn (L at x, R at x+2, one-cell gap) so
	// the stereo balance of the mix is visible live.
	MixerService *ms=MixerService::GetInstance() ;

	// TREEFROG_MIXER_MASTER_BAR_V1 (H38.7):
	// Master (MST) bar drawn live on the left of the channel bars, in cyan so
	// it stands out from the white channel fills. When selected it lights
	// purple like a selected channel.
	SetColor(masterSelected_?CD_HILITE2:CD_PLAY) ;
	props.invert_=false ;
	DrawString(x-1,y,"MST",props) ;

	drawMeterBar(x,y,height,ms->GetMasterPeakL(),volume,masterSelected_,false,props,CD_PLAY,0,meterRecords_[SONG_CHANNEL_COUNT]) ;
	drawMeterBar(x,y,height,ms->GetMasterPeakR(),volume,masterSelected_,false,props,CD_PLAY,1,meterRecords_[SONG_CHANNEL_COUNT]) ;
	// TREEFROG_MIXER_VU_STOP_RESET_V1 (Bacon 1.1.1 V14): the CUE/MST meter
	// reads the raw master peak; zero it while stopped so the bar returns to
	// 0 like the channel bars instead of freezing at the last position.
	if (!Player::GetInstance()->IsRunning()) {
		meterRecords_[SONG_CHANNEL_COUNT].levelL=0.0f ;
		meterRecords_[SONG_CHANNEL_COUNT].levelR=0.0f ;
	}

	SetColor(masterSelected_?CD_HILITE2:CD_PLAY) ;
	props.invert_=masterSelected_ ;
	sprintf(buffer,"%3d",volume) ;
	DrawString(x-1,y+height+2,buffer,props) ;
	props.invert_=false ;
}

// TREEFROG_MIXER_HALF_CELL_BARS_V1 (Bacon 1.1.1):
// Records ONE side of a meter into the meter record.  Side 0 keeps painting
// the cell track (so the label column and the char cache stay coherent);
// side 1 only records.  The pixel-level L/R split (px 0..2 L, px 3..4 seam,
// px 5..7 R) is drawn by PostFlushDraw() every Flush.
void MixerView::drawMeterBar(int x,int y,int height,float peak,int volume,
                             bool selected,bool muted,GUITextProperties &props,
                             ColorDefinition onColor,int side,MeterRecord &rec) {
	rec.valid=true ;
	rec.xCell=x ;
	rec.yCell=y+1 ;
	rec.height=height ;
	rec.selected=selected ;
	rec.muted=muted ;
	rec.onColor=onColor ;
	// TREEFROG_MIXER_COMPACT_BARS_V1 (Bacon 1.1.1 V13): the record carries
	// the normalized post-volume level; PostFlushDraw renders the compact
	// 2-px-step meter from it.
	float level=mixVULevel(peak)*float(volume)*0.01f ;
	if (level<0.0f) level=0.0f ;
	if (level>1.0f) level=1.0f ;
	if (side==0) {
		rec.levelL=level ;
	} else {
		rec.levelR=level ;
	}
	if (side!=0) return ;
	// TREEFROG_MIXER_PIXEL_BARS_V2 (Bacon 1.1.1): the pixel layer
	// (PostFlushDraw) owns the whole meter column, so the char layer only
	// blanks the cells.  This kills both artifacts of the mixed rendering:
	// the old full-width mono bar (the char-layer inverted fill) and the
	// dim track cells that read as static "--" dashes below the fill.
	SetColor(CD_BACKGROUND) ;
	props.invert_=false ;
	for (int row=0;row<height;row++) {
		DrawString(x,y+1+row," ",props) ;
	}
}

// TREEFROG_MIXER_HALF_CELL_BARS_V1 (Bacon 1.1.1):
// Pixel layer of the L/R half-cell bars.  Runs from AppWindow::Flush AFTER
// the character screen is rendered, so it repaints the bar columns on top
// every frame: left bar px 0..2, dark seam px 3..4, right bar px 5..7.
// TREEFROG_MIXER_COMPACT_BARS_V1 (Bacon 1.1.1 V13): M8-style compact meters:
// each level is 3 px tall (2 px fill + 1 px gap), so a 12-cell bar renders
// 32 fine steps instead of 12 chunky 8-px blocks.  The top 12.5% of the bar
// is the 0 dB+ zone: it fills solid red when the level reaches it (instead
// of the whole bar turning red at full scale).
void MixerView::PostFlushDraw() {
#if defined(PLATFORM_TREEFROG)
	AppWindow *app=(AppWindow *)&w_ ;
	uint16_t *fb=TreeFrogGetFramebuffer() ;
	if (!fb) return ;
	// TREEFROG_MIXER_MODAL_OVERLAY_V1 (Bacon 1.1.1 V14): modal menus (L1+A
	// mixer menu, help, ...) and the FX pages own the whole screen; the
	// pixel VU bars must not be repainted over them (menu letters were
	// interleaved with the bars).
	if (GetModal() || fxPage_ != FX_PAGE_MIX) return ;
	unsigned short seamC=app->ResolveColor565(CD_BACKGROUND) ;
	unsigned short borderC=app->ResolveColor565(CD_BORDER) ;
	unsigned short redC=app->ResolveColor565(CD_ERROR) ;
	const int LEVEL_H=3 ;
	for (int m=0;m<=SONG_CHANNEL_COUNT;m++) {
		MeterRecord &r=meterRecords_[m] ;
		if (!r.valid) continue ;
		int px=r.xCell*8 ;
		int py=r.yCell*8 ;
		int totalPx=r.height*8 ;
		int totalLevels=totalPx/LEVEL_H ;
		if (totalLevels<1) continue ;
		// 0 dB+ zone: the top of the bar above the 0 dB row on the shared
		// -36..+3 scale (mixVULevel(0 dB) = 36/39), rendered solid red.  The
		// zone starts right above 0 dB (the "0" CUE mark) and covers the +3
		// top cell, matching the CUE scale's +3/0 markers.  TREEFROG_MIXER_RED_BAND_TOP_V1
		// (Bacon 1.1.1 V15): with the bottom-up bars the band must sit at the
		// top of the bar (row 0 = top of screen), not at the bottom as the
		// pre-V14 top-down row test left it.
		const float zeroDbLevel=36.0f/39.0f ;
		int redBandLevels=totalLevels-(int)(zeroDbLevel*(float)totalLevels+0.5f) ;
		if (redBandLevels<1) redBandLevels=1 ;
		int redBandPx=redBandLevels*LEVEL_H ;
		int filledL=(int)(r.levelL*(float)totalLevels+0.5f)*LEVEL_H ;
		int filledR=(int)(r.levelR*(float)totalLevels+0.5f)*LEVEL_H ;
		if (filledL>totalPx) filledL=totalPx ;
		if (filledR>totalPx) filledR=totalPx ;
		// TREEFROG_MIXER_BARS_BOTTOM_UP_V1 (Bacon 1.1.1 V14): bars grow from
		// the bottom up, aligned with the Cue meter (previously they filled
		// from the top and read inverted next to the Cue bar).
		int filledLLevels=filledL/LEVEL_H ;
		int filledRLevels=filledR/LEVEL_H ;
		unsigned short fillC ;
		if (r.selected) {
			fillC=app->ResolveColor565(CD_HILITE2) ;
		} else if (r.muted) {
			fillC=borderC ;
		} else {
			fillC=app->ResolveColor565(r.onColor) ;
		}
		int iBase=py*TREEFROG_LGPT_WIDTH+px ;
		for (int row=0;row<totalPx;row++) {
			bool inBand=(row<redBandPx) ;
			int levelIdx=(totalPx-1-row)/LEVEL_H ;
			bool fillL=(levelIdx<filledLLevels) ;
			bool fillR=(levelIdx<filledRLevels) ;
			// 1-px gap between levels (bottom row of each 3-px group); the
			// red band is solid.
			bool gapRow=((row%LEVEL_H)==0)&&!inBand ;
			unsigned short lC=(fillL&&inBand)?redC:
			                  ((fillL&&!gapRow)?fillC:seamC) ;
			unsigned short rC=(fillR&&inBand)?redC:
			                  ((fillR&&!gapRow)?fillC:seamC) ;
			fb[iBase+0]=lC ;
			fb[iBase+1]=lC ;
			fb[iBase+2]=lC ;
			fb[iBase+3]=seamC ;
			fb[iBase+4]=seamC ;
			fb[iBase+5]=rC ;
			fb[iBase+6]=rC ;
			fb[iBase+7]=rC ;
			iBase+=TREEFROG_LGPT_WIDTH ;
		}
	}
#endif
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
	// TREEFROG_GLOBAL_UNDO_V1: record the old float value for L1+X undo.
	float oldVal=fxGet(targetId) ;
	pushMixUndo(ME_FX,targetId,oldVal) ;
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
	// TREEFROG_GLOBAL_UNDO_V7: record the post-edit value for redo.
	if (mixUndoCount_>0) mixUndo_[0].newValue=v ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

// TREEFROG_FX_NAV_A_B_DEFAULT_V1 (PLAN_FX_REDESIGN_ES.md, Fase 6):
// A+B restores the hovered parameter to its legacy default so the whole page
// can be brought back to the Fase 5 "all defaults" state without hunting.
void MixerView::fxResetRow() {
	int targetId=fxIdForRow(fxRow_) ;
	if (targetId<0) return ;
	// TREEFROG_GLOBAL_UNDO_V1: A+B on the FX pages restores vdef; record the
	// old value so L1+X brings it back.
	pushMixUndo(ME_FX,targetId,fxGet(targetId),kFxParams_[targetId].vdef) ;
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
	case FX_P_EQ_LOW_GAI: fx.SetEqBandGainDb(0,fl2fp(v)) ;
	                     fx.SetEqBandEnabled(0,true) ; fx.SetEqBypass(false) ; break ;
	case FX_P_EQ_LOW_Q:   fx.SetEqBandQ(0,fl2fp(v)) ; break ;
	case FX_P_EQ_LOW_EN:  fx.SetEqBandEnabled(0,v>=0.5f) ; break ;
	case FX_P_EQ_MID_FRQ: fx.SetEqBandFreq(1,fl2fp(v)) ; break ;
	case FX_P_EQ_MID_GAI: fx.SetEqBandGainDb(1,fl2fp(v)) ;
	                     fx.SetEqBandEnabled(1,true) ; fx.SetEqBypass(false) ; break ;
	case FX_P_EQ_MID_Q:   fx.SetEqBandQ(1,fl2fp(v)) ; break ;
	case FX_P_EQ_MID_EN:  fx.SetEqBandEnabled(1,v>=0.5f) ; break ;
	case FX_P_EQ_HI_FRQ:  fx.SetEqBandFreq(2,fl2fp(v)) ; break ;
	case FX_P_EQ_HI_GAI:  fx.SetEqBandGainDb(2,fl2fp(v)) ;
	                     fx.SetEqBandEnabled(2,true) ; fx.SetEqBypass(false) ; break ;
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
	// RC6: each dedicated page draws its own title at the top of its centered
	// block (ml.startY-1) so the title and its menu form one centered unit.
	if (page==FX_PAGE_DELAY) {
		drawDelayPage(pageTitle) ;
		return ;
	}
	if (page==FX_PAGE_REVERB) {
		drawReverbPage(pageTitle) ;
		return ;
	}
	// TREEFROG_EQ_MENU_V1 (PLAN_FX_REDESIGN_ES.md, Fase 12): the EQ page is a
	// dedicated exclusive menu with a banded LOW/MID/HIGH layout (EN/FRQ/GAI/Q
	// per band), so it does not use the generic parameter list.
	if (page==FX_PAGE_EQ) {
		drawEqPage(pageTitle) ;
		return ;
	}
	// TREEFROG_COMP_MENU_V1 (PLAN_FX_REDESIGN_ES.md, Fase 13): the COMP page
	// is a dedicated exclusive menu (BYP first, centered labeled rows, GR
	// meter always visible) instead of the generic parameter list.
	if (page==FX_PAGE_COMP) {
		drawCompPage(pageTitle) ;
		return ;
	}
	UiDraw::DrawCenteredTitleAt(*this,1,pageTitle) ;
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

void MixerView::drawDelayPage(const char *title) {
	char buffer[16] ;
	static const char *labels[7]={"BYPASS","TIME","FEEDBACK","MIX",
	                              "WIDTH","PING/PONG","SATURATE"} ;
	static const int ids[7]={FX_P_DLY_BYP,FX_P_DLY_TIME,FX_P_DLY_FBK,
	                         FX_P_DLY_MIX,FX_P_DLY_WID,FX_P_DLY_PP,
	                         FX_P_DLY_SAT} ;
	// RC5: the whole 7-row block is centered in the safe menu band 3..25
	// (label column 9 = "PING/PONG", value column 8, two-cell spacing).
	MenuLayout ml=UiDraw::MakeCenteredMenuLayout(7,9,8,2) ;
	// RC6: the page title sits on the row just above the centered block.
	UiDraw::DrawCenteredTitleAt(*this,ml.startY-1,title) ;
	for (int p=0;p<7;p++) {
		int id=ids[p] ;
		float v=fxGet(id) ;
		bool selected=(fxRowForId(id)==fxRow_) ;
		int y=ml.startY+p ;
		if (id==FX_P_DLY_BYP) {
			UiDraw::DrawBypassRow(*this,ml.labelX,ml.valueX,y,v>=0.5f,selected) ;
			continue ;
		}
		switch(id) {
		case FX_P_DLY_TIME: sprintf(buffer,"%4.0f ms",v) ; break ;
		case FX_P_DLY_PP:
		case FX_P_DLY_SAT:  sprintf(buffer,"%s",v>=0.5f?"ON":"OFF") ; break ;
		default:            sprintf(buffer,"%.2f",v) ; break ;  // FBK/MIX/WID
		}
		drawMasterFxRow(labels[p],buffer,selected,ml.labelX,y,ml.valueX) ;
	}
}

void MixerView::drawReverbPage(const char *title) {
	char buffer[16] ;
	static const char *labels[7]={"BYPASS","PREDELAY","DECAY","SIZE",
	                              "DAMPING","WIDTH","MODE"} ;
	static const int ids[7]={FX_P_RVB_BYP,FX_P_RVB_PRE,FX_P_RVB_DEC,
	                         FX_P_RVB_SIZ,FX_P_RVB_DMP,FX_P_RVB_WID,
	                         FX_P_RVB_MODE} ;
	// RC5: centered 7-row block in the safe menu band 3..25 (label column
	// 8 = "PREDELAY", value column 8, two-cell spacing).
	MenuLayout ml=UiDraw::MakeCenteredMenuLayout(7,8,8,2) ;
	// RC6: the page title sits on the row just above the centered block.
	UiDraw::DrawCenteredTitleAt(*this,ml.startY-1,title) ;
	for (int p=0;p<7;p++) {
		int id=ids[p] ;
		float v=fxGet(id) ;
		bool selected=(fxRowForId(id)==fxRow_) ;
		int y=ml.startY+p ;
		if (id==FX_P_RVB_BYP) {
			UiDraw::DrawBypassRow(*this,ml.labelX,ml.valueX,y,v>=0.5f,selected) ;
			continue ;
		}
		switch(id) {
		case FX_P_RVB_PRE:  sprintf(buffer,"%4.0f ms",v) ; break ;
		case FX_P_RVB_DEC:  sprintf(buffer,"%.2f s",v) ; break ;
		case FX_P_RVB_MODE: sprintf(buffer,"%s",v>=0.5f?"NORMAL":"ECO") ; break ;
		default:            sprintf(buffer,"%.2f",v) ; break ;  // SIZ/DMP/WID
		}
		drawMasterFxRow(labels[p],buffer,selected,ml.labelX,y,ml.valueX) ;
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

void MixerView::drawEqRow(int id,int labelX,int valueX,int y) {
	GUITextProperties props ;
	char buffer[16] ;
	bool selected=(fxRowForId(id)==fxRow_) ;
	bool on=(fxGet(id)>=0.5f) ;
	// RC4 P2 + RC5: EQ Bypass renders through the unified row; the band EN
	// toggles keep the "[ ON ]" inline style (they are a per-band enable).
	// RC5 splits label and value into the centered columns.
	if (id==FX_P_EQ_BYP) {
		UiDraw::DrawBypassRow(*this,labelX,valueX,y,on,selected) ;
		return ;
	}
	SetColor(selected?CD_HILITE2:CD_NORMAL) ;
	props.invert_=selected ;
	if (id==FX_P_EQ_LOW_EN||id==FX_P_EQ_MID_EN||id==FX_P_EQ_HI_EN) {
		sprintf(buffer,"[ %s ]",on?"ON":"OFF") ;
	} else if (fxUsesCurve(id)) {
		sprintf(buffer,"%6.0f Hz",fxGet(id)) ;
	} else if (id==FX_P_EQ_LOW_GAI||id==FX_P_EQ_MID_GAI||id==FX_P_EQ_HI_GAI) {
		sprintf(buffer,"%+5.1f dB",fxGet(id)) ;
	} else {
		sprintf(buffer,"%5.2f",fxGet(id)) ;
	}
	DrawString(labelX,y,eqParamLabel(id),props) ;
	DrawString(valueX,y,buffer,props) ;
	props.invert_=false ;
	SetColor(CD_NORMAL) ;
}

void MixerView::drawEqPage(const char *title) {
	// Title is drawn here, at the top of the centered block (row 5 for the
	// 16-row EQ block); hints moved to the HelpOverlay (SELECT+R1).  Rows:
	// bypass first, then each band is a header line followed by EN/FRQ/GAIN/Q.
	// RC5 centers the whole 16-row block in the safe menu band 3..25 (label
	// column 6, value column 9, two-cell spacing) so the full EQ stays on
	// screen and balanced.
	MenuLayout ml=UiDraw::MakeCenteredMenuLayout(16,6,9,2) ;
	UiDraw::DrawCenteredTitleAt(*this,ml.startY-1,title) ;
	GUITextProperties props ;
	drawEqRow(FX_P_EQ_BYP,ml.labelX,ml.valueX,ml.startY) ;
	const int bandBase[3]={FX_P_EQ_LOW_EN,FX_P_EQ_MID_EN,FX_P_EQ_HI_EN} ;
	int yHeader=ml.startY+1 ;
	for (int b=0;b<3;b++) {
		SetColor(CD_NORMAL) ;
		props.invert_=false ;
		DrawString(ml.labelX,yHeader,eqBandName(bandBase[b]),props) ;
		for (int p=0;p<4;p++) {
			drawEqRow(bandBase[b]+p,ml.labelX,ml.valueX,yHeader+1+p) ;
		}
		yHeader+=5 ;
	}
}

// TREEFROG_COMP_MENU_V1 (PLAN_FX_REDESIGN_ES.md, Fase 13): dedicated COMP
// menu.  BYP is the first row so it is never off-screen; rows are centered
// with a fixed value column, ratio shows as x:1, booleans as ON/OFF, and the
// GR meter (a readout, not a selectable row) stays visible below the
// parameters.  No clipping indicator: the engine exposes no reliable real
// audio clip reading (GetRtViolations is buffer RT telemetry, must stay 0).
void MixerView::drawCompPage(const char *title) {
	// RC5: the whole 9-row block is centered in the safe menu band 3..25
	// (label column 11 = "Stereo Link", value column 8, three-cell spacing so
	// the 14-cell "Gain Reduction" line below fits beside its value); the GR
	// meter stays visible one row below the last parameter.
	MenuLayout ml=UiDraw::MakeCenteredMenuLayout(9,11,8,3) ;
	UiDraw::DrawCenteredTitleAt(*this,ml.startY-1,title) ;
	GUITextProperties props ;
	char buffer[20] ;
	const char *labels[9]={"Bypass","Threshold","Ratio","Knee",
	                       "Attack","Release","Makeup","Stereo Link","Soft Clip"} ;
	for (int p=0;p<9;p++) {
		int id=FX_P_CMP_BYP+p ;  // enum is BYP first, contiguous
		const FxParamSpec &spec=kFxParams_[id] ;
		bool selected=(fxRowForId(id)==fxRow_) ;
		float v=fxGet(id) ;
		int y=ml.startY+p ;
		// RC4 P2 + RC5: COMP bypass through the unified row; the rest keep the
		// label/value two-column layout on the centered block.
		if (p==0) {
			UiDraw::DrawBypassRow(*this,ml.labelX,ml.valueX,y,v>=0.5f,selected) ;
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
		DrawString(ml.labelX,y,labels[p],props) ;
		DrawString(ml.valueX,y,buffer,props) ;
		props.invert_=false ;
		SetColor(CD_NORMAL) ;
	}
	// GR meter: always visible below the parameters.
	float gr=fp2fl(FxEngine::FxEngine::GetInstance().GetCompGainReductionDb()) ;
	float mag=(gr<0.0f)?-gr:gr ;
	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	sprintf(buffer,"-%05.1f dB",mag) ;
	int grY=ml.startY+9 ;
	DrawString(ml.labelX,grY,"Gain Reduction",props) ;
	DrawString(ml.valueX,grY,buffer,props) ;
}

void MixerView::drawMixReturns(int y) {
	// TREEFROG_FX_PAGES_V3 (PLAN_FX_REDESIGN_ES.md, Fase 9): the MIX page
	// shows the master FX returns (wet levels of the delay/reverb into the
	// master bus) instead of the old per-track D/R send readouts, which are
	// now per-instrument (Fase 6/7) and edited in InstrumentView.  The
	// return level being edited by UP/DOWN is highlighted.  TREEFROG_MIXER_RET_TOP_V1
	// (Bacon 1.1.1 V16): the row is a header on the top safe-band row (y=3),
	// above the CUE scale and bars, freeing the bottom of the screen for the
	// taller 15-cell bars.
	GUITextProperties props ;
	char buffer[16] ;
	FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
	int dly=fxReturnPercent(fx.GetDelayReturn()) ;
	int rvb=fxReturnPercent(fx.GetReverbReturn()) ;
	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	// TREEFROG_FX_PAGES_V4 (Bacon 1.1.1): the return readout is centered
	// over the meter bank: "RET D:xxx R:xxx FX RETURNS" (26 chars) starts at
	// column 7, leaving a 7-cell margin on both sides.
	DrawString(7,y,"RET",props) ;
	SetColor((fxEditTarget_==1)?CD_HILITE2:CD_NORMAL) ;
	props.invert_=(fxEditTarget_==1) ;
	sprintf(buffer,"D:%3d",dly) ;
	DrawString(11,y,buffer,props) ;
	SetColor((fxEditTarget_==2)?CD_HILITE2:CD_NORMAL) ;
	props.invert_=(fxEditTarget_==2) ;
	sprintf(buffer,"R:%3d",rvb) ;
	DrawString(17,y,buffer,props) ;
	props.invert_=false ;
	SetColor(CD_NORMAL) ;
	DrawString(23,y,"FX RETURNS",props) ;
}

void MixerView::drawFxPages() {
	GUITextProperties props ;
	SetColor(CD_NORMAL) ;
	props.invert_=false ;
	if (fxPage_==FX_PAGE_MIX) {
		// RC6 (compact single-cell meters) + TREEFROG_MIXER_ZERO_DB_CLIP_V5
		// (Bacon 1.1.1): the MIX page lays out 10 columns: the static CUE
		// scale (+3/0/-6/-12/-24/-36 dB, compact, right-aligned to the
		// master), the MST live bar, the 8 channel bars one cell each 4
		// columns apart, and the CH/VL labels right of the last channel.
		// The bank spans the CUE scale at x=0..1 .. the CH label at x=38..39.
		// The whole block (labels, bars, volume numbers, pan/mute markers and
		// the centered FX RETURNS line) stays in the safe band 3..25 of the
		// 40x30 screen.  TREEFROG_MIXER_TALL_BARS_V1 (Bacon 1.1.1 V16): the
		// 15-cell bars reclaim the rows freed by the RET/FX RETURNS move to
		// the top: header at 3, labelY 4, bars 5..19, volumes at 21.
		const int masterX=4 ;
		const int channel0X=8 ;
		const int channelPitch=4 ;
		const int chLabelX=38 ;
		const int labelY=4 ;
		const int barHeight=15 ;
		const int numY=labelY+barHeight+2 ;
		const int retY=3 ;
		DrawString(chLabelX,labelY,"CH",props) ;
		DrawString(chLabelX,numY,"VL",props) ;
		// TREEFROG_MIXER_ZERO_DB_CLIP_V5 (Bacon 1.1.1):
		// Static CUE scale drawn to the LEFT of the master, right-aligned to
		// the master column so the marks sit as close to the bars as possible
		// (compact 2-3 cell labels).  The labels mark the +3, 0, -6, -12, -24
		// and -36 dB rows of the 15-cell bar using the same mixVULevel mapping
		// the master and channel bars use, so the 0 dB row is exactly the row
		// where the fills sit at volume 100 and the +3 row (red) is the cell
		// where they turn red.  The scale never moves with the volume; the
		// bars move against this fixed reference.
		SetColor(CD_NORMAL) ;
		DrawString(masterX-3,labelY,"C",props) ;
		SetColor(CD_ERROR) ;
		DrawString(masterX-3,labelY+1+0,"+3",props) ;
		SetColor(CD_HILITE2) ;
		DrawString(masterX-3,labelY+1+1,"0",props) ;
		SetColor(CD_HILITE1) ;
		DrawString(masterX-3,labelY+1+2,"-6",props) ;
		DrawString(masterX-4,labelY+1+3,"-12",props) ;
		DrawString(masterX-4,labelY+1+6,"-24",props) ;
		DrawString(masterX-4,labelY+1+14,"-36",props) ;
		drawMasterBar(masterX,labelY,barHeight) ;
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			drawVolumeBar(i,channel0X+i*channelPitch,labelY,barHeight) ;
		}
		drawMixReturns(retY) ;
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
	// RC4 P3: "R+UP Song" navigation hint retired from the title row;
	// documented in HelpRegistry (MIXER section, SELECT+R1).
	DrawString(pos._x,pos._y,"Mixer",props) ;

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
		// TREEFROG_MIXER_VU_STOP_RESET_V1 (Bacon 1.1.1 V14): the player keeps
		// the last peak values when the transport stops, so the bars used to
		// freeze at the last position.  Sample 0 while stopped; the release
		// decay then pulls every bar back to 0.
		bool running=player->IsRunning() ;
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			// TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1): each side of a
			// channel is smoothed independently from its own post-pan peak.
			float measuredL=running?player->GetChannelPeakL(i):0.0f ;
			if (measuredL>vuDisplayL_[i]) {
				vuDisplayL_[i]=measuredL ;
			} else {
				vuDisplayL_[i]*=0.6f ;
				if (vuDisplayL_[i]<0.001f) vuDisplayL_[i]=0.0f ;
			}
			float measuredR=running?player->GetChannelPeakR(i):0.0f ;
			if (measuredR>vuDisplayR_[i]) {
				vuDisplayR_[i]=measuredR ;
			} else {
				vuDisplayR_[i]*=0.6f ;
				if (vuDisplayR_[i]<0.001f) vuDisplayR_[i]=0.0f ;
			}
		}
	}

	// TREEFROG_MIXER_LIVE_VU_V2 (H38.7):
	// Frame updates are independent of Player transport (same as the USB-C
	// record meter): request a redraw every frame so the VU bars track the
	// channel activity in real time, including quick mute/stop decay.
	// TREEFROG_MIXER_ZERO_DB_CLIP_V4 (Bacon 1.1.1):
	// No clip latch here: the bars compute their over-0 dB state directly
	// from the displayed level (filledCells vs the 0 dB row) every frame,
	// which tracks the real clipping of the output path even when the core's
	// internal hard-clip flag (Player::Clipped) is never raised.
	++frameRefreshDivider_ ;
	if (frameRefreshDivider_ >= 1) {
		frameRefreshDivider_ = 0 ;
		isDirty_ = true ;
		((AppWindow &)w_).SetDirty() ;
	}
} ;
