#include "UIController.h"

#include "Application/Player/Player.h"

UIController::UIController() : soloActive_(false) {
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) soloMask_[i]=false ;
} ;

UIController *UIController::GetInstance() {
	if (instance_==0) {
		instance_=new UIController() ;
	}
	return instance_ ;
}

void UIController::Init(Project *project,ViewData *viewData) {
	viewData_=viewData ;
	project_=project ;
}

void UIController::Reset() {
	viewData_=0 ;
	project_=0 ;
	soloActive_=false ;
} ;

void UIController::UnMuteAll() {

	Player *player=Player::GetInstance() ;
	for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			player->SetChannelMute(i,false) ;
			soloMask_[i]=false ;
	} ;
	soloActive_=false ;
} ;

void UIController::ToggleMute(int from,int to) {

	Player *player=Player::GetInstance() ;
	for (int i=from;i<to+1;i++) {
		bool muted=player->IsChannelMuted(i) ;
		player->SetChannelMute(i,!muted) ;
	};
} ;

void UIController::SwitchSoloMode(int from,int to,bool soloing) {

	(void)soloing ;
	Player *player=Player::GetInstance() ;

	/* U2.43: R1+A is now a deterministic two-state command across Mixer,
	   Song, Chain and Phrase: first press solos the current channel/selection;
	   second press clears all mutes.  Previous builds tried to restore the
	   pre-solo mute mask, but most tracker views reset their local ViewMode on
	   every input pass, so the second press could never reach the unsolo path. */
	if (!soloActive_) {
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			soloMask_[i]=player->IsChannelMuted(i) ;
			player->SetChannelMute(i,(i<from)||(i>to)) ;
		} ;
		soloActive_=true ;
	} else {
		for (int i=0;i<SONG_CHANNEL_COUNT;i++) {
			player->SetChannelMute(i,false) ;
			soloMask_[i]=false ;
		} ;
		soloActive_=false ;
	}
} ;
