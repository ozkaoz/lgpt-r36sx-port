// MIXER_VU_CHAIN (U2.52.7): host test of the REAL mixer VU chain with a
// REAL BassSynth.
//
// Verifies the exact path the MIX page draws from when a synth plays on a
// song channel:
//   PlayerChannel::StartInstrument -> PlayerChannel::Render (instrument
//   renders, post-pan buffer) -> per-block peak scan -> GetPeakValueL/R ->
//   MixerMeters::SmoothFrame -> MixerMeters::BarLevel (mixVULevel * vol/100).
//
// Checks:
//   1. the scan sees the synth audio on BOTH sides at the device rate
//      (48 kHz) and the host rate (44.1 kHz);
//   2. the bar level (what the MIX page would draw) is visibly filled at
//      volume 100 and 127;
//   3. the peaks stay up across sustained buffers (no decay between blocks);
//   4. mute -> audible false -> scan gated -> VU decays to 0;
//   5. volume 0 -> same gating;
//   6. hard pan -100/+100 -> the post-pan scan shows the opposite side
//      empty (stereo meters reflect the pan);
//   7. transport stopped (SmoothFrame running=false) -> bar empties;
//   8. wall-clock idle decay in GetPeakValueL/R (player stopped).
//
// Compiles the real PlayerChannel.cpp + BassSynth.cpp (+ FilterV2,
// InstrumentEq, TablePlayback) with the same stub pattern as
// bass_synth_host_test.  Mixer model and FxEngine are stubbed so the FX
// send path is never entered (channel sends default to 0).
#include "Application/Player/PlayerChannel.h"
#include "Application/Instruments/BassSynth.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Instruments/FilterV2.h"
#include "Application/Model/Groove.h"
#include "Application/Model/Mixer.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Mixer/MixerMeters.h"
#include "Application/Audio/FxEngine/FxEngine.h"
#include "Services/Audio/AudioOut.h"
#include "Services/Audio/Audio.h"
#include "System/System/System.h"
#include "Foundation/Variables/Variable.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Minimal Audio stub: the real Audio.cpp pulls Config/Trace; the chain only
// needs a stable driver rate (the device runs at 48000).
Audio::Audio(AudioSettings &hints) : T_SimpleList<AudioOut>(true), settings_() {}
Audio::~Audio() {}

class StubAudio : public Audio {
  public:
    explicit StubAudio(int rate) : Audio(stubSettings_), rate_(rate) {}
    virtual void Init() {}
    virtual void Close() {}
    virtual int GetSampleRate() { return rate_; }
    void SetRate(int rate) { rate_ = rate; }
  private:
    static AudioSettings stubSettings_;
    int rate_;
};
AudioSettings StubAudio::stubSettings_;

// Advanceable clock: lets the test exercise the wall-clock idle decay of
// GetPeakValueL/R that runs when Render() stops being called.
static unsigned long g_clock = 0;

// Minimal System stub for SYS_MEMSET / GetClock.
class StubSystem : public System {
  public:
    virtual unsigned long GetClock() { return g_clock; }
    virtual int GetBatteryLevel() { return 100; }
    virtual void *Malloc(unsigned size) { return malloc(size); }
    virtual void Free(void *p) { free(p); }
    virtual void Memset(void *addr, char value, int size) {
        memset(addr, value, size);
    }
    virtual void *Memcpy(void *s1, const void *s2, int n) {
        return memcpy(s1, s2, n);
    }
    virtual void PostQuitMessage() {}
    virtual unsigned int GetMemoryUsage() { return 0; }
};

// Minimal SyncMaster stub (TableSlice is the only method the chain reads).
SyncMaster::SyncMaster() : tempo_(120), currentSlice_(0), tableRatio_(1),
    beatCount_(0), playSampleCount_(0), tickSampleCount_(0) {}
float SyncMaster::GetPlaySampleCount() { return 0; }
bool SyncMaster::TableSlice() { return false; }

// AudioOut stub: same pattern as bass_synth_host_test (only the symbols
// needed by T_SimpleList<AudioOut> / AudioModule are required here).
AudioOut::AudioOut() : AudioMixer("AudioOut"), sampleOffset_(0) {}
AudioOut::~AudioOut() {}
AudioMixer::AudioMixer(const char *name) :
    T_SimpleList<AudioModule>(false), enableRendering_(0), writer_(0),
    name_(name) {
    volume_ = i2fp(1);
    softclip_ = -1;
    softclipGain_ = 0;
    masterVolume_ = 100;
    masterVolumeCached_ = -1;
    dampCached_ = 1.0f;
    clipped_ = false;
    clipBypass_ = false;
    peakValue_ = 0.0f;
    lastPeakClock_ = 0;
}
AudioMixer::~AudioMixer() {}
bool AudioMixer::Render(fixed *buffer, int samplecount) { return true; }
void AudioMixer::SetSoftclip(int clip, int gain) {}
void AudioMixer::SetMasterVolume(int volume) {}
bool AudioMixer::Clipped() { return false; }

// Groove stub: TablePlayback.cpp instantiates the T_Singleton<Groove>.
Groove::Groove() : Persistent("groove") {}
Groove::~Groove() {}
Persistent::Persistent(const char *nodeName) : SubService(0), nodeName_(nodeName) {}
SubService::SubService(int fourCC) : fourCC_(fourCC) {}
SubService::~SubService() {}
bool Groove::UpdateGroove(ChannelGroove &g, bool reverse) { return false; }
void Groove::SaveContent(TiXmlNode *node) {}
void Groove::RestoreContent(TiXmlElement *element) {}

// Mixer model stub: only the send getters the chain reads; both default to
// 0 so the FxEngine send path is never entered.
Mixer::Mixer() : Persistent("mixer") {}
Mixer::~Mixer() {}
int Mixer::GetChannelDelaySend(int) { return 0; }
int Mixer::GetChannelReverbSend(int) { return 0; }
void Mixer::SaveContent(TiXmlNode *) {}
void Mixer::RestoreContent(TiXmlElement *) {}

// MixerService stub: SetMixBus never runs in the test, but the chain links
// against the ctor + GetMixBus (PlayerChannel.cpp) and the vtable needs the
// pure virtuals of I_Observer/CommandExecuter.
MixerService::MixerService() : Observable() {}
MixerService::~MixerService() {}
MixBus *MixerService::GetMixBus(int) { return 0; }
void MixerService::Update(Observable &, I_ObservableData *) {}
void MixerService::Execute(FourCC, float) {}

// FxEngine stub: unreachable with sends at 0, but the chain links against
// GetInstance + AccumulateChannelSend (and the engine members' ctors).
FxEngine::FxEngine::FxEngine() {}
FxEngine::DelayLine::DelayLine() {}
FxEngine::Reverb::Reverb() {}
FxEngine::ParametricEQ::ParametricEQ() {}
FxEngine::Compressor::Compressor() {}
FxEngine::FxEngine &FxEngine::FxEngine::GetInstance() {
    static FxEngine f;
    return f;
}
void FxEngine::FxEngine::AccumulateChannelSend(int, const fixed *, int,
                                               fixed, fixed) {}

static int failures = 0;
static int checks = 0;

static void check(bool cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static bool bufferFinite(const fixed *b, int n) {
    for (int i = 0; i < n; i++) {
        if (isnan((double)b[i]) || isinf((double)b[i])) return false;
    }
    return true;
}

static void runChain(int rate, const char *tag) {
    fixed buffer[1024 * 2];

    BassSynth synth;
    check(synth.Init(), "synth Init");

    // Direct render first: the synth must write non-zero audio in this
    // harness before the channel is involved.
    memset(buffer, 0, sizeof(buffer));
    check(synth.Start(0, 60, true), "synth Start ch0 direct");
    bool directGot = synth.Render(0, buffer, 512, false);
    float dmax = 0.0f;
    for (int i = 0; i < 512 * 2; i++) {
        float v = fp2fl(buffer[i]);
        if (v < 0.0f) v = -v;
        if (v > dmax) dmax = v;
    }
    printf("[%s] direct render got=%d max=%.4f\n", tag, directGot, dmax);

    PlayerChannel pc(0);
    check(synth.Start(0, 60, true), "synth Start ch0");
    pc.StartInstrument(&synth, 60, true);
    check(pc.GetInstrument() == &synth, "channel holds the synth");

    // ---- 1. render one buffer: the scan must see the synth on both sides
    memset(buffer, 0, sizeof(buffer));
    bool got = pc.Render(buffer, 1024);
    check(got, "chain Render audible");
    check(bufferFinite(buffer, 1024 * 2), "chain buffer finite");
    float cmax = 0.0f;
    for (int i = 0; i < 1024 * 2; i++) {
        float v = fp2fl(buffer[i]);
        if (v < 0.0f) v = -v;
        if (v > cmax) cmax = v;
    }
    printf("[%s] chain render max=%.4f\n", tag, cmax);
    float pL = pc.GetPeakValueL();
    float pR = pc.GetPeakValueR();
    printf("[%s] peakL=%.4f peakR=%.4f\n", tag, pL, pR);
    check(pL > 0.02f, "scan sees synth audio on L");
    check(pR > 0.02f, "scan sees synth audio on R");

    // ---- 2. bar level through the real MixerMeters path (what the MIX
    // page draws): volume 100 and 127 must produce a visibly filled bar.
    MixerMeters meters;
    float peaksL[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    float peaksR[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    peaksL[0] = pL;
    peaksR[0] = pR;
    meters.SmoothFrame(true, 8, peaksL, peaksR);
    float lvl100 = MixerMeters::BarLevel(meters.LevelL(0), 100);
    float lvl127 = MixerMeters::BarLevel(meters.LevelL(0), 127);
    printf("[%s] barLevel vol100=%.3f vol127=%.3f\n", tag, lvl100, lvl127);
    check(lvl100 > 0.5f, "bar more than half full at volume 100");
    check(lvl127 > 0.5f, "bar more than half full at volume 127");

    // ---- 3. sustained buffers: the peaks must stay up (the sub-block
    // decay must never pull a sounding channel down between buffers).
    for (int i = 0; i < 8; i++) {
        pc.Render(buffer, 1024);
    }
    pL = pc.GetPeakValueL();
    pR = pc.GetPeakValueR();
    check(pL > 0.02f, "peaks sustained over 8 buffers");
    check(pR > 0.02f, "peaks sustained over 8 buffers (R)");

    // ---- 4. mute: audible false -> scan gated -> VU decays to 0.
    pc.SetMute(true);
    pc.Render(buffer, 1024);
    pc.SetMute(false);
    check(pc.GetPeakValueL() < 0.01f, "mute empties the VU (scan gated)");
    check(pc.GetPeakValueR() < 0.01f, "mute empties the VU R (scan gated)");

    // ---- 5. volume 0: same gating as mute.
    pc.SetVolume(0);
    pc.Render(buffer, 1024);
    check(pc.GetPeakValueL() < 0.01f, "volume 0 empties the VU");
    pc.SetVolume(100);

    // ---- 6. hard pans: the post-pan scan must show the opposite side
    // empty (the mixer's stereo meters reflect the pan).
    pc.StartInstrument(&synth, 60, true);
    pc.SetPan(-100);  // hard left
    pc.Render(buffer, 1024);
    float pL_hardL = pc.GetPeakValueL();
    float pR_hardL = pc.GetPeakValueR();
    printf("[%s] hardL peakL=%.4f peakR=%.4f\n", tag, pL_hardL, pR_hardL);
    check(pL_hardL > 0.02f, "hard left keeps L peak");
    check(pR_hardL < 0.01f, "hard left empties R peak");
    pc.SetPan(100);  // hard right
    pc.Render(buffer, 1024);
    float pL_hardR = pc.GetPeakValueL();
    float pR_hardR = pc.GetPeakValueR();
    printf("[%s] hardR peakL=%.4f peakR=%.4f\n", tag, pL_hardR, pR_hardR);
    check(pL_hardR < 0.01f, "hard right empties L peak");
    check(pR_hardR > 0.02f, "hard right keeps R peak");
    pc.SetPan(0);

    // ---- 7. transport stopped: OnFrameUpdate samples 0 and SmoothFrame
    // pulls the bar to 0.
    MixerMeters stopped;
    float zL[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    float zR[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    pc.StartInstrument(&synth, 60, true);
    pc.Render(buffer, 1024);
    peaksL[0] = pc.GetPeakValueL();
    peaksR[0] = pc.GetPeakValueR();
    stopped.SmoothFrame(true, 8, peaksL, peaksR);
    check(stopped.LevelL(0) > 0.02f, "running frame fills the bar");
    for (int f = 0; f < 12; f++) {
        stopped.SmoothFrame(false, 8, zL, zR);
    }
    check(stopped.LevelL(0) < 0.01f, "stopped transport empties the bar");

    // ---- 8. wall-clock idle decay: Render() stops -> GetPeakValueL/R
    // decays by elapsed time (the stopped-player path of the getters).
    g_clock += 50;  // ensure lastPeakClock_ != 0 so the getter decays
    pc.StartInstrument(&synth, 60, true);
    pc.Render(buffer, 1024);
    check(pc.GetPeakValueL() > 0.02f, "peak alive before idle");
    g_clock += 200;  // 200 ms without Render
    check(pc.GetPeakValueL() < 0.01f, "idle decay empties the peak");
}

int main() {
    Audio::Install(new StubAudio(44100));
    System::Install(new StubSystem());

    runChain(44100, "44k1");

    ((StubAudio *)Audio::GetInstance())->SetRate(48000);
    runChain(48000, "48k");

    printf("MIXER_VU_CHAIN: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}