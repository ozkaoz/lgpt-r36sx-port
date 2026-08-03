#ifndef _UI_BIG_HEX_VAR_FIELD_H_
#define _UI_BIG_HEX_VAR_FIELD_H_

#include "UIIntVarField.h"

class UIBigHexVarField: public UIIntVarField {

public:

	UIBigHexVarField(GUIPoint &position,Variable &v,int precision,const char *format,int min,int max,int power,bool wrap=false) ;
	virtual ~UIBigHexVarField() {} ;
	virtual void Draw(GUIWindow &w,int offset=0) ;
	virtual void ProcessArrow(unsigned short mask) ;
	// TREEFROG_COMMAND_SPECS_V1 (Fase 6): re-target the same variable with a
	// different digit count / range / wrap without recreating the field.
	void SetHexMode(int precision,const char *format,int min,int max,bool wrap) ;
private:
	unsigned int precision_ ;
	unsigned int power_ ;
	unsigned int position_ ;
	bool wrap_ ;
};
#endif
