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

void Mixer::Clear() {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		channelBus_[i]=i ;
		channelVolume_[i]=100 ;
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

void Mixer::SaveContent(TiXmlNode *node) {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
		TiXmlElement channel("CHANNEL") ;
		channel.SetAttribute("INDEX",i) ;
		channel.SetAttribute("BUS",GetBus(i)) ;
		channel.SetAttribute("VOLUME",GetChannelVolume(i)) ;
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
		if (current->Attribute("INDEX",&index)) {
			if (index>=0 && index<SONG_CHANNEL_COUNT) {
				if (current->Attribute("BUS",&bus)) {
					SetBus(index,bus) ;
				}
				if (current->Attribute("VOLUME",&volume)) {
					SetChannelVolume(index,volume) ;
				}
			}
		}
		current=current->NextSiblingElement("CHANNEL") ;
	}
} ;
