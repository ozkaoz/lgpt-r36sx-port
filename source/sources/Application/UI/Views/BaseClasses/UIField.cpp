#include "UIField.h"

UIField::UIField(GUIPoint &position) {
    x_=position._x ;
    y_=position._y ;
    focus_=false ;
} ;

UIField::~UIField() {
} ;

GUIPoint UIField::GetPosition() {
	GUIPoint point (x_,y_) ;
	return  point; 
}

void UIField::SetPosition(GUIPoint &p) {
	x_=p._x ;
	y_=p._y ;
} ;

void UIField::ClearFocus() {
	focus_=false ;
} ;

void UIField::SetFocus() {
	focus_=true ;
} ;

bool UIField::HasFocus() {
	return focus_;
} ;

bool UIField::IsStatic() {
	return false ;
} ;

// TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): default impls (no int value).
bool UIField::CaptureIntValue(int &out) {
	return false ;
} ;

void UIField::RestoreIntValue(int v) {
} ;
