#ifndef _BASS_SYNTH_H_
#define _BASS_SYNTH_H_

#include "I_Instrument.h"
#include "Application/Model/Song.h"
#include "Foundation/Observable.h"
#include "Foundation/Types/Types.h"
#include "Application/Audio/InstrumentEq.h"

// BASS_SYNTH (bacon-1.5, item 6): native subtractive bass synthesizer.
//
// Monophonic per player channel with glide/legato; the voice model below is
// shared with the future polyphonic PianoSynth (4 voices).  One voice state
// per player channel, osc + sub + noise -> drive -> TPT SVF (FilterV2, one
// filter per channel) -> pan -> EQ8 (same variable contract as the sample
// instrument so InstrumentEqModal works unchanged).
//
// All variables persist by NAME (XML PARAM NAME/VALUE), 0..100 % UI range.

#define SBP_WAVE    MAKE_FOURCC('W', 'A', 'V', 'E')
#define SBP_SUB     MAKE_FOURCC('S', 'U', 'B', 'L')
#define SBP_NOISE   MAKE_FOURCC('N', 'O', 'I', 'S')
#define SBP_GLIDE   MAKE_FOURCC('G', 'L', 'I', 'D')
#define SBP_VOLUME  MAKE_FOURCC('S', 'V', 'O', 'L')
#define SBP_PAN     MAKE_FOURCC('S', 'P', 'A', 'N')
#define SBP_ATTACK  MAKE_FOURCC('S', 'A', 'T', 'K')
#define SBP_DECAY   MAKE_FOURCC('S', 'D', 'E', 'C')
#define SBP_SUSTAIN MAKE_FOURCC('S', 'S', 'U', 'S')
#define SBP_RELEASE MAKE_FOURCC('S', 'R', 'E', 'L')
#define SBP_FTYPE   MAKE_FOURCC('S', 'F', 'T', 'Y')
#define SBP_FCUT    MAKE_FOURCC('S', 'F', 'C', 'T')
#define SBP_FRES    MAKE_FOURCC('S', 'F', 'R', 'S')
#define SBP_FENV    MAKE_FOURCC('S', 'F', 'E', 'N')
#define SBP_FATK    MAKE_FOURCC('S', 'F', 'A', 'T')
#define SBP_FDEC    MAKE_FOURCC('S', 'F', 'D', 'E')
#define SBP_FSUS    MAKE_FOURCC('S', 'F', 'S', 'U')
#define SBP_FREL    MAKE_FOURCC('S', 'F', 'R', 'E')
#define SBP_DRIVE   MAKE_FOURCC('S', 'D', 'R', 'V')
#define SBP_ACCENT  MAKE_FOURCC('S', 'A', 'C', 'C')
#define SBP_LRATE   MAKE_FOURCC('S', 'L', 'R', 'T')
#define SBP_LDEPTH  MAKE_FOURCC('S', 'L', 'D', 'P')
#define SBP_LTARGET MAKE_FOURCC('S', 'L', 'T', 'G')
#define SBP_DRY     MAKE_FOURCC('S', 'D', 'R', 'Y')
#define SBP_DLYSEND MAKE_FOURCC('S', 'D', 'L', 'Y')
#define SBP_RVBSEND MAKE_FOURCC('S', 'R', 'V', 'B')

// Voice envelope stages shared by the amp and the filter envelopes.
enum SynthEnvStage {
    SES_ATTACK = 0,
    SES_DECAY,
    SES_SUSTAIN,
    SES_RELEASE,
    SES_DONE
};

// Monophonic voice state (one per player channel).  The same struct will be
// array-ified for the polyphonic PianoSynth.
struct SynthVoice {
    bool active_;
    float oscPhase_;
    float subPhase_;
    unsigned int noiseState_;
    float freq_;
    float targetFreq_;
    float glideTime_;      // seconds (0 = snap)
    float glideK_;         // per-sample approach factor
    float lfoPhase_;
    float lfoStep_;
    float ampEnv_;
    int ampStage_;
    float ampStep_;        // per-sample attack step
    float ampDecayStep_;
    float ampReleaseStep_;
    float ampSustain_;
    float fEnv_;
    int fStage_;
    float fStep_;
    float fDecayStep_;
    float fReleaseStep_;
    float fSustain_;
    float cutoff_;         // smoothed 0..1
    float reso_;
    float peak_;           // volume * accent
    int dlySend_;          // live send overrides (-1 = inherit)
    int rvbSend_;
    unsigned char note_;
};

class BassSynth: public I_Instrument {

public:
    BassSynth() ;
    virtual ~BassSynth() ;

    virtual bool Init() ;
    virtual bool Start(int channel,unsigned char note,bool retrigger=true) ;
    virtual void Stop(int channel) ;
    virtual void OnStart() ;
    virtual bool Render(int channel,fixed *buffer,int size,bool updateTick) ;
    virtual bool IsInitialized() ;
    virtual bool IsEmpty() ;
    virtual InstrumentType GetType() { return IT_SYNTH ; } ;
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

    Variable *wave_ ;
    Variable *sub_ ;
    Variable *noise_ ;
    Variable *glide_ ;
    Variable *volume_ ;
    Variable *pan_ ;
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
    Variable *drive_ ;
    Variable *accent_ ;
    Variable *lrate_ ;
    Variable *ldepth_ ;
    Variable *ltarget_ ;
    Variable *dry_ ;
    Variable *dlySend_ ;
    Variable *rvbSend_ ;

    // TREEFROG_INSTRUMENT_GRAPHIC_EQ_V1: same variable contract as
    // SampleInstrument (SIP_EQ* IDs + names), fingerprint-cached rebuild.
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
    SynthVoice voices_[SONG_CHANNEL_COUNT] ;
    int lastNote_[SONG_CHANNEL_COUNT] ;
    bool legatoNext_[SONG_CHANNEL_COUNT] ;
    char name_[32] ;
} ;

#endif