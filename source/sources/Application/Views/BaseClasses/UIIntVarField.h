#ifndef _UI_INT_VAR_FIELD_H_
#define _UI_INT_VAR_FIELD_H_

#include "UIField.h"
#include "Foundation/Variables/Variable.h"

class UIIntVarField: public UIField {

public:

	UIIntVarField(
    GUIPoint &position,
    Variable &v,
    const char *format,
    int min,
    int max,
    int xOffset,
    int yOffset,
    int displayOffset = 0);
  
	virtual ~UIIntVarField() {} ;
	virtual void Draw(GUIWindow &w,int offset=0) ;
	virtual void ProcessArrow(unsigned short mask) ;
	virtual void OnClick() {} ;
	// TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): A+B resets the option to its
	// default state; Capture/RestoreIntValue drive the global L1+X/R1+X
	// undo/redo history.
	virtual void OnABClick() ;
	virtual bool CaptureIntValue(int &out) ;
	virtual void RestoreIntValue(int v) ;
  
	// TREEFROG_FX_SEND_BAR_V1 (PLAN_FX_REDESIGN_ES.md, Fase 8): optional
	// percent-bar rendering for the EFFECT SENDS rows.  When enabled, Draw()
	// renders "LABEL [solid bar]  85%" (solid bar = inverted cells, MixerView
	// style, RC2 point 5) instead of format_; the value is taken from src_
	// (0..100), and -1 renders as "INH" (inherit).  Default off, so every
	// existing UIIntVarField keeps its exact current rendering.
	void SetBar(const char *label, int width) ;

  FourCC GetVariableID() ;
	Variable &GetVariable() ;
protected:
	Variable &src_ ;
	const char *format_ ;
	int min_ ;
	int max_ ;
	int xOffset_ ;
	int yOffset_ ;
  int displayOffset_;
	// TREEFROG_FX_SEND_BAR_V1 members (only active when barLabel_ != 0)
	const char *barLabel_ ;
	int barWidth_ ;
} ;

#endif
