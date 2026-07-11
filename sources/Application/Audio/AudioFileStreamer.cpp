#include "AudioFileStreamer.h"
#include "Application/Utils/fixed.h"
#include "System/Console/Trace.h"
#include "Application/Model/Config.h"
#include <string.h>
#include <stdlib.h>

AudioFileStreamer::AudioFileStreamer() {
	wav_=0 ;
	shift_=1 ;
	position_=0 ;
	startFrame_=0 ;
	endFrame_=-1 ;
	mode_=AFSM_STOPPED ;
	newPath_=false ;
} ;

AudioFileStreamer::~AudioFileStreamer() {
	SAFE_DELETE(wav_) ;
} ;
 
bool AudioFileStreamer::Start(const Path &path) {
	return StartAt(path,0) ;
} ;

bool AudioFileStreamer::StartAt(const Path &path,int startFrame) {
	return StartRangeAt(path,startFrame,-1) ;
} ;

bool AudioFileStreamer::StartRangeAt(const Path &path,int startFrame,int endFrame) {
	Trace::Debug("Starting to stream %s at frame %d",path.GetPath().c_str(),startFrame);
	path_=path ;
	const char *shift=Config::GetInstance()->GetValue("PRELISTENATTENUATION") ;
	shift_=(shift)?atoi(shift):1 ;
	if (shift_<0) shift_=0 ;
	if (shift_>12) shift_=12 ;
	Trace::Debug("Streaming shift is %d",shift_);
	startFrame_=(startFrame<0)?0:startFrame ;
	position_=startFrame_ ;
	endFrame_=endFrame ;
	newPath_=true ;
	mode_=AFSM_PLAYING ;
	return true ;
} ;

void AudioFileStreamer::Stop() {
	mode_=AFSM_STOPPED ;
	newPath_=false ;
	endFrame_=-1 ;
	Trace::Debug("Streaming stopped");
} ;

bool AudioFileStreamer::IsPlaying() const { return mode_==AFSM_PLAYING ; }
int AudioFileStreamer::GetPosition() const { return position_ ; }
int AudioFileStreamer::GetStartFrame() const { return startFrame_ ; }
int AudioFileStreamer::GetEndFrame() const { return endFrame_ ; }

bool AudioFileStreamer::Render(fixed *buffer,int samplecount) {
	if (!buffer || samplecount<=0) return false ;

	if (mode_==AFSM_STOPPED) {
		SAFE_DELETE(wav_) ;
		memset(buffer,0,2*samplecount*sizeof(fixed)) ;
		return false ;
	}

	if (newPath_) {
		SAFE_DELETE(wav_) ;
		newPath_=false ;
		position_=startFrame_ ;
	}

	if (!wav_) {
		wav_=WavFile::Open(path_.GetPath().c_str()) ;
		if (!wav_) {
			Trace::Error("Failed to open streaming of %s",path_.GetPath().c_str());
			mode_=AFSM_STOPPED ;
			memset(buffer,0,2*samplecount*sizeof(fixed)) ;
			return false ;
		}
	}

	long size=wav_->GetSize(-1) ;
	if (position_<0) position_=0 ;
	if (size>0 && position_>=size) position_=size-1 ;
	long effectiveEnd=size-1 ;
	if (endFrame_>=0 && endFrame_<effectiveEnd) effectiveEnd=endFrame_ ;
	if (size<=0 || position_>effectiveEnd) {
		mode_=AFSM_STOPPED ;
		memset(buffer,0,2*samplecount*sizeof(fixed)) ;
		return false ;
	}
	long remaining=effectiveEnd-position_+1 ;
	if (remaining<=0) {
		mode_=AFSM_STOPPED ;
		memset(buffer,0,2*samplecount*sizeof(fixed)) ;
		return false ;
	}

	int count=samplecount ;
	if (remaining<samplecount) {
		count=(int)remaining ;
		mode_=AFSM_STOPPED ;
	}
	if (count<=0) {
		mode_=AFSM_STOPPED ;
		memset(buffer,0,2*samplecount*sizeof(fixed)) ;
		return false ;
	}

	memset(buffer,0,2*samplecount*sizeof(fixed)) ;
	wav_->GetBuffer(position_,count) ;
	short *src=(short *)wav_->GetSampleBuffer(-1) ;
	int channel=wav_->GetChannelCount(-1) ;
	if (!src || (channel!=1 && channel!=2)) {
		Trace::Error("Invalid streaming buffer for %s",path_.GetPath().c_str());
		mode_=AFSM_STOPPED ;
		return false ;
	}

	fixed *dst=buffer ;
	for (int i=0;i<count;i++) {
		fixed v=i2fp((*src++)>>(1+shift_)) ;
		*dst++=v ;
		if (channel==2) {
			*dst++=i2fp((*src++)>>(1+shift_)) ;
		} else {
			*dst++=v ;
		}
	}
	position_+=count ;
	return true ;
}
