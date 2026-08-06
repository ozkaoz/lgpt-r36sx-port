#ifndef _SAMPLE_INSTRUMENT_H_
#define _SAMPLE_INSTRUMENT_H_

#include "I_Instrument.h"
#include "SampleRenderingParams.h"
#include "SRPUpdaters.h"

#include "SoundSource.h"
#include "Application/Model/Song.h" 
#include "Foundation/Observable.h"
#include "Foundation/Types/Types.h"
#include "Foundation/Variables/WatchedVariable.h"
#include "Application/Audio/InstrumentEq.h"
#include "Application/Audio/SpectrumAnalyzer.h"

enum SampleInstrumentLoopMode {
    SILM_ONESHOT = 0,
    SILM_LOOP,
    SILM_LOOP_PINGPONG,
    SILM_OSC,
    //	SILM_OSCFINE,
    SILM_LOOPSYNC,
    SILM_LAST
};

#define NO_SAMPLE (-1)
#define SIP_VOLUME    		MAKE_FOURCC('V','O','L','M')
#define SIP_CRUSH 	  		MAKE_FOURCC('C','R','S','H')
#define SIP_CRUSHVOL 	  	MAKE_FOURCC('C','R','S','V')
#define SIP_DOWNSMPL 	  	MAKE_FOURCC('D','S','P','L')
#define SIP_ROOTNOTE  		MAKE_FOURCC('R','O','O','T')
#define SIP_FINETUNE  		MAKE_FOURCC('F','N','T','N')
#define SIP_PAN 	  		MAKE_FOURCC('P','A','N','_')
#define SIP_START       	MAKE_FOURCC('S','T','R','T')
#define SIP_END       		MAKE_FOURCC('E','N','D','_')
#define SIP_LOOPMODE  		MAKE_FOURCC('L','M','O','D')
#define SIP_LOOPSTART 		MAKE_FOURCC('L','S','T','A')
#define SIP_LOOPLEN 		MAKE_FOURCC('L','L','E','N')
#define SIP_INTERPOLATION   MAKE_FOURCC('I','N','T','P')
#define SIP_SAMPLE 		    MAKE_FOURCC('S','M','P','L')
#define SIP_SLICES 		    MAKE_FOURCC('S','L','C','S')
#define SIP_FILTMODE		MAKE_FOURCC('F','I','M','O') 
#define SIP_ATTENUATE		MAKE_FOURCC('F','I','A','T')
#define SIP_FILTMIX			MAKE_FOURCC('F','M','I','X')
#define SIP_FILTCUTOFF		MAKE_FOURCC('F','C','U','T')
#define SIP_FILTRESO		MAKE_FOURCC('F','R','E','S')
#define SIP_TABLE			MAKE_FOURCC('T','A','B','L')
#define SIP_TABLEAUTO		MAKE_FOURCC('T','B','L','A')
#define SIP_FBTUNE			MAKE_FOURCC('F','B','T','U')
#define SIP_FBMIX			MAKE_FOURCC('F','B','M','X')
#define SIP_PRINTFX MAKE_FOURCC('P', 'R', 'F', 'X')
#define SIP_IR_PAD MAKE_FOURCC('I', 'R', 'P', 'D')
#define SIP_IR_WET MAKE_FOURCC('I', 'R', 'W', 'T')
// TREEFROG_INSTRUMENT_SENDS_V1 (Fase 6): per-instrument FX sends.
// DRY (0..100, default 100) scales the effective sends; DLY/RVB sends are
// 0..100 overrides (default -1 = inherit the per-track Mixer send).  All are
// persisted automatically as instrument PARAMs (InstrumentBank iterator).
#define SIP_DRY MAKE_FOURCC('D', 'R', 'Y', '_')
#define SIP_DLY_SEND MAKE_FOURCC('D', 'S', 'N', 'D')
#define SIP_RVB_SEND MAKE_FOURCC('R', 'S', 'N', 'D')

// TREEFROG_INSTRUMENT_GRAPHIC_EQ_V1: 8-band graphic EQ per sample instrument.
// SIP_EQEN: master enable (0/1).  SIP_EQMASK: bitmask of enabled bands.
// SIP_EQF0..7: frequency scaled by 100 (Hz).  SIP_EQG0..7: band gain in dB
// (-24..+24).  SIP_EQT0..7: band type (0=bell,1=low shelf,2=high shelf,
// 3=low pass,4=high pass,5=notch).  SIP_EQ_Q0..7: Q scaled by 100.
// All persisted automatically as instrument PARAMs.
#define SIP_EQEN MAKE_FOURCC('E', 'Q', 'E', 'N')
#define SIP_EQMASK MAKE_FOURCC('E', 'Q', 'M', 'K')
#define SIP_EQF0 MAKE_FOURCC('E', 'Q', 'F', '0')
#define SIP_EQF1 MAKE_FOURCC('E', 'Q', 'F', '1')
#define SIP_EQF2 MAKE_FOURCC('E', 'Q', 'F', '2')
#define SIP_EQF3 MAKE_FOURCC('E', 'Q', 'F', '3')
#define SIP_EQF4 MAKE_FOURCC('E', 'Q', 'F', '4')
#define SIP_EQF5 MAKE_FOURCC('E', 'Q', 'F', '5')
#define SIP_EQF6 MAKE_FOURCC('E', 'Q', 'F', '6')
#define SIP_EQF7 MAKE_FOURCC('E', 'Q', 'F', '7')
#define SIP_EQG0 MAKE_FOURCC('E', 'Q', 'G', '0')
#define SIP_EQG1 MAKE_FOURCC('E', 'Q', 'G', '1')
#define SIP_EQG2 MAKE_FOURCC('E', 'Q', 'G', '2')
#define SIP_EQG3 MAKE_FOURCC('E', 'Q', 'G', '3')
#define SIP_EQG4 MAKE_FOURCC('E', 'Q', 'G', '4')
#define SIP_EQG5 MAKE_FOURCC('E', 'Q', 'G', '5')
#define SIP_EQG6 MAKE_FOURCC('E', 'Q', 'G', '6')
#define SIP_EQG7 MAKE_FOURCC('E', 'Q', 'G', '7')
#define SIP_EQT0 MAKE_FOURCC('E', 'Q', 'T', '0')
#define SIP_EQT1 MAKE_FOURCC('E', 'Q', 'T', '1')
#define SIP_EQT2 MAKE_FOURCC('E', 'Q', 'T', '2')
#define SIP_EQT3 MAKE_FOURCC('E', 'Q', 'T', '3')
#define SIP_EQT4 MAKE_FOURCC('E', 'Q', 'T', '4')
#define SIP_EQT5 MAKE_FOURCC('E', 'Q', 'T', '5')
#define SIP_EQT6 MAKE_FOURCC('E', 'Q', 'T', '6')
#define SIP_EQT7 MAKE_FOURCC('E', 'Q', 'T', '7')
#define SIP_EQ_Q0 MAKE_FOURCC('E', 'Q', 'Q', '0')
#define SIP_EQ_Q1 MAKE_FOURCC('E', 'Q', 'Q', '1')
#define SIP_EQ_Q2 MAKE_FOURCC('E', 'Q', 'Q', '2')
#define SIP_EQ_Q3 MAKE_FOURCC('E', 'Q', 'Q', '3')
#define SIP_EQ_Q4 MAKE_FOURCC('E', 'Q', 'Q', '4')
#define SIP_EQ_Q5 MAKE_FOURCC('E', 'Q', 'Q', '5')
#define SIP_EQ_Q6 MAKE_FOURCC('E', 'Q', 'Q', '6')
#define SIP_EQ_Q7 MAKE_FOURCC('E', 'Q', 'Q', '7')

#define FB_BUFFER_LENGTH 3500 // (in samples)

class SampleInstrument: public I_Instrument,I_Observer {

public:
       SampleInstrument() ;
       virtual ~SampleInstrument() ;
       // I_Instrument implementation
	   virtual bool Init() ;
       virtual bool Start(int channel,unsigned char note,bool trigger=true) ;
       virtual void Stop(int channel) ;
       virtual bool Render(int channel,fixed *buffer,int size,bool updateTick) ;
       virtual bool IsInitialized() ;
	   virtual bool IsEmpty() ;

	   virtual InstrumentType GetType() { return IT_SAMPLE ; } ;
  	   virtual void ProcessCommand(int channel,FourCC cc,ushort value) ;
	   virtual void Purge() ;
	   virtual int GetTable() ;
	   virtual bool GetTableAutomation();
	   virtual void GetTableState(TableSaveState &state) ;	 
	   virtual void SetTableState(TableSaveState &state) ;	 

	   // TREEFROG_INSTRUMENT_SENDS_V1 (Fase 6): per-instrument FX sends.
	   // Returns the override if set (>=0) or 0xFF ("inherit Mixer") otherwise.
	   virtual int GetFxDelaySendOverride() ;
	   virtual int GetFxReverbSendOverride() ;
	   virtual int GetFxDry() ;

	   // TREEFROG_SEND_LIVE_V1 (Fase 15): live per-channel FX send overrides
	   // (0xFF = "inherit Mixer", 0..100 = explicit).  A new trigger restores
	   // them from the base overrides above; DLYS/RVBS automation writes them.
	   virtual int GetLiveDelaySend(int channel) ;
	   virtual int GetLiveReverbSend(int channel) ;

	   bool IsMulti() ;

	  // Engine playback  start callback

	  virtual void OnStart() ;

	   // I_Observer
       virtual void Update(Observable &o,I_ObservableData *d);
       // Additional
       void AssignSample(int i) ;
	   int GetSampleIndex() ;
	   int GetVolume() ;
	   void SetVolume(int) ;
	   void SetRowVolume(int channel,unsigned char vol) ;
	   // TREEFROG_PHRASE_PITCH_COLUMN_V2 (H38.7): transposes the running voice
	   // by `pitch` semitones without touching the note/chop index.
	   void SetRowPitch(int channel,int pitch) ;
	   int GetSampleSize(int channel=-1) ;
       int GetLoopEnd();
       virtual const char *GetName();
       virtual const char *GetFileName();
 
  static void EnableDownsamplingLegacy();

protected:
		void updateInstrumentData(bool search) ;
		void doTickUpdate(int channel) ;
		void doKRateUpdate(int channel) ;
		void updateFeedback(renderParams *rp) ;

private:
       // TREEFROG_INSTRUMENT_GRAPHIC_EQ_V1: 8-band graphic EQ + live spectrum.
       // eqDsp_ holds the per-channel biquad states; eqCache_ snapshots the
       // persisted variables so coefficients are recomputed only on change
       // (control rate) and the audio thread never does per-frame floats.
       void syncInstrumentEq() ;

       Variable *eqEnable_ ;
       Variable *eqMask_ ;
       Variable *eqGain_[8] ;
       Variable *eqFreq_[8] ;
       Variable *eqType_[8] ;
       Variable *eqQ_[8] ;
       int eqCache_[34] ;
       FxEngine::InstrumentEq eqDsp_ ;

private:
       SoundSource *source_ ;
       struct renderParams renderParams_[SONG_CHANNEL_COUNT] ;
       bool running_ ;
       bool dirty_ ;
	   TableSaveState tableState_ ;
	   
	   static int lastMidiNote_[SONG_CHANNEL_COUNT] ;
	   static fixed lastSample_[SONG_CHANNEL_COUNT][2] ;
	   static fixed feedback_[SONG_CHANNEL_COUNT][FB_BUFFER_LENGTH*2] ;

	   Variable *volume_ ;
	   Variable *crush_ ;
	   Variable *cutoff_ ;
	   Variable *reso_ ;
	   Variable *table_ ;
	   Variable *tableAuto_ ;
	   Variable *downsample_ ;
	   Variable *rootNote_ ;
	   Variable *fineTune_ ;
	   Variable *drive_ ;
	   Variable *fbMix_ ;
	   Variable *fbTune_ ;
	   WatchedVariable *start_ ;
	   WatchedVariable *loopStart_ ;
	   WatchedVariable *loopEnd_ ;
	   Variable *filterMix_ ;
	   Variable *filterMode_ ;
	   Variable *attenuate_ ;
	   Variable *pan_ ;
	   Variable *loopMode_ ;
	   Variable *slices_ ;
	   Variable *interpolation_ ;
       Variable *printFx_;
       Variable *irPad_;
       Variable *irWet_;
       Variable *dry_;
       Variable *dlySend_;
       Variable *rvbSend_;

       static bool useDirtyDownsampling_;
       char *fxPresets[4];
} ;
#endif
