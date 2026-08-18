
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
	pan_=0 ;
	mixBus_=0 ;
	busIndex_=-1 ;
	peakValue_ = 0.0f ;
	peakValueL_ = 0.0f ;
	peakValueR_ = 0.0f ;
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
      // BACON_1.5_ANALYZER_MIX (bacon-1.5, item 7, feedback): the analyzer
      // tap moved to the master output (AudioOutDriver/DummyAudioOut), so
      // the spectrum shows the whole mix.  No per-channel tap here anymore.
      if (audible&&((volume_!=100)||(pan_!=0))) {
        /* H38.7 OPT_PERF: scale the whole buffer once in fixed point.
         * The old ((long long)x * volume_)/100LL did a 64-bit division
         * per sample (software divdi3 on MIPS32). A precomputed fixed
         * scale turns that into one multiply+shift per sample.
         * TREEFROG_MIXER_PAN_V1 (Bacon 1.1.1): the pan applies in the same
         * pass with a compensated equal-power law (cos/sin, both channels
         * scaled by sqrt(2)): pan 0 keeps both gains at 1.0 so the output
         * is bit-identical to the legacy path, and a hard pan boosts the
         * active side +3 dB while the opposite side goes silent. */
        float panL=1.0f ;
        float panR=1.0f ;
        if (pan_!=0) {
           float angle=((float)pan_+100.0f)/200.0f*1.57079632679f ;
           panL=cosf(angle)*1.41421356237f ;
           panR=sinf(angle)*1.41421356237f ;
        }
        fixed scaleL=fl2fp((float)volume_/100.0f*panL) ;
        fixed scaleR=fl2fp((float)volume_/100.0f*panR) ;
        fixed *left=buffer ;
        fixed *right=buffer+1 ;
        for (int i=0;i<samplecount;i++) {
           *left=fp_mul(*left,scaleL) ;
           *right=fp_mul(*right,scaleR) ;
           left+=2 ;
           right+=2 ;
        }
     }
   }

   // TREEFROG_FX_SENDS_V1 (Fase 4) + TREEFROG_INSTRUMENT_SENDS_V1 (Fase 6):
   // Accumulate this track's rendered audio into the FxEngine delay/reverb
   // send buses BEFORE the master mix.  Only audible (post-volume, non-muted)
   // audio is sent, matching what is heard.  The mixer channels feed the
   // global delay/reverb returns which are summed back into the master in
   // FxEngine::processSendReturns().
   //
   // Fase 6: per-instrument sends.  The instrument's DLY/RVB override wins
   // when set (>=0); otherwise the per-track Mixer send is inherited (legacy
   // compat).  DRY (0..100, default 100) scales the effective send gains, so
   // DRY=100 is bit-identical to Fase 4/5 behaviour.
   //
   // Fase 7 (PLAN_FX_REDESIGN_ES.md): controlled non-destructive migration.
   // Projects saved by the exploratory tag keep their per-track DELAYSEND /
   // REVERBSEND in the Mixer CHANNEL attributes AND may carry instrument
   // PARAMs for DRY / DLY send / RVB send.  On load:
   //   - instruments without an override (default -1) inherit the per-track
   //     Mixer send, so legacy exploratory songs reproduce exactly as before;
   //   - instruments with an override keep it, so a track using several
   //     instruments never loses the send of any of them;
   //   - saving writes both layers again, so no original value is lost.
   // The per-track sends are never deleted; they are simply superseded on a
   // per-instrument basis when an explicit instrument send exists.
   // TREEFROG_SEND_LIVE_V1 (Fase 15): the override source is now the LIVE
   // per-channel value (GetLiveDelaySend/GetLiveReverbSend).  It equals the
   // instrument base on a new trigger and is modulated by phrase/table
   // DLYS/RVBS automation without ever writing the persisted base.
   if (audible) {
       Mixer *mixer=Mixer::GetInstance() ;
       int dSend=mixer->GetChannelDelaySend(index_) ;
       int rSend=mixer->GetChannelReverbSend(index_) ;
       int dry=100 ;
       if (instr_) {
           int dOv=instr_->GetLiveDelaySend(index_) ;
           int rOv=instr_->GetLiveReverbSend(index_) ;
           if (dOv!=0xFF) dSend=dOv ;
           if (rOv!=0xFF) rSend=rOv ;
           dry=instr_->GetFxDry() ;
           if (dry>100) dry=100 ;
           if (dry<0) dry=0 ;
       }
       // Effective gain = send% * DRY% / 10000  (both 0..100)
       fixed dg=fl2fp((float)(dSend*dry)/10000.0f) ;
       fixed rg=fl2fp((float)(rSend*dry)/10000.0f) ;
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
       // TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1): the scan is split
       // per side (even samples = L, odd = R) on the post-pan buffer, so the
       // L/R bars of the MIX page reflect exactly how much pan is applied.
       float blockPeakL=0.0f ;
       float blockPeakR=0.0f ;
        if (audible) {
           // H38.7 OPT_PERF: sample every 4th sample for the peak. Saves 3/4
           // of the fp2fl conversions per buffer with no audible or visual
           // change in a 60fps bouncing meter.
           // TREEFROG_MIXER_STEREO_METERS_V3 (Bacon 1.1.1): a stride-4 scan
           // only visits EVEN indices, so a per-index parity test classified
           // everything as L and the R side stayed dead.  The buffer is
           // interleaved and off is always even (block is even): c[0] is L,
           // c[1] is R; take the pair per 8 samples (same density) so both
           // sides are measured and the pan shows on the bars.
            fixed *c=buffer+off ;
            for (int i=0;i<n;i+=8) {
               float vL=fp2fl(c[0])*(1.0f/32767.0f) ;
               if (vL<0.0f) vL=-vL ;
               if (vL>blockPeakL) blockPeakL=vL ;
               float vR=fp2fl(c[1])*(1.0f/32767.0f) ;
               if (vR<0.0f) vR=-vR ;
               if (vR>blockPeakR) blockPeakR=vR ;
               c+=8 ;
            }
        }
       if (blockPeakL > peakValueL_) {
           peakValueL_ = blockPeakL ;
       } else {
           peakValueL_ *= 0.5f ;
           if (peakValueL_ < 0.002f) peakValueL_ = 0.0f ;
       }
       if (blockPeakR > peakValueR_) {
           peakValueR_ = blockPeakR ;
       } else {
           peakValueR_ *= 0.5f ;
           if (peakValueR_ < 0.002f) peakValueR_ = 0.0f ;
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

// TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1): per-side variants of
// GetPeakValue() with the same idle decay.
float PlayerChannel::GetPeakValueL() {
    unsigned long now = System::GetInstance()->GetClock() ;
    unsigned long elapsed = (lastPeakClock_ == 0) ? 0 : (now - lastPeakClock_) ;
    if (elapsed > 100) {
        peakValueL_ *= powf(0.5f, (float)elapsed / 16.6f) ;
        if (peakValueL_ < 0.002f) peakValueL_ = 0.0f ;
        lastPeakClock_ = now ;
    }
    return peakValueL_ ;
} ;

float PlayerChannel::GetPeakValueR() {
    unsigned long now = System::GetInstance()->GetClock() ;
    unsigned long elapsed = (lastPeakClock_ == 0) ? 0 : (now - lastPeakClock_) ;
    if (elapsed > 100) {
        peakValueR_ *= powf(0.5f, (float)elapsed / 16.6f) ;
        if (peakValueR_ < 0.002f) peakValueR_ = 0.0f ;
        lastPeakClock_ = now ;
    }
    return peakValueR_ ;
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
	// TREEFROG_MIXER_VOLUME_127_V1 (Bacon 1.1.1): 0..127, gain = volume/100,
	// so 127 gives a +2.1 dB boost over 0 dBFS and the bars reach +3 zone.
	if (volume<0) volume=0 ;
	if (volume>127) volume=127 ;
	volume_=volume ;
} ;

void PlayerChannel::SetPan(int pan) {
	if (pan<-100) pan=-100 ;
	if (pan>100) pan=100 ;
	pan_=pan ;
} ;

void PlayerChannel::Reset() {
	if (mixBus_) {
		mixBus_->Remove(*this) ;
	}
	mixBus_=0 ;
	muted_=false ;
	volume_=100 ;
	pan_=0 ;
	busIndex_=-1 ;
} ;