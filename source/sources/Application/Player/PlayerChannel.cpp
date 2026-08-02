
#include "PlayerChannel.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Mixer.h"
#include "Application/Audio/FxEngine/FxEngine.h"
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
   bool audible = false ;
   if (instr_) {
     bool tableSlice=SyncMaster::GetInstance()->TableSlice() ;
     bool status=instr_->Render(index_,buffer,samplecount,tableSlice) ;
     audible=((status)&&(!muted_)&&(volume_>0)) ;
      if (audible&&(volume_!=100)) {
        /* H38.7 OPT_PERF: scale the whole buffer once in fixed point.
         * The old ((long long)x * volume_)/100LL did a 64-bit division
         * per sample (software divdi3 on MIPS32). A precomputed fixed
         * scale turns that into one multiply+shift per sample. */
        fixed scale=fl2fp((float)volume_/100.0f) ;
        fixed *current=buffer ;
        int count=samplecount*2 ;
        while (count--) {
           *current=fp_mul(*current,scale) ;
           current++ ;
        }
     }
   }

   // TREEFROG_FX_SENDS_V1 (Fase 4):
   // Accumulate this track's rendered audio into the FxEngine delay/reverb
   // send buses with the per-track send gains, BEFORE the master mix.  Only
   // audible (post-volume, non-muted) audio is sent, matching what is heard.
   // The mixer channels feed the global delay/reverb returns which are summed
   // back into the master in FxEngine::processSendReturns().
   if (audible) {
       Mixer *mixer=Mixer::GetInstance() ;
       fixed dg=fl2fp((float)mixer->GetChannelDelaySend(index_)/100.0f) ;
       fixed rg=fl2fp((float)mixer->GetChannelReverbSend(index_)/100.0f) ;
       if (dg!=0 || rg!=0) {
           FxEngine::FxEngine::GetInstance().AccumulateChannelSend(
               index_,buffer,samplecount,dg,rg) ;
       }
   }
   lastPeakClock_ = System::GetInstance()->GetClock() ;
   // TREEFROG_MIXER_PER_CHANNEL_VU_V2 (H38.7):
   // Scan the rendered buffer in short sub-blocks and apply fast decay
   // between them. Even when notes are very close (hi-hats) the meter dips
   // between hits instead of staying pinned at the held peak, because the
   // attack/release runs at sub-buffer resolution (~2-3ms) instead of once
   // per whole audio buffer.
   const int block = 128 ; // stereo samples
   for (int off=0; off<samplecount*2; off+=block) {
       int n=samplecount*2-off ;
       if (n>block) n=block ;
       float blockPeak=0.0f ;
       if (audible) {
          // H38.7 OPT_PERF: sample every 4th sample for the peak. Saves 3/4
          // of the fp2fl conversions per buffer with no audible or visual
          // change in a 60fps bouncing meter.
          fixed *c=buffer+off ;
          for (int i=0;i<n;i+=4) {
             float v=fp2fl(*c) ;
             if (v<0.0f) v=-v ;
             if (v>blockPeak) blockPeak=v ;
             c+=4 ;
          }
       }
       if (blockPeak > peakValue_) {
           peakValue_ = blockPeak ;
       } else {
           peakValue_ *= 0.5f ;
           if (peakValue_ < 0.002f) peakValue_ = 0.0f ;
       }
   }
   return audible ;
} ;

float PlayerChannel::GetPeakValue() {
    // TREEFROG_MIXER_PER_CHANNEL_VU_V2 (H38.7):
    // When the player is stopped Render() stops being called, so decay by
    // wall-clock time here (fast, ~10 blocks to empty). While the player runs
    // Render() owns the decay and the getter stays a pure read (elapsed below
    // the idle threshold) to avoid double-decaying the bars.
    unsigned long now = System::GetInstance()->GetClock() ;
    unsigned long elapsed = (lastPeakClock_ == 0) ? 0 : (now - lastPeakClock_) ;
    if (elapsed > 100) {
        peakValue_ *= powf(0.5f, (float)elapsed / 16.6f) ;
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