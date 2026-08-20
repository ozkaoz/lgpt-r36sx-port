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
// FXP_FILTER_V2 (bacon-1.5, item 2): additive filter topology (FK_LP default).
#define SIP_FILTERKIND		MAKE_FOURCC('F','K','I','N')
// SIP_TABLE / SIP_TABLEAUTO / SIP_DRY / SIP_DLY_SEND / SIP_RVB_SEND and the
// SIP_EQ* graphic EQ FourCCs moved to I_Instrument.h (shared by all types).
#define SIP_FBTUNE			MAKE_FOURCC('F','B','T','U')
#define SIP_FBMIX			MAKE_FOURCC('F','B','M','X')
#define SIP_PRINTFX MAKE_FOURCC('P', 'R', 'F', 'X')
#define SIP_IR_PAD MAKE_FOURCC('I', 'R', 'P', 'D')
#define SIP_IR_WET MAKE_FOURCC('I', 'R', 'W', 'T')
// TREEFROG_INSTRUMENT_SENDS_V1 (Fase 6): SIP_DRY / SIP_DLY_SEND /
// SIP_RVB_SEND moved to I_Instrument.h (shared by all instrument types).

// TREEFROG_INSTRUMENT_GRAPHIC_EQ_V1: 8-band graphic EQ.  SIP_EQ* FourCCs
// moved to I_Instrument.h (shared by SampleInstrument and BassSynth).

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

	   // BACON_1.5_EQ8_VIEW: the UI curve reads the real DSP module.
	   virtual FxEngine::InstrumentEq *GetInstrumentEq() { return &eqDsp_ ; }

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
       // BACON_1.5_EQ8_SLOPE (U2.62, feedback #14): per-band slope
       // (1 = 12 dB/oct, 2 = 24 dB/oct), persisted as SIP_EQS0..7.
       Variable *eqSlope_[8] ;
       int eqCache_[42] ;
       // FXP_INSTRUMENT_EQ_RATE (bacon-1.5, item 2): the audio driver sample
       // rate cached at the last syncInstrumentEq so InstrumentEq is rebuilt
       // at the real rate (48 kHz) instead of the ctor default 44100.
       int eqRateCache_ ;
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
	   Variable *filterKind_ ;
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
