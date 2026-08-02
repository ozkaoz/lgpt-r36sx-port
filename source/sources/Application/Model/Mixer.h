#ifndef _MIXER_H_
#define _MIXER_H_

#include "Foundation/T_Singleton.h"
#include "Application/Persistency/Persistent.h"
#include "Song.h"

class Mixer:public T_Singleton<Mixer>,Persistent {
public:
	Mixer() ;
	virtual ~Mixer() ;

	void Clear() ;

	int GetBus(int i) ;
	void SetBus(int i,int bus) ;

	int GetChannelVolume(int i) ;
	void SetChannelVolume(int i,int volume) ;
	void NudgeChannelVolume(int i,int delta) ;

	// Per-track FX sends (Fase 4): 0..100 %, mapped to the FxEngine send buses.
	int GetChannelDelaySend(int i) ;
	void SetChannelDelaySend(int i,int send) ;
	void NudgeChannelDelaySend(int i,int delta) ;
	int GetChannelReverbSend(int i) ;
	void SetChannelReverbSend(int i,int send) ;
	void NudgeChannelReverbSend(int i,int delta) ;
	void NotifyFxSends() ;

	virtual void SaveContent(TiXmlNode *node) ;
	virtual void RestoreContent(TiXmlElement *element);
private:
	int clampChannel(int i) const ;
	int clampVolume(int volume) const ;
	int clampBus(int bus) const ;
	int clampSend(int send) const ;

	char channelBus_[SONG_CHANNEL_COUNT] ;
	unsigned char channelVolume_[SONG_CHANNEL_COUNT] ;
	unsigned char channelDelaySend_[SONG_CHANNEL_COUNT] ;
	unsigned char channelReverbSend_[SONG_CHANNEL_COUNT] ;
} ;	

#endif
