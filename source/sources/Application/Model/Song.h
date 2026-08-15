#ifndef _SONG_H_
#define _SONG_H_

#include "Chain.h"
#include "Phrase.h"
#include "Application/Persistency/Persistent.h"

#define SONG_CHANNEL_COUNT 8
#define SONG_ROW_COUNT 256

#define MAX_SAMPLEINSTRUMENT_COUNT 0x80
#define MAX_MIDIINSTRUMENT_COUNT 0x10
#define MAX_SYNTHINSTRUMENT_COUNT 0x10

// BASS_SYNTH (bacon-1.5, item 6): the synth range lives AFTER the legacy
// sample (00-7F) and MIDI (80-8F) ranges, so existing projects load with
// their instrument IDs untouched.  New synth slots are 0x90..0x9F.  The
// legacy binary project import (Project.cpp) clamps to the data actually
// read, so the larger constant never feeds uninitialized stack data into
// the new instruments.
#define MAX_INSTRUMENT_COUNT (MAX_SAMPLEINSTRUMENT_COUNT+MAX_MIDIINSTRUMENT_COUNT+MAX_SYNTHINSTRUMENT_COUNT)

class Song:Persistent {
public:
	Song() ;
	~Song() ;

	virtual void SaveContent(TiXmlNode *node) ;
	virtual void RestoreContent(TiXmlElement *element);

	unsigned char *data_ ;
	Chain *chain_ ;
	Phrase *phrase_ ;
} ;

#endif
