// TREEFROG_V42_NO_WHITE_BOX_UI
#include "MixerView.h"
#include "Application/Model/Mixer.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Mixer/MixerMenu.h"
#include "Application/Model/Project.h"
#include "Application/Model/ProjectDatas.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Player/Player.h"
#include "Application/UI/Views/UIController.h"
#include "Application/UI/Views/BaseClasses/UiDraw.h"
#include "Application/UI/Views/BaseClasses/ModalView.h"
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
    // F3-4c: la estructura del menu (filas, etiquetas, clamps, accion) vive
    // en Application/Mixer/MixerMenu.h; aqui solo se dibuja.
    int rowCount = mixerMenuRowCount(masterMenu_) ;
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
    const int labelX = 1 ;
    const int valueX = 14 ;
    for (int i = 0; i < rowCount; i++) {
        int row = 1 + i ;
        bool selected = (i == item_) ;
        char label[16] ;
        char value[16] ;
        if (masterMenu_) {
            snprintf(label, sizeof(label), "%s",
                     mixerMenuLabel(true, i)) ;
            switch (i) {
            case 0:
                if (project) {
                    int idx = mixerMenuClampSoftclip(
                        project->GetSoftclip()) ;
                    snprintf(value, sizeof(value), "%s",
                             softclipStates[idx]) ;
                } else snprintf(value, sizeof(value), "-") ;
                break ;
            case 1:
                if (project) {
                    int idx = mixerMenuClampSoftclipGain(
                        project->GetSoftclipGain()) ;
                    snprintf(value, sizeof(value), "%s",
                             softclipGainStates[idx]) ;
                } else snprintf(value, sizeof(value), "-") ;
                break ;
            default:
                snprintf(value, sizeof(value), "A: open") ;
                break ;
            }
        } else {
            snprintf(label, sizeof(label), "%s",
                     mixerMenuLabel(false, i)) ;
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
    int rowCount = mixerMenuRowCount(masterMenu_) ;
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
            int idx = mixerMenuClampSoftclip(project->GetSoftclip() + delta) ;
            if (idx == project->GetSoftclip()) return ;
            mixer_.pushMixUndo(ME_SOFTCLIP, -1,
                               (float)project->GetSoftclip(),
                               (float)idx) ;
            Variable *var = project->FindVariable(VAR_SOFTCLIP) ;
            if (var) var->SetInt(idx, false) ;
            MixerService::GetInstance()->SetSoftclip(
                idx, project->GetSoftclipGain()) ;
        } else {
            int idx = mixerMenuClampSoftclipGain(
                project->GetSoftclipGain() + delta) ;
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
        pendingAction_ = mixerMenuActionForRow(masterMenu_, item_) ;
        if (pendingAction_ == 0) return ;
        EndModal(pendingAction_) ;
        return ;
    }
}

// F3-4a: la tabla de parametros (kFxParams_), FxParamSpec, mixVULevel,
// fxReturnPercent/fxReturnFromPercent y los helpers de navegacion/edicion
// viven en Application/Mixer/FxPages.h (capa pura, sin GUI/audio).

void MixerView::JumpToFxPage(FxPage page) {
	// F3-4d: rango + reset de fila en el navigator puro.
	navigator_.SetPage(page) ;
	isDirty_ = true ;
	((AppWindow &)w_).SetDirty() ;
}

// BACON_1.5_MIXER_JUMP_REAL_INSTRUMENT (U2.52.9, feedback #6): L1+A (track
// section) and R2+A on a mixer bar open the instrument that REALLY sounds on
// that channel, not the channel index (bar 3 used to open instrument 02
// while the track was playing instrument 20).  Resolution order:
//   1. Player::GetPlayedInstrument(channel) -- the hex instrument byte of
//      the row currently sounding; it persists after STOP, so it is also
//      the best guess when the transport is idle after a run;
//   2. the song data at the cursor row (chain/phrase instr of the track),
//      the same layout Player::updateSongPos reads;
//   3. the channel index as the last resort.
static int mixerChannelInstrumentIndex(ViewData *viewData, int channel) {
    Player *player = Player::GetInstance();
    if (player) {
        char *s = player->GetPlayedInstrument(channel);
        if (s && s[0] != ' ') {
            int hi = (s[0] >= 'A') ? s[0] - 'A' + 10 : s[0] - '0';
            int lo = (s[1] >= 'A') ? s[1] - 'A' + 10 : s[1] - '0';
            int index = hi * 16 + lo;
            if (index >= 0 && index < MAX_INSTRUMENT_COUNT) return index;
        }
    }
    if (viewData && viewData->song_) {
        int pos = viewData->songX_;
        if (pos < 0 || pos >= SONG_ROW_COUNT) pos = 0;
        unsigned char chain = viewData->song_->data_[channel + 8 * pos];
        if (chain != 0xFF && viewData->song_->chain_) {
            int chainPos = viewData->chainRow_;
            if (chainPos < 0 || chainPos >= 16) chainPos = 0;
            unsigned char phrase =
                viewData->song_->chain_->data_[16 * chain + chainPos];
            if (phrase != 0xFF && viewData->song_->phrase_) {
                unsigned char ins =
                    viewData->song_->phrase_->instr_[16 * phrase + chainPos];
                if (ins != 0xFF && ins < MAX_INSTRUMENT_COUNT) return ins;
            }
        }
    }
    return channel;
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
        // F3-4c: los hints FourCC de seccion vienen de MixerMenu.h.
        unsigned int hintId = mixerMenuSectionHint(section) ;
        int channel = mixer.viewData_->mixerCol_ ;
        if (channel >= 0 && channel < SONG_CHANNEL_COUNT) {
            // BACON_1.5_MIXER_JUMP_REAL_INSTRUMENT (U2.52.9, feedback #6):
            // open the instrument the channel REALLY plays (hex byte of the
            // sounding row, then the song/chain/phrase data at the cursor),
            // not the channel index.
            mixer.viewData_->currentInstrument_ =
                mixerChannelInstrumentIndex(mixer.viewData_, channel) ;
            mixer.viewData_->instrumentFocusHint_ = hintId ;
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
// The parameter table, FxParamSpec, mixVULevel and the return converters
// moved to FxPages.h in F3-4a (byte-identical golden; see
// docs/REFACTOR_ROADMAP_ES.md F3-4a).

// TREEFROG_FX_PAGES_V3 (PLAN_FX_REDESIGN_ES.md, Fase 9):
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
	// F3-4d: navigator_ arranca en su constructor (pagina MIX, fila 0,
	// target VOL); el estado del cursor FX ya no vive aqui.
	// F3-4b: meters_ (MixerMeters) se autoceroiza en su constructor; las
	// barras VU arrancan en 0.
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
	if (navigator_.Page()==FX_PAGE_MIX) {
		if (navigator_.EditTarget()==1) {
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
		if (navigator_.EditTarget()==2) {
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
	const ContextId ctx=(navigator_.Page()==FX_PAGE_MIX)?CTX_MIXER:CTX_MIXER_FX ;
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
			navigator_.CycleEditTarget() ;
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
	if (navigator_.Page()!=FX_PAGE_MIX) {
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
	// BACON_1.5_MIXER_JUMP_REAL_INSTRUMENT (U2.52.9, feedback #6): resolve
	// the instrument the channel REALLY plays (see mixerChannelInstrumentIndex
	// above) instead of the channel index.
	int channel=viewData_->mixerCol_ ;
	if (channel>=0 && channel<SONG_CHANNEL_COUNT) {
		viewData_->currentInstrument_=
			mixerChannelInstrumentIndex(viewData_,channel) ;
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
	// BACON_1.5_MIXER_FULLSCREEN (U2.53, feedback #7) + BACON_1.5_MIXER_VOL_BELOW
	// (U2.54, feedback #8): the 2-cell channel label is drawn two rows
	// above the bar (row y-2) and the volume number BELOW the bar (row
	// y+height+1, over the M/C marker): the strips read top-down like a
	// DAW (hex, bar, volume, M/C).
	DrawString(x-1,y-2,hex,props) ;
	props.invert_=false ;

	// TREEFROG_MIXER_LIVE_BAR_V4 (H38.7) + RC5:
	// The bar fill follows the real-time output level (GetChannelPeak), so a
	// low volume reads as a small wave and a loud volume as a big one. The
	// channel volume setting is drawn as an accent marker cell plus the
	// numeric value below. Selected bars stay purple, muted bars dim.  RC5:
	// each row is a single cell (one-column meter) so the 9 meters of the
	// MIX page fit the centered bank; totalCells == height.
	// BACON_1.5_VU_DB_SCALE (U2.52.9, feedback #6):
	// The bar fill = mixVULevel(peak) on the dB scale ((20*log10(p)+24)/24
	// over -24..0 dBFS, see FxPages.h; the scanned peak already includes
	// the track volume, so no extra volume factor).  A track at volume 20
	// on a full-scale instrument reads ~42% of the bar, and 0 dBFS reads
	// the FULL bar.  BACON_1.5_VU_TOP0DB (U2.59): the bar turns red
	// (CD_ERROR) only when the fill reaches the 0 dBFS ceiling (the top
	// cell), i.e. a real pre-clip level at/over 0 dBFS -- the condition
	// that produces the clipped sound, and the same 0 dB reference the
	// other consoles/DAWs use.
	// The 4-cell pitch separates
	// the 3-digit volume numbers ("100 100" instead of "100100").
	// TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1) + TREEFROG_MIXER_HALF_CELL_BARS_V2:
	// The channel bar is TWO independent bars sharing ONE cell column (8 px):
	// L at px 0..2, a 2-px dark seam at px 3..4, R at px 5..7.  The old
	// one-cell gap column (x+1) is gone and the bars are narrower, so they
	// fit exactly over the previous single column ("quepan sobre su propia
	// columna anterior").  Each side shows its own post-pan peak, so the pan
	// is visible in the bars themselves: center = both equal, hard left =
	// left full / right empty.  Each side turns red on its own when it
	// passes 0 dB (BACON_1.5_VU_TOP0DB: 0 dBFS = the top of the bar).  With
	// the 0..127 volume scale (127 =
	// +2.1 dB) the fill can push past 0 dB and reach the red zone.  Side 0
	// records L and paints the cell track; side 1 records R at the same x;
	// the pixels are drawn by PostFlushDraw() after the char flush.
	drawMeterBar(x,y,height,meters_.LevelL(channel),volume,selected,muted,props,CD_NORMAL,0,meterRecords_[channel]) ;
	drawMeterBar(x,y,height,meters_.LevelR(channel),volume,selected,muted,props,CD_NORMAL,1,meterRecords_[channel]) ;

	SetColor(selected?CD_HILITE2:(muted?CD_BORDER:CD_NORMAL)) ;
	props.invert_=selected ;
	// BACON_1.5_MIXER_VOL_BELOW (U2.54, feedback #8): the volume number
	// moves BELOW the bar (row y+height+1, the first free row under it),
	// with the pan/mute marker one row under the number (y+height+2) --
	// DAW order: hex label, bar, volume, M/C.
	sprintf(buffer,"%3d",volume) ;
	DrawString(x-1,y+height+1,buffer,props) ;
	props.invert_=false ;

	// TREEFROG_MIXER_PAN_V1 (Bacon 1.1.1):
	// The row under the volume numbers shows the stereo pan of every
	// channel: "L/R" + the hard-side value (0..100), "C" for center, drawn
	// under the number column (4 cells, so hard pans read "L100"/"R100"
	// without touching the neighbour).  A muted channel shows its "M"
	// marker instead -- its pan is inaudible anyway.  Center pans sit at
	// the same digit column as the L/R values (right-aligned value).
	int pan=mixer->GetChannelPan(channel) ;
	// BACON_1.5_MIXER_FULLSCREEN (U2.53, feedback #7) + BACON_1.5_MIXER_VOL_BELOW
	// (U2.54, feedback #8): the pan/mute marker row sits at the bottom of
	// the strip (y+height+2, the last free row above the played-notes
	// block at rows 27-29), with the volume number directly above it.
	if (muted) {
		SetColor(CD_HILITE2) ;
		DrawString(x,y+height+2,"M",props) ;
	} else {
		SetColor(selected?CD_HILITE2:CD_NORMAL) ;
		props.invert_=selected ;
		if (pan==0) {
			DrawString(x-1,y+height+2,"  C",props) ;
		} else if (pan<0) {
			sprintf(buffer,"L%3d",-pan) ;
			DrawString(x-1,y+height+2,buffer,props) ;
		} else {
			sprintf(buffer,"R%3d",pan) ;
			DrawString(x-1,y+height+2,buffer,props) ;
		}
		props.invert_=false ;
	}
}

void MixerView::drawMasterBar(int x,int y,int height) {
	Project *project=viewData_->project_ ;
	int volume=project?project->GetMasterVolume():100 ;
	GUITextProperties props ;
	char buffer[8] ;

	// BACON_1.5_VU_DB_SCALE (U2.52.9, feedback #6):
	// Master bars = mixVULevel(master peak) on the dB scale
	// ((20*log10(p)+24)/24 over -24..0 dBFS, see FxPages.h).  The peak
	// already includes the master fader (applied pre-scan on the master
	// bus, MixerService::SetMasterVolume), so the bar shows the real
	// loudness: one track at volume 20 on a full-scale instrument reads
	// ~42%, 0 dBFS reads the full bar.  BACON_1.5_VU_TOP0DB (U2.59): it
	// turns red (CD_ERROR) only when the fill reaches the 0 dBFS ceiling
	// (the top cell, the clip lamp), i.e. the pre-clip mix sum is really
	// at/over 0 dBFS (MixerService::GetMasterPeak, true pre-clip mix sum,
	// can exceed 1.0, which reads as the full red top cell).
	// Two bars are drawn (L at x, R at x+2, one-cell gap) so
	// the stereo balance of the mix is visible live.
	MixerService *ms=MixerService::GetInstance() ;

	// TREEFROG_MIXER_MASTER_BAR_V1 (H38.7):
	// Master (MST) bar drawn live on the left of the channel bars, in cyan so
	// it stands out from the white channel fills. When selected it lights
	// purple like a selected channel.
	// BACON_1.5_MIXER_FULLSCREEN (U2.53, feedback #7): MST label and volume
	// move above the bar like the channel strips (y-2 / y-1).
	SetColor(masterSelected_?CD_HILITE2:CD_PLAY) ;
	props.invert_=false ;
	DrawString(x-1,y-2,"MST",props) ;

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
	// BACON_1.5_MIXER_VOL_BELOW (U2.54, feedback #8): MST volume number
	// under the bar like the channel strips (row y+height+1).
	sprintf(buffer,"%3d",volume) ;
	DrawString(x-1,y+height+1,buffer,props) ;
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
	// 2-px-step meter from it.  F3-4b: derived in MixerMeters::BarLevel.
	float level=MixerMeters::BarLevel(peak,volume) ;
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
// 32 fine steps instead of 12 chunky 8-px blocks.  BACON_1.5_VU_TOP0DB
// (U2.59): the TOP CELL of the bar is the 0 dBFS zone: it fills solid red
// when the level reaches the full bar (a real pre-clip level at/over
// 0 dBFS), the clip lamp -- 0 dBFS is now the top of the bar like other
// consoles/DAWs.
void MixerView::PostFlushDraw() {
#if defined(PLATFORM_TREEFROG)
	AppWindow *app=(AppWindow *)&w_ ;
	uint16_t *fb=TreeFrogGetFramebuffer() ;
	if (!fb) return ;
	// TREEFROG_MIXER_MODAL_OVERLAY_V1 (Bacon 1.1.1 V14): modal menus (L1+A
	// mixer menu, help, ...) and the FX pages own the whole screen; the
	// pixel VU bars must not be repainted over them (menu letters were
	// interleaved with the bars).
	if (GetModal() || navigator_.Page() != FX_PAGE_MIX) return ;
	unsigned short seamC=app->ResolveColor565(CD_BACKGROUND) ;
	unsigned short borderC=app->ResolveColor565(CD_BORDER) ;
	unsigned short redC=app->ResolveColor565(CD_ERROR) ;
	for (int m=0;m<=SONG_CHANNEL_COUNT;m++) {
		MeterRecord &r=meterRecords_[m] ;
		if (!r.valid) continue ;
		int px=r.xCell*8 ;
		int py=r.yCell*8 ;
		// F3-4b: la metrica de la barra (totalPx/totalLevels, banda roja
		// 0 dB+, fill L/R en niveles de 2 px) la calcula MixerMeters::GeometryFor
		// (TREEFROG_MIXER_RED_BAND_TOP_V1 + TREEFROG_MIXER_BARS_BOTTOM_UP_V1);
		// aqui solo se resuelven colores y se escriben los pixels.
		MixerMeters::Geometry g=MixerMeters::GeometryFor(r.height,r.levelL,r.levelR) ;
		if (g.totalLevels<1) continue ;
		int totalPx=g.totalPx ;
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
			// 1-px gap between levels (bottom row of each 3-px group); the
			// red band is solid.  F3-4b: decision logica en
			// MixerMeters::RowStateFor.
			MixerMeters::RowState s=MixerMeters::RowStateFor(row,totalPx,g.redBandPx,g.filledLLevels,g.filledRLevels) ;
			unsigned short lC=(s.fillL&&s.inBand)?redC:
			                  ((s.fillL&&!s.gapRow)?fillC:seamC) ;
			unsigned short rC=(s.fillR&&s.inBand)?redC:
			                  ((s.fillR&&!s.gapRow)?fillC:seamC) ;
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
	// F3-4d: ciclo MIX->DELAY->REVERB->EQ->COMP->MIX + reset de fila en el
	// navigator puro.
	navigator_.CyclePage() ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

// F3-4a: fxIdOnPage/fxBypassId/fxCountOnPage/fxRowForId/fxIdForRow/
// fxUsesCurve/fxEditCurveValue now live in FxPages.h (pure layer); the
// mapping functions below delegate to it so MixerView keeps its exact
// public surface.  TREEFROG_MASTER_BYPASS_FIRST_V1 (PLAN_RC3... point 7)
// and TREEFROG_FX_EDIT_CURVE_V1 (Fase 12+14) comments moved with them.
bool MixerView::fxIdOnPage(int id,FxPage page) const {
	return ::fxIdOnPage(id,page) ;
}

int MixerView::fxBypassId(FxPage page) const {
	return ::fxBypassId(page) ;
}

int MixerView::fxCountOnPage(FxPage page) const {
	return ::fxCountOnPage(page) ;
}

int MixerView::fxRowForId(int id) const {
	return ::fxRowForId(id) ;
}

int MixerView::fxIdForRow(int row) const {
	return ::fxIdForRow(navigator_.Page(),row) ;
}

void MixerView::fxEditCurve(int id,int delta,bool coarse) {
	// F3-4d: la matematica (semitono/octava + floor + clamps) vive en
	// FxNavigator::EditValue; aqui solo se lee y se aplica al engine.
	float v=fxGet(id) ;
	fxSet(id,FxNavigator::EditValue(id,v,delta,coarse)) ;
}

void MixerView::fxMoveRow(int delta) {
	// F3-4d: wrap de la fila dentro de la pagina en el navigator puro.
	navigator_.MoveRow(delta) ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

// TREEFROG_FX_EDIT_CURVE_V1 (PLAN_FX_REDESIGN_ES.md, Fase 12 + Fase 14):
// Wide-range proportional parameters are edited on a musical/log curve,
// never with a linear 1/10 step: fine (L/R) steps by one semitone (x2^(1/12)),
// coarse (A+UP/DOWN) by one octave (x2).  The relative error is constant, so
// the whole range is traversable in a bounded number of presses and editing
// stays musically meaningful.  Applies to EQ frequencies and to every other
// wide-range time/ratio parameter (delay time, reverb pre-delay/decay,
// compressor attack/release/ratio).  F3-4a: fxUsesCurve/fxEditCurveValue
// moved to FxPages.h.
void MixerView::fxEditRow(int delta,bool coarse) {
	// F3-4d: la fila actual y la matematica de pasos (lineal fino/grueso,
	// bool-ish a paso 1, curva musical) viven en el navigator puro; aqui
	// solo se hacen el undo, la lectura/escritura del engine y el repintado.
	int targetId=navigator_.IdForRow() ;
	if (targetId<0) return ;
	// TREEFROG_GLOBAL_UNDO_V1: record the old float value for L1+X undo.
	float oldVal=fxGet(targetId) ;
	pushMixUndo(ME_FX,targetId,oldVal) ;
	float v=FxNavigator::EditValue(targetId,oldVal,delta,coarse) ;
	fxSet(targetId,v) ;
	// TREEFROG_GLOBAL_UNDO_V7: record the post-edit value for redo.  Golden:
	// solo el path lineal captura newValue (las ediciones en curva no lo
	// actualizaban en Bacon 1.2.1).
	if (!fxUsesCurve(targetId) && mixUndoCount_>0) mixUndo_[0].newValue=v ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

// TREEFROG_FX_NAV_A_B_DEFAULT_V1 (PLAN_FX_REDESIGN_ES.md, Fase 6):
// A+B restores the hovered parameter to its legacy default so the whole page
// can be brought back to the Fase 5 "all defaults" state without hunting.
void MixerView::fxResetRow() {
	// F3-4d: la fila actual y el vdef golden vienen del navigator puro.
	int targetId=navigator_.IdForRow() ;
	if (targetId<0) return ;
	// TREEFROG_GLOBAL_UNDO_V1: A+B on the FX pages restores vdef; record the
	// old value so L1+X brings it back.
	float vdef=FxNavigator::ResetValue(targetId) ;
	pushMixUndo(ME_FX,targetId,fxGet(targetId),vdef) ;
	fxSet(targetId,vdef) ;
	isDirty_=true ;
	((AppWindow &)w_).SetDirty() ;
}

// (Fase 9) nudgeDelayReturn/nudgeReverbReturn defined above; see MixerView.h.

float MixerView::fxGet(int id) const {
	// bacon-1.5 item 5: la lectura unica del motor vive en la API unificada
	// FxEngine::GetParam (UI, automatizacion y persistencia comparten el
	// mismo mapeo y clamp por id de kFxParams_).
	return FxEngine::FxEngine::GetInstance().GetParam(id) ;
}

void MixerView::fxSet(int id,float v) {
	// bacon-1.5 item 5: la escritura unica del motor vive en la API unificada
	// FxEngine::SetParam (clamp al rango de la tabla incluido).
	FxEngine::FxEngine::GetInstance().SetParam(id,v) ;
}

void MixerView::drawFxParamRow(int id,int x,int y,int col) {
	(void)col ;
	GUITextProperties props ;
	char buffer[16] ;
	const FxParamSpec &spec=kFxParams_[id] ;
	bool selected=(fxRowForId(id)==navigator_.Row()) ;
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
	// the page position [n/6] so the SELECT cycle is always visible.  RC3
	// (PLAN_RC3 point 2/26): the title is centered with UiDraw and the
	// permanent hint lines moved to HelpRegistry (SELECT+R1).
	char pageTitle[24] ;
	int pageNum=(int)page+1 ;
	switch(page) {
	case FX_PAGE_DELAY:  sprintf(pageTitle,"DELAY MASTER [%d/6]",pageNum) ; break ;
	case FX_PAGE_REVERB: sprintf(pageTitle,"REVERB MASTER [%d/6]",pageNum) ; break ;
	case FX_PAGE_EQ:     sprintf(pageTitle,"MASTER EQ [%d/6]",pageNum) ; break ;
	case FX_PAGE_EQ_EXT: sprintf(pageTitle,"MASTER EQ EXT [%d/6]",pageNum) ; break ;
	default:             sprintf(pageTitle,"MASTER COMP [%d/6]",pageNum) ; break ;
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
	// FXP_MASTER_EQ8 (bacon-1.5, item 2): EQ EXT page, dedicated self-labeled
	// 21-row menu (BYP + 5 bands x FRQ/GAI/Q/TYP).
	if (page==FX_PAGE_EQ_EXT) {
		drawEqExtPage(pageTitle) ;
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

// FXP_DESCRIPTORS_V1 (bacon-1.5, item 1): vista comun 0..100 % de una fila
// master.  Los continuos muestran el percent (primario) mas el valor
// natural (secundario); los signed con signo explicito; los switches se
// renderizan aparte (ON/OFF).
static void fxPctBuffer(char *out,int id,float v) {
	FxParamDescriptor d=fxDescForId(id) ;
	int p=fxDspToPercentId(id,v) ;
	if (d.kind_==FX_PARAM_SIGNED) sprintf(out,"%+3d%%",p) ;
	else sprintf(out,"%3d%%",p) ;
}

void MixerView::drawDelayPage(const char *title) {
	char buffer[16] ;
	// bacon-1.5 item 3: FREE/SYNC + DIVISION + LOW/HIGH CUT (11 rows).
	static const char *labels[11]={"BYPASS","TIME","FEEDBACK","MIX",
	                               "WIDTH","PING/PONG","SATURATE",
	                               "SYNC","DIVISION","LOW CUT","HIGH CUT"} ;
	static const int ids[11]={FX_P_DLY_BYP,FX_P_DLY_TIME,FX_P_DLY_FBK,
	                          FX_P_DLY_MIX,FX_P_DLY_WID,FX_P_DLY_PP,
	                          FX_P_DLY_SAT,FX_P_DLY_SYNC,FX_P_DLY_DIV,
	                          FX_P_DLY_LOW,FX_P_DLY_HIG} ;
	// FXP_DESCRIPTORS_V1: value column widened to 12 to carry the percent
	// (primary) plus the natural value (secondary) on the same row.
	MenuLayout ml=UiDraw::MakeCenteredMenuLayout(11,9,12,2) ;
	// RC6: the page title sits on the row just above the centered block.
	UiDraw::DrawCenteredTitleAt(*this,ml.startY-1,title) ;
	for (int p=0;p<11;p++) {
		int id=ids[p] ;
		float v=fxGet(id) ;
		bool selected=(fxRowForId(id)==navigator_.Row()) ;
		int y=ml.startY+p ;
		if (id==FX_P_DLY_BYP) {
			UiDraw::DrawBypassRow(*this,ml.labelX,ml.valueX,y,v>=0.5f,selected) ;
			continue ;
		}
		char pct[8] ;
		fxPctBuffer(pct,id,v) ;
		switch(id) {
		case FX_P_DLY_TIME: sprintf(buffer,"%s %4.0f ms",pct,v) ; break ;
		case FX_P_DLY_PP:
		case FX_P_DLY_SAT:  sprintf(buffer,"%s",v>=0.5f?"ON":"OFF") ; break ;
		case FX_P_DLY_SYNC: sprintf(buffer,"%s",v>=0.5f?"SYNC":"FREE") ; break ;
		case FX_P_DLY_DIV: {
			int div=(int)v ;
			if (div<0) div=0 ;
			if (div>=FxEngine::SDIV_COUNT) div=0 ;
			sprintf(buffer,"%s",FxEngine::kSyncDivisions[div].name) ;
		} break ;
		case FX_P_DLY_LOW:
		case FX_P_DLY_HIG:  sprintf(buffer,"%s %4.0f Hz",pct,v) ; break ;
		default:            sprintf(buffer,"%s %.2f",pct,v) ; break ;  // FBK/MIX/WID
		}
		drawMasterFxRow(labels[p],buffer,selected,ml.labelX,y,ml.valueX) ;
	}
}

void MixerView::drawReverbPage(const char *title) {
	char buffer[16] ;
	// bacon-1.5 item 3: input HP/LP (9 rows).
	static const char *labels[9]={"BYPASS","PREDELAY","DECAY","SIZE",
	                              "DAMPING","WIDTH","MODE","IN HP","IN LP"} ;
	static const int ids[9]={FX_P_RVB_BYP,FX_P_RVB_PRE,FX_P_RVB_DEC,
	                         FX_P_RVB_SIZ,FX_P_RVB_DMP,FX_P_RVB_WID,
	                         FX_P_RVB_MODE,FX_P_RVB_HP,FX_P_RVB_LP} ;
	// FXP_DESCRIPTORS_V1: value column widened to 12 (percent + natural).
	MenuLayout ml=UiDraw::MakeCenteredMenuLayout(9,8,12,2) ;
	// RC6: the page title sits on the row just above the centered block.
	UiDraw::DrawCenteredTitleAt(*this,ml.startY-1,title) ;
	for (int p=0;p<9;p++) {
		int id=ids[p] ;
		float v=fxGet(id) ;
		bool selected=(fxRowForId(id)==navigator_.Row()) ;
		int y=ml.startY+p ;
		if (id==FX_P_RVB_BYP) {
			UiDraw::DrawBypassRow(*this,ml.labelX,ml.valueX,y,v>=0.5f,selected) ;
			continue ;
		}
		char pct[8] ;
		fxPctBuffer(pct,id,v) ;
		switch(id) {
		case FX_P_RVB_PRE:  sprintf(buffer,"%s %4.0f ms",pct,v) ; break ;
		case FX_P_RVB_DEC:  sprintf(buffer,"%s %.2f s",pct,v) ; break ;
		case FX_P_RVB_MODE: sprintf(buffer,"%s",v>=0.5f?"NORMAL":"ECO") ; break ;
		case FX_P_RVB_HP:
		case FX_P_RVB_LP:   sprintf(buffer,"%s %4.0f Hz",pct,v) ; break ;
		default:            sprintf(buffer,"%s %.2f",pct,v) ; break ;  // SIZ/DMP/WID
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
	bool selected=(fxRowForId(id)==navigator_.Row()) ;
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
	} else {
		char pct[8] ;
		fxPctBuffer(pct,id,fxGet(id)) ;
		if (fxUsesCurve(id)) {
			sprintf(buffer,"%s %6.0f Hz",pct,fxGet(id)) ;
		} else if (id==FX_P_EQ_LOW_GAI||id==FX_P_EQ_MID_GAI||id==FX_P_EQ_HI_GAI) {
			sprintf(buffer,"%s %+5.1f dB",pct,fxGet(id)) ;
		} else {
			sprintf(buffer,"%s %5.2f",pct,fxGet(id)) ;
		}
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
	MenuLayout ml=UiDraw::MakeCenteredMenuLayout(16,6,13,2) ;
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

// FXP_MASTER_EQ8 (bacon-1.5, item 2): EQ EXT page.  Dedicated self-labeled
// 21-row menu (BYP + 5 bands x FRQ/GAI/Q/TYP) backed by ParametricEQ
// BAND3..BAND7.  There are no EN rows: a band is audible when gain != 0 dB
// or type != BELL (derived in ParametricEQ).  RC5 centers the block in the
// safe menu band 3..25 (label column 6, value column 13, two-cell spacing)
// so all 21 rows stay on screen; drawNotes()/drawMap() always render below.
static const char *eqExtTypeName(int t) {
	if (t<0) t=0 ;
	if (t>6) t=6 ;
	static const char *names[7]={"LSHELF","BELL","HSHELF","LPASS","HPASS",
	                             "BANDP","NOTCH"} ;
	return names[t] ;
}

void MixerView::drawEqExtRow(int id,int labelX,int valueX,int y) {
	GUITextProperties props ;
	char buffer[16] ;
	bool selected=(fxRowForId(id)==navigator_.Row()) ;
	bool on=(fxGet(id)>=0.5f) ;
	if (id==FX_P_EQX_BYP) {
		UiDraw::DrawBypassRow(*this,labelX,valueX,y,on,selected) ;
		return ;
	}
	SetColor(selected?CD_HILITE2:CD_NORMAL) ;
	props.invert_=selected ;
	char pct[8] ;
	fxPctBuffer(pct,id,fxGet(id)) ;
	if (fxUsesCurve(id)) {
		sprintf(buffer,"%s %6.0f Hz",pct,fxGet(id)) ;
	} else {
		switch (id) {
		case FX_P_EQX_B3_TYP:
		case FX_P_EQX_B4_TYP:
		case FX_P_EQX_B5_TYP:
		case FX_P_EQX_B6_TYP:
		case FX_P_EQX_B7_TYP:
			sprintf(buffer,"%s %-6s",pct,eqExtTypeName((int)fxGet(id))) ;
			break ;
		case FX_P_EQX_B3_GAI:
		case FX_P_EQX_B4_GAI:
		case FX_P_EQX_B5_GAI:
		case FX_P_EQX_B6_GAI:
		case FX_P_EQX_B7_GAI:
			sprintf(buffer,"%s %+5.1f dB",pct,fxGet(id)) ;
			break ;
		default:
			sprintf(buffer,"%s %5.2f",pct,fxGet(id)) ;
			break ;
		}
	}
	DrawString(labelX,y,kFxParams_[id].label,props) ;
	DrawString(valueX,y,buffer,props) ;
	props.invert_=false ;
	SetColor(CD_NORMAL) ;
}

void MixerView::drawEqExtPage(const char *title) {
	MenuLayout ml=UiDraw::MakeCenteredMenuLayout(21,6,13,2) ;
	UiDraw::DrawCenteredTitleAt(*this,ml.startY-1,title) ;
	// Row order matches fxRowForId: BYP first, then FRQ/GAI/Q/TYP per band
	// (B3..B7) so UP/DOWN walks the page in visual order.  The ids are
	// contiguous after the page bypass (enum order == row order).
	drawEqExtRow(FX_P_EQX_BYP,ml.labelX,ml.valueX,ml.startY) ;
	for (int b=0;b<5;b++) {
		int bandId=FX_P_EQX_B3_FRQ+4*b ;
		for (int p=0;p<4;p++) {
			drawEqExtRow(bandId+p,ml.labelX,ml.valueX,ml.startY+1+4*b+p) ;
		}
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
	// (label column 11 = "Stereo Link", value column 13 = percent + natural,
	// three-cell spacing so the 14-cell "Gain Reduction" line below fits
	// beside its value); the GR meter stays visible one row below the last
	// parameter.
	MenuLayout ml=UiDraw::MakeCenteredMenuLayout(13,11,13,3) ;
	UiDraw::DrawCenteredTitleAt(*this,ml.startY-1,title) ;
	GUITextProperties props ;
	char buffer[20] ;
	const char *labels[13]={"Bypass","Threshold","Ratio","Knee",
	                        "Attack","Release","Makeup","Stereo Link","Soft Clip",
	                        "Mix","SC Source","SC HPF","SC Amount"} ;
	for (int p=0;p<13;p++) {
		int id=fxIdForRow(p) ;  // rows map via the unified table (F3-4a)
		const FxParamSpec &spec=kFxParams_[id] ;
		bool selected=(fxRowForId(id)==navigator_.Row()) ;
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
		if (spec.vmax-spec.vmin<=1.5f && id!=FX_P_CMP_MIX && id!=FX_P_CMP_SCAMT) {
			sprintf(buffer,"%s",v>=0.5f?"ON":"OFF") ;
		} else {
			char pct[8] ;
			fxPctBuffer(pct,id,v) ;
			switch (id) {
			case FX_P_CMP_RAT: sprintf(buffer,"%s %3.1f:1",pct,v) ; break ;
			case FX_P_CMP_KNE: sprintf(buffer,"%s %5.1f dB",pct,v) ; break ;
			case FX_P_CMP_ATK: sprintf(buffer,"%s %5.1f ms",pct,v) ; break ;
			case FX_P_CMP_REL: sprintf(buffer,"%s %5.1f ms",pct,v) ; break ;
			case FX_P_CMP_MIX:
			case FX_P_CMP_SCAMT: sprintf(buffer,"%s %5.0f%%",pct,v*100.0f) ; break ;
			case FX_P_CMP_SCSRC: {
				// Discrete sidechain source: OFF / TRK n / DLY / RVB.
				int src=(int)(v+0.5f) ;
				if (src==0) sprintf(buffer,"%s OFF",pct) ;
				else if (src>=1 && src<=8) sprintf(buffer,"%s TRK %d",pct,src) ;
				else if (src==9) sprintf(buffer,"%s DLY RET",pct) ;
				else if (src==10) sprintf(buffer,"%s RVB RET",pct) ;
				else sprintf(buffer,"%s --",pct) ;
				break ;
			}
			case FX_P_CMP_SCFLT: {
				if (v<=30.0f) sprintf(buffer,"%s OPEN",pct) ;
				else sprintf(buffer,"%s %5.0f Hz",pct,v) ;
				break ;
			}
			default:           sprintf(buffer,"%s %+5.1f dB",pct,v) ; break ;  // THR, MKU
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
	int grY=ml.startY+13 ;
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
	SetColor((navigator_.EditTarget()==1)?CD_HILITE2:CD_NORMAL) ;
	props.invert_=(navigator_.EditTarget()==1) ;
	sprintf(buffer,"D:%3d",dly) ;
	DrawString(11,y,buffer,props) ;
	SetColor((navigator_.EditTarget()==2)?CD_HILITE2:CD_NORMAL) ;
	props.invert_=(navigator_.EditTarget()==2) ;
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
if (navigator_.Page()==FX_PAGE_MIX) {
		// RC6 (compact single-cell meters) + TREEFROG_MIXER_ZERO_DB_CLIP_V5
		// (Bacon 1.1.1): the MIX page lays out 10 columns: the static CUE
		// scale (0/-6/-12/-24 dB, compact, right-aligned to the master),
		// the MST live bar, the 8 channel bars one cell each 4 columns
		// apart.  The bank spans the CUE scale at x=0..1 .. the last
		// channel at x=36.
		// BACON_1.5_MIXER_FULLSCREEN (U2.53, feedback #7): fullscreen DAW
		// channel-strip layout.  Rows 0-3 keep the title/transport block
		// (title + Song/Live at x=0/21, RET/FX RETURNS at row 1, and the
		// clip/%/batt/time overlay at x=35..39); the played-notes boxes and
		// the view map are REMOVED (feedback #10: "quitar los recuadros de
		// notas y las letras SCPI PG M TT" -- drawNotes/drawMap calls are
		// gone), so the strips extend to the bottom of the screen.
		// The channel strips read top-down like a DAW: hex label at
		// row 4, the 18-cell live bar at rows 7..24, the volume number
		// BELOW the bar (row 25) and the pan/mute marker under it (row
		// 26) -- BACON_1.5_MIXER_VOL_BELOW (U2.54, feedback #8): the
		// volume reads under the bar like FL Studio (M/C sits at the
		// bottom, the number right above it).  The bars are 20% taller
		// than the old 15-cell ones and the CUE scale marks their dB rows.
		const int masterX=4 ;
		const int channel0X=8 ;
		const int channelPitch=4 ;
		const int labelY=4 ;        // hex label row (above the bars)
		const int barY=labelY+2 ;   // bars start on the row below the labels
		const int barHeight=18 ;    // bar cells = barY+1 .. barY+barHeight
		const int retY=1 ;
// BACON_1.5_VU_TOP0DB (U2.59, feedback #12): 0 dBFS is the TOP of the bar
// (the +3 dB headroom zone is REMOVED -- "si es necesario quitar +3DB se
// quita"), so the meter reads 0 dB exactly when the level reaches 0 dBFS,
// like the meters of other consoles/DAWs (SP404MKII, FL Studio): a
// full-scale sample at volume 128 pins the bar at 0 dB.
	// Static CUE scale drawn to the LEFT of the master, right-aligned to
	// the master column so the marks sit as close to the bars as possible
	// (compact 2-3 cell labels).  The bars are dB now (mixVULevel maps the
	// true peak onto its dB position over -24..0 dBFS, 1 dB per 1/24 of
	// the bar, see FxPages.h), so each mark sits on its REAL dB row of the
	// bar: "0" is the top cell (0 dBFS = full bar, red clip lamp), and
	// -6/-12/-24 dB sit at their log positions.  The scale never moves
	// with the volume; the bars move against this fixed reference.
	SetColor(CD_NORMAL) ;
	DrawString(masterX-3,labelY,"C",props) ;
	SetColor(CD_ERROR) ;
	DrawString(masterX-3,barY+1+0,"0",props) ;
	SetColor(CD_HILITE2) ;
	DrawString(masterX-3,barY+1+4,"-6",props) ;
	SetColor(CD_HILITE1) ;
	DrawString(masterX-4,barY+1+9,"-12",props) ;
	DrawString(masterX-4,barY+1+17,"-24",props) ;
		drawMasterBar(masterX,barY,barHeight) ;
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			drawVolumeBar(i,channel0X+i*channelPitch,barY,barHeight) ;
		}
		drawMixReturns(retY) ;
	} else {
		drawFxParamPage(navigator_.Page()) ;
	}
}

void MixerView::DrawView() {

	Clear() ;

	GUITextProperties props ;
	GUIPoint pos=GetTitlePosition() ;
	GUIPoint anchor=GetAnchor() ;

	SetColor(CD_NORMAL) ;
	// BACON_1.5_MIXER_NO_FRAME (U2.57b, feedback #10): the chopper frame is
	// REMOVED -- the fullscreen strips keep the DAW look without the thick
	// border ("el recuadro es demasiado ancho y de un color no correcto,
	// quitemos el recuadro pero mantengamos el aspecto full screen").  The
	// played-notes boxes (drawNotes) and the P G/SCPI/M TT map (drawMap)
	// stay removed (feedback #10): the strips own rows 4..26 of the whole
	// width.
	Player *player=Player::GetInstance() ;
	DrawString(pos._x,pos._y,"Mixer",props) ;
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
	// BACON_1.5_MIXER_FULLSCREEN (U2.53, feedback #7): the transport readout
	// (clip / % / batt / time) sits on the far right edge (x=35..39, rows
	// 0..3) so it never collides with the RET line (row 1) or the channel
	// strips (labels at rows 4-5).
	GUIPoint pos(35,0) ;
	
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

} ;

void MixerView::OnFrameUpdate(unsigned long frameClock) {
	(void)frameClock ;

	// TREEFROG_MIXER_VU_SMOOTH_V1 (H38.7):
	// Once per frame, blend the raw audio peak into the display levels used
	// by the bars. Attack is instant (a note pops straight up), release is a
	// smooth per-frame exponential fall (~0.6^12 empties a full bar in about
	// 12 frames) so the bars never jump full->empty in one frame. This runs
	// every frame regardless of transport, so the fall is also smooth when
	// the player is stopped.  F3-4b: el smoothing golden (ataque instantaneo,
	// release *0.6 con piso 0.001, muestreo a 0 al parar el transporte) vive
	// en MixerMeters::SmoothFrame; aqui solo se muestrean los picos del
	// Player y se pasa el flag de running.
	{
		Player *player=Player::GetInstance() ;
		// TREEFROG_MIXER_VU_STOP_RESET_V1 (Bacon 1.1.1 V14): the player keeps
		// the last peak values when the transport stops, so the bars used to
		// freeze at the last position.  Sample 0 while stopped; the release
		// decay then pulls every bar back to 0.
		bool running=player->IsRunning() ;
		float peakL[SONG_CHANNEL_COUNT] ;
		float peakR[SONG_CHANNEL_COUNT] ;
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			// TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1): each side of a
			// channel is smoothed independently from its own post-pan peak.
			peakL[i]=running?player->GetChannelPeakL(i):0.0f ;
			peakR[i]=running?player->GetChannelPeakR(i):0.0f ;
		}
		meters_.SmoothFrame(running,SONG_CHANNEL_COUNT,peakL,peakR) ;
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
