
#ifndef _COMMAND_LIST_H_
#define _COMMAND_LIST_H_

#include "Foundation/Types/Types.h"

#define I_CMD_NONE MAKE_FOURCC('-','-','-','-')
#define I_CMD_KILL MAKE_FOURCC('K','I','L','L')
#define I_CMD_LPOF MAKE_FOURCC('L','P','O','F')
#define I_CMD_ARPG MAKE_FOURCC('A','R','P','G')
#define I_CMD_VOLM MAKE_FOURCC('V','O','L','M')
#define I_CMD_PTCH MAKE_FOURCC('P','T','C','H')
#define I_CMD_HOP  MAKE_FOURCC('H','O','P',' ')
#define I_CMD_LEGA MAKE_FOURCC('L','E','G','A')
#define I_CMD_RTRG MAKE_FOURCC('R','T','R','G')
#define I_CMD_TMPO MAKE_FOURCC('T','M','P','O')
#define I_CMD_MDCC MAKE_FOURCC('M','D','C','C')
#define I_CMD_MDPG MAKE_FOURCC('M','D','P','G')
#define I_CMD_MVEL MAKE_FOURCC('M','V','E','L')
#define I_CMD_PLOF MAKE_FOURCC('P','L','O','F')
#define I_CMD_FLTR MAKE_FOURCC('F','L','T','R')
#define I_CMD_TABL MAKE_FOURCC('T','A','B','L')
#define I_CMD_CRSH MAKE_FOURCC('C','R','S','H')
#define I_CMD_FCUT MAKE_FOURCC('F','C','U','T')
#define I_CMD_FRES MAKE_FOURCC('F','R','E','S')
#define I_CMD_PAN_ MAKE_FOURCC('P','A','N',' ')
#define I_CMD_GROV MAKE_FOURCC('G','R','O','V')
#define I_CMD_FBTU MAKE_FOURCC('F','B','T','U')
#define I_CMD_FBAM MAKE_FOURCC('F','B','A','M')
#define I_CMD_IRTG MAKE_FOURCC('I','R','T','G')
#define I_CMD_PFIN MAKE_FOURCC('P','F','I','N')
#define I_CMD_DLAY MAKE_FOURCC('D','L','A','Y')
#define I_CMD_FBMX MAKE_FOURCC('F','B','M','X')
#define I_CMD_FBTN MAKE_FOURCC('F','B','T','N')
#define I_CMD_SLCE MAKE_FOURCC('S','L','C','E')
#define I_CMD_STOP MAKE_FOURCC('S','T','O','P')

// TREEFROG_FX_ENGINE_COMMANDS_V1 (Fase 4):
// New phrase commands driving the FxEngine master bus (delay/reverb sends are
// per-track via the Mixer model; time/feedback/decay/size/comp-threshold are
// global master FX). All use a monotonic 00-FF mapping of the low param byte
// (value&0xFF): 0x00 = minimum, 0xFF = maximum. The high byte (speed) is
// reserved (not used by these commands).
#define I_CMD_DLYS MAKE_FOURCC('D','L','Y','S')  // delay send (track)
#define I_CMD_RVBS MAKE_FOURCC('R','V','B','S')  // reverb send (track)
#define I_CMD_DLYT MAKE_FOURCC('D','L','Y','T')  // delay time (master)
#define I_CMD_DLYF MAKE_FOURCC('D','L','Y','F')  // delay feedback (master)
#define I_CMD_RVDC MAKE_FOURCC('R','V','D','C')  // reverb decay (master)
#define I_CMD_RVSZ MAKE_FOURCC('R','V','S','Z')  // reverb size (master)
#define I_CMD_CMPT MAKE_FOURCC('C','M','P','T')  // comp threshold (master)

// TREEFROG_COMMAND_SPECS_V1 (Fase 6):
// Central definition of every command's on-screen identity and parameter
// editor format.  The FourCC stored in .lsdsx projects never changes
// (compatibility); only the display name and the hex editor precision are
// driven from here.  Legacy commands keep the 4-digit 16-bit hex editor; the
// Fase 4 FX-engine commands use a 2-digit 8-bit hex editor because they only
// read the low param byte (value & 0xFF).
enum CommandParamFormat {
    CMD_PARAM_FORMAT_HEX16 = 0, // 4 hex digits, 0x0000..0xFFFF (legacy)
    CMD_PARAM_FORMAT_HEX8,      // 2 hex digits, 0x00..0xFF (Fase 4 FX cmds)
    CMD_PARAM_FORMAT_COUNT
};

struct CommandSpec {
    FourCC command ;
    const char *displayName ;   // short grid label, 4 chars incl. trailing space
    CommandParamFormat paramFormat ;
};

class CommandList {
public:
	static FourCC GetNext(FourCC current) ;
	static FourCC GetPrev(FourCC current) ;
	static FourCC GetNextAlpha(FourCC current) ;
    static FourCC GetPrevAlpha(FourCC current);
    static int GetCount() ;
	static FourCC GetAt(int index) ;
	static int IndexOf(FourCC current) ;
	static FourCC GetFirst() ;
	static FourCC GetLast() ;
	static bool IsFirst(FourCC current) ;
	static bool IsLast(FourCC current) ;

	// TREEFROG_COMMAND_SPECS_V1 (Fase 6)
	static const CommandSpec *GetSpec(FourCC command) ;
	static const char *GetDisplayName(FourCC command) ;
	static CommandParamFormat GetParamFormat(FourCC command) ;
	static int GetParamPrecision(FourCC command) ;
	static const char *GetParamFormatString(FourCC command) ;
	static int GetParamMin(FourCC command) ;
	static int GetParamMax(FourCC command) ;
	static bool GetParamWrap(FourCC command) ;
};
#endif

