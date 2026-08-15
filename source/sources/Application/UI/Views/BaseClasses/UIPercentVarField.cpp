#include "UIPercentVarField.h"

#include "UIFramework/Interfaces/I_GUIGraphics.h"
#include "Application/AppWindow.h"

UIPercentVarField::UIPercentVarField(
  GUIPoint &position,
  Variable &v,
  const FxParamDescriptor &desc,
  const char *label,
  int xOffset,
  int yOffset)
:UIIntVarField(position,v,"%d",
               desc.kind_==FX_PARAM_SIGNED?-100:0,
               100,xOffset,yOffset)
,desc_(desc)
,label_(label)
{
} ;

void UIPercentVarField::Draw(GUIWindow &w,int offset) {

	GUITextProperties props ;
	GUIPoint position=GetPosition() ;
	position._y+=offset ;

	if (focus_) {
		((AppWindow&)w).SetColor(CD_HILITE2) ;
	} else {
		((AppWindow&)w).SetColor(CD_NORMAL) ;
	}
	char buffer[32] ;
	int p=fxRawToPercent(desc_,src_.GetInt()) ;
	if (desc_.kind_==FX_PARAM_SIGNED) {
		sprintf(buffer,"%s: %+d%%",label_,p) ;
	} else {
		sprintf(buffer,"%s: %d%%",label_,p) ;
	}
	w.DrawString(buffer,position,props) ;
} ;

void UIPercentVarField::ProcessArrow(unsigned short mask) {

	int p=fxRawToPercent(desc_,src_.GetInt()) ;

	switch(mask) {
		case EPBM_UP:
			p+=yOffset_ ;
			break ;
		case EPBM_DOWN:
			p-=yOffset_ ;
			break ;
		case EPBM_LEFT:
			p-=xOffset_ ;
			break ;
		case EPBM_RIGHT:
			p+=xOffset_ ;
			break ;
	} ;
	if (p<min_) {
		p=min_ ;
	} ;
	if (p>max_) {
		p=max_ ;
	}
	src_.SetInt(fxPercentToRaw(desc_,p)) ;
} ;