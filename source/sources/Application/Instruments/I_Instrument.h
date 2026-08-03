#ifndef _I_INSTRUMENT_H_
#define _I_INSTRUMENT_H_

#include "Foundation/Variables/VariableContainer.h"
#include "Foundation/Observable.h"
#include "Application/Utils/fixed.h"

#include "Application/Player/TablePlayback.h"

enum InstrumentType {
	IT_SAMPLE=0,
	IT_MIDI,
	IT_LAST
} ;

class I_Instrument:public VariableContainer, public Observable {
      
public:
	I_Instrument() {} ;
	virtual ~I_Instrument() {} ;

	  // Initialisation routine

	  virtual bool Init()=0 ;

	  // Start & stop the instument
      virtual bool Start(int channel,unsigned char note,bool retrigger=true)=0 ;
      virtual void Stop(int channel)=0 ;

	  // Engine playback  start callback

	  virtual void OnStart()=0 ;

      // size refers to the number of samples
      // should always fill interleaved stereo / 16bit
      
      virtual bool Render(int channel,fixed *buffer,int size,bool updateTick)=0 ;

      virtual bool IsInitialized()=0 ;

	  virtual bool IsEmpty()=0 ;

	  virtual InstrumentType GetType()=0 ;

	  virtual const char *GetName()=0 ; 
	 
	  virtual void ProcessCommand(int channel,FourCC cc,ushort value)=0 ;

	  virtual void Purge()=0 ;

	  // Row volume override (0xFF = no override, 0x00-0xFE = proportional gain)

	  virtual void SetRowVolume(int channel,unsigned char vol) { (void)channel; (void)vol; } ;

	  // TREEFROG_PHRASE_PITCH_COLUMN_V2 (H38.7): row pitch override in semitones.
	  // Used by SampleInstrument to transpose a chop row's playback while keeping
	  // the same chop index. Non-sample instruments ignore it.

	  virtual void SetRowPitch(int channel,int pitch) { (void)channel; (void)pitch; } ;

	  virtual int GetTable()=0 ;
	  virtual bool GetTableAutomation()=0 ;

	  virtual void GetTableState(TableSaveState &state)=0 ;	 
	  virtual void SetTableState(TableSaveState &state)=0 ;	 

	  // TREEFROG_INSTRUMENT_SENDS_V1 (Fase 6):
	  // Per-instrument FX sends.  Overrides return 0xFF ("unset") by default so
	  // the legacy per-track Mixer sends keep working unchanged; SampleInstrument
	  // overrides them with real 0..100 values once the user drives DLYS/RVBS.
	  // GetFxDry() is a 0..100 send gain (100 = full).  PlayerChannel combines
	  // these with the per-track Mixer sends at render time.

	  virtual int GetFxDelaySendOverride() { return 0xFF; }
	  virtual int GetFxReverbSendOverride() { return 0xFF; }
	  virtual int GetFxDry() { return 100; }

	  // TREEFROG_SEND_LIVE_V1 (Fase 15):
	  // Live per-channel FX send overrides, read by PlayerChannel at render
	  // time.  Return 0xFF ("unset" / inherit the per-track Mixer send) unless
	  // the instrument keeps a live value per active channel.  A new note
	  // trigger restores the live value from GetFx*SendOverride(); phrase and
	  // table DLYS/RVBS automation only ever writes these live values.

	  virtual int GetLiveDelaySend(int) { return 0xFF; }
	  virtual int GetLiveReverbSend(int) { return 0xFF; }

};
#endif
