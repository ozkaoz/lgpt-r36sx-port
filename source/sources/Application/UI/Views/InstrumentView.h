#ifndef _INSTRUMENT_VIEW_H_
#define _INSTRUMENT_VIEW_H_

#include "Application/FX/FxPrinter.h"
#include "BaseClasses/FieldView.h"
#include "Foundation/Observable.h"
#include "Foundation/Variables/WatchedVariable.h"
#include "ViewData.h"
#include "System/FileSystem/FileSystem.h"

class InstrumentView: public FieldView, public I_Observer {
public:
	InstrumentView(GUIWindow &w,ViewData *data) ;
	virtual ~InstrumentView() ;

	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType type,unsigned int tick) { UpdateActiveModal(type,tick); } ;
	virtual void OnFocus() ;
	virtual void OnFrameUpdate(unsigned long frameClock) ;

	// BASS_SYNTH_SOURCE_MENU (bacon-1.5, feedback): the Import/Synth browser
	// menu converts the current slot through this entry point.  Same safety
	// steps as the "src" selector: observer detach, Stop, SetInstrumentType,
	// page rebuild.  Refuses (returns false) when the slot still holds an
	// assigned sample so nothing is silently discarded.
	bool ConvertCurrentToSynth(InstrumentType target) ;

protected:
	void warpToNext(int offset) ;
	void onInstrumentChange() ;
	void fillSampleParameters() ;
	void fillMidiParameters() ;
	void fillSynthParameters() ;
	void fillPianoParameters() ;
	InstrumentType getInstrumentType() ;
	void Update(Observable &o,I_ObservableData *d) ;

private:
	Project *project_ ;
	FourCC lastFocusID_ ;
	I_Instrument *current_ ;
	// BASS_SYNTH_SOURCE (bacon-1.5, feedback): view-local selector that maps
	// the current slot to its engine (Sample/Bass/Piano).  Not persisted: the
	// converted type is what SaveContent writes.
	WatchedVariable srcVar_ ;
} ;
#endif
