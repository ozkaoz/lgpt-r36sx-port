
#ifndef _PLAYER_CHANNEL_H_
#define _PLAYER_CHANNEL_H_

#include "Services/Audio/AudioModule.h"
#include "Application/Instruments/I_Instrument.h"
#include "Application/Mixer/MixBus.h"

class PlayerChannel: public AudioModule {
public:
	PlayerChannel(int index) ;
	virtual ~PlayerChannel() ;
	virtual bool Render(fixed *buffer,int samplecount) ;
	void StartInstrument(I_Instrument *instr,unsigned char note,bool cleanStart) ;
	void StopInstrument() ;
	I_Instrument *GetInstrument() ;
	void SetMute(bool muted) ;
	bool IsMuted() ;
	void SetMixBus(int i) ;
	void SetVolume(int volume) ;
	void SetPan(int pan) ;
	int GetPeak() const ;
	bool GetClipped() const ;
	void Reset() ;
private:
	int index_ ;
	I_Instrument *instr_ ;
	bool muted_ ;
	int busIndex_ ;
	int volume_ ;
	int pan_ ;
	int lastPeak_ ;
	bool lastClipped_ ;
	MixBus *mixBus_ ;
} ;

#endif
