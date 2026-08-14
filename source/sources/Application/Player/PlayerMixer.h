
#ifndef _APPLICATION_MIXER_H_
#define _APPLICATION_MIXER_H_

#include "Foundation/T_Singleton.h"
#include "Application/Model/Project.h"
#include "Application/UI/Views/ViewData.h"
#include "Application/Utils/fixed.h"
#include "Application/Audio/AudioFileStreamer.h"
#include "PlayerChannel.h"
#include "Foundation/Observable.h"
#include "Services/Audio/AudioOut.h"

#define STREAM_MIX_BUS 8

class PlayerMixer: public T_Singleton<PlayerMixer>,public Observable,public I_Observer {
public:
	PlayerMixer() ;
	virtual ~PlayerMixer() {} ;

	bool Start() ;
	void Stop() ;
	bool Init(Project *project) ;
	void Close() ;

	void OnPlayerStart() ;
	void OnPlayerStop() ;

	void StartInstrument(int channel,I_Instrument *instrument,unsigned char note,bool newInstrument) ;
	void StopInstrument(int channel) ;

	int GetChannelNote(int Channel) ;

	I_Instrument *GetInstrument(int channel) ;

	I_Instrument *GetLastInstrument(int channel) ;
	
	void StartChannel(int channel) ;
	void StopChannel(int channel) ;

	bool IsChannelPlaying(int channel) ;
	
	void StartStreaming(const Path &) ;
	void StartStreamingAt(const Path &, int startFrame) ;
	void StartStreamingRangeAt(const Path &, int startFrame, int endFrame) ;
void StopStreaming()  ;
	void StopStreamingAndRelease() ;
	bool IsStreaming() ;
	int GetStreamingPosition() ;
	int GetStreamingStartFrame() ;
	int GetStreamingEndFrame() ;

	bool Clipped() ;

	void Update(Observable &o,I_ObservableData *d) ;
	int GetPlayedBufferPercentage() ;   

	void SetChannelMute(int channel,bool mute) ;
	bool IsChannelMuted(int channel) ;

    // TREEFROG_MIXER_PER_CHANNEL_VU_V1 (H38.7):
    // Live per-channel level (0..1) read from the channel's own rendered audio.
    float GetChannelPeak(int channel) { return channel_[channel]->GetPeakValue() ; }
    float GetChannelPeakL(int channel) { return channel_[channel]->GetPeakValueL() ; }
    float GetChannelPeakR(int channel) { return channel_[channel]->GetPeakValueR() ; }

	char *GetPlayedNote(int channel) ;
	char *GetPlayedOctive(int channel) ;
	
	AudioOut *GetAudioOut() ;

	void Lock() ;
	void Unlock() ;

private:

	Project *project_ ;
	bool clipped_ ;
	
    I_Instrument *lastInstrument_[SONG_CHANNEL_COUNT] ;
	bool isChannelPlaying_[SONG_CHANNEL_COUNT] ;

	AudioFileStreamer fileStreamer_ ;
	PlayerChannel *channel_[SONG_CHANNEL_COUNT] ;

	// store trigger notes, 0xFF = none
	
    unsigned char notes_[SONG_CHANNEL_COUNT] ;
} ;

#endif
