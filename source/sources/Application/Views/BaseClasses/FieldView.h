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
	virtual bool GlobalUndo() ;
	virtual bool GlobalRedo() ;
	virtual bool GlobalResetOption() ;

	void SetFocus(UIField *) ;
	UIField *GetFocus() ;
	void ClearFocus() ;
	int GetFocusIndex() ;
	void SetSize(int size) ;

	// TREEFROG_NAV_X_DIR (Bacon 1.1.1): X+directions jump focus by 4 fields.
	void moveFocusFast(unsigned short mask) ;

private:
	// TREEFROG_GLOBAL_UNDO_V7 (Bacon 1.1.1 V16): newValue is the post-edit
	// value captured when the undo entry is popped (GlobalUndo reads the
	// field before restoring), so R1+X redo restores the edited state.
	struct FieldEdit {
		UIField *field ;
		int value ;
		int newValue ;
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
