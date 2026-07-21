#include "AudioFileStreamer.h"
#include "Application/Utils/fixed.h"
#include "System/Console/Trace.h"
#include "Application/Model/Config.h"
#include "Services/Audio/Audio.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

AudioFileStreamer::AudioFileStreamer() {
	wav_=0 ;
	shift_=1 ;
	position_=0 ;
	startFrame_=0 ;
	endFrame_=-1 ;
	mode_=AFSM_STOPPED ;
	newPath_=false ;
	sourcePosition_=0.0 ;
	sourceStep_=1.0 ;
	sourceRate_=0 ;
	outputRate_=44100 ;
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
	sourcePosition_=(double)startFrame_ ;
	sourceStep_=1.0 ;
	sourceRate_=0 ;
	outputRate_=Audio::GetInstance()->GetSampleRate() ;
	if (outputRate_<=0) outputRate_=44100 ;
	endFrame_=endFrame ;
	newPath_=true ;
	mode_=AFSM_PLAYING ;
	return true ;
} ;

void AudioFileStreamer::Stop() {
	mode_=AFSM_STOPPED ;
	newPath_=false ;
	endFrame_=-1 ;
	sourceRate_=0 ;
	sourceStep_=1.0 ;
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
        sourcePosition_=(double)startFrame_ ;
    }

    if (!wav_) {
        wav_=WavFile::Open(path_.GetPath().c_str()) ;
        if (!wav_) {
            Trace::Error("Failed to open streaming of %s",path_.GetPath().c_str());
            mode_=AFSM_STOPPED ;
            memset(buffer,0,2*samplecount*sizeof(fixed)) ;
            return false ;
        }

        sourceRate_=wav_->GetSampleRate(-1) ;
        outputRate_=Audio::GetInstance()->GetSampleRate() ;
        if (sourceRate_<=0) sourceRate_=44100 ;
        if (outputRate_<=0) outputRate_=44100 ;
        sourceStep_=(double)sourceRate_/(double)outputRate_ ;
        if (sourceStep_<=0.0) sourceStep_=1.0 ;

        Trace::Log(
            "AudioFileStreamer",
            "U2510_RATE_CORRECT_FILE_STREAMER source=%d output=%d step=%.8f path=%s",
            sourceRate_,
            outputRate_,
            sourceStep_,
            path_.GetPath().c_str()) ;
    }

    long size=wav_->GetSize(-1) ;
    long effectiveEnd=size-1 ;
    if (endFrame_>=0 && endFrame_<effectiveEnd) effectiveEnd=endFrame_ ;
    if (size<=0 || sourcePosition_>(double)effectiveEnd) {
        mode_=AFSM_STOPPED ;
        memset(buffer,0,2*samplecount*sizeof(fixed)) ;
        return false ;
    }

    memset(buffer,0,2*samplecount*sizeof(fixed)) ;

    const long readStart=(long)floor(sourcePosition_) ;
    double projectedEnd=sourcePosition_+
        sourceStep_*(double)(samplecount>1?samplecount-1:0) ;
    long readEnd=(long)floor(projectedEnd)+1 ;
    if (readEnd>effectiveEnd) readEnd=effectiveEnd ;
    if (readEnd<readStart) readEnd=readStart ;
    const long readFrames=readEnd-readStart+1 ;

    if (!wav_->GetBuffer(readStart,readFrames)) {
        Trace::Error("Failed rate-correct WAV read start=%ld frames=%ld",readStart,readFrames) ;
        mode_=AFSM_STOPPED ;
        return false ;
    }

    short *src=(short *)wav_->GetSampleBuffer(-1) ;
    int channel=wav_->GetChannelCount(-1) ;
    if (!src || (channel!=1 && channel!=2)) {
        Trace::Error("Invalid streaming buffer for %s",path_.GetPath().c_str());
        mode_=AFSM_STOPPED ;
        return false ;
    }

    fixed *dst=buffer ;
    int produced=0 ;
    for (int out=0;out<samplecount;out++) {
        if (sourcePosition_>(double)effectiveEnd) break ;

        long absoluteIndex=(long)floor(sourcePosition_) ;
        double frac=sourcePosition_-(double)absoluteIndex ;
        long absoluteNext=absoluteIndex+1 ;
        if (absoluteNext>effectiveEnd) absoluteNext=effectiveEnd ;

        long localIndex=absoluteIndex-readStart ;
        long localNext=absoluteNext-readStart ;
        if (localIndex<0) localIndex=0 ;
        if (localNext<0) localNext=0 ;
        if (localIndex>=readFrames) localIndex=readFrames-1 ;
        if (localNext>=readFrames) localNext=readFrames-1 ;

        int leftA=src[localIndex*channel] ;
        int leftB=src[localNext*channel] ;
        int left=(int)((double)leftA+((double)(leftB-leftA)*frac)) ;

        int right=left ;
        if (channel==2) {
            int rightA=src[localIndex*channel+1] ;
            int rightB=src[localNext*channel+1] ;
            right=(int)((double)rightA+((double)(rightB-rightA)*frac)) ;
        }

        *dst++=i2fp(((short)left)>>(1+shift_)) ;
        *dst++=i2fp(((short)right)>>(1+shift_)) ;

        sourcePosition_+=sourceStep_ ;
        produced++ ;
    }

    position_=(int)floor(sourcePosition_) ;
    if (sourcePosition_>(double)effectiveEnd)
        mode_=AFSM_STOPPED ;

    return produced>0 ;
}
