#ifndef _MIXER_VIEW_H_
#define _MIXER_VIEW_H_

#include "BaseClasses/View.h"
#include "ViewData.h"
#include "Application/Model/Song.h"

class MixerView: public View {
public:
	MixerView(GUIWindow &w,ViewData *viewData) ;
	~MixerView() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int tick=0) ;
	virtual void OnFrameUpdate(unsigned long frameClock) ;
	virtual void OnFocus() ;
protected:
	void processNormalButtonMask(unsigned int mask) ;
    void processSelectionButtonMask(unsigned int mask) ;
	void onStart() ;
	void onStop() ;
	void updateCursor(int dx,int dy)  ;
	void updateVolume(int delta) ;
	void adjustMasterVolume(int delta) ;
	void toggleMute() ;
	void switchSoloMode() ;
	void drawVolumeBar(int channel,int x,int y,int height) ;
	void drawMasterBar(int x,int y,int height) ;
	void showInstrumentFxMenu() ;
private:
	const char *song_ ;

	struct {                      // .Clipboard structure
        bool active_ ;            // .If currently making a selection
        unsigned char *data_ ;    // .Null if clipboard empty
        int x_ ;                  // .Current selection positions
        int y_ ;                  // .
        int offset_ ;             // .
        int width_ ;              // .Size of selection
        int height_ ;             // .
    } clipboard_ ;

	int saveX_ ;
	int saveY_ ;
	int saveOffset_ ;
	bool invertBatt_ ;
	bool soloMode_ ;
	bool masterSelected_ ;
	int frameRefreshDivider_ ;
	// TREEFROG_MIXER_VU_SMOOTH_V1 (H38.7):
	// Per-channel display level that attacks instantly on a note and falls
	// with a smooth per-frame exponential release, so the bars never jump
	// from full to empty in a single frame (which looked like screen flicker).
	float vuDisplay_[SONG_CHANNEL_COUNT] ;
} ;
#endif
