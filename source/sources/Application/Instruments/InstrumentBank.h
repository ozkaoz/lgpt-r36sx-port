#ifndef _INSTRUMENT_BANK_H_
#define _INSTRUMENT_BANK_H_

#include "Application/Persistency/Persistent.h"
#include "Application/Model/Song.h"
#include "Application/Instruments/I_Instrument.h"

#define NO_MORE_INSTRUMENT 0x100

class InstrumentBank: public Persistent {
public:
	InstrumentBank() ;
	~InstrumentBank() ;
	void AssignDefaults() ;
	I_Instrument *GetInstrument(int i) ;
	virtual void SaveContent(TiXmlNode *node);
	virtual void RestoreContent(TiXmlElement *element);
	void Init() ;
	void OnStart() ;
	unsigned short GetNext() ;
	unsigned short Clone(unsigned short i) ;
	// BASS_SYNTH_SOURCE (bacon-1.5, feedback): swap a slot's engine class in
	// place (Sample <-> BassSynth <-> PianoSynth).  Used by the Instrument
	// view "src" selector; the resulting type round-trips through SaveContent/
	// RestoreContent like any project-loaded conversion.
	bool SetInstrumentType(int id, InstrumentType type) ;
private:
	I_Instrument *instrument_[MAX_INSTRUMENT_COUNT] ;
} ;

#endif
