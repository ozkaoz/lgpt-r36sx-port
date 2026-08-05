#ifndef _AUDIO_MIXER_H_
#define _AUDIO_MIXER_H_

#include "AudioModule.h"
#include "Foundation/T_SimpleList.h"
#include "Application/Instruments/WavFileWriter.h"
#include <string>

struct SoftClipData {
    float alpha;
	float alpha23;
	float alphaInv;
	float gainCmp;
};

class AudioMixer: public AudioModule,public T_SimpleList<AudioModule> {
public:
	AudioMixer(const char *name) ;
	virtual ~AudioMixer() ;
	virtual bool Render(fixed *buffer,int samplecount) ;
	void SetFileRenderer(const char *path) ;
	void EnableRendering(bool enable) ;
	void SetVolume(fixed volume) ;
    virtual void SetSoftclip(int clip, int gain);
    virtual void SetMasterVolume(int volume) ;
	virtual bool Clipped() ;
    // TREEFROG_VU_METERS_V1:
    // Smoothed peak of the last rendered buffers, 0..1. Each MixBus instance
    // reports its own channel level; the AudioOut instance reports the master
    // level, so the Mixer and Record views can draw live VU bars.
    float GetPeakValue() ;
    // TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1): per-side smoothed peaks
    // of the post-volume, PRE-CLIP output (so the MIX page can draw
    // independent L/R bars that follow the pan and CAN exceed 1.0 / 0 dB).
    float GetPeakValueL() ;
    float GetPeakValueR() ;
    void ResetPeak() { peakValue_ = 0.0f; peakValueL_ = 0.0f; peakValueR_ = 0.0f; }
	
private:
  fixed hardClip(fixed sample);
  fixed softClip(fixed sample);
  bool enableRendering_;
  std::string renderPath_;
  WavFileWriter *writer_;
  fixed volume_;
  std::string name_;
  SoftClipData softClipData_[4];
  int softclip_;
  int softclipGain_;
  int masterVolume_;
  int masterVolumeCached_;
  float dampCached_;
  bool clipped_;
  float peakValue_;
  float peakValueL_;
  float peakValueR_;
  unsigned long lastPeakClock_ ;
} ;
#endif
