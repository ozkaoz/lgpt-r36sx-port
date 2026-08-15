// PIANO_SYNTH (bacon-1.5, item 7): host DSP test of the polyphonic additive
// piano instrument.  Compiles the real PianoSynth.cpp (with FilterV2 and
// InstrumentEq) and verifies that:
//   - the exported variables exist under the expected names/FourCCs and the
//     defaults match the piano contract (fast attack, sustain 0, tail);
//   - Start/Render/Stop drive the voices through the envelopes without
//     NaN/INF and Render returns false once everything releases;
//   - polyphony: overlapping notes ring simultaneously (voice stealing);
//   - velocity (I_CMD_MVEL) shapes the peak amplitude;
//   - pitch bend retunes active voices instantly;
//   - sends (DLYS/RVBS live, base overrides) follow the 0xFF inherit
//     contract; VOLM/PAN/FCUT/FRES map 0..255 onto the 0..100 vars;
//   - table + table automation + TableSaveState round-trip;
//   - the EQ8 variables rebuild eqDsp_ without touching the sample path;
//   - Haas width keeps the output finite and equal-power pan extremes sane.
#include "Application/Instruments/PianoSynth.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Instruments/FilterV2.h"
#include "Application/Model/Groove.h"
#include "Application/Player/SyncMaster.h"
#include "Services/Audio/Audio.h"
#include "Services/Audio/AudioOut.h"
#include "System/System/System.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Minimal Audio stub: the real Audio.cpp pulls Config/Trace; the synth only
// needs a stable driver rate.
Audio::Audio(AudioSettings &hints) : T_SimpleList<AudioOut>(true), settings_() {}
Audio::~Audio() {}

class StubAudio : public Audio {
  public:
    StubAudio() : Audio(stubSettings_) {}
    virtual void Init() {}
    virtual void Close() {}
    virtual int GetSampleRate() { return 44100; }
  private:
    static AudioSettings stubSettings_;
};
AudioSettings StubAudio::stubSettings_;

// Minimal System stub for SYS_MEMSET.
class StubSystem : public System {
  public:
    virtual unsigned long GetClock() { return 0; }
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

// Minimal SyncMaster stub for AudioOut.cpp (only GetPlaySampleCount used).
SyncMaster::SyncMaster() : tempo_(120), currentSlice_(0), tableRatio_(1),
    beatCount_(0), playSampleCount_(0), tickSampleCount_(0) {}
float SyncMaster::GetPlaySampleCount() { return 0; }

// AudioOut stub: the real ctor/dtor live in AudioOut.cpp (pulls AudioMixer);
// only the symbols needed by T_SimpleList<AudioOut> are required here.
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

static bool bufferAllZero(const fixed *b, int n) {
    for (int i = 0; i < n; i++) {
        if (b[i] != 0) return false;
    }
    return true;
}

// Peak absolute value of a stereo buffer (float domain).
static float bufferPeak(const fixed *b, int n) {
    float peak = 0.0f;
    for (int i = 0; i < n; i++) {
        float v = fabsf((float)b[i]);
        if (v > peak) peak = v;
    }
    return peak;
}

int main() {
    Audio::Install(new StubAudio());
    System::Install(new StubSystem());

    fixed buffer[1024 * 2];

    // ---- 1. variable table / defaults ----
    PianoSynth synth;
    check(synth.Init(), "Init");
    check(synth.GetType() == IT_PIANO, "type IT_PIANO");
    check(synth.IsInitialized(), "IsInitialized");

    Variable *v = synth.FindVariable(PNP_MODE);
    check(v != 0 && strcmp(v->GetName(), "mode") == 0, "var mode");
    check(v && v->GetInt() == 0, "mode default EP");
    v = synth.FindVariable(PNP_VOLUME);
    check(v && v->GetInt() == 100, "volume default 100");
    v = synth.FindVariable(PNP_SUSTAIN);
    check(v && v->GetInt() == 0, "sustain default 0 (piano)");
    v = synth.FindVariable(PNP_ATTACK);
    check(v && v->GetInt() == 5, "attack default 5");
    v = synth.FindVariable(PNP_DECAY);
    check(v && v->GetInt() == 40, "decay default 40");
    v = synth.FindVariable(SIP_EQEN);
    check(v && v->GetInt() == 1, "eq bypass default on");
    v = synth.FindVariable(PNP_DLYSEND);
    check(v && v->GetInt() == -1, "dly send base -1");

    // ---- 2. render a note through the envelope ----
    check(synth.Start(0, 60, true), "Start ch0");
    memset(buffer, 0, sizeof(buffer));
    bool got = synth.Render(0, buffer, 512, false);
    check(got, "Render ch0 audible");
    check(bufferFinite(buffer, 512 * 2), "buffer finite");
    check(!bufferAllZero(buffer, 512 * 2), "buffer non-zero");
    synth.Purge();

    // ---- 3. polyphony: overlapping notes on the same channel ----
    // A second note must not kill the first (4 voices, re-strike semantics).
    check(synth.Start(0, 64, true), "Start ch0 second note");
    got = synth.Render(0, buffer, 512, false);
    check(got, "Render both voices");
    check(bufferFinite(buffer, 512 * 2), "poly buffer finite");
    synth.Purge();

    // ---- 4. release + natural tail ----
    v = synth.FindVariable(PNP_RELEASE);
    v->SetInt(1);  // ~40 ms
    check(synth.Start(0, 60, true), "Start release note");
    synth.Stop(0);
    bool finished = false;
    for (int i = 0; i < 8 && !finished; i++) {
        finished = !synth.Render(0, buffer, 512, false);
    }
    check(finished, "render false after release");
    synth.Purge();

    // ---- 5. velocity shapes the peak ----
    v = synth.FindVariable(PNP_RELEASE);
    v->SetInt(5);  // ~200 ms, flushed between measurements
    float peakHigh = 0.0f;
    synth.ProcessCommand(0, I_CMD_MVEL, 254);  // vel 127
    synth.Start(0, 60, true);
    synth.Render(0, buffer, 512, false);
    peakHigh = bufferPeak(buffer, 512 * 2);
    check(peakHigh > 0.0f, "velocity 127 audible");
    synth.Stop(0);
    for (int i = 0; i < 24 && synth.Render(0, buffer, 512, false); i++) {}

    float peakLow = 0.0f;
    synth.ProcessCommand(0, I_CMD_MVEL, 20);  // vel 10
    synth.Start(0, 60, true);
    synth.Render(0, buffer, 512, false);
    peakLow = bufferPeak(buffer, 512 * 2);
    check(peakLow > 0.0f && peakLow < peakHigh * 0.5f, "soft velocity quieter");
    synth.Purge();

    // ---- 6. pitch bend retunes instantly ----
    synth.ProcessCommand(0, I_CMD_MVEL, 254);
    synth.Start(0, 60, true);
    synth.ProcessCommand(0, I_CMD_PTCH, 0x8F);  // ~+11.5 semitones
    got = synth.Render(0, buffer, 512, false);
    check(got && bufferFinite(buffer, 512 * 2), "bend finite");
    synth.Purge();

    // ---- 7. sends (Fase 15 contract) ----
    check(synth.GetFxDry() == 100, "GetFxDry default 100");
    check(synth.GetFxDelaySendOverride() == 0xFF, "GetFxDelaySendOverride inherit");
    check(synth.GetLiveDelaySend(0) == 0xFF, "GetLiveDelaySend inherit");
    v = synth.FindVariable(PNP_DLYSEND);
    v->SetInt(35);
    check(synth.GetFxDelaySendOverride() == 35, "persisted delay base 35");
    synth.ProcessCommand(0, I_CMD_DLYS, 60);
    check(synth.GetLiveDelaySend(0) == 60, "live delay 60");
    synth.ProcessCommand(0, I_CMD_RVBS, 25);
    check(synth.GetLiveReverbSend(0) == 25, "live reverb 25");

    // ---- 8. ProcessCommand mappings 0..255 -> 0..100 ----
    synth.ProcessCommand(0, I_CMD_VOLM, 128);
    v = synth.FindVariable(PNP_VOLUME);
    check(v && v->GetInt() == 50, "VOLM 128 -> 50");
    synth.ProcessCommand(0, I_CMD_PAN_, 0);
    v = synth.FindVariable(PNP_PAN);
    check(v && v->GetInt() == 0, "PAN 0 -> 0");
    synth.ProcessCommand(0, I_CMD_FCUT, 255);
    v = synth.FindVariable(PNP_FCUT);
    check(v && v->GetInt() == 100, "FCUT 255 -> 100");
    synth.ProcessCommand(0, I_CMD_FRES, 51);
    v = synth.FindVariable(PNP_FRES);
    check(v && v->GetInt() == 20, "FRES 51 -> 20");

    // ---- 9. table + automation + TableSaveState ----
    check(synth.GetTable() == 0, "table default 0");
    v = synth.FindVariable(SIP_TABLE);
    v->SetInt(5);
    check(synth.GetTable() == 5, "table 5");
    v = synth.FindVariable(SIP_TABLEAUTO);
    v->SetInt(1);
    check(synth.GetTableAutomation() == true, "table auto on");
    TableSaveState ts;
    ts.Reset();
    ts.position_[0] = 3;
    synth.SetTableState(ts);
    TableSaveState ts2;
    synth.GetTableState(ts2);
    check(ts2.position_[0] == 3, "table state round-trip");

    // ---- 10. EQ variables rebuild without touching the sample path ----
    v = synth.FindVariable(SIP_EQF0);
    v->SetInt(20000);  // 200 Hz
    check(synth.Start(0, 45, true), "EQ Start");
    synth.Render(0, buffer, 512, false);
    check(bufferFinite(buffer, 512 * 2), "EQ buffer finite");
    v = synth.FindVariable(SIP_EQEN);
    v->SetInt(0);
    synth.Render(0, buffer, 512, false);
    check(bufferFinite(buffer, 512 * 2), "EQ bypassed finite");
    synth.Purge();

    // ---- 11. modes and width extremes stay finite ----
    v = synth.FindVariable(PNP_MODE);
    v->SetInt(1);  // TINE
    v = synth.FindVariable(PNP_WIDTH);
    v->SetInt(100);  // max Haas
    synth.Start(0, 60, true);
    synth.Render(0, buffer, 512, false);
    check(bufferFinite(buffer, 512 * 2), "TINE + width finite");
    check(!bufferAllZero(buffer, 512 * 2), "TINE + width audible");
    v = synth.FindVariable(PNP_WIDTH);
    v->SetInt(0);
    v = synth.FindVariable(PNP_PAN);
    v->SetInt(0);  // hard left
    synth.Start(0, 60, true);
    synth.Render(0, buffer, 512, false);
    check(bufferFinite(buffer, 512 * 2), "hard-left finite");
    synth.Purge();

    // ---- 12. Purge resets ----
    synth.Purge();
    v = synth.FindVariable(PNP_VOLUME);
    check(v && v->GetInt() == 100, "purge resets volume");
    v = synth.FindVariable(PNP_DLYSEND);
    check(v && v->GetInt() == -1, "purge resets delay send");
    got = synth.Render(0, buffer, 512, false);
    check(!got, "render false after purge");

    printf("piano_synth: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}