#ifndef _FIELD_VIEW_H_
#define _FIELD_VIEW_H_

#include "View.h"
#include "Foundation/T_SimpleList.h"
#include "UIField.h"

class FieldView: public View,public T_SimpleList<UIField> {
public:
	FieldView(GUIWindow &w,ViewData *viewData) ;

	virtual void Redraw() ;
	virtual void ProcessButtonMask(unsigned short mask) ;
	// TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): L1+X / R1+X global undo/redo of
	// focused-field edits, A+B resets the focused option to its default.
	virtual void GlobalUndo() ;
	virtual void GlobalRedo() ;
	virtual void GlobalResetOption() ;

	void SetFocus(UIField *) ;
	UIField *GetFocus() ;
	void ClearFocus() ;
	int GetFocusIndex() ;
	void SetSize(int size) ;
private:
	struct FieldEdit {
		UIField *field ;
		int value ;
	} ;
	void pushFieldUndo() ;
	static const int FIELD_HISTORY_SIZE=16 ;
	T_SimpleList<UIField> fieldList_ ;
	UIField *focus_ ;
	FieldEdit fieldUndo_[FIELD_HISTORY_SIZE] ;
	int fieldUndoCount_ ;
	FieldEdit fieldRedo_[FIELD_HISTORY_SIZE] ;
	int fieldRedoCount_ ;
} ;

#endif
