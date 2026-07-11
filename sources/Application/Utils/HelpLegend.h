#ifndef _HELP_LEGEND_H_
#define _HELP_LEGEND_H_

#include <string>
#include <stdlib.h>
#include <string.h>

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
			result[0].assign("VOLuMe:aabb legacy");
			result[1].assign("use Phrase Vol instead");
			break;
		case I_CMD_PTCH:
			result[0].assign("PiTCH:aabb");
			result[1].assign("approach pitch");
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
			result[0].assign("FiLTer LP:aabb");
			result[1].assign("cutoff aa resonance bb");
			result[2].assign("legacy low pass");
			break;
		case I_CMD_LPF_:
			result[0].assign("LowPass:aabb");
			result[1].assign("cutoff aa resonance bb");
			result[2].assign("explicit low pass");
			break;
		case I_CMD_HPF_:
			result[0].assign("HighPass:aabb");
			result[1].assign("cutoff aa resonance bb");
			result[2].assign("explicit high pass");
			break;
		case I_CMD_FBYP:
			result[0].assign("FilterBYP:----");
			result[1].assign("bypass filter now");
			result[2].assign("clears filter ramps");
			break;
		case I_CMD_TABL:
			result[0].assign("TABLe:--bb");
			result[1].assign("trigger table bb");
			result[2].assign("");
			break;
		case I_CMD_CRSH:
			result[0].assign("drive&CRuSH:aa-b");
			result[1].assign("drive aa");
			result[2].assign("crush -b");
			break;
		case I_CMD_FCUT:
			result[0].assign("FilterCUToff:aabb");
			result[1].assign("set cutoff to");
			break;
		case I_CMD_FRES:
			result[0].assign("FilterRESonance:aabb");
			result[1].assign("set resonance to");
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
			result[0].assign("PitchFINetune:aabb");
			result[1].assign("fine tune to ");
			break;
		case I_CMD_DLAY:
			result[0].assign("DeLAY:--bb legacy");
			result[1].assign("delays note trigger");
			result[2].assign("use ECHO for audio delay");
			break;
		case I_CMD_FBMX:
			result[0].assign("FeedBack MiX:aabb");
			result[1].assign("legacy feedback mix");
			break;
		case I_CMD_FBTN:
			result[0].assign("FeedBack TuNe:aabb");
			result[1].assign("legacy delay time");
			break;
		case I_CMD_ECHO:
			result[0].assign("ECHO:aabb");
			result[1].assign("aa time bb wet");
			result[2].assign("audio delay feedback");
			break;
		case I_CMD_RVRB:
			result[0].assign("ReVeRB:aabb");
			result[1].assign("aa size bb wet");
			result[2].assign("short ambience smear");
			break;
		case I_CMD_STOP:
			result[0].assign("STOP playing song");
			result[1].assign("immediately");
			result[2].assign("");
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
