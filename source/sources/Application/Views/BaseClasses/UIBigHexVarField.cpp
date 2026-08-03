
// TREEFROG_V42_NO_WHITE_BOX_UI
#include "UIBigHexVarField.h"
#include "Application/AppWindow.h"

UIBigHexVarField::UIBigHexVarField(GUIPoint &position,Variable &v,int precision,const char *format,int min,int max,int power,bool wrap)
				 :UIIntVarField(position,v,format,min,max,0,0) {
	precision_=precision-1 ;
	power_=power ;
	position_=0 ;
	wrap_=wrap ;
} ; 

// TREEFROG_COMMAND_SPECS_V1 (Fase 6): re-target precision/range/format in
// place (used when the edited cell's command switches between the 4-digit
// legacy format and the 2-digit Fase 4 FX format).  position_ is reset to the
// least significant digit so the nibble cursor stays coherent after the
// switch.
void UIBigHexVarField::SetHexMode(int precision,const char *format,int min,int max,bool wrap) {
	precision_=(precision>0)?(unsigned int)(precision-1):0 ;
	format_=format ;
	min_=min ;
	max_=max ;
	wrap_=wrap ;
	position_=0 ;
} ; 

void UIBigHexVarField::Draw(GUIWindow &w,int offset) {

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
	int value=src_.GetInt() ;
	sprintf(buffer,format_,value) ;
	w.DrawString(buffer,position,props) ;
	
	int percentPos=-1 ;
	for (unsigned int i=0;i<strlen(format_);i++) {
		if (format_[i]=='%') {
			percentPos=i ;
			break ;
		} ;
	} ;
	if (percentPos>=0) {
		int offset=(precision_-position_)+percentPos ;
		buffer[offset+1]=0 ;
		position._x+=offset ;
		#if defined(PLATFORM_TREEFROG)
			/* U2.14: draw the active nibble with a distinct visual treatment so
			   Phrase/Table parameter editing shows which digit LEFT/RIGHT selected. */
			GUITextProperties digitProps = props ;
			digitProps.invert_ = focus_ ? true : props.invert_ ;
			((AppWindow&)w).SetColor(focus_ ? CD_HILITE1 : CD_NORMAL) ;
			w.DrawString(buffer+offset,position,digitProps) ;
		#else
			((AppWindow&)w).SetColor(CD_NORMAL) ;
			w.DrawString(buffer+offset,position,props) ;
		#endif
	}
} ;

void UIBigHexVarField::ProcessArrow(unsigned short mask) {

	int value=src_.GetInt() ;
	int offset=1 ;
	for (unsigned int i=0;i<position_;i++) {
		 offset*=power_ ;
	}
	
	switch(mask) {
		case EPBM_LEFT:
			if (position_<precision_) {
				position_++ ;
			} ;
			break ;
		case EPBM_RIGHT:
			if (position_>0) {
				position_-- ;
			} ;
			break ;
		case EPBM_UP:
			value+=offset ;
			break ;
			
		case EPBM_DOWN:
			value-=offset ;
			break ;
	} ;
	if (value>max_) {
		value=(wrap_)?value-max_+min_-1:max_ ;
	} ;
	if (value<min_) {
		value=(wrap_)?max_+(value-min_)+1:min_ ;
	} ;
	src_.SetInt(value) ;
} ;
