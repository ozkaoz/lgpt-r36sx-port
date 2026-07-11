
#include "PlayerChannel.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Model/Mixer.h"
#include "Application/Utils/fixed.h"

PlayerChannel::PlayerChannel(int index) {             
    index_=index ;
    instr_=0 ;
    muted_=false ;
	volume_=100 ;
	pan_=0 ;
	lastPeak_=0 ;
	lastClipped_=false ;
	mixBus_=0 ;
	busIndex_=-1 ;
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
   lastPeak_=0 ;
   lastClipped_=false ;
   if (instr_) {
     bool tableSlice=SyncMaster::GetInstance()->TableSlice() ;
     bool status=instr_->Render(index_,buffer,samplecount,tableSlice) ;
     bool audible=((status)&&(!muted_)&&(volume_>0)) ;
     if (audible&&(volume_!=100)) {
        fixed *current=buffer ;
        int count=samplecount*2 ;
        while (count--) {
           // Exact linear percentage scaling: 100=unity, 50=half, 0=silence.
           *current=(fixed)(((long long)(*current)*(long long)volume_)/100LL) ;
           current++ ;
        }
     }
     if (audible&&(pan_!=0)) {
        int leftGain=(pan_>0)?(100-pan_):100 ;
        int rightGain=(pan_<0)?(100+pan_):100 ;
        fixed *current=buffer ;
        int frames=samplecount ;
        while (frames--) {
           current[0]=(fixed)(((long long)current[0]*(long long)leftGain)/100LL) ;
           current[1]=(fixed)(((long long)current[1]*(long long)rightGain)/100LL) ;
           current+=2 ;
        }
     }
     if (audible) {
        fixed *current=buffer ;
        int count=samplecount*2 ;
        int peak=0 ;
        while (count--) {
           fixed sample=*current++ ;
           if (sample<0) sample=-sample ;
           int v=fp2i(sample) ;
           if (v>peak) peak=v ;
        }
        lastClipped_=(peak>=32767) ;
        lastPeak_=(peak>=32767)?100:(peak*100/32767) ;
     }
     return audible ;
   } else {
     return false ;
   }
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

int PlayerChannel::GetPeak() const {
     return lastPeak_ ;
}

bool PlayerChannel::GetClipped() const {
     return lastClipped_ ;
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
	lastPeak_=0 ;
	lastClipped_=false ;
	busIndex_=-1 ;
} ;