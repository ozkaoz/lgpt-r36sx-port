#ifndef _PIANO_SYNTH_H_
#define _PIANO_SYNTH_H_

#include "I_Instrument.h"
#include "Application/Model/Song.h"
#include "Foundation/Observable.h"
#include "Foundation/Types/Types.h"
#include "Application/Audio/InstrumentEq.h"
#include "BassSynth.h"   // SynthEnvStage (shared envelope stages)

// PIANO_SYNTH (bacon-1.5, item 7): polyphonic additive piano / electric
// piano synthesizer, independent of SampleInstrument/MidiInstrument.
//
// Slots 0xA0..0xAF (IT_PIANO).  Four voices per player channel, each voice
// is a bank of sine partials (256-entry Q15 table, linear interpolation =
// band-limited by construction, no aliasing).  EP mode uses harmonic
// partials (1, 2, 3), TINE mode slightly inharmonic (1, 2.31, 3.62) for a
// Rhodes-ish tine.  Higher partials decay faster (pdecay), the amp ADSR
// defaults to piano-like behavior (fast attack, sustain 0, natural tail).
//
// DSP chain per channel: 4 voices x partials -> channel Filter V2 (TPT SVF,
// control-rate target) -> equal-power pan -> Haas stereo width -> EQ8 ->
// sends (same variable contract as SampleInstrument/BassSynth).
//
// Velocity support: I_CMD_MVEL (0..255 -> 0..127, same mapping as
// MidiInstrument) shapes the note peak; pattern playback uses 127.
// All variables persist by NAME (XML PARAM NAME/VALUE), 0..100 % UI range.

#define PNP_MODE    MAKE_FOURCC('P', 'M', 'O', 'D')
#define PNP_PARTIALS MAKE_FOURCC('P', 'P', 'A', 'R')
#define PNP_VOLUME  MAKE_FOURCC('P', 'V', 'O', 'L')
#define PNP_PAN     MAKE_FOURCC('P', 'P', 'A', 'N')
#define PNP_WIDTH   MAKE_FOURCC('P', 'W', 'I', 'D')
#define PNP_TIMBRE  MAKE_FOURCC('P', 'T', 'I', 'M')
#define PNP_PDECAY  MAKE_FOURCC('P', 'P', 'D', 'C')
#define PNP_ACCENT  MAKE_FOURCC('P', 'A', 'C', 'C')
#define PNP_ATTACK  MAKE_FOURCC('P', 'A', 'T', 'K')
#define PNP_DECAY   MAKE_FOURCC('P', 'D', 'E', 'C')
#define PNP_SUSTAIN MAKE_FOURCC('P', 'S', 'U', 'S')
#define PNP_RELEASE MAKE_FOURCC('P', 'R', 'E', 'L')
#define PNP_FTYPE   MAKE_FOURCC('P', 'F', 'T', 'Y')
#define PNP_FCUT    MAKE_FOURCC('P', 'F', 'C', 'T')
#define PNP_FRES    MAKE_FOURCC('P', 'F', 'R', 'S')
#define PNP_FENV    MAKE_FOURCC('P', 'F', 'E', 'N')
#define PNP_FATK    MAKE_FOURCC('P', 'F', 'A', 'T')
#define PNP_FDEC    MAKE_FOURCC('P', 'F', 'D', 'E')
#define PNP_FSUS    MAKE_FOURCC('P', 'F', 'S', 'U')
#define PNP_FREL    MAKE_FOURCC('P', 'F', 'R', 'E')
#define PNP_DRY     MAKE_FOURCC('P', 'D', 'R', 'Y')
#define PNP_DLYSEND MAKE_FOURCC('P', 'D', 'L', 'Y')
#define PNP_RVBSEND MAKE_FOURCC('P', 'R', 'V', 'B')

#define PIANO_VOICE_COUNT 4
#define PIANO_WIDTH_DELAY_MAX 1024   // ~21 ms @ 48 kHz (Haas stereo width)

enum PianoMode {
    PPM_EP = 0,   // harmonic partials (1, 2, 3)
    PPM_TINE      // inharmonic tine (1, 2.31, 3.62)
};

struct PianoVoice {
    bool active_;
    float phase_[PIANO_VOICE_COUNT];       // one osc per partial
    float partEnv_[PIANO_VOICE_COUNT];     // per-partial decay
    float partDecayStep_[PIANO_VOICE_COUNT];
    float freq_;                           // current (bend applied)
    float baseFreq_;                       // note frequency
    float ampEnv_;
    int ampStage_;
    float ampStep_;
    float ampDecayStep_;
    float ampReleaseStep_;
    float ampSustain_;
    float fEnv_;
    int fStage_;
    float fStep_;
    float fDecayStep_;
    float fReleaseStep_;
    float fSustain_;
    float peak_;                           // volume * velocity * accent
    unsigned char note_;
};

class PianoSynth: public I_Instrument {

public:
    PianoSynth() ;
    virtual ~PianoSynth() ;

    virtual bool Init() ;
    virtual bool Start(int channel,unsigned char note,bool retrigger=true) ;
    virtual void Stop(int channel) ;
    virtual void OnStart() ;
    virtual bool Render(int channel,fixed *buffer,int size,bool updateTick) ;
    virtual bool IsInitialized() ;
    virtual bool IsEmpty() ;
    virtual InstrumentType GetType() { return IT_PIANO ; } ;
    virtual const char *GetName() ;
    virtual void ProcessCommand(int channel,FourCC cc,ushort value) ;
    virtual void Purge() ;
    virtual int GetTable() ;
    virtual bool GetTableAutomation() ;
    virtual void GetTableState(TableSaveState &state) ;
    virtual void SetTableState(TableSaveState &state) ;
    virtual int GetFxDelaySendOverride() ;
    virtual int GetFxReverbSendOverride() ;
    virtual int GetFxDry() ;
    virtual int GetLiveDelaySend(int channel) ;
    virtual int GetLiveReverbSend(int channel) ;

private:
    void syncInstrumentEq() ;
    float noteToFreq(unsigned char note) const ;
    void resetVoice(int channel) ;
    PianoVoice *allocateVoice(int channel, unsigned char note) ;

    Variable *mode_ ;
    Variable *partials_ ;
    Variable *volume_ ;
    Variable *pan_ ;
    Variable *width_ ;
    Variable *timbre_ ;
    Variable *pdecay_ ;
    Variable *accent_ ;
    Variable *attack_ ;
    Variable *decay_ ;
    Variable *sustain_ ;
    Variable *release_ ;
    Variable *ftype_ ;
    Variable *fcut_ ;
    Variable *fres_ ;
    Variable *fenv_ ;
    Variable *fatk_ ;
    Variable *fdec_ ;
    Variable *fsus_ ;
    Variable *frel_ ;
    Variable *dry_ ;
    Variable *dlySend_ ;
    Variable *rvbSend_ ;

    Variable *eqEnable_ ;
    Variable *eqMask_ ;
    Variable *eqFreq_[8] ;
    Variable *eqGain_[8] ;
    Variable *eqType_[8] ;
    Variable *eqQ_[8] ;
    FxEngine::InstrumentEq eqDsp_ ;
    int eqCache_[34] ;
    int eqRateCache_ ;

    Variable *table_ ;
    Variable *tableAuto_ ;

    TableSaveState tableState_ ;
    PianoVoice voices_[SONG_CHANNEL_COUNT][PIANO_VOICE_COUNT] ;
    int lastNote_[SONG_CHANNEL_COUNT] ;
    int velocity_[SONG_CHANNEL_COUNT] ;    // 0..127 (I_CMD_MVEL)
    float bendSemis_[SONG_CHANNEL_COUNT] ;
    int liveDly_[SONG_CHANNEL_COUNT] ;     // -1 = inherit
    int liveRvb_[SONG_CHANNEL_COUNT] ;
    bool legatoNext_[SONG_CHANNEL_COUNT] ;
    float widthDelay_[SONG_CHANNEL_COUNT][PIANO_WIDTH_DELAY_MAX] ;
    int widthIdx_[SONG_CHANNEL_COUNT] ;
    char name_[32] ;
} ;

#endif