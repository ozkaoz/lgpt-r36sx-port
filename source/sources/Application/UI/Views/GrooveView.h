#ifndef _GROOVE_VIEW_H_
#define _GROOVE_VIEW_H_

#include "BaseClasses/View.h"
#include "ViewData.h"

class GrooveView: public View {
public:
	GrooveView(GUIWindow &w,ViewData *viewData) ;
	~GrooveView() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int tick=0) ;
	virtual void OnFocus() ;

protected:
	void updateCursorValue(int val,bool sync=false) ;
	void updateCursor(int dir) ;
	void initCursorValue() ;
	void clearCursorValue() ;
	void warpGroove(int dir) ;
	void processNormalButtonMask(unsigned short mask) ;
	void processSelectionButtonMask(unsigned short mask) ;

public:
	// TREEFROG_GLOBAL_UNDO_GROOVE (Bacon 1.1.1): global L1+X/R1+X
	// undo/redo for the groove editor, matching Song/Phrase/Table.
	virtual bool GlobalUndo() ;
	virtual bool GlobalRedo() ;

private:
	int position_ ;
	int lastPosition_ ;

	// Groove history: a full 16-byte groove snapshot per entry.
	static const int kGrooveHistorySize = 16 ;
	typedef struct {
		unsigned char data[16] ;
	} GrooveEdit ;
	GrooveEdit grooveUndo_[kGrooveHistorySize] ;
	GrooveEdit grooveRedo_[kGrooveHistorySize] ;
	int grooveUndoCount_ ;
	int grooveRedoCount_ ;
	void pushGrooveUndo() ;
} ;
#endif