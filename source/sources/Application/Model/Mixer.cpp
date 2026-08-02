#include "Mixer.h"

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
	if (volume<0) return 0 ;
	if (volume>100) return 100 ;
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

void Mixer::Clear() {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		channelBus_[i]=i ;
		channelVolume_[i]=100 ;
		channelDelaySend_[i]=0 ;
		channelReverbSend_[i]=0 ;
	}
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

int Mixer::GetChannelDelaySend(int i) {
	i=clampChannel(i) ;
	return clampSend(channelDelaySend_[i]) ;
} ;

void Mixer::SetChannelDelaySend(int i,int send) {
	i=clampChannel(i) ;
	channelDelaySend_[i]=clampSend(send) ;
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
} ;

void Mixer::NudgeChannelReverbSend(int i,int delta) {
	i=clampChannel(i) ;
	SetChannelReverbSend(i,GetChannelReverbSend(i)+delta) ;
} ;

void Mixer::SaveContent(TiXmlNode *node) {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		TiXmlElement channel("CHANNEL") ;
		channel.SetAttribute("INDEX",i) ;
		channel.SetAttribute("BUS",GetBus(i)) ;
		channel.SetAttribute("VOLUME",GetChannelVolume(i)) ;
		channel.SetAttribute("DELAYSEND",GetChannelDelaySend(i)) ;
		channel.SetAttribute("REVERBSEND",GetChannelReverbSend(i)) ;
		node->InsertEndChild(channel) ;
	}
} ;

void Mixer::RestoreContent(TiXmlElement *element) {
	Clear() ;
	if (!element) return ;
	TiXmlElement *current=element->FirstChildElement("CHANNEL") ;
	while (current) {
		int index=-1 ;
		int bus=0 ;
		int volume=100 ;
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
} ;
