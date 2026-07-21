
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