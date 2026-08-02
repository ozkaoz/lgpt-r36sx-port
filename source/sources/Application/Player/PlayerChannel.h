
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
	void Reset() ;
    // TREEFROG_MIXER_PER_CHANNEL_VU_V1 (H38.7):
    // Live 0..1 level of the audio this channel is actually producing right
    // now (instrument buffer, post volume/mute), so the Mixer view can draw
    // real-time per-channel bars that follow the instrument's activity.
    float GetPeakValue() ;
    void ResetPeak() { peakValue_ = 0.0f; lastPeakClock_ = 0; }
private:
	int index_ ;
	I_Instrument *instr_ ;
	bool muted_ ;
	int busIndex_ ;
	int volume_ ;
	MixBus *mixBus_ ;
    float peakValue_ ;
    unsigned long lastPeakClock_ ;
} ;

#endif
