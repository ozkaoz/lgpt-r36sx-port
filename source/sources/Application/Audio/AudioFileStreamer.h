#ifndef _AUDIO_FILE_STREAMER_H_
#define _AUDIO_FILE_STREAMER_H_

#include "Services/Audio/AudioModule.h"
#include "System/FileSystem/FileSystem.h"
#include "Application/Instruments/WavFile.h"

enum AudioFileStreamerMode {
	AFSM_STOPPED,
	AFSM_PLAYING
} ;

class AudioFileStreamer: public AudioModule {
public:
	AudioFileStreamer() ;
	virtual ~AudioFileStreamer() ;
	virtual bool Render(fixed *buffer,int samplecount) ;
	bool Start(const Path &) ;
	bool StartAt(const Path &, int startFrame) ;
	bool StartRangeAt(const Path &, int startFrame, int endFrame) ;
	void Stop() ;
	bool IsPlaying() const ;
	int GetPosition() const ;
	int GetStartFrame() const ;
	int GetEndFrame() const ;
protected:
	AudioFileStreamerMode mode_ ;
	Path path_ ;
	bool newPath_ ;
	WavFile *wav_ ;
	int position_ ;
	int startFrame_ ;
	int endFrame_ ;
	int shift_ ;
	double sourcePosition_ ;
	double sourceStep_ ;
	int sourceRate_ ;
	int outputRate_ ;
} ;

#endif 
