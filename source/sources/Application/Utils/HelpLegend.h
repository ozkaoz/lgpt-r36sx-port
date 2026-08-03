#ifndef _HELP_LEGEND_H_
#define _HELP_LEGEND_H_

#include <string>
#include <stdlib.h>
#include <string.h>

#include "Application/Instruments/CommandList.h"

// TREEFROG_BEATMAKING_FX_V1 / TREEFROG_COMMAND_SPECS_V1 (Fase 6):
// Display names for the phrase commands, shown in the phrase grid, the table
// editor and the command selector. The stored FourCC is never changed, so
// projects stay compatible; only the on-screen name follows the expected
// beatmaking labels.  RC2 normalized the labels to table T1
// (CFM/CFT/NDL/FCR/FCU/FRS/BCR/PFT + DSE/RSE/DTM/DFB/RDC/RSZ/CTH).
// TREEFROG_COMMAND_SPECS_V1 (Fase 6): the names (and the per-command hex
// editor format) now live in CommandList::_specs_; this function just copies
// the central label so the grid, the table and the selector never drift.
static inline void getCommandDisplayName(FourCC command, char *out) {
	strcpy(out, CommandList::GetDisplayName(command)) ;
	out[4] = 0 ;
}

static inline std::string* getHelpLegend(FourCC command) {
	std::string* result = new std::string[3];
	result[2].assign("bb at speed aa");
	switch (command) {
		case I_CMD_KILL:
			result[0].assign("KILl:--bb");
			result[1].assign("stop playing note");
			result[2].assign("after bb ticks");
			break;
		case I_CMD_LPOF:
			result[0].assign("LooP OFset: Shift both");
			result[1].assign("the loop start & loop ");
			result[2].assign("end values aaaa digits");
			break;
		case I_CMD_ARPG:
			result[0].assign("ARPeGgio:abcd Cycle");
			result[1].assign("through relative pitches");
			result[2].assign("from original pitch");
			break;
		case I_CMD_VOLM:
			result[0].assign("VOLuMe:aabb");
			result[1].assign("approach volume");
			break;
		case I_CMD_HOP:
			result[0].assign("HOP:aabb");
			result[1].assign("hop to bb");
			result[2].assign("aa times");
			break;
		case I_CMD_LEGA:
			result[0].assign("LEGAto: slide from");
			result[1].assign("previous note to pitch");
			break;
		case I_CMD_RTRG:
			result[0].assign("ReTRiG:aabb retrigger loop");
			result[1].assign("from current position over");
			result[2].assign("bb ticks at speed aa");
			break;
		case I_CMD_TMPO:
			result[0].assign("TeMPO:--bb");
			result[1].assign("sets the tempo to hex");
			result[2].assign("value bb");
			break;
		case I_CMD_MDCC:
			result[0].assign("MiDiCC:aabb");
			result[1].assign("CC message aa");
			result[2].assign("value bb");
			break;
		case I_CMD_MDPG:
			result[0].assign("MiDi ProGram Change");
			result[1].assign("send program change bb");
			result[2].assign("to current channel");
			break;
		case I_CMD_MVEL:
			result[0].assign("MidiVELocity:--bb");
			result[1].assign("Set velocity bb for step");
			result[2].assign("");
	    break;
		case I_CMD_PLOF:
			result[0].assign("PLay OFfset:aabb");
			result[1].assign("jump abs to aa or");
			result[2].assign("move rel bb chunks");
			break;
		case I_CMD_FLTR:
			result[0].assign("FiLTer:aa bb");
			result[1].assign("cutoff aa, res bb");
			result[2].assign("res max 50% (safe)");
			break;
		case I_CMD_TABL:
			result[0].assign("TABLe:--bb");
			result[1].assign("trigger table bb");
			result[2].assign("");
			break;
		case I_CMD_CRSH:
			result[0].assign("bit CrusH:aa-b");
			result[1].assign("drive aa (max 128)");
			result[2].assign("bit depth -b (max 8)");
			break;
		case I_CMD_FCUT:
			result[0].assign("EQ CUToff:aa bb");
			result[1].assign("ramp cutoff to aa");
			result[2].assign("bb = ramp speed");
			break;
		case I_CMD_FRES:
			result[0].assign("RESonance:aa bb");
			result[1].assign("ramp resonance to aa");
			result[2].assign("max 50% (no ring)");
			break;
		case I_CMD_PAN_:
			result[0].assign("PAN:aabb");
			result[1].assign("pan to value");
			break;
		case I_CMD_GROV:
			result[0].assign("GROoVe:--bb");
			result[1].assign("trigger groove bb");
			result[2].assign("");
			break;
		case I_CMD_IRTG:
			result[0].assign("InstrumentReTriG:aabb");
			result[1].assign("retrig and transpose to");
			break;
		case I_CMD_PFIN:
			result[0].assign("PitchFINe:aa bb");
			result[1].assign("fine tune to aa");
			result[2].assign("at speed bb");
			break;
		case I_CMD_DLAY:
			result[0].assign("Note DeLay:--bb");
			result[1].assign("delay note trigger bb tics");
			result[2].assign("");
			break;
		case I_CMD_FBMX:
			result[0].assign("legacy FBack mix:aa bb");
			result[1].assign("comb feedback wetness aa");
			result[2].assign("max 50% (no buzz)");
			break;
		case I_CMD_FBTN:
			result[0].assign("legacy FBack tune:aa bb");
			result[1].assign("comb feedback pitch aa");
			result[2].assign("max 50% (no buzz)");
			break;
		case I_CMD_STOP:
			result[0].assign("STOP playing song");
			result[1].assign("immediately");
			result[2].assign("");
			break;
		// TREEFROG_FX_ENGINE_COMMANDS_V1 (Fase 4) /
		// TREEFROG_SEND_LIVE_V1 (Fase 15):
		// DLYS/RVBS modulate the LIVE per-channel instrument send override
		// (never the persisted base nor the per-track Mixer send).  The rest
		// are master-bus FX, monotonic 00-FF on the low byte (0=min, FF=max).
		case I_CMD_DLYS:
			result[0].assign("Delay Send:--bb");
			result[1].assign("instrument delay send bb");
			result[2].assign("00-FF -> 0-100% (live)");
			break;
		case I_CMD_RVBS:
			result[0].assign("ReveRb Send:--bb");
			result[1].assign("instrument reverb send bb");
			result[2].assign("00-FF -> 0-100% (live)");
			break;
		case I_CMD_DLYT:
			result[0].assign("Delay TiMe:--bb");
			result[1].assign("master delay time");
			result[2].assign("00-FF -> 10-2000 ms");
			break;
		case I_CMD_DLYF:
			result[0].assign("Delay FeedBack:--bb");
			result[1].assign("master delay feedback");
			result[2].assign("00-FF -> 0-98%");
			break;
		case I_CMD_RVDC:
			result[0].assign("ReveRb DeCay:--bb");
			result[1].assign("master reverb RT60");
			result[2].assign("00-FF -> 0.2-8 s");
			break;
		case I_CMD_RVSZ:
			result[0].assign("ReveRb Size:--bb");
			result[1].assign("master reverb size");
			result[2].assign("00-FF -> 0.5-1.5");
			break;
		case I_CMD_CMPT:
			result[0].assign("CoMP threshold:--bb");
			result[1].assign("master comp threshold");
			result[2].assign("00-FF -> -60-0 dB");
			break;
		default:

			result[0].assign("");
			result[1].assign("");
			result[2].assign("");
		break;
	}
	return result;
}

#endif //_HELP_LEGEND_H_
