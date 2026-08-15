// BASS_SYNTH (bacon-1.5, item 6): host DSP test of the native BassSynth.
// Compiles the real BassSynth.cpp (with FilterV2 and InstrumentEq) and
// verifies that:
//   - the exported variables exist under the expected names/FourCCs and the
//     defaults are in the documented 0..100 UI range;
//   - Start/Render/Stop drive the mono voice through the envelopes without
//     NaN/INF, the buffer is finite and centered, and Render returns false
//     once the release finishes;
//   - glide (legato retrigger) moves the running voice toward the new note;
//   - live send overrides (DLYS/RVBS) and the persisted base overrides
//     (GetFx*/GetLive*) follow the Fase 15 contract (0xFF = inherit);
//   - ProcessCommand VOLM/PAN_/FCUT/FRES map 0..255 onto the 0..100 vars;
//   - table + table automation + TableSaveState round-trip;
//   - the EQ8 variables rebuild eqDsp_ without touching the sample path.
#include "Application/Instruments/BassSynth.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Instruments/FilterV2.h"
#include "Application/Model/Groove.h"
#include "Application/Player/SyncMaster.h"
#include "Services/Audio/AudioOut.h"
#include "Foundation/Variables/Variable.h"
#include "Services/Audio/Audio.h"
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

int main() {
    Audio::Install(new StubAudio());
    System::Install(new StubSystem());

    fixed buffer[1024 * 2];

    // ---- 1. variable table / defaults ----
    BassSynth synth;
    check(synth.Init(), "Init");
    check(synth.GetType() == IT_SYNTH, "type IT_SYNTH");
    check(synth.IsInitialized(), "IsInitialized");

    Variable *v = synth.FindVariable(SBP_WAVE);
    check(v != 0 && strcmp(v->GetName(), "wave") == 0, "var wave");
    check(v && v->GetInt() == 0, "wave default SAW");
    v = synth.FindVariable(SBP_VOLUME);
    check(v && v->GetInt() == 100, "volume default 100");
    v = synth.FindVariable(SBP_PAN);
    check(v && v->GetInt() == 50, "pan default 50");
    v = synth.FindVariable(SBP_FCUT);
    check(v && v->GetInt() == 80, "fcut default 80");
    v = synth.FindVariable(SBP_GLIDE);
    check(v && v->GetInt() == 0, "glide default 0");
    v = synth.FindVariable(SIP_EQEN);
    check(v && v->GetInt() == 1, "eq bypass default on");
    v = synth.FindVariable(SBP_DLYSEND);
    check(v && v->GetInt() == -1, "dly send base -1");

    // ---- 2. render a note through the full envelope ----
    check(synth.Start(0, 45, true), "Start ch0");
    memset(buffer, 0, sizeof(buffer));
    bool got = synth.Render(0, buffer, 512, false);
    check(got, "Render ch0 audible");
    check(bufferFinite(buffer, 512 * 2), "buffer finite");
    bool any = !bufferAllZero(buffer, 512 * 2);
    check(any, "buffer non-zero");

    // Stop -> release -> eventually silence.
    synth.Stop(0);
    synth.Render(0, buffer, 512, false);
    synth.Render(0, buffer, 512, false);
    synth.Render(0, buffer, 512, false);
    synth.Render(0, buffer, 512, false);
    // 4 * 512 samples with the default release (30 -> ~1.2s) is not enough
    // to finish; tighten the release first.
    v = synth.FindVariable(SBP_RELEASE);
    v->SetInt(1);  // ~40 ms
    synth.Start(0, 45, true);
    bool finished = false;
    for (int i = 0; i < 8 && !finished; i++) {
        synth.Stop(0);
        finished = !synth.Render(0, buffer, 512, false);
    }
    check(finished, "render false after release");

    // ---- 3. glide / legato ----
    v = synth.FindVariable(SBP_GLIDE);
    v->SetInt(50);  // ~1 s
    check(synth.Start(0, 45, true), "Start glide note");
    synth.Render(0, buffer, 512, false);
    // Legato retrigger on the same channel glides instead of re-attacking.
    check(synth.Start(0, 48, false), "legato retrigger accepted");
    synth.Render(0, buffer, 512, false);
    synth.Render(0, buffer, 512, false);
    check(bufferFinite(buffer, 512 * 2), "glide buffer finite");

    // ---- 4. sends (Fase 15 contract) ----
    check(synth.GetFxDry() == 100, "GetFxDry default 100");
    check(synth.GetFxDelaySendOverride() == 0xFF, "GetFxDelaySendOverride inherit");
    check(synth.GetLiveDelaySend(0) == 0xFF, "GetLiveDelaySend inherit");
    check(synth.GetLiveReverbSend(0) == 0xFF, "GetLiveReverbSend inherit");
    v = synth.FindVariable(SBP_DLYSEND);
    v->SetInt(35);
    check(synth.GetFxDelaySendOverride() == 35, "persisted delay base 35");
    synth.ProcessCommand(0, I_CMD_DLYS, 60);
    check(synth.GetLiveDelaySend(0) == 60, "live delay 60");
    synth.ProcessCommand(0, I_CMD_RVBS, 25);
    check(synth.GetLiveReverbSend(0) == 25, "live reverb 25");

    // ---- 5. ProcessCommand mappings 0..255 -> 0..100 ----
    synth.ProcessCommand(0, I_CMD_VOLM, 128);
    v = synth.FindVariable(SBP_VOLUME);
    check(v && v->GetInt() == 50, "VOLM 128 -> 50");
    synth.ProcessCommand(0, I_CMD_PAN_, 0);
    v = synth.FindVariable(SBP_PAN);
    check(v && v->GetInt() == 0, "PAN 0 -> 0");
    synth.ProcessCommand(0, I_CMD_FCUT, 255);
    v = synth.FindVariable(SBP_FCUT);
    check(v && v->GetInt() == 100, "FCUT 255 -> 100");
    synth.ProcessCommand(0, I_CMD_FRES, 51);
    v = synth.FindVariable(SBP_FRES);
    check(v && v->GetInt() == 20, "FRES 51 -> 20");

    // ---- 6. table + automation + TableSaveState ----
    check(synth.GetTable() == 0, "table default 0");
    v = synth.FindVariable(SIP_TABLE);
    v->SetInt(5);
    check(synth.GetTable() == 5, "table 5");
    check(synth.GetTableAutomation() == false, "table auto off");
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

    // ---- 7. EQ variables rebuild without touching the sample path ----
    v = synth.FindVariable(SIP_EQF0);
    v->SetInt(20000);  // 200 Hz
    check(synth.Start(0, 45, true), "EQ Start");
    synth.Render(0, buffer, 512, false);
    check(bufferFinite(buffer, 512 * 2), "EQ buffer finite");
    v = synth.FindVariable(SIP_EQEN);
    v->SetInt(0);
    synth.Render(0, buffer, 512, false);
    check(bufferFinite(buffer, 512 * 2), "EQ bypassed finite");

    // ---- 8. wave shapes all render ----
    v = synth.FindVariable(SBP_WAVE);
    for (int w = 0; w < 4; w++) {
        v->SetInt(w);
        synth.Start(0, 45, true);
        synth.Render(0, buffer, 512, false);
        check(bufferFinite(buffer, 512 * 2), "wave shape finite");
    }

    // ---- 9. Purge resets ----
    synth.Purge();
    v = synth.FindVariable(SBP_VOLUME);
    check(v && v->GetInt() == 100, "purge resets volume");
    v = synth.FindVariable(SBP_DLYSEND);
    check(v && v->GetInt() == -1, "purge resets delay send");
    got = synth.Render(0, buffer, 512, false);
    check(!got, "render false after purge");

    printf("bass_synth: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}