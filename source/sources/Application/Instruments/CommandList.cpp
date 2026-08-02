
#include "CommandList.h"

static FourCC _all[]= {
	I_CMD_NONE,
	// TREEFROG_BEATMAKING_FX_V1:
	// Phrase command list trimmed to the beatmaking FX families:
	// legacy comb feedback (FBMX/FBTN), note delay (DLAY),
	// filter (FLTR/FCUT/FRES), bit crusher (CRSH), fine pitch (PFIN).
	// PTCH was removed (H38.7): pitch now lives in its own phrase column.
	// Engine processing is untouched: projects using other commands still
	// play and are editable from their views.
	I_CMD_FBMX,
	I_CMD_FBTN,
	I_CMD_DLAY,
	I_CMD_FLTR,
	I_CMD_FCUT,
	I_CMD_FRES,
	I_CMD_CRSH,
	I_CMD_PFIN,
	// TREEFROG_FX_ENGINE_COMMANDS_V1 (Fase 4):
	// Master-bus FX automation, monotonic 00-FF on the low param byte:
	// DLYS/RVBS set the track sends, DLYT/DLYF the master delay,
	// RVDC/RVSZ the master reverb, CMPT the master compressor threshold.
	I_CMD_DLYS,
	I_CMD_RVBS,
	I_CMD_DLYT,
	I_CMD_DLYF,
	I_CMD_RVDC,
	I_CMD_RVSZ,
	I_CMD_CMPT
} ;

int CommandList::GetCount() { return sizeof(_all) / sizeof(FourCC); }

FourCC CommandList::GetAt(int index) {
	int count = GetCount() ;
	if (count <= 0) {
		return I_CMD_NONE ;
	}
	while (index < 0) {
		index += count;
	}
	index %= count;
	return _all[index] ;
}

int CommandList::IndexOf(FourCC current) {
	for (int i=0;i<GetCount();i++) {
		if (_all[i] == current) {
			return i;
		}
	}
	return -1;
}

FourCC CommandList::GetNext(FourCC current) {
	for (uint i=0;i<sizeof(_all)/sizeof(FourCC)-1;i++) {
		if (_all[i]==current) {
			return _all[i+1] ;
		} ;
	} ;
    // Wrap around: if current is last, return first
    if (_all[sizeof(_all)/sizeof(FourCC)-1] == current) {
        return _all[0];
    }
	return _all[0] ;
} ;

FourCC CommandList::GetPrev(FourCC current) {
    uint count=sizeof(_all)/sizeof(FourCC) ;
    for (uint i = 1; i < count; i++) {
        if (_all[i]==current) {
            return _all[i - 1];
        } ;
    };
    // Wrap around: if current is first, return last
    if (_all[0] == current) {
        return _all[count - 1];
    }
	return _all[count-1] ;
} ;

FourCC CommandList::GetNextAlpha(FourCC current) {
	char letter=((char *)&current)[0];
	bool found=false ;
	for (uint i=0;i<sizeof(_all)/sizeof(FourCC);i++) {
		char tLetter=((char *)&_all[i])[0];
		if (!found) {
			if (tLetter==letter) {
				found=true ;
			}
		} else {
			if (tLetter!=letter) {
				return _all[i] ;
			}
		} ;
	} ;
	return current ;
} ;

FourCC CommandList::GetPrevAlpha(FourCC current) {

	char letter=((char *)&current)[0];
	bool found=false ;
	FourCC tReturn=0xFFFFFFFF ;
	uint count=sizeof(_all)/sizeof(FourCC) ;

	for (uint i=count-1;i>0;i--) {
		char tLetter=((char *)&_all[i])[0];
		if (!found) {
			if (tLetter==letter) {
				found=true ;
			}
		} else {
			if (tLetter!=letter) {
				if (tReturn==0xFFFFFFFF) {
					tReturn=_all[i] ;
				} else {
					if (tLetter!=((char *)&tReturn)[0]) {
						return tReturn ;
					} else {
						tReturn=_all[i] ;
					}
				}
			}
		} ;
	} ;
	if (tReturn!=0xFFFFFFFF) {
		return tReturn ;
	} 
	return current ;
} ;

FourCC CommandList::GetFirst() { return _all[0]; }

FourCC CommandList::GetLast() {
	uint count = sizeof(_all)/sizeof(FourCC) ;
	return _all[count-1] ;
}

bool CommandList::IsFirst(FourCC current) {
	return current == _all[0] ;
}

bool CommandList::IsLast(FourCC current) {
	uint count = sizeof(_all)/sizeof(FourCC) ;
	return current == _all[count-1] ;
}
