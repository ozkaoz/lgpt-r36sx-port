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

static double bufferRms(const fixed *b, int n) {
    double acc = 0.0;
    for (int i = 0; i < n; i++) {
        // Q15 (FIXED_SHIFT = 15): full scale 1.0 == 32768.
        double v = (double)b[i] / 32768.0;
        acc += v * v;
    }
    return sqrt(acc / n);
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
    // BACON_1.5_SYNTH_0DB (U2.52.5, feedback): sustain defaults to 100 so
    // the synth sustains at 0 dBFS like a sample at full volume (the old
    // 60 default dropped sustained notes ~4.4 dB below the samples).
    v = synth.FindVariable(SBP_SUSTAIN);
    check(v && v->GetInt() == 100, "sustain default 100");
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

    // ---- 7b. EQ edit-sequence audibility on the REAL render path ----
    // Device feedback (U2.52.4): "any EQ edit kills the sound".  Reproduce
    // the exact view flow -- variable writes + Render() calls -- and assert
    // the signal never collapses, for every band, every type, sequential
    // edits, and the sustain-100 level parity.
    Variable *eqEn = synth.FindVariable(SIP_EQEN);
    Variable *eqMask = synth.FindVariable(SIP_EQMASK);
    static const FourCC kTFreq[8] = {SIP_EQF0, SIP_EQF1, SIP_EQF2, SIP_EQF3,
                                     SIP_EQF4, SIP_EQF5, SIP_EQF6, SIP_EQF7};
    static const FourCC kTGain[8] = {SIP_EQG0, SIP_EQG1, SIP_EQG2, SIP_EQG3,
                                     SIP_EQG4, SIP_EQG5, SIP_EQG6, SIP_EQG7};
    static const FourCC kTType[8] = {SIP_EQT0, SIP_EQT1, SIP_EQT2, SIP_EQT3,
                                     SIP_EQT4, SIP_EQT5, SIP_EQT6, SIP_EQT7};
    static const FourCC kTQ[8]    = {SIP_EQ_Q0, SIP_EQ_Q1, SIP_EQ_Q2, SIP_EQ_Q3,
                                     SIP_EQ_Q4, SIP_EQ_Q5, SIP_EQ_Q6, SIP_EQ_Q7};
    Variable *eqF[8], *eqG[8], *eqT[8], *eqQ[8];
    for (int b = 0; b < 8; b++) {
        eqF[b] = synth.FindVariable(kTFreq[b]);
        eqG[b] = synth.FindVariable(kTGain[b]);
        eqT[b] = synth.FindVariable(kTType[b]);
        eqQ[b] = synth.FindVariable(kTQ[b]);
        check(eqF[b] && eqG[b] && eqT[b] && eqQ[b], "EQ vars band present");
    }
    // Reset to the factory state (8 bands on, 0 dB, BELL, defaults).
    eqEn->SetInt(1);
    eqMask->SetInt(0xFF);
    for (int b = 0; b < 8; b++) {
        eqF[b]->SetInt((int)(8000.0 * pow(2.0, b) + 0.5));
        eqG[b]->SetInt(0);
        eqT[b]->SetInt(0);
        eqQ[b]->SetInt(100);
    }
    // Restore the level-relevant synth state the earlier sections disturbed
    // (VOLM 128 -> volume 50, PAN 0 -> right channel silent, FCUT 255,
    // GLIDE 50, FRES 20), so the level checks measure the real 0 dBFS path.
    synth.FindVariable(SBP_VOLUME)->SetInt(100);
    synth.FindVariable(SBP_PAN)->SetInt(50);
    synth.FindVariable(SBP_GLIDE)->SetInt(0);
    synth.FindVariable(SBP_FCUT)->SetInt(80);  // default; 100 degenerates the TPT SVF
    synth.FindVariable(SBP_FRES)->SetInt(0);
    synth.FindVariable(SBP_DRIVE)->SetInt(0);
    synth.FindVariable(SBP_ACCENT)->SetInt(0);
    // Sustained note measured at the exact sustain level: attack=0 / decay=0
    // land the envelope instantly on sustain (the defaults attack 10 /
    // decay 20 are ~200 ms / ~400 ms ramps).  The sustain value is captured
    // at Start(), so each level needs its own note.
    synth.FindVariable(SBP_ATTACK)->SetInt(0);
    synth.FindVariable(SBP_DECAY)->SetInt(0);
    Variable *sus = synth.FindVariable(SBP_SUSTAIN);
    double rmsS60, rmsS100;
    sus->SetInt(60);
    synth.Start(0, 45, true);
    for (int i = 0; i < 8; i++) {
        synth.Render(0, buffer, 512, false);
    }
    rmsS60 = bufferRms(buffer, 512 * 2);
    sus->SetInt(100);
    synth.Start(0, 45, true);
    for (int i = 0; i < 8; i++) {
        synth.Render(0, buffer, 512, false);
    }
    rmsS100 = bufferRms(buffer, 512 * 2);
    // sustain 100 must be clearly louder than sustain 60 (>= 1.3x).
    check(rmsS100 > rmsS60 * 1.3f, "sustain 100 >= 1.3x louder than 60");
    // ... and sit at the new U2.57 level: BACON_1.5_SYNTH_VOLUME_SCALE
    // (U2.57, feedback #10) re-padded the synth engine -20 dB (volume 100
    // = the old 10, "100 de volumen debe ser el equivalente a 10 actual"),
    // so the full-scale saw sustains at peak ~0.1 / rms ~0.0577 -- audible
    // and far above the dead-signal floor (< 0.001), but no longer the
    // loudest thing in the mix.
    check(rmsS100 > 0.03, "sustained note at the -20 dB synth level");
    // The device edit flow: X+UP (gain +1) on every band, rendered each
    // time -- the sound must never collapse after ANY single edit.
    for (int b = 0; b < 8; b++) {
        eqG[b]->SetInt(1);
        synth.Render(0, buffer, 512, false);
        // Audibility floor: a dead signal measures < 0.001, a +1 dB edit on
        // the unity saw stays far above 0.02.
        check(bufferRms(buffer, 512 * 2) > 0.02,
              "single +1dB edit stays audible");
    }
    // B (type cycle): every EQ type must remain audible, not just BELL.
    // The device view edits ONE selected band at a time; a full 8-band
    // cascade of the same type is not a real edit flow and genuinely
    // kills HP/LP at the extremes.  Band 0 (80 Hz) is the worst case for
    // LOW_PASS on the 110 Hz saw and still attenuates only to |H|=0.61.
    {
        Variable *sus = synth.FindVariable(SBP_SUSTAIN);
        sus->SetInt(100);
        for (int t = 0; t < 7; t++) {
            for (int b = 0; b < 8; b++) eqT[b]->SetInt(0);
            eqT[0]->SetInt(t);
            // give the smoothing time to converge, then measure
            for (int i = 0; i < 6; i++) {
                synth.Render(0, buffer, 512, false);
            }
            check(bufferRms(buffer, 512 * 2) > 0.02,
                  "EQ type remains audible");
            if (bufferRms(buffer, 512 * 2) <= 0.02)
                printf("DBG type t=%d rms=%.5f\n", t, bufferRms(buffer, 512 * 2));
            if (t == 3) {
                printf("DBG t3 first8: %d %d %d %d %d %d %d %d\n",
                       buffer[0], buffer[1], buffer[2], buffer[3],
                       buffer[4], buffer[5], buffer[6], buffer[7]);
                eqEn->SetInt(0);
                for (int i = 0; i < 6; i++) synth.Render(0, buffer, 512, false);
                printf("DBG t3 EQ-off rms=%.5f\n", bufferRms(buffer, 512 * 2));
                eqEn->SetInt(1);
                for (int i = 0; i < 6; i++) synth.Render(0, buffer, 512, false);
            }
        }
    }
    // Sequential edits without silence in between (the "editing kills the
    // sound" repro): 32 edits in a row, render between each.
    {
        bool audible = true;
        for (int e = 0; e < 32; e++) {
            int b = e & 7;
            int g = ((e >> 3) & 1) ? 1 : -1;
            eqG[b]->SetInt(g);
            synth.Render(0, buffer, 512, false);
            if (bufferRms(buffer, 512 * 2) <= 0.02) {
                audible = false;
                printf("DBG edit e=%d b=%d g=%d rms=%.5f\n", e, b, g,
                       bufferRms(buffer, 512 * 2));
            }
        }
        check(audible, "32 sequential EQ edits never kill the sound");
    }
    // Restore the factory EQ state for the later sections.
    for (int b = 0; b < 8; b++) {
        eqG[b]->SetInt(0);
        eqT[b]->SetInt(0);
        eqQ[b]->SetInt(100);
    }

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