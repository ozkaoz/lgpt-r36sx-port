#ifndef _MIXER_SERVICE_H_
#define _MIXER_SERVICE_H_

#ifdef SDL2
#include <SDL2/SDL.h>
#else
#include <SDL/SDL.h>
#endif
#include "Application/Commands/CommandDispatcher.h" // Would be better done externally and call an API here
#include "Foundation/Observable.h"
#include "Foundation/T_Singleton.h"
#include "Services/Audio/AudioMixer.h"
#include "Services/Audio/AudioOut.h"
#include "MixBus.h"

enum MixerServiceRenderMode {
    MSRM_PLAYBACK,
    MSRM_STEREO,
    MSRM_STEMS,
};

#define MAX_BUS_COUNT 10

class MixerService: 
      public T_Singleton<MixerService>,
      public Observable,
      public I_Observer,
      public CommandExecuter      
{

public:
	MixerService() ;
	virtual ~MixerService() ;

	bool Init() ;
	void Close() ;

	bool Start() ;
	void Stop() ;

	MixBus *GetMixBus(int i) ;	

	virtual void Update(Observable &o,I_ObservableData *d) ;	

	void OnPlayerStart() ;
	void OnPlayerStop() ;

	bool Clipped() ;
    void SetPregain(int);
    void SetSoftclip(int, int);
    void SetMasterVolume(int);
    void SetRenderMode(int);
    bool IsRendering();
    int GetPlayedBufferPercentage() ;

    // TREEFROG_VU_METERS_V1:
    // Live per-channel and master levels (0..1) for VU bars in the views.
    float GetChannelPeak(int channel) { return bus_[channel].GetPeakValue(); }
    float GetMasterPeak() { return out_ ? out_->GetPeakValue() : 0.0f; }
    // TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1): per-side (L/R) peaks so
    // the MIX page can draw the split bars linked to the pan.
    float GetChannelPeakL(int channel) { return bus_[channel].GetPeakValueL(); }
    float GetChannelPeakR(int channel) { return bus_[channel].GetPeakValueR(); }
    float GetMasterPeakL() { return out_ ? out_->GetPeakValueL() : 0.0f; }
    float GetMasterPeakR() { return out_ ? out_->GetPeakValueR() : 0.0f; }
	
	virtual void Execute(FourCC id,float value) ;

	AudioOut *GetAudioOut() ;

	void Lock() ;
	void Unlock() ;

protected:
	void toggleRendering(bool enable) ;
private:
  void initRendering(MixerServiceRenderMode);
  AudioOut *out_;
  MixBus master_;
  MixBus bus_[MAX_BUS_COUNT];
  MixerServiceRenderMode mode_;
  SDL_mutex *sync_;
  bool isRendering_;
} ;
#endif
