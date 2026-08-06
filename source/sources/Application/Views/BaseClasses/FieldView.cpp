#include "FieldView.h"
#include "System/Console/Trace.h"

FieldView::FieldView(GUIWindow &w,ViewData *data):View(w,data),T_SimpleList<UIField>(true) {
	focus_=0 ;	
} ;

void FieldView::SetFocus(UIField *field) {

	if (focus_) {
		focus_->ClearFocus() ;
	}
	focus_=field ;

//  Empty field view, we don't have anything to do

	if (focus_==0) return ;

	focus_->SetFocus() ;

} ;

void FieldView::ClearFocus() {
	if (focus_) {
		focus_->ClearFocus() ;
	} ;
	focus_=0 ;
} ;

UIField *FieldView::GetFocus() {
    return focus_ ;
} ;

void FieldView::Redraw() {

	if (focus_==0) {
		SetFocus(T_SimpleList<UIField>::GetFirst()) ;
	}

	IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;

	for (it->Begin();!it->IsDone();it->Next()) {
		UIField &current=it->CurrentItem() ;
		current.Draw(w_) ;
	} ;
};

void FieldView::ProcessButtonMask(unsigned short mask) {

	if (focus_==0) {
		focus_=T_SimpleList<UIField>::GetFirst() ;
	//  Empty field view, we don't have anything to do
		if (focus_==0) return ;
 		focus_->SetFocus() ;
	}

	
	// TREEFROG_NAV_X_DIR (Bacon 1.1.1): X+directions jump focus by 4 fields.
	// Also covers horizontal jumps (X+LEFT/RIGHT) using the same fast step.
	if (mask & EPBM_X) {
		moveFocusFast(mask) ;
		return ;
	}

	if (mask&EPBM_A) {  // A or A+ARROW is sent to the field
		if (mask&EPBM_DOWN) {
			pushFieldUndo() ;
			focus_->ProcessArrow(EPBM_DOWN) ;
			isDirty_=true ;
		}
		if (mask&EPBM_UP){
			pushFieldUndo() ;
			focus_->ProcessArrow(EPBM_UP)  ;
			isDirty_=true ;
		}

		if (mask&EPBM_LEFT) {
			pushFieldUndo() ;
			focus_->ProcessArrow(EPBM_LEFT) ;
			isDirty_=true ;
		}

		if (mask&EPBM_RIGHT){
			pushFieldUndo() ;
			focus_->ProcessArrow(EPBM_RIGHT)  ;
			isDirty_=true ;
		}

		if (mask==EPBM_A) {
			focus_->OnClick() ;
		};

	} else {
		if (mask&EPBM_B) {  // B or B+ARROW is sent to the field

			if (mask==EPBM_B) {
				focus_->OnBClick() ;
				isDirty_=true ;
			};

			if (mask&EPBM_DOWN) {
				pushFieldUndo() ;
				focus_->ProcessBArrow(EPBM_DOWN) ;
				isDirty_=true ;
			}
			if (mask&EPBM_UP){
				pushFieldUndo() ;
				focus_->ProcessBArrow(EPBM_UP)  ;
				isDirty_=true ;
			}

			if (mask&EPBM_LEFT) {
				pushFieldUndo() ;
				focus_->ProcessBArrow(EPBM_LEFT) ;
				isDirty_=true ;
			}

			if (mask&EPBM_RIGHT){
				pushFieldUndo() ;
				focus_->ProcessBArrow(EPBM_RIGHT)  ;
				isDirty_=true ;
			}

		} else { // Nor B or A is pressed

			if (!(mask&(EPBM_A|EPBM_B|EPBM_L|EPBM_R|EPBM_SELECT|EPBM_START))) {

				if (mask&EPBM_DOWN) {
					UIField *next=0 ;
					UIField *first=0 ;

					GUIPoint focusPos=focus_->GetPosition() ;

					IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
					for (it->Begin();!it->IsDone();it->Next()) {
						UIField &current=it->CurrentItem() ;
						if (!current.IsStatic()) {
							if (first) {
								if (current.GetPosition()._y<first->GetPosition()._y) {
									first=&current ;
								} ;
							} else {
								first=&current ;
							}
							if (current.GetPosition()._y>focus_->GetPosition()._y) {
								if (next) {
									if (current.GetPosition()._y<next->GetPosition()._y) {
										next=&current ;
									} else {
										// if both target at same height
									} ;
								} else {
									next=&current ;
								};
							} ;

						}
					}
					if (next==0) {
						next=first ;
					}

					focus_->ClearFocus() ;
					focus_=next ;
					focus_->SetFocus() ;
					isDirty_=true ;
				}


				if (mask&EPBM_UP){

					UIField *prev=0 ;
					UIField *last=0 ;

					IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
					for (it->Begin();!it->IsDone();it->Next()) {
						UIField &current=it->CurrentItem() ;
						if (!current.IsStatic()) {
							if (last) {
								if (current.GetPosition()._y>last->GetPosition()._y) {
									last=&current ;
								} ;
							} else {
								last=&current ;
							}
							if (current.GetPosition()._y<focus_->GetPosition()._y) {
								if (prev) {
									if (current.GetPosition()._y>prev->GetPosition()._y) {
										prev=&current ;
									} else {
										// if both target at same height
									} ;
								} else {
									prev=&current ;
								};
							} ;

						}
					}
					if (prev==0) {
						prev=last ;
					}

					focus_->ClearFocus() ;
 					focus_=prev ;
					focus_->SetFocus() ;
					isDirty_=true ;
				}

				if (mask&EPBM_RIGHT) {
					UIField *next=0 ;
					UIField *first=0 ;

					GUIPoint focusPos=focus_->GetPosition() ;

					IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
					for (it->Begin();!it->IsDone();it->Next()) {
						UIField &current=it->CurrentItem() ;
						if (!current.IsStatic()&&(current.GetPosition()._y==focus_->GetPosition()._y)) {
							if (first) {
								if (current.GetPosition()._x<first->GetPosition()._x) {
									first=&current ;
								} ;
							} else {
								first=&current ;
							}
							if (current.GetPosition()._x>focus_->GetPosition()._x) {
								if (next) {
									if (current.GetPosition()._x<next->GetPosition()._x) {
										next=&current ;
									} else {
										// if both target at same height
									} ;
								} else {
									next=&current ;
								};
							} ;

						}
					}
					if (next==0) {
						next=first ;
					}

					focus_->ClearFocus() ;
					focus_=next ;
					focus_->SetFocus() ;
					isDirty_=true ;
				}

				if (mask&EPBM_LEFT){

					UIField *prev=0 ;
					UIField *last=0 ;

					IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
					for (it->Begin();!it->IsDone();it->Next()) {
						UIField &current=it->CurrentItem() ;
						if (!current.IsStatic()&&(current.GetPosition()._y==focus_->GetPosition()._y)) {
							if (last) {
								if (current.GetPosition()._x>last->GetPosition()._x) {
									last=&current ;
								} ;
							} else {
								last=&current ;
							}
							if (current.GetPosition()._x<focus_->GetPosition()._x) {
								if (prev) {
									if (current.GetPosition()._x>prev->GetPosition()._x) {
										prev=&current ;
									} else {
										// if both target at same height
									} ;
								} else {
									prev=&current ;
								};
							} ;

						}
					}
					if (prev==0) {
						prev=last ;
					}

					focus_->ClearFocus() ;
 					focus_=prev ;
					focus_->SetFocus() ;
					isDirty_=true ;
				}
			}
		}
	}
}

int FieldView::GetFocusIndex() {

	int focusIndex=0 ;
	IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
	for (it->Begin();!it->IsDone();it->Next()) {
		if (&(it->CurrentItem())==focus_) {
			break ;
		} ;
		focusIndex++ ;
	} ;
	return focusIndex ;
}

// TREEFROG_GLOBAL_UNDO_V1 (Bacon 1.1.1): global undo/redo/reset for the
// focused field.  History entries are (field, previous int value); edits
// are captured in ProcessButtonMask before every arrow change.  Undoing
// restores the value and re-focuses the field so repeated L1+X presses
// walk the history backwards.
void FieldView::pushFieldUndo() {
	if (!focus_) return ;
	int value=0 ;
	if (!focus_->CaptureIntValue(value)) return ;
	for (int i=FIELD_HISTORY_SIZE-1;i>0;i--) {
		fieldUndo_[i]=fieldUndo_[i-1] ;
	}
	fieldUndo_[0].field=focus_ ;
	fieldUndo_[0].value=value ;
	fieldUndo_[0].newValue=value ;
	fieldUndoCount_++ ;
	if (fieldUndoCount_>FIELD_HISTORY_SIZE) fieldUndoCount_=FIELD_HISTORY_SIZE ;
	fieldRedoCount_=0 ;
}

bool FieldView::GlobalUndo() {
	if (fieldUndoCount_==0) return true ;
	FieldEdit e=fieldUndo_[0] ;
	for (int i=0;i<fieldUndoCount_-1;i++) {
		fieldUndo_[i]=fieldUndo_[i+1] ;
	}
	fieldUndoCount_-- ;
	for (int i=FIELD_HISTORY_SIZE-1;i>0;i--) {
		fieldRedo_[i]=fieldRedo_[i-1] ;
	}
	fieldRedo_[0]=e ;
	fieldRedoCount_++ ;
	if (fieldRedoCount_>FIELD_HISTORY_SIZE) fieldRedoCount_=FIELD_HISTORY_SIZE ;
	// TREEFROG_GLOBAL_UNDO_V7: the popped entry now holds the edited value;
	// capture it while the field still shows the edit so REDO can restore it.
	int capturedValue=0 ;
	e.newValue = e.field->CaptureIntValue(capturedValue)
	                 ? capturedValue : e.value ;
	focus_=e.field ;
	focus_->SetFocus() ;
	e.field->RestoreIntValue(e.value) ;
	isDirty_=true ;
	return true ;
}

bool FieldView::GlobalRedo() {
	if (fieldRedoCount_==0) return true ;
	FieldEdit e=fieldRedo_[0] ;
	for (int i=0;i<fieldRedoCount_-1;i++) {
		fieldRedo_[i]=fieldRedo_[i+1] ;
	}
	fieldRedoCount_-- ;
	for (int i=FIELD_HISTORY_SIZE-1;i>0;i--) {
		fieldUndo_[i]=fieldUndo_[i-1] ;
	}
	fieldUndo_[0]=e ;
	fieldUndoCount_++ ;
	if (fieldUndoCount_>FIELD_HISTORY_SIZE) fieldUndoCount_=FIELD_HISTORY_SIZE ;
	focus_=e.field ;
	focus_->SetFocus() ;
	// TREEFROG_GLOBAL_UNDO_V7: redo restores the post-edit value (the user
	// expects R1+X to replay the action, not to sit still).
	e.field->RestoreIntValue(e.newValue) ;
	isDirty_=true ;
	return true ;
}

bool FieldView::GlobalResetOption() {
	if (!focus_) return true ;
	pushFieldUndo() ;
	focus_->OnABClick() ;
	isDirty_=true ;
	return true ;
}

// TREEFROG_NAV_X_DIR (Bacon 1.1.1): quick focus jumps. X+UP/DOWN / X+LEFT/RIGHT
// move the focus by 4 focusable fields, wrapping around the field list.
void FieldView::moveFocusFast(unsigned short mask) {
	static const int kFastStep = 4 ;
	UIField *cells[128] ;
	int count = 0 ;
	IteratorPtr<UIField> it(T_SimpleList<UIField>::GetIterator()) ;
	for (it->Begin();!it->IsDone();it->Next()) {
		UIField &current=it->CurrentItem() ;
		if (!current.IsStatic() && count<128) {
			cells[count]=&current ;
			count++ ;
		}
	}
	if (count==0) return ;
	int idx=0 ;
	if (focus_) {
		for (int i=0;i<count;i++) {
			if (cells[i]==focus_) { idx=i ; break ; }
		}
	}
	int dist = 0 ;
	if (mask & (EPBM_DOWN|EPBM_RIGHT)) dist = kFastStep ;
	if (mask & (EPBM_UP|EPBM_LEFT)) dist = -kFastStep ;
	if (dist==0) return ;
	idx += dist ;
	idx %= count ;
	if (idx<0) idx+=count ;
	focus_->ClearFocus() ;
	focus_=cells[idx] ;
	focus_->SetFocus() ;
	isDirty_=true ;
}
