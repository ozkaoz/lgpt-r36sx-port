#ifndef _SDL_SOUND_H_
#define _SDL_SOUND_H_

#include "Services/Audio/AudioDriver.h"
#include <SDL2/SDL.h>
#include "System/Process/Process.h"

// BUG2 FIX (Bacon 1.5 FX): SDL2 AudioDevice API
static const int kSDL2FixMarker = 1;

class SDLAudioDriver ;

class SDLAudioDriverThread: public SysThread {
public:
	SDLAudioDriverThread(SDLAudioDriver *driver) ;
	virtual ~SDLAudioDriverThread() {} ;
	virtual bool Execute() ;
	virtual void RequestTermination() ;
	void Notify() ;
private:
	SDLAudioDriver *driver_ ;
	SysSemaphore *semaphore_ ;
} ;

class SDLAudioDriver:public AudioDriver {
public:
  SDLAudioDriver(AudioSettings &settings) ;
  virtual ~SDLAudioDriver() ;
  
   // Sound implementation
	virtual bool InitDriver() ; 
	virtual void CloseDriver();
	virtual bool StartDriver() ; 
	virtual void StopDriver();
	virtual int GetPlayedBufferPercentage() ;
	virtual int GetSampleRate() { return 44100 ; } ;
	virtual bool Interlaced() { return true ; } ;	
	virtual double GetStreamTime() ;
	// Additional
	void OnChunkDone(Uint8 *stream,int len) ;

private:
  SDL_AudioDeviceID devId_; // BUG2 FIX: SDL2 device
  int fragSize_ ;  // Actual fragsize used by the driver
  char *unalignedMain_ ;
  char *mainBuffer_ ;
  char *miniBlank_ ;
  int bufferPos_ ;
  int bufferSize_ ;
  SDLAudioDriverThread *thread_ ;
	Uint32 startTime_ ;
} ;


#endif
