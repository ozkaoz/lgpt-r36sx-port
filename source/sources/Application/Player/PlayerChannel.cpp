
#include "PlayerChannel.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Mixer.h"
#include "Application/Utils/fixed.h"
#include "System/System/System.h"
#include <math.h>

PlayerChannel::PlayerChannel(int index) {             
    index_=index ;
    instr_=0 ;
    muted_=false ;
	volume_=100 ;
	mixBus_=0 ;
	busIndex_=-1 ;
	peakValue_ = 0.0f ;
	lastPeakClock_ = 0 ;
}

PlayerChannel::~PlayerChannel() {
}

void PlayerChannel::StartInstrument(I_Instrument *instr,unsigned char note,bool trigger) {
   if (instr_) {
      StopInstrument() ;
   }
   if (instr->Start(index_,note,trigger)) { // note could be refused coz it's out of the keymap
	   instr_=instr ;
   } else {
	   instr_=0 ;
   };
} ;

void PlayerChannel::StopInstrument() {
     if (instr_) {
       instr_->Stop(index_) ;
     }
     instr_=0 ;
} ;

bool PlayerChannel::Render(fixed *buffer,int samplecount) {
   float peak = 0.0f ;
   bool audible = false ;
   if (instr_) {
     bool tableSlice=SyncMaster::GetInstance()->TableSlice() ;
     bool status=instr_->Render(index_,buffer,samplecount,tableSlice) ;
     audible=((status)&&(!muted_)&&(volume_>0)) ;
     // TREEFROG_MIXER_PER_CHANNEL_VU_V1 (H38.7):
     // Measure the RAW instrument buffer level (pre-volume) so the Mixer view
     // can combine it with the channel volume setting: activity * volume.
     // This reflects the real flow of the instrument (notes on/off, decay)
     // instead of the saturated mixed bus level.
     fixed *c=buffer ;
     for (int i=0;i<samplecount*2;i++) {
        float v=fp2fl(*c) ;
        if (v<0.0f) v=-v ;
        if (v>peak) peak=v ;
        c++ ;
     }
     if (!audible) peak = 0.0f ;
     if (audible&&(volume_!=100)) {
        fixed *current=buffer ;
        int count=samplecount*2 ;
        while (count--) {
           // Exact linear percentage scaling: 100=unity, 50=half, 0=silence.
           *current=(fixed)(((long long)(*current)*(long long)volume_)/100LL) ;
           current++ ;
        }
     }
   }
   lastPeakClock_ = System::GetInstance()->GetClock() ;
   if (peak > peakValue_) {
       peakValue_ = peak ;
   } else {
       // Decay while the player runs so quiet buffers empty the bar quickly.
       peakValue_ *= 0.6f ;
       if (peakValue_ < 0.002f) peakValue_ = 0.0f ;
   }
   return audible ;
} ;

float PlayerChannel::GetPeakValue() {
    // TREEFROG_MIXER_PER_CHANNEL_VU_V1 (H38.7):
    // When the player is stopped Render() stops being called, so decay by
    // wall-clock time here (fast, ~10 frames to empty). While the player runs
    // Render() owns the decay and the getter stays a pure read (elapsed below
    // the idle threshold) to avoid double-decaying the bars.
    unsigned long now = System::GetInstance()->GetClock() ;
    unsigned long elapsed = (lastPeakClock_ == 0) ? 0 : (now - lastPeakClock_) ;
    if (elapsed > 100) {
        peakValue_ *= powf(0.6f, (float)elapsed / 16.6f) ;
        if (peakValue_ < 0.002f) peakValue_ = 0.0f ;
        lastPeakClock_ = now ;
    }
    return peakValue_ ;
} ;

I_Instrument *PlayerChannel::GetInstrument() {
   return instr_ ;
} ;

void PlayerChannel::SetMute(bool muted) {
     muted_=muted ;
}

bool PlayerChannel::IsMuted() {
     return muted_ ;
}

void PlayerChannel::SetMixBus(int i) {

	if (i==busIndex_) return ;

	if (mixBus_) {
		mixBus_->Remove(*this) ;
	}
	mixBus_=MixerService::GetInstance()->GetMixBus(i) ;
	if (mixBus_) {
		mixBus_->Insert(*this) ;
		busIndex_=i ;
	} else {
		busIndex_=-1 ;
	}
} ;

void PlayerChannel::SetVolume(int volume) {
	if (volume<0) volume=0 ;
	if (volume>100) volume=100 ;
	volume_=volume ;
} ;

void PlayerChannel::Reset() {
	if (mixBus_) {
		mixBus_->Remove(*this) ;
	}
	mixBus_=0 ;
	muted_=false ;
	volume_=100 ;
	busIndex_=-1 ;
} ;