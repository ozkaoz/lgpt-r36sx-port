#include "Mixer.h"
#include "Application/Audio/FxEngine/FxEngine.h"
#include <stdio.h>

Mixer::Mixer():Persistent("MIXER")  {
	Clear() ;
} ;

Mixer::~Mixer() {
} ;

int Mixer::clampChannel(int i) const {
	if (i<0) return 0 ;
	if (i>=SONG_CHANNEL_COUNT) return SONG_CHANNEL_COUNT-1 ;
	return i ;
} ;

int Mixer::clampVolume(int volume) const {
	// TREEFROG_MIXER_VOLUME_127_V1 (Bacon 1.1.1): the volume scale goes to
	// 127 so a channel can boost past 100% (gain = volume/100, so 127 =
	// +2.1 dB) and the bars can genuinely reach the red +3 zone.
	if (volume<0) return 0 ;
	if (volume>127) return 127 ;
	return volume ;
} ;

int Mixer::clampBus(int bus) const {
	if (bus<0) return 0 ;
	if (bus>9) return 9 ;
	return bus ;
} ;

int Mixer::clampSend(int send) const {
	if (send<0) return 0 ;
	if (send>100) return 100 ;
	return send ;
} ;

int Mixer::clampPan(int pan) const {
	if (pan<-100) return -100 ;
	if (pan>100) return 100 ;
	return pan ;
} ;

void Mixer::Clear() {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		channelBus_[i]=i ;
		channelVolume_[i]=100 ;
		channelDelaySend_[i]=0 ;
		channelReverbSend_[i]=0 ;
		channelPan_[i]=0 ;
	}
	NotifyFxSends() ;
} ;

int Mixer::GetBus(int i) {
	i=clampChannel(i) ;
	return clampBus(channelBus_[i]) ;
} ;

void Mixer::SetBus(int i,int bus) {
	i=clampChannel(i) ;
	channelBus_[i]=clampBus(bus) ;
} ;

int Mixer::GetChannelVolume(int i) {
	i=clampChannel(i) ;
	return clampVolume(channelVolume_[i]) ;
} ;

void Mixer::SetChannelVolume(int i,int volume) {
	i=clampChannel(i) ;
	channelVolume_[i]=clampVolume(volume) ;
} ;

void Mixer::NudgeChannelVolume(int i,int delta) {
	i=clampChannel(i) ;
	SetChannelVolume(i,GetChannelVolume(i)+delta) ;
} ;

int Mixer::GetChannelPan(int i) {
	i=clampChannel(i) ;
	return clampPan(channelPan_[i]) ;
} ;

void Mixer::SetChannelPan(int i,int pan) {
	i=clampChannel(i) ;
	channelPan_[i]=clampPan(pan) ;
} ;

void Mixer::NudgeChannelPan(int i,int delta) {
	i=clampChannel(i) ;
	SetChannelPan(i,GetChannelPan(i)+delta) ;
} ;

int Mixer::GetChannelDelaySend(int i) {
	i=clampChannel(i) ;
	return clampSend(channelDelaySend_[i]) ;
} ;

void Mixer::SetChannelDelaySend(int i,int send) {
	i=clampChannel(i) ;
	channelDelaySend_[i]=clampSend(send) ;
	NotifyFxSends() ;
} ;

void Mixer::NudgeChannelDelaySend(int i,int delta) {
	i=clampChannel(i) ;
	SetChannelDelaySend(i,GetChannelDelaySend(i)+delta) ;
} ;

int Mixer::GetChannelReverbSend(int i) {
	i=clampChannel(i) ;
	return clampSend(channelReverbSend_[i]) ;
} ;

void Mixer::SetChannelReverbSend(int i,int send) {
	i=clampChannel(i) ;
	channelReverbSend_[i]=clampSend(send) ;
	NotifyFxSends() ;
} ;

void Mixer::NudgeChannelReverbSend(int i,int delta) {
	i=clampChannel(i) ;
	SetChannelReverbSend(i,GetChannelReverbSend(i)+delta) ;
} ;

// Fase 5 auto-engage: any raised channel send must take the FxEngine out of
// legacy bypass, otherwise the send would be silent.  RefreshLegacy() then
// re-engages bypass only when every send is back at 0.
void Mixer::NotifyFxSends() {
	bool any=false ;
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		if ((channelDelaySend_[i]!=0)||(channelReverbSend_[i]!=0)) {
			any=true ;
			break ;
		}
	}
	FxEngine::FxEngine::GetInstance().NotifyChannelSendActive(any) ;
} ;

void Mixer::SaveContent(TiXmlNode *node) {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		TiXmlElement channel("CHANNEL") ;
		channel.SetAttribute("INDEX",i) ;
		channel.SetAttribute("BUS",GetBus(i)) ;
		channel.SetAttribute("VOLUME",GetChannelVolume(i)) ;
		channel.SetAttribute("PAN",GetChannelPan(i)) ;
		channel.SetAttribute("DELAYSEND",GetChannelDelaySend(i)) ;
		channel.SetAttribute("REVERBSEND",GetChannelReverbSend(i)) ;
		node->InsertEndChild(channel) ;
	}
	// Fase 5: persist the FxEngine master parameters (delay/reverb/EQ/comp)
	// with defaults = legacy.  A project without FXMASTER (old files) restores
	// to the legacy default state.
	FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
	TiXmlElement fxMaster("FXMASTER") ;
	fxMaster.SetAttribute("DLYSEND",(int)fx.GetDelaySend()) ;
	fxMaster.SetAttribute("DLYRET",(int)fx.GetDelayReturn()) ;
	fxMaster.SetAttribute("DLYTIME",(int)fx.GetDelayTimeMs()) ;
	fxMaster.SetAttribute("DLYFB",(int)fx.GetDelayFeedback()) ;
	fxMaster.SetAttribute("DLYMIX",(int)fx.GetDelayMix()) ;
	fxMaster.SetAttribute("DLYWID",(int)fx.GetDelayWidth()) ;
	fxMaster.SetAttribute("DLYPP",fx.GetDelayPingPong()?1:0) ;
	fxMaster.SetAttribute("DLYSAT",fx.GetDelaySaturation()?1:0) ;
	fxMaster.SetAttribute("DLYBYP",fx.GetDelayBypass()?1:0) ;
	fxMaster.SetAttribute("RVBSEND",(int)fx.GetReverbSend()) ;
	fxMaster.SetAttribute("RVBRET",(int)fx.GetReverbReturn()) ;
	fxMaster.SetAttribute("RVBPRE",(int)fx.GetReverbPredelayMs()) ;
	fxMaster.SetAttribute("RVBDEC",(int)fx.GetReverbDecay()) ;
	fxMaster.SetAttribute("RVBSIZ",(int)fx.GetReverbSize()) ;
	fxMaster.SetAttribute("RVBDMP",(int)fx.GetReverbDamping()) ;
	fxMaster.SetAttribute("RVBWID",(int)fx.GetReverbWidth()) ;
	fxMaster.SetAttribute("RVBMODE",fx.GetReverbMode()) ;
	fxMaster.SetAttribute("RVBMIX",(int)fx.GetReverbMix()) ;
	fxMaster.SetAttribute("RVBBYP",fx.GetReverbBypass()?1:0) ;
	fxMaster.SetAttribute("EQBYP",fx.GetEqBypass()?1:0) ;
	for (int b=0;b<3;b++) {
		char attr[16] ;
		sprintf(attr,"EQ%dFRQ",b) ;
		fxMaster.SetAttribute(attr,(int)fx.GetEqBandFreq(b)) ;
		sprintf(attr,"EQ%dGAI",b) ;
		fxMaster.SetAttribute(attr,(int)fx.GetEqBandGainDb(b)) ;
		sprintf(attr,"EQ%dQ",b) ;
		fxMaster.SetAttribute(attr,(int)fx.GetEqBandQ(b)) ;
		sprintf(attr,"EQ%dEN",b) ;
		fxMaster.SetAttribute(attr,fx.GetEqBandEnabled(b)?1:0) ;
	}
	// FXP_MASTER_EQ8 (bacon-1.5, item 2): EXT EQ chain (ParametricEQ
	// BAND3..BAND7) + its bypass.  No EQ%dEN: enabled is derived on load from
	// GAI/TYP.
	fxMaster.SetAttribute("EQXBYP",fx.GetEqExtBypass()?1:0) ;
	for (int b=3;b<8;b++) {
		char attr[16] ;
		sprintf(attr,"EQ%dFRQ",b) ;
		fxMaster.SetAttribute(attr,(int)fx.GetEqBandFreq(b)) ;
		sprintf(attr,"EQ%dGAI",b) ;
		fxMaster.SetAttribute(attr,(int)fx.GetEqBandGainDb(b)) ;
		sprintf(attr,"EQ%dQ",b) ;
		fxMaster.SetAttribute(attr,(int)fx.GetEqBandQ(b)) ;
		sprintf(attr,"EQ%dTYP",b) ;
		fxMaster.SetAttribute(attr,fx.GetEqBandType(b)) ;
	}
	fxMaster.SetAttribute("CMPTHR",(int)fx.GetCompThresholdDb()) ;
	fxMaster.SetAttribute("CMPRAT",(int)fx.GetCompRatio()) ;
	fxMaster.SetAttribute("CMPKNE",(int)fx.GetCompKneeDb()) ;
	fxMaster.SetAttribute("CMPATK",(int)fx.GetCompAttackMsFixed()) ;
	fxMaster.SetAttribute("CMPREL",(int)fx.GetCompReleaseMsFixed()) ;
	fxMaster.SetAttribute("CMPMKU",(int)fx.GetCompMakeupDb()) ;
	fxMaster.SetAttribute("CMPLNK",fx.GetCompStereoLink()?1:0) ;
	fxMaster.SetAttribute("CMPSC",fx.GetCompSoftClip()?1:0) ;
	fxMaster.SetAttribute("CMPBYP",fx.GetCompBypass()?1:0) ;
	node->InsertEndChild(fxMaster) ;
} ;

void Mixer::RestoreContent(TiXmlElement *element) {
	Clear() ;
	if (!element) return ;
	TiXmlElement *current=element->FirstChildElement("CHANNEL") ;
	while (current) {
		int index=-1 ;
		int bus=0 ;
		int volume=100 ;
		int pan=0 ;
		int delaySend=0 ;
		int reverbSend=0 ;
		if (current->Attribute("INDEX",&index)) {
			if (index>=0 && index<SONG_CHANNEL_COUNT) {
				if (current->Attribute("BUS",&bus)) {
					SetBus(index,bus) ;
				}
				if (current->Attribute("VOLUME",&volume)) {
					SetChannelVolume(index,volume) ;
				}
				// TREEFROG_MIXER_PAN_V1 (Bacon 1.1.1): PAN is optional for
				// backwards compatibility; missing attribute = center.
				if (current->Attribute("PAN",&pan)) {
					SetChannelPan(index,pan) ;
				}
				if (current->Attribute("DELAYSEND",&delaySend)) {
					SetChannelDelaySend(index,delaySend) ;
				}
				if (current->Attribute("REVERBSEND",&reverbSend)) {
					SetChannelReverbSend(index,reverbSend) ;
				}
			}
		}
		current=current->NextSiblingElement("CHANNEL") ;
	}
	// Fase 5: restore FxEngine master params from FXMASTER.  Old project files
	// (no FXMASTER) keep the legacy default state: engine bypass, FX off.
	TiXmlElement *fxMaster=element->FirstChildElement("FXMASTER") ;
	FxEngine::FxEngine &fx=FxEngine::FxEngine::GetInstance() ;
	if (fxMaster) {
		int value ;
		if (fxMaster->Attribute("DLYSEND",&value)) fx.SetDelaySend((fixed)value) ;
		if (fxMaster->Attribute("DLYRET",&value)) fx.SetDelayReturn((fixed)value) ;
		if (fxMaster->Attribute("DLYTIME",&value)) fx.SetDelayTimeMs((fixed)value) ;
		if (fxMaster->Attribute("DLYFB",&value)) fx.SetDelayFeedback((fixed)value) ;
		if (fxMaster->Attribute("DLYMIX",&value)) fx.SetDelayMix((fixed)value) ;
		if (fxMaster->Attribute("DLYWID",&value)) fx.SetDelayWidth((fixed)value) ;
		if (fxMaster->Attribute("DLYPP",&value)) fx.SetDelayPingPong(value!=0) ;
		if (fxMaster->Attribute("DLYSAT",&value)) fx.SetDelaySaturation(value!=0) ;
		if (fxMaster->Attribute("DLYBYP",&value)) fx.SetDelayBypass(value!=0) ;
		if (fxMaster->Attribute("RVBSEND",&value)) fx.SetReverbSend((fixed)value) ;
		if (fxMaster->Attribute("RVBRET",&value)) fx.SetReverbReturn((fixed)value) ;
		if (fxMaster->Attribute("RVBPRE",&value)) fx.SetReverbPredelayMs((fixed)value) ;
		if (fxMaster->Attribute("RVBDEC",&value)) fx.SetReverbDecay((fixed)value) ;
		if (fxMaster->Attribute("RVBSIZ",&value)) fx.SetReverbSize((fixed)value) ;
		if (fxMaster->Attribute("RVBDMP",&value)) fx.SetReverbDamping((fixed)value) ;
		if (fxMaster->Attribute("RVBWID",&value)) fx.SetReverbWidth((fixed)value) ;
		if (fxMaster->Attribute("RVBMODE",&value)) fx.SetReverbMode(value) ;
		if (fxMaster->Attribute("RVBMIX",&value)) fx.SetReverbMix((fixed)value) ;
		if (fxMaster->Attribute("RVBBYP",&value)) fx.SetReverbBypass(value!=0) ;
		if (fxMaster->Attribute("EQBYP",&value)) fx.SetEqBypass(value!=0) ;
		for (int b=0;b<3;b++) {
			char attr[16] ;
			sprintf(attr,"EQ%dFRQ",b) ;
			if (fxMaster->Attribute(attr,&value)) fx.SetEqBandFreq(b,(fixed)value) ;
			sprintf(attr,"EQ%dGAI",b) ;
			if (fxMaster->Attribute(attr,&value)) fx.SetEqBandGainDb(b,(fixed)value) ;
			sprintf(attr,"EQ%dQ",b) ;
			if (fxMaster->Attribute(attr,&value)) fx.SetEqBandQ(b,(fixed)value) ;
			sprintf(attr,"EQ%dEN",b) ;
			if (fxMaster->Attribute(attr,&value)) fx.SetEqBandEnabled(b,value!=0) ;
		}
		if (fxMaster->Attribute("EQXBYP",&value)) fx.SetEqExtBypass(value!=0) ;
		for (int b=3;b<8;b++) {
			char attr[16] ;
			sprintf(attr,"EQ%dFRQ",b) ;
			if (fxMaster->Attribute(attr,&value)) fx.SetEqBandFreq(b,(fixed)value) ;
			sprintf(attr,"EQ%dGAI",b) ;
			if (fxMaster->Attribute(attr,&value)) fx.SetEqBandGainDb(b,(fixed)value) ;
			sprintf(attr,"EQ%dQ",b) ;
			if (fxMaster->Attribute(attr,&value)) fx.SetEqBandQ(b,(fixed)value) ;
			sprintf(attr,"EQ%dTYP",b) ;
			if (fxMaster->Attribute(attr,&value)) fx.SetEqBandType(b,value) ;
		}
		if (fxMaster->Attribute("CMPTHR",&value)) fx.SetCompThresholdDb((fixed)value) ;
		if (fxMaster->Attribute("CMPRAT",&value)) fx.SetCompRatio((fixed)value) ;
		if (fxMaster->Attribute("CMPKNE",&value)) fx.SetCompKneeDb((fixed)value) ;
		if (fxMaster->Attribute("CMPATK",&value)) fx.SetCompAttackMs((fixed)value) ;
		if (fxMaster->Attribute("CMPREL",&value)) fx.SetCompReleaseMs((fixed)value) ;
		if (fxMaster->Attribute("CMPMKU",&value)) fx.SetCompMakeupDb((fixed)value) ;
		if (fxMaster->Attribute("CMPLNK",&value)) fx.SetCompStereoLink(value!=0) ;
		if (fxMaster->Attribute("CMPSC",&value)) fx.SetCompSoftClip(value!=0) ;
		if (fxMaster->Attribute("CMPBYP",&value)) fx.SetCompBypass(value!=0) ;
	} else {
		// Legacy project: reset the engine to its default (bypass) state so
		// old songs load with the exact original behaviour.
		fx.SetLegacyMode(true) ;
		fx.SetDelayTimeMs(0) ;
		fx.SetDelayFeedback(0) ;
		fx.SetDelayPingPong(false) ;
		fx.SetDelayWidth(i2fp(1)) ;
		fx.SetDelayMix(i2fp(1)) ;
		fx.SetDelayBypass(false) ;
		fx.SetDelaySaturation(false) ;
		fx.SetReverbPredelayMs(0) ;
		fx.SetReverbDecay(fl2fp(1.0f)) ;
		fx.SetReverbSize(i2fp(1)) ;
		fx.SetReverbDamping(fl2fp(0.5f)) ;
		fx.SetReverbWidth(i2fp(1)) ;
		fx.SetReverbMode((int)FxEngine::Reverb::NORMAL) ;
		fx.SetReverbMix(i2fp(1)) ;
		fx.SetReverbBypass(false) ;
		fx.SetEqBypass(true) ;
		for (int b=0;b<3;b++) {
			fx.SetEqBandEnabled(b,false) ;
		}
		fx.SetCompBypass(true) ;
		fx.SetCompSoftClip(true) ;
		fx.SetCompStereoLink(true) ;
	}
} ;
