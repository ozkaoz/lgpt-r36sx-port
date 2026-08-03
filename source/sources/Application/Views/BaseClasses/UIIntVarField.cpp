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

	// TREEFROG_FX_SEND_BAR_V1 (Fase 8): percent-bar rendering for sends.
	// The value is the percentage (0..100); -1 means "inherit" (no bar).
	if (barLabel_) {
		int value=src_.GetInt()+displayOffset_ ;
		if (value<0) {
			sprintf(buffer,"%s: INH   ",barLabel_) ;
		} else {
			int v=value ;
			if (v<0) v=0 ;
			if (v>100) v=100 ;
			int filled=(barWidth_*v)/100 ;
			char bar[32] ;
			int i=0 ;
			bar[i++]='[' ;
			for (int b=0;b<barWidth_;b++) bar[i++]=(b<filled)?'=':'-' ;
			bar[i++]=']' ;
			bar[i]=0 ;
			sprintf(buffer,"%s: %s %3d%%",barLabel_,bar,value) ;
		}
		w.DrawString(buffer,position,props) ;
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
