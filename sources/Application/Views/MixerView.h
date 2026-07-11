#ifndef _MIXER_VIEW_H_
#define _MIXER_VIEW_H_

#include "BaseClasses/View.h"
#include "ViewData.h"

class MixerView: public View {
public:
	MixerView(GUIWindow &w,ViewData *viewData) ;
	~MixerView() ;
	virtual void ProcessButtonMask(unsigned short mask,bool pressed) ;
	virtual void DrawView() ;
	virtual void OnPlayerUpdate(PlayerEventType ,unsigned int tick=0) ;
	virtual void OnFocus() ;
protected:
	void processNormalButtonMask(unsigned int mask) ;
    void processSelectionButtonMask(unsigned int mask) ;
	void onStart() ;
	void onStop() ;
	void updateCursor(int dx,int dy)  ;
	void updateVolume(int delta) ;
	void updatePan(int delta) ;
	void centerPan() ;
	void updateTempo(int delta) ;
	void toggleMute() ;
	void switchSoloMode() ;
	void drawVolumeBar(int channel,int x,int y,int height) ;
	void drawMeterBar(int peak,bool clipped,int x,int y,int height,bool selected) ;
	void drawMasterMeter(int x,int y,int height) ;
	void drawTempoLabel(int x,int y) ;
	void formatPan(int pan,char *buffer) ;
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
	int tempoFlashFrames_ ;
} ;
#endif
