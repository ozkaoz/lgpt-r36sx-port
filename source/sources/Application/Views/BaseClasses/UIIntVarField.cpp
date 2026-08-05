// TREEFROG_V42_NO_WHITE_BOX_UI
#include "UIIntVarField.h"


#include "UIIntVarField.h"
#include "UIFramework/Interfaces/I_GUIGraphics.h"
#include "System/Console/Trace.h"
#include "Application/AppWindow.h"

#define abs(x) (x<0?-x:x)

UIIntVarField::UIIntVarField(
  GUIPoint &position,
  Variable &v,
  const char *format,
  int min,
  int max,
  int xOffset,
  int yOffset,
  int displayOffset)
:UIField(position)
,src_(v) 
{
	format_=format ;
	min_=min ;
	max_=max ;
	xOffset_=xOffset ;
	yOffset_=yOffset ;
  displayOffset_ = displayOffset;
	barLabel_=0 ;
	barWidth_=0 ;
} ;

// TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): A+B resets the option to its real
// default state (Variable::Reset restores the constructor default).
void UIIntVarField::OnABClick() {
	src_.Reset() ;
} ;

// TREEFROG_GLOBAL_UNDO_V1: int value snapshot for the undo history.
bool UIIntVarField::CaptureIntValue(int &out) {
	out=src_.GetInt() ;
	return true ;
} ;

// TREEFROG_GLOBAL_UNDO_V1: restore an int value snapshot.
void UIIntVarField::RestoreIntValue(int v) {
	src_.SetInt(v) ;
} ;

// TREEFROG_FX_SEND_BAR_V1 (Fase 8): switch this field to percent-bar mode.
void UIIntVarField::SetBar(const char *label,int width) {
	barLabel_=label ;
	barWidth_=width ;
} ;

void UIIntVarField::Draw(GUIWindow &w,int offset) {

	GUITextProperties props ;
	GUIPoint position=GetPosition() ;
	position._y+=offset ;

	if (focus_) {
		((AppWindow&)w).SetColor(CD_HILITE2) ;
		props.invert_ = false;
	} else {
		((AppWindow&)w).SetColor(CD_NORMAL) ;
	}
	char buffer[80] ;

	// TREEFROG_FX_SEND_BAR_V1 (Fase 8) + RC2 (PLAN_FX_REDESIGN_ES.md, point 5):
	// percent-bar rendering for the EFFECT SENDS rows.  The value is the
	// percentage (0..100); -1 means "inherit" (INH, no bar).  RC2 renders the
	// bar as a solid block of inverted cells (MixerView style) instead of the
	// ASCII "[====----]": filled cells are CD_NORMAL inverted, empty cells are
	// CD_HILITE1 non-inverted, so the send reads as a solid meter.  Default
	// off, so every existing UIIntVarField keeps its exact current rendering.
	if (barLabel_) {
		int value=src_.GetInt()+displayOffset_ ;
		ColorDefinition baseCol=focus_?CD_HILITE2:CD_NORMAL ;
		((AppWindow&)w).SetColor(baseCol) ;
		props.invert_=false ;
		if (value<0) {
			sprintf(buffer,"%s: INH",barLabel_) ;
			w.DrawString(buffer,position,props) ;
			GUIPoint clearPos=position ;
			clearPos._x+=(int)strlen(buffer) ;
			for (int c=0;c<barWidth_+5;c++) {  // clear stale bar + percent
				w.DrawString(" ",clearPos,props) ;
				clearPos._x++ ;
			}
			return ;
		}
		int v=value ;
		if (v<0) v=0 ;
		if (v>100) v=100 ;
		int filled=(barWidth_*v)/100 ;
		GUIPoint barPos=position ;
		char label[24] ;
		sprintf(label,"%s: ",barLabel_) ;
		w.DrawString(label,barPos,props) ;
		barPos._x+=(int)strlen(label) ;
		int i=0 ;
		for (;i<filled;i++) {
			((AppWindow&)w).SetColor(CD_NORMAL) ;
			props.invert_=true ;
			w.DrawString(" ",barPos,props) ;
			barPos._x++ ;
		}
		for (;i<barWidth_;i++) {
			((AppWindow&)w).SetColor(CD_HILITE1) ;
			props.invert_=false ;
			w.DrawString(" ",barPos,props) ;
			barPos._x++ ;
		}
		((AppWindow&)w).SetColor(baseCol) ;
		props.invert_=false ;
		sprintf(buffer," %3d%%",value) ;
		w.DrawString(buffer,barPos,props) ;
		return ;
	}

	Variable::Type type=src_.GetType() ;
	switch (type) {
		case Variable::INT:
			{
			int ivalue=src_.GetInt()+displayOffset_ ;
			sprintf(buffer,format_,ivalue,ivalue) ;
			}
			break ;
		case Variable::CHAR_LIST:
		case Variable::BOOL:
			{
			const char *cvalue=src_.GetString() ;
			sprintf(buffer,format_,cvalue) ;
			}
			break ;

		default:
			strcpy(buffer,"++wtf++");
	}
	w.DrawString(buffer,position,props) ;
} ;

void UIIntVarField::ProcessArrow(unsigned short mask) {
	int value=src_.GetInt() ;

	switch(mask) {
		case EPBM_UP:
			value+=yOffset_ ;
			break ;
		case EPBM_DOWN:
			value-=yOffset_ ;
			break ;
		case EPBM_LEFT:
			value-=xOffset_ ;
			break ;
  		case EPBM_RIGHT:
			value+=xOffset_ ;
			break ;
	} ;
	if (value<min_) {
		value=min_ ;
	} ;
	if (value>max_) {
		value=max_ ;
	}
	
	src_.SetInt(value) ;
} ;

FourCC UIIntVarField::GetVariableID() {
    return src_.GetID() ;
} ;

Variable &UIIntVarField::GetVariable() {
	return src_ ;
} ;
