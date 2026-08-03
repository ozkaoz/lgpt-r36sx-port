
#include "CommandList.h"
#include <string.h>

// TREEFROG_COMMAND_SPECS_V1 (Fase 6) / TREEFROG_FX_LABELS_RC2 (RC2):
// Single source of truth for the phrase command list.  The first entry is the
// empty command (I_CMD_NONE, index 0) which the UI skips; navigation helpers
// below operate on this table so the popup grid and the hex editor stay in
// sync.  RC2 normalized the visible abbreviations to table T1: the on-screen
// label now always reads as a 3-letter mnemonic of its meaning (CFM/CFT for
// the legacy comb, FCU/FRS/FCR for the filter family, DSE/RSE for the sends,
// etc.).  The stored FourCC is never changed (project compatibility); the
// parameter editor format is per-command (HEX16 for legacy, HEX8 for Fase 4
// FX).
static const CommandSpec _specs[] = {
	// command,                       displayName,       paramFormat
	{ I_CMD_NONE,                     "----",            CMD_PARAM_FORMAT_HEX16 },
	// TREEFROG_BEATMAKING_FX_V1:
	// Phrase command list trimmed to the beatmaking FX families:
	// legacy comb feedback (FBMX/FBTN), note delay (DLAY),
	// filter (FLTR/FCUT/FRES), bit crusher (CRSH), fine pitch (PFIN).
	// PTCH was removed (H38.7): pitch now lives in its own phrase column.
	// Engine processing is untouched: projects using other commands still
	// play and are editable from their views.
	{ I_CMD_FBMX,                     "CFM ",            CMD_PARAM_FORMAT_HEX16 },
	{ I_CMD_FBTN,                     "CFT ",            CMD_PARAM_FORMAT_HEX16 },
	{ I_CMD_DLAY,                     "NDL ",            CMD_PARAM_FORMAT_HEX16 },
	{ I_CMD_FLTR,                     "FCR ",            CMD_PARAM_FORMAT_HEX16 },
	{ I_CMD_FCUT,                     "FCU ",            CMD_PARAM_FORMAT_HEX16 },
	{ I_CMD_FRES,                     "FRS ",            CMD_PARAM_FORMAT_HEX16 },
	{ I_CMD_CRSH,                     "BCR ",            CMD_PARAM_FORMAT_HEX16 },
	{ I_CMD_PFIN,                     "PFT ",            CMD_PARAM_FORMAT_HEX16 },
	// TREEFROG_FX_ENGINE_COMMANDS_V1 (Fase 4) /
	// TREEFROG_SEND_LIVE_V1 (Fase 15):
	// Master-bus FX automation, monotonic 00-FF on the low param byte:
	// DLYS/RVBS modulate the LIVE per-channel instrument send override (never
	// the persisted base nor the per-track Mixer send), DLYT/DLYF the master
	// delay, RVDC/RVSZ the master reverb, CMPT the master comp threshold.
	// These commands only read (value & 0xFF), so the editor is 2-digit HEX8.
	{ I_CMD_DLYS,                     "DSE ",            CMD_PARAM_FORMAT_HEX8 },
	{ I_CMD_RVBS,                     "RSE ",            CMD_PARAM_FORMAT_HEX8 },
	{ I_CMD_DLYT,                     "DTM ",            CMD_PARAM_FORMAT_HEX8 },
	{ I_CMD_DLYF,                     "DFB ",            CMD_PARAM_FORMAT_HEX8 },
	{ I_CMD_RVDC,                     "RDC ",            CMD_PARAM_FORMAT_HEX8 },
	{ I_CMD_RVSZ,                     "RSZ ",            CMD_PARAM_FORMAT_HEX8 },
	{ I_CMD_CMPT,                     "CTH ",            CMD_PARAM_FORMAT_HEX8 },
} ;

static const int _count = sizeof(_specs) / sizeof(CommandSpec) ;

int CommandList::GetCount() { return _count; }

FourCC CommandList::GetAt(int index) {
	int count = GetCount() ;
	if (count <= 0) {
		return I_CMD_NONE ;
	}
	while (index < 0) {
		index += count;
	}
	index %= count;
	return _specs[index].command ;
}

int CommandList::IndexOf(FourCC current) {
	for (int i=0;i<GetCount();i++) {
		if (_specs[i].command == current) {
			return i;
		}
	}
	return -1;
}

FourCC CommandList::GetNext(FourCC current) {
	for (uint i=0;i<_count-1;i++) {
		if (_specs[i].command==current) {
			return _specs[i+1].command ;
		} ;
	} ;
    // Wrap around: if current is last, return first
    if (_specs[_count-1].command == current) {
        return _specs[0].command;
    }
	return _specs[0].command ;
} ;

FourCC CommandList::GetPrev(FourCC current) {
    uint count=_count ;
    for (uint i = 1; i < count; i++) {
        if (_specs[i].command==current) {
            return _specs[i - 1].command;
        } ;
    };
    // Wrap around: if current is first, return last
    if (_specs[0].command == current) {
        return _specs[count - 1].command;
    }
	return _specs[count-1].command ;
} ;

FourCC CommandList::GetNextAlpha(FourCC current) {
	char letter=((char *)&current)[0];
	bool found=false ;
	for (uint i=0;i<_count;i++) {
		char tLetter=((char *)&_specs[i].command)[0];
		if (!found) {
			if (tLetter==letter) {
				found=true ;
			}
		} else {
			if (tLetter!=letter) {
				return _specs[i].command ;
			}
		} ;
	} ;
	return current ;
} ;

FourCC CommandList::GetPrevAlpha(FourCC current) {

	char letter=((char *)&current)[0];
	bool found=false ;
	FourCC tReturn=0xFFFFFFFF ;
	uint count=_count ;

	for (uint i=count-1;i>0;i--) {
		char tLetter=((char *)&_specs[i].command)[0];
		if (!found) {
			if (tLetter==letter) {
				found=true ;
			}
		} else {
			if (tLetter!=letter) {
				if (tReturn==0xFFFFFFFF) {
					tReturn=_specs[i].command ;
				} else {
					if (tLetter!=((char *)&tReturn)[0]) {
						return tReturn ;
					} else {
						tReturn=_specs[i].command ;
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

FourCC CommandList::GetFirst() { return _specs[0].command; }

FourCC CommandList::GetLast() {
	return _specs[_count-1].command ;
}

bool CommandList::IsFirst(FourCC current) {
	return current == _specs[0].command ;
}

bool CommandList::IsLast(FourCC current) {
	return current == _specs[_count-1].command ;
}

// TREEFROG_COMMAND_SPECS_V1 (Fase 6)

const CommandSpec *CommandList::GetSpec(FourCC command) {
	for (int i=0;i<_count;i++) {
		if (_specs[i].command == command) {
			return &_specs[i] ;
		}
	}
	return 0 ;
}

const char *CommandList::GetDisplayName(FourCC command) {
	const CommandSpec *spec = GetSpec(command) ;
	if (spec) return spec->displayName ;
	return "----" ;
}

CommandParamFormat CommandList::GetParamFormat(FourCC command) {
	const CommandSpec *spec = GetSpec(command) ;
	if (spec) return spec->paramFormat ;
	return CMD_PARAM_FORMAT_HEX16 ;
}

int CommandList::GetParamPrecision(FourCC command) {
	return (GetParamFormat(command) == CMD_PARAM_FORMAT_HEX8) ? 2 : 4 ;
}

const char *CommandList::GetParamFormatString(FourCC command) {
	return (GetParamFormat(command) == CMD_PARAM_FORMAT_HEX8) ? "%2.2X" : "%4.4X" ;
}

int CommandList::GetParamMin(FourCC command) {
	return (GetParamFormat(command) == CMD_PARAM_FORMAT_HEX8) ? 0 : 0 ;
}

int CommandList::GetParamMax(FourCC command) {
	return (GetParamFormat(command) == CMD_PARAM_FORMAT_HEX8) ? 0xFF : 0xFFFF ;
}

bool CommandList::GetParamWrap(FourCC command) {
	return true ;
}
