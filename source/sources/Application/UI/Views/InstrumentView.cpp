#include "InstrumentView.h"
extern "C" void TreeFrogInputTrace_LogView(
    const char *phase,
    int viewType,
    int hasModal,
    unsigned short incomingMask,
    unsigned short activeMask,
    int pressed,
    int audioLatched);

#include "Application/UI/Views/ModalDialogs/UsbRecordModal.h"
#include "Application/Instruments/MidiInstrument.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Model/Config.h"
#include "Application/Player/Player.h"
#include "BaseClasses/UIBigHexVarField.h"
#include "BaseClasses/UIIntVarOffField.h"
#include "BaseClasses/UINoteVarField.h"
#include "BaseClasses/UiDraw.h"
#include "BaseClasses/UIStaticField.h"
#include "Foundation/Variables/Variable.h"
#include "ModalDialogs/ImportSampleDialog.h"
#include "ModalDialogs/SampleChopperModal.h"
#include "ModalDialogs/InstrumentEqModal.h"
#include "ModalDialogs/MessageBox.h"
#include "System/System/System.h"
#include "System/FileSystem/FileSystem.h"
#include <stdio.h>
#include <string.h>
#include <string>

// U2.52.0: destructive sample deletion was moved out of Instrument fields.
// It is now available only in the Import/Listen/Manage/Exit sample browser,
// where the selected source path and project identity can be validated safely.

InstrumentView::InstrumentView(GUIWindow &w,ViewData *data):FieldView(w,data) {

	project_=data->project_ ;
	lastFocusID_=0 ;
	current_=0 ;
	onInstrumentChange() ;
}


InstrumentView::~InstrumentView() {
	if (current_) {
		current_->RemoveObserver(*this) ;
		current_=0 ;
	}
}

InstrumentType InstrumentView::getInstrumentType() {
	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	I_Instrument *instrument=bank->GetInstrument(i) ;
    return instrument->GetType() ;
} ;

void InstrumentView::onInstrumentChange() {

	ClearFocus() ;

	I_Instrument *old=current_ ;

	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	current_=bank->GetInstrument(i) ;

	if ((old)&&(current_!=old)) {
		old->RemoveObserver(*this) ;
	} ;
	T_SimpleList<UIField>::Empty() ;

	InstrumentType it=getInstrumentType() ;

    switch (it) {
		case IT_MIDI:
			fillMidiParameters() ;
			break ;
		case IT_SAMPLE:
			fillSampleParameters() ;
			break ;
	} ;

	SetFocus(T_SimpleList<UIField>::GetFirst()) ;
	// TREEFROG_MIXER_ACTION_MENU_V1 (Bacon 1.1.1 V13): the mixer L1+A track
	// menu can request a landing section by variable ID; it wins over the
	// remembered field and is consumed.
	unsigned int hint=viewData_->instrumentFocusHint_ ;
	viewData_->instrumentFocusHint_=0 ;
	IteratorPtr<UIField> it2(T_SimpleList<UIField>::GetIterator()) ;
	for (it2->Begin();!it2->IsDone();it2->Next()) {
        UIField &rawField=it2->CurrentItem() ;
        if (rawField.IsStatic()) continue ;
        UIIntVarField &field=(UIIntVarField &)rawField ;
        if (hint!=0) {
            if (field.GetVariableID()==hint) {
                SetFocus(&field) ;
                break ;
            }
        } else if (field.GetVariableID()==lastFocusID_) {
            SetFocus(&field) ;
            break ;
        }
    } ;
	if ((current_)&&(current_!=old)) {
		current_->AddObserver(*this) ;
	}
} ;

void InstrumentView::fillSampleParameters() {

	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	I_Instrument *instr=bank->GetInstrument(i) ;
	SampleInstrument *instrument=(SampleInstrument *)instr  ;
	// RC6: the two-column form (labels at x=6, x+16=22) is centered on the
	// 40-cell screen; GetAnchor()._x would leave it ~3-4 cells right.
	GUIPoint position=GetAnchor() ;
	position._x -= 4 ;

	// TREEFROG_FX_BLOCKS_V1 (PLAN_FX_REDESIGN_ES.md, Fase 8):
	// Reorganized as vertical blocks.  Row numbers below are screen rows
	// (GetAnchor()._y==4).  Block headers are drawn by DrawView(), NOT
	// inserted as UIStaticField, so T_SimpleList<UIField>::GetFirst() stays
	// the sample field and GetLast() stays the table field (L2+A cut/clear
	// relies on both).
	//
	// Row 4  : INSTRUMENT (header)
	// Row 5  : sample
	// Row 6  : volume | pan
	// Row 7  : root note | detune
	// Row 8  : FILTER (header)
	// Row 9  : type | mode
	// Row 10 : cutoff | reso
	// Row 11 : attenuate
	// Row 12 : BITCRUSHER (header)
	// Row 13 : bit depth | drive
	// Row 14 : downsample
	// Row 15 : PLAYBACK (header)
	// Row 16 : interpolation | loop mode
	// Row 17 : slices
	// Row 18 : start
	// Row 19 : loop start
	// Row 20 : loop end
	// Row 21 : EFFECT SENDS (header)
	// Row 22 : DRY [bar]
	// Row 23 : DELAY [bar]
	// Row 24 : REVERB [bar]
	// Row 25 : AUTOMATION (header)
	// Row 26 : table auto | table

	Variable *v ;
	UIIntVarField *f1 ;
	UIIntVarField *f2 ;
	GUIPoint col2 ;

	// ----------------------------------------------------------
	// Block 1: INSTRUMENT (source + level)
	// ----------------------------------------------------------
	position._y+=1 ;  // skip header row 4
	v=instrument->FindVariable(SIP_SAMPLE) ;
	SamplePool *sp=SamplePool::GetInstance() ;
	int sampleMax=sp->GetNameListSize()-1 ;
	if (sampleMax<0) sampleMax=0 ;
	f1=new UIIntVarField(position,*v,"sample: %s",0,sampleMax,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	f1->SetFocus() ;

	position._y+=1 ;
	col2=position ;
	col2._x+=16 ;
	v=instrument->FindVariable(SIP_VOLUME) ;
	f1=new UIIntVarField(position,*v,"volume: %d",0,255,1,10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	v=instrument->FindVariable(SIP_PAN) ;
	f2=new UIIntVarField(col2,*v,"pan: %2.2X",0,0xFE,1,0x10) ;
	T_SimpleList<UIField>::Insert(f2) ;

	position._y+=1 ;
	col2=position ;
	col2._x+=16 ;
	v=instrument->FindVariable(SIP_ROOTNOTE) ;
	f1=new UINoteVarField(position,*v,"root note: %s",0,0x7F,1,0x0C) ;
	T_SimpleList<UIField>::Insert(f1) ;
	v=instrument->FindVariable(SIP_FINETUNE) ;
	f2=new UIIntVarField(col2,*v,"detune: %2.2X",0,255,1,0x10) ;
	T_SimpleList<UIField>::Insert(f2) ;

	// ----------------------------------------------------------
	// Block 2: FILTER (type/mode/cutoff/reso/attenuate)
	// ----------------------------------------------------------
	position._y+=2 ;  // skip header row 8
	col2=position ;
	col2._x+=16 ;
	v=instrument->FindVariable(SIP_FILTMIX) ;
	f1=new UIIntVarField(position,*v,"type: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	v=instrument->FindVariable(SIP_FILTMODE) ;
	f2=new UIIntVarField(col2,*v,"mode: %s",0,2,1,1) ;
	T_SimpleList<UIField>::Insert(f2) ;

	position._y+=1 ;
	col2=position ;
	col2._x+=16 ;
	v=instrument->FindVariable(SIP_FILTCUTOFF) ;
	f1=new UIIntVarField(position,*v,"cutoff: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;
	v=instrument->FindVariable(SIP_FILTRESO) ;
	f2=new UIIntVarField(col2,*v,"reso: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f2) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_ATTENUATE) ;
	f1=new UIIntVarField(position,*v,"attenuate: %d [%2.2X]",1,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	// ----------------------------------------------------------
	// Block 3: BITCRUSHER (bit depth / drive / downsample).
	// Deliberately labeled "bit depth", never "compressor".
	// ----------------------------------------------------------
	position._y+=2 ;  // skip header row 12
	col2=position ;
	col2._x+=16 ;
	v=instrument->FindVariable(SIP_CRUSH) ;
	f1=new UIIntVarField(position,*v,"bit depth: %d",1,0x10,1,4) ;
	T_SimpleList<UIField>::Insert(f1) ;
	v=instrument->FindVariable(SIP_CRUSHVOL) ;
	f2=new UIIntVarField(col2,*v,"drive: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f2) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_DOWNSMPL) ;
	f1=new UIIntVarField(position,*v,"downsample: %d",0,8,1,4) ;
	T_SimpleList<UIField>::Insert(f1) ;

	// ----------------------------------------------------------
	// Block 4: PLAYBACK
	// ----------------------------------------------------------
	position._y+=2 ;  // skip header row 15
	col2=position ;
	col2._x+=16 ;
	v=instrument->FindVariable(SIP_INTERPOLATION) ;
	f1=new UIIntVarField(position,*v,"interpolation: %s",0,1,1,1) ;
	T_SimpleList<UIField>::Insert(f1) ;
	v=instrument->FindVariable(SIP_LOOPMODE) ;
	f2=new UIIntVarField(col2,*v,"loop mode: %s",0,SILM_LAST-1,1,1) ;
	T_SimpleList<UIField>::Insert(f2) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_SLICES) ;
	f1=new UIIntVarField(position,*v,"slices: %2.2X",1,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	int sampleSize = instrument->GetSampleSize() ;
	if (sampleSize <= 0) sampleSize = 1 ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_START) ;
	f1=new UIBigHexVarField(position,*v,7,"start: %7.7X",0,sampleSize-1,16) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_LOOPSTART) ;
	f1=new UIBigHexVarField(position,*v,7,"loop start: %7.7X",0,sampleSize-1,16) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_END) ;
	f1=new UIBigHexVarField(position,*v,7,"loop end: %7.7X",0,sampleSize-1,16) ;
	T_SimpleList<UIField>::Insert(f1) ;

	// ----------------------------------------------------------
	// Block 5: EFFECT SENDS (percent bars, TREEFROG_FX_SEND_BAR_V1).
	// DRY 0..100 (default 100).  DELAY/REVERB 0..100 = the PERSISTED base
	// (default 0); -1 = inherit per-track Mixer send (legacy projects only).
	// Since Fase 15 only these edits write the persisted base; phrase/table
	// DLYS/RVBS automation modulates the live per-channel override instead.
	// ----------------------------------------------------------
	position._y+=2 ;  // skip header row 21
	v=instrument->FindVariable(SIP_DRY) ;
	f1=new UIIntVarField(position,*v,"dry:%3d",0,100,1,10) ;
	f1->SetBar("DRY",14) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1 ;
	v=instrument->FindVariable(SIP_DLY_SEND) ;
	f1=new UIIntVarField(position,*v,"dly:%3d",-1,100,1,10) ;
	f1->SetBar("DELAY",14) ;
	T_SimpleList<UIField>::Insert(f1) ;

position._y+=1 ;
	v=instrument->FindVariable(SIP_RVB_SEND) ;
	f1=new UIIntVarField(position,*v,"rvb:%3d",-1,100,1,10) ;
	f1->SetBar("REVERB",14) ;
	T_SimpleList<UIField>::Insert(f1) ;

	// ----------------------------------------------------------
	// Block 6: GRAPHIC EQ (TREEFROG_INSTRUMENT_GRAPHIC_EQ_V1).
	// A focused ENTER/ACTION opens the graphical 8-band editor Modal; the
	// EQ bypass can also be toggled from this field directly.
	// ----------------------------------------------------------
	position._y+=2 ;  // skip header row
	col2=position ;
	col2._x+=12 ;
	v=instrument->FindVariable(SIP_EQEN) ;
	f1=new UIIntVarField(position,*v,"EQ 8-B:%d",0,1,1,1) ;
	T_SimpleList<UIField>::Insert(f1) ;
	v=instrument->FindVariable(SIP_EQMASK) ;
	f2=new UIIntVarOffField(col2,*v,"mask:%2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f2) ;

	// ----------------------------------------------------------
	// Offline render FX (print fx / wet / pad).  Only compiled on
	// desktop builds: the R36SX build has no FFMPEG_ENABLED so these
	// fields do not exist there (PLAN_FX_REDESIGN_ES.md, Fase 8).
	// ----------------------------------------------------------
#ifdef FFMPEG_ENABLED
	position._y+=2 ;
	UIStaticField *offlineHeader=new UIStaticField(position,"OFFLINE RENDER FX: ") ;
	T_SimpleList<UIField>::Insert(offlineHeader) ;
	position._y+=1 ;
	col2=position ;
	col2._x+=12 ;
	v=instrument->FindVariable(SIP_PRINTFX) ;
	f1=new UIIntVarField(position,*v,"print fx: %s",0,3,1,2) ;
	T_SimpleList<UIField>::Insert(f1) ;
	v=instrument->FindVariable(SIP_IR_WET) ;
	f2=new UIIntVarField(col2,*v,"wet:%d%%",0,100,1,10) ;
	T_SimpleList<UIField>::Insert(f2) ;
	position._y+=1 ;
	v=instrument->FindVariable(SIP_IR_PAD) ;
	f1=new UIIntVarField(position,*v,"pad:%dms",0,5000,5,100) ;
	T_SimpleList<UIField>::Insert(f1) ;
#endif

	// ----------------------------------------------------------
	// Block 6: AUTOMATION (must remain the last fields so that
	// T_SimpleList<UIField>::GetLast() is the table field).
	// ----------------------------------------------------------
	position._y+=2 ;  // skip header row 25
	col2=position ;
	col2._x+=16 ;
	v=instrument->FindVariable(SIP_TABLEAUTO) ;
	f2=new UIIntVarField(position,*v,"table auto: %s",0,1,1,1) ;
	T_SimpleList<UIField>::Insert(f2) ;
	v=instrument->FindVariable(SIP_TABLE) ;
	f1=new UIIntVarOffField(col2,*v,"table: %2.2X",0x00,0x7F,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

} ;

void InstrumentView::fillMidiParameters() {

	int i=viewData_->currentInstrument_ ;
	InstrumentBank *bank=viewData_->project_->GetInstrumentBank() ;
	I_Instrument *instr=bank->GetInstrument(i) ;
	MidiInstrument *instrument=(MidiInstrument *)instr  ;
	// RC6: the form column is centered on the 40-cell screen (x=6).
	GUIPoint position=GetAnchor() ;
	position._x -= 4 ;

	Variable *v=instrument->FindVariable(MIP_CHANNEL) ;
	UIIntVarField* f1=new UIIntVarField(position,*v,"channel: %2.2d",0,0x0F,1,0x04,1) ;
	T_SimpleList<UIField>::Insert(f1) ;
	f1->SetFocus() ;

	position._y+=1;
	v=instrument->FindVariable(MIP_VOLUME) ;
	f1=new UIIntVarField(position,*v,"volume: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	position._y+=1;
	v=instrument->FindVariable(MIP_NOTELENGTH) ;
	f1=new UIIntVarField(position,*v,"length: %2.2X",0,0xFF,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

	v=instrument->FindVariable(MIP_TABLEAUTO) ;
	position._y+=2 ;
	UIIntVarField *f2=new UIIntVarField(position,*v,"automation: %s",0,1,1,1) ;
	T_SimpleList<UIField>::Insert(f2) ;

	position._y+=1;
	v=instrument->FindVariable(MIP_TABLE) ;
	f1=new UIIntVarOffField(position,*v,"table: %2.2X",0,0x7F,1,0x10) ;
	T_SimpleList<UIField>::Insert(f1) ;

} ;


void InstrumentView::warpToNext(int offset) {
	int instrument=viewData_->currentInstrument_+offset ;
	if (instrument>=MAX_INSTRUMENT_COUNT) {
		instrument=instrument-MAX_INSTRUMENT_COUNT ;
	} ;
	if (instrument<0) {
		instrument=MAX_INSTRUMENT_COUNT+instrument ;
	} ;
	viewData_->currentInstrument_=instrument ;
	onInstrumentChange() ;
	isDirty_=true ;
} ;

void InstrumentView::ProcessButtonMask(unsigned short mask,bool pressed) {

    TreeFrogInputTrace_LogView(
        "Instrument.ProcessButtonMask",
        (int)VT_INSTRUMENT,
        HasModal() ? 1 : 0,
        mask,
        mask,
        pressed ? 1 : 0,
        0);

	if (!pressed) return ;

	isDirty_=false ;

	// TREEFROG_FX_NAV_A_B_DEFAULT_V1 (PLAN_FX_REDESIGN_ES.md, Fase 6):
	// A+B resets the focused field's variable to its default.  Because A+B and
	// B+A share the same bitmask, the old "B+A cut instrument" action moved to
	// L2+A (see below).
	if ((mask & (EPBM_A | EPBM_B)) == (EPBM_A | EPBM_B) &&
	    !(mask & (EPBM_LEFT | EPBM_RIGHT | EPBM_UP | EPBM_DOWN |
	              EPBM_L | EPBM_R | EPBM_L2 | EPBM_R2 |
	              EPBM_X | EPBM_Y | EPBM_SELECT | EPBM_START))) {
		UIField *rawFocus=GetFocus() ;
		if (rawFocus && !rawFocus->IsStatic()) {
			UIIntVarField *field=(UIIntVarField *)rawFocus ;
			Variable &v=field->GetVariable() ;
			v.Reset() ;
			isDirty_=true ;
		}
		return ;
	}

    // U2.52.0: L1+Y deletion is intentionally not handled in Instrument.
    // Use the Import/Listen/Manage/Exit browser so the selected WAV path is
    // explicit and deletion can be confirmed before project and SD cleanup.

    // U2.40: dedicated USB sampler, independent from Chopper.
    const bool openUsbRecord =
        (mask & EPBM_R) && (mask & EPBM_RIGHT) &&
        !(mask & (EPBM_LEFT | EPBM_UP | EPBM_DOWN |
                  EPBM_A | EPBM_B | EPBM_X | EPBM_Y |
                  EPBM_L | EPBM_L2 | EPBM_R2 |
                  EPBM_SELECT | EPBM_START));
    if (openUsbRecord) {
        if (getInstrumentType() != IT_SAMPLE) {
            View::SetNotification("USB REC requires sample instrument");
            return;
        }
        Player *p = Player::GetInstance();
        if (p) {
            p->Stop();
            p->StopStreaming();
        }
        DoModal(new UsbRecordModal(*this, viewData_->currentInstrument_));
        isDirty_ = true;
        return;
    }

    // TREEFROG_U2_33_LISTEN_PREVIEW_AUDIO_FIX_STABLE
    // Restore stable semantics: plain B in Instrument is not listen/preview.
    // L2+B remains a hard stop for any sample preview started from Listen/Import.
    bool stopPreview = (mask & EPBM_L2) && (mask & EPBM_B) &&
        !(mask & (EPBM_LEFT | EPBM_RIGHT | EPBM_UP | EPBM_DOWN |
                  EPBM_A | EPBM_X | EPBM_Y | EPBM_L | EPBM_R |
                  EPBM_R2 | EPBM_SELECT | EPBM_START));
    if (stopPreview && getInstrumentType() == IT_SAMPLE) {
        Player::GetInstance()->StopStreaming();
        isDirty_ = true;
        return;
    }

    // TREEFROG_FX_NAV_A_B_DEFAULT_V1: cut instrument / clear table moved from
    // B+A to L2+A (A+B is now "reset field to default").
    bool cutInstrument = (mask & EPBM_L2) && (mask & EPBM_A) &&
        !(mask & (EPBM_LEFT | EPBM_RIGHT | EPBM_UP | EPBM_DOWN |
                  EPBM_B | EPBM_X | EPBM_Y | EPBM_L | EPBM_R |
                  EPBM_R2 | EPBM_SELECT | EPBM_START));
    if (cutInstrument) {
        if (getInstrumentType() == IT_SAMPLE &&
            GetFocus() == T_SimpleList<UIField>::GetFirst()) {
            int i = viewData_->currentInstrument_;
            InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
            I_Instrument *instr = bank->GetInstrument(i);
            instr->Purge();
            isDirty_ = true;
        }
        if (GetFocus() == T_SimpleList<UIField>::GetLast()) {
            int i = viewData_->currentInstrument_;
            InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
            I_Instrument *instr = bank->GetInstrument(i);
            Variable *v = instr->FindVariable(SIP_TABLE);
            v->SetInt(-1);
            isDirty_ = true;
        }
        return;
    }

	if (viewMode_==VM_NEW) {
		if (mask==EPBM_A) {
			UIField *rawFocus=GetFocus() ;
			if (!rawFocus || rawFocus->IsStatic()) { viewMode_=VM_NORMAL; return; }
			UIIntVarField *field=(UIIntVarField *)rawFocus ;
			Variable &v=field->GetVariable() ;
			switch(v.GetID()) {
				case SIP_SAMPLE:
				 {
                    // First check if the samplelib exists

					 Path sampleLib(SamplePool::GetInstance()->GetSampleLib()) ;
					 if (FileSystem::GetInstance()->GetFileType(sampleLib.GetPath().c_str())!=FT_DIR) {
						 MessageBox *mb=new MessageBox(*this,"Can't access the samplelib",MBBF_OK) ;
						 DoModal(mb) ;
					 } else { ;
						// Go to import sample

						 ImportSampleDialog *isd=new ImportSampleDialog(*this) ;
						 DoModal(isd) ;
					}
					break ;
				 }
				case SIP_TABLE:
				 {
					int next=TableHolder::GetInstance()->GetNext() ;
					if (next!=NO_MORE_TABLE) {
						v.SetInt(next) ;
						isDirty_=true ;
					}
					break ;
                }
                case SIP_PRINTFX: {
                    FxPrinter printer(viewData_);
                    isDirty_ = printer.Run();
                    View::SetNotification(printer.GetNotification());
                    break;
                }
                case SIP_EQEN: {
                    if (getInstrumentType() == IT_SAMPLE) {
                        DoModal(new InstrumentEqModal(*this,
                                 viewData_->currentInstrument_));
                        isDirty_ = true;
                    }
                    break ;
                }
                default:
                    break ;
			}
			mask&=(0xFFFF-EPBM_A) ;
		}
	}

	if (viewMode_==VM_CLONE) {
        if ((mask&EPBM_A)&&(mask&EPBM_L)) {
			UIField *rawFocus=GetFocus() ;
			if (!rawFocus || rawFocus->IsStatic()) { viewMode_=VM_NORMAL; return; }
			UIIntVarField *field=(UIIntVarField *)rawFocus ;
			mask&=(0xFFFF-EPBM_A) ;
			Variable &v=field->GetVariable() ;
			int current=v.GetInt() ;
			if (current==-1) return ;

			int next=TableHolder::GetInstance()->Clone(current) ;
			if (next!=NO_MORE_TABLE) {
				v.SetInt(next) ;
				isDirty_=true ;
			}
		}
		mask&=(0xFFFF-(EPBM_A|EPBM_L)) ;
	} ;

	if (viewMode_==VM_SELECTION) {
	} else {
		viewMode_=VM_NORMAL ;
	}

	FieldView::ProcessButtonMask(mask) ;

    Player *player=Player::GetInstance() ;

	// B Modifier

    if (mask & EPBM_B) {
        if (mask&EPBM_LEFT) warpToNext(-1) ;
		if (mask&EPBM_RIGHT) warpToNext(+1);
		if (mask&EPBM_DOWN) warpToNext(-16) ;
		if (mask&EPBM_UP) warpToNext(+16);
        if (mask&EPBM_L) {
            viewMode_=VM_CLONE ;
        } ;
    } else {

        // A modifier

        if (mask == EPBM_A) {
            UIField *rawFocus = GetFocus();
            if (rawFocus && !rawFocus->IsStatic()) {
                FourCC varID = ((UIIntVarField *)rawFocus)->GetVariableID();
                if ((varID == SIP_TABLE) || (varID == MIP_TABLE) ||
                    (varID == SIP_SAMPLE) || (varID == SIP_PRINTFX) ||
                    (varID == SIP_EQEN)) {
                    viewMode_ = VM_NEW;
                }
            }
        } else {

            // R Modifier

            if (mask & EPBM_R) {
                // Graphical Chopper U2.4.3 route: Instrument sample field + R+A.
                if ((mask & EPBM_A) && (getInstrumentType() == IT_SAMPLE) &&
                    (GetFocus() == T_SimpleList<UIField>::GetFirst())) {
                    int instrIndex = viewData_->currentInstrument_;
                    InstrumentBank *bank = viewData_->project_->GetInstrumentBank();
                    I_Instrument *instr = bank->GetInstrument(instrIndex);
                    SampleInstrument *sampleInstr = (SampleInstrument *)instr;
                    Variable *sampleVar = sampleInstr->FindVariable(SIP_SAMPLE);
                    int sampleIndex = sampleVar ? sampleVar->GetInt() : NO_SAMPLE;
                    SampleChopperModal *scm = new SampleChopperModal(*this,
                        instrIndex,
                        sampleIndex,
                        (sampleIndex == NO_SAMPLE) ? "" : sampleInstr->GetFileName(),
                        (sampleIndex == NO_SAMPLE) ? 0 : sampleInstr->GetSampleSize());
                    DoModal(scm);
                    /* U2.27: chopper destructive edits can change the in-memory
                       WavFile size without changing the sample index. Rebuild the
                       Instrument fields so Instrument-menu B preview uses current
                       START/END limits instead of stale pre-edit values. */
                    onInstrumentChange();
                    isDirty_ = true;
                    return;
                }

                if (mask & EPBM_LEFT) {
                    ViewType vt = VT_PHRASE;
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                if (mask & EPBM_DOWN) {

                    // Go to table view

                    ViewType vt = VT_TABLE2;

                    int i = viewData_->currentInstrument_;
                    InstrumentBank *bank =
                        viewData_->project_->GetInstrumentBank();
                    I_Instrument *instr = bank->GetInstrument(i);
                    int table = instr->GetTable();
                    if (table != VAR_OFF) {
                        viewData_->currentTable_ = table;
                    }
                    ViewEvent ve(VET_SWITCH_VIEW, &vt);
                    SetChanged();
                    NotifyObservers(&ve);
                }

                // if (mask&EPBM_RIGHT) {

                //	// Go to import sample

                //		ViewType vt=VT_IMPORT ;
                //		ViewEvent ve(VET_SWITCH_VIEW,&vt) ;
                //		SetChanged();
                //		NotifyObservers(&ve) ;
                //}

                if (mask & EPBM_START) {
                    player->OnStartButton(PM_PHRASE, viewData_->songX_, true,
                                          viewData_->chainRow_);
                }
            } else {
                // No modifier
                if (mask & EPBM_START) {
                    player->OnStartButton(PM_PHRASE, viewData_->songX_, false,
                                          viewData_->chainRow_);
                }
            }
        }
    }

    UIField *rawFocus = GetFocus();
    if (rawFocus && !rawFocus->IsStatic()) {
       UIIntVarField *field = (UIIntVarField *)rawFocus;
       lastFocusID_=field->GetVariableID() ;
    }

} ;

void InstrumentView::DrawView() {

	Clear() ;
    View::EnableNotification();

    GUITextProperties props;
    GUIPoint pos = GetTitlePosition();

    // Draw title

    char title[20];
    SetColor(CD_NORMAL);
    sprintf(title, "Instrument %2.2X", viewData_->currentInstrument_);
    DrawString(pos._x, pos._y, title, props);

    // TREEFROG_FX_BLOCKS_V1 (PLAN_FX_REDESIGN_ES.md, Fase 8): block
    // headers for fillSampleParameters().  Drawn here (not as fields) so
    // the field list's first/last stay sample/table for L2+A cut/clear.
    // RC3 (point 19): rendered through UiDraw::DrawSectionHeader.
    if (getInstrumentType()==IT_SAMPLE) {
        // RC6: block headers align with the centered field columns (x=6).
        GUIPoint hp = GetAnchor();
        hp._x -= 4 ;
        props.invert_ = false;
        UiDraw::DrawSectionHeader(*this, hp._x, hp._y, "INSTRUMENT");
        UiDraw::DrawSectionHeader(*this, hp._x, hp._y + 4, "FILTER");
        UiDraw::DrawSectionHeader(*this, hp._x, hp._y + 8, "BITCRUSHER");
        UiDraw::DrawSectionHeader(*this, hp._x, hp._y + 11, "PLAYBACK");
        UiDraw::DrawSectionHeader(*this, hp._x, hp._y + 17, "EFFECT SENDS");
        UiDraw::DrawSectionHeader(*this, hp._x, hp._y + 21, "AUTOMATION");
    }

    // Draw fields

    FieldView::Redraw();
    drawMap();
} ;

void InstrumentView::OnFocus() { onInstrumentChange(); }

void InstrumentView::Update(Observable &o,I_ObservableData *d) {
	onInstrumentChange() ;
}
