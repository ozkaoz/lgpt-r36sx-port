#ifndef _I_INSTRUMENT_H_
#define _I_INSTRUMENT_H_

#include "Foundation/Variables/VariableContainer.h"
#include "Foundation/Observable.h"
#include "Application/Utils/fixed.h"

#include "Application/Player/TablePlayback.h"

enum InstrumentType {
	IT_SAMPLE=0,
	IT_MIDI,
	IT_SYNTH,	// BASS_SYNTH (bacon-1.5, item 6): slots 0x90..0x9F
	IT_PIANO,	// PIANO_SYNTH (bacon-1.5, item 7): slots 0xA0..0xAF
	IT_LAST
} ;

// Shared instrument FourCCs used by every instrument type (table,
// per-instrument FX sends and the 8-band graphic EQ).  Defined here so
// SampleInstrument and BassSynth expose the same PARAMs to the UI.
#define SIP_TABLE			MAKE_FOURCC('T','A','B','L')
#define SIP_TABLEAUTO		MAKE_FOURCC('T','B','L','A')

// TREEFROG_INSTRUMENT_SENDS_V1 (Fase 6): per-instrument FX sends.
#define SIP_DRY MAKE_FOURCC('D', 'R', 'Y', '_')
#define SIP_DLY_SEND MAKE_FOURCC('D', 'S', 'N', 'D')
#define SIP_RVB_SEND MAKE_FOURCC('R', 'S', 'N', 'D')

// TREEFROG_INSTRUMENT_GRAPHIC_EQ_V1: 8-band graphic EQ.  SIP_EQEN: master
// enable (0/1).  SIP_EQMASK: bitmask of enabled bands.  SIP_EQF0..7:
// frequency scaled by 100 (Hz).  SIP_EQG0..7: band gain in dB (-24..+24).
// SIP_EQT0..7: band type (0=bell,1=low shelf,2=high shelf,3=low pass,
// 4=high pass,5=notch).  SIP_EQ_Q0..7: Q scaled by 100.
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
