#ifndef _PHRASE_H_
#define _PHRASE_H_

#include "Foundation/Types/Types.h"
#define PHRASE_COUNT 0xFF
#define NO_MORE_PHRASE 0x100

// TREEFROG_PHRASE_PITCH_COLUMN_V2 (H38.7): the pitch column is stored with an
// offset so that -1 (0xFF) does not collide with the "empty" marker. 0x00 means
// "no pitch" (empty, shown as --), stored values 0x28..0x58 map to -24..+24
// semitones. Anything out of range is treated as "no pitch" (backwards safe).
#define PITCH_STORED_NONE 0x00
#define PITCH_STORED_ZERO 0x40
#define PITCH_STORED_MIN 0x28
#define PITCH_STORED_MAX 0x58

static inline int phrasePitchStoredToInt(unsigned char stored) {
    if (stored < PITCH_STORED_MIN || stored > PITCH_STORED_MAX) return 0;
    return (int)stored - PITCH_STORED_ZERO;
}
static inline unsigned char phrasePitchIntToStored(int pitch) {
    if (pitch < -24) pitch = -24;
    if (pitch > 24) pitch = 24;
    return (unsigned char)(pitch + PITCH_STORED_ZERO);
}

class Phrase {
public:
	Phrase() ;
	~Phrase() ;
	unsigned short GetNext() ;
	bool IsUsed(uchar i) { return isUsed_[i] ; } ;
	void SetUsed(uchar c) ;
	void ClearAllocation() ;

	uchar *note_ ;
	uchar *instr_ ;
	uchar *vol_ ;
	uchar *pitch_ ;
	FourCC *cmd1_ ;
	ushort *param1_ ;
	FourCC *cmd2_ ;
	ushort *param2_ ;
	FourCC *cmd3_ ;
	ushort *param3_ ;
	
private:
	bool isUsed_[PHRASE_COUNT] ;

} ;

#endif
