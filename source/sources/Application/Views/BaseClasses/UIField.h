#ifndef _UI_FIELD_H_
#define _UI_FIELD_H_

#include "UIFramework/BasicDatas/GUIPoint.h"
#include "UIFramework/SimpleBaseClasses/GUIWindow.h"
#include "View.h"
#include "System/Console/Trace.h"

class UIField {
public:
	UIField(GUIPoint &position) ;
	virtual ~UIField() ;
	virtual void Draw(GUIWindow &w,int offset=0)=0 ;
	virtual void OnClick()=0 ; // A depressed
	virtual void ProcessArrow(unsigned short mask)=0 ;
	virtual void OnBClick() {} ; // B depressed
	virtual void ProcessBArrow(unsigned short mask) {} ;
	// TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): A+B global combo resets the
	// option to its default state (UIIntVarField restores initial_).
	virtual void OnABClick() {} ;
	// TREEFROG_GLOBAL_UNDO_V1: undo/redo support.  CaptureIntValue() returns
	// false when the field does not hold an int value (no history recorded).
	virtual bool CaptureIntValue(int &out) ;
	virtual void RestoreIntValue(int v) ;
	void SetFocus() ;
	void ClearFocus() ;
	bool HasFocus() ;
	void SetPosition(GUIPoint &) ;
	GUIPoint GetPosition() ;
	GUIColor GetColor() ;

	virtual bool IsStatic() ;

protected:
	int x_ ;
	int y_ ;
	bool focus_ ;
} ;
#endif

