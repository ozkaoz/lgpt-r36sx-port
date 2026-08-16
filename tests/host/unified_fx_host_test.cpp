// FXP_UNIFIED_FX (bacon-1.5, item 5): host DSP test of the UNIFIED FX API.
// Compiles the real FxEngine.cpp (with DelayLine/Reverb/ParametricEQ/
// Compressor) and verifies that:
//   - FxEngine::SetParam/GetParam round-trip every master FX parameter
//     through the kFxParams_ table with the same conversions the UI used;
//   - the phrase/table automation conversion fxParamFromByte (byte 0x00-0xFF
//     -> percent -> natural) matches the UI percent curve exactly, so a
//     phrase byte and a UI percent produce the same engine state;
//   - SetParam clamps to the table range and GetParam is safe out of range;
//   - no RT violations are raised by any of the setters.
#include "Application/Audio/FxEngine/FxEngine.h"
#include "Application/Mixer/FxPages.h"
#include "Application/Player/SyncMaster.h"
#include "Services/Audio/Audio.h"
#include "Services/Audio/AudioOut.h"
#include "System/System/System.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Console/Trace.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

using namespace FxEngine;

// Minimal SyncMaster stubs for the host test: the real SyncMaster.cpp pulls
// in the Audio service stack (Config, drivers); the engine only needs the
// tempo for the musical-sync delay path.
SyncMaster::SyncMaster() : tempo_(120), currentSlice_(0), tableRatio_(1),
    beatCount_(0), playSampleCount_(0), tickSampleCount_(0) {}
int SyncMaster::GetTempo() { return tempo_; }

// ---- Stubs for the WavFileWriter dependency pulled by FxEngine.cpp ----
Audio::Audio(AudioSettings &hints) : T_SimpleList<AudioOut>(true), settings_() {}
Audio::~Audio() {}

class StubAudio : public Audio {
  public:
    StubAudio() : Audio(settings_) {}
    virtual void Init() {}
    virtual void Close() {}
    virtual int GetSampleRate() { return 44100; }
  private:
    static AudioSettings settings_;
};
AudioSettings StubAudio::settings_;

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

// Null filesystem: WavFileWriter opens are only used by the stems capture
// (never exercised here), so returning null files is enough to link.
class NullFileSystem : public FileSystem {
  public:
    virtual I_File *Open(const char *path, const char *mode) { return 0; }
    virtual I_Dir *Open(const char *path) { return 0; }
    virtual Result MakeDir(const char *path) { return Result::NoError; }
    virtual void Delete(const char *path) {}
    virtual FileType GetFileType(const char *path) { return FT_UNKNOWN; }
};

void Trace::Debug(const char *, ...) {}
void Trace::Log(const char *, const char *, ...) {}
void Trace::Error(const char *, ...) {}

short Swap16(short from) { return from; }
int Swap32(int from) { return from; }

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

static int failures = 0;
static int checks = 0;

static void check(bool cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static bool feq(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

// ---- SetParam/GetParam round-trip on every row of the table ----
static void testRoundTripAll() {
    FxEngine::FxEngine &fx = FxEngine::FxEngine::GetInstance();
    fx.Reset();
    for (int id = 0; id < FX_PARAM_COUNT; id++) {
        const FxParamSpec &spec = kFxParams_[id];
        if (fxIsDiscreteParam(id)) {
            // Switches (vmin..vmax) and discrete multi-value rows (DLY DIV,
            // RVB MODE, SC SRC, EQ EXT TYPE): step by integer values.
            float dps[3] = { spec.vmin, spec.vmin + 1.0f, spec.vmax };
            if (dps[1] > spec.vmax) dps[1] = spec.vmin;
            for (int p = 0; p < 3; p++) {
                fx.SetParam(id, dps[p]);
                float got = fx.GetParam(id);
                bool ok = ((int)got == (int)dps[p]);
                if (!ok) printf("DIAG disc id=%d probe=%f got=%f\n", id, dps[p], got);
                check(ok, "discrete round-trip");
            }
        } else {
            // Continuous rows: probe near-min, midpoint, near-max (natural).
            float probes[3] = { spec.vmin, 0.5f * (spec.vmin + spec.vmax),
                                spec.vmax };
            for (int p = 0; p < 3; p++) {
                fx.SetParam(id, probes[p]);
                float got = fx.GetParam(id);
                float tol = 0.02f * (spec.vmax - spec.vmin) + 0.01f;
                if (!feq(got, probes[p], tol))
                    printf("DIAG cont id=%d probe=%f got=%f tol=%f\n", id, probes[p], got, tol);
                check(feq(got, probes[p], tol), "continuous round-trip");
            }
        }
    }
    check(fx.GetRtViolations() == 0, "SetParam never raises RT violations");
}

// ---- SetParam clamps to the table range ----
static void testClamp() {
    FxEngine::FxEngine &fx = FxEngine::FxEngine::GetInstance();
    fx.SetParam(FX_P_CMP_THR, -999.0f);
    check(fx.GetParam(FX_P_CMP_THR) == -60.0f, "clamp low (CMP THR)");
    fx.SetParam(FX_P_CMP_THR, 999.0f);
    check(fx.GetParam(FX_P_CMP_THR) == 0.0f, "clamp high (CMP THR)");
    fx.SetParam(FX_P_DLY_TIME, 12345.0f);
    check(fx.GetParam(FX_P_DLY_TIME) == 2000.0f, "clamp high (DLY TIME)");
    fx.SetParam(FX_PARAM_COUNT, 42.0f);   // out of range id: no-op
    fx.SetParam(-1, 42.0f);
    check(fx.GetParam(FX_PARAM_COUNT) == 0.0f, "GetParam out of range -> 0");
    check(fx.GetParam(-1) == 0.0f, "GetParam negative id -> 0");
}

// ---- Automation byte maps to the same engine state as the UI percent ----
static void testByteMatchesUiPercent() {
    FxEngine::FxEngine &fx = FxEngine::FxEngine::GetInstance();
    // The five master automation commands (bacon-1.5 item 4 set: DLYT/DLYF/
    // RVDC/RVSZ/CMPT) map 0x00 -> vmin, 0xFF -> vmax, 0x80 -> exactly the
    // same natural value as percent 50 via fxPercentToDspId.
    int ids[5] = { FX_P_DLY_TIME, FX_P_DLY_FBK, FX_P_RVB_DEC,
                   FX_P_RVB_SIZ, FX_P_CMP_THR };
    for (int i = 0; i < 5; i++) {
        int id = ids[i];
        const FxParamSpec &spec = kFxParams_[id];
        check(feq(fxParamFromByte(id, 0), spec.vmin, 0.02f), "byte 0x00 -> vmin");
        check(feq(fxParamFromByte(id, 0xFF), spec.vmax, 0.02f), "byte 0xFF -> vmax");
        check(fxParamFromByte(id, 0x80) == fxPercentToDspId(id, 50),
              "byte 0x80 == percent 50 (same curve)");
        // Monotonic over the whole byte range.
        float prev = fxParamFromByte(id, 0);
        for (int b = 1; b <= 0xFF; b++) {
            float cur = fxParamFromByte(id, b);
            check(cur >= prev - 1e-4f, "byte mapping monotonic");
            prev = cur;
        }
        // Applying the byte through the unified API sets the same value the
        // UI would set for the same percent.
        fx.SetParam(id, fxParamFromByte(id, 0x80));
        check(feq(fx.GetParam(id), fxPercentToDspId(id, 50), 0.05f),
              "SetParam(byte) == SetParam(percent 50)");
    }
    // Perceptual sanity: LOG2 params sit at the geometric midpoint for
    // 0x80 (equal octaves), not the linear midpoint.
    check(feq(fxParamFromByte(FX_P_DLY_TIME, 0x80),
              sqrtf(10.0f * 2000.0f), 0.5f), "DLYT 0x80 = sqrt(10*2000) ms");
    check(feq(fxParamFromByte(FX_P_RVB_DEC, 0x80),
              sqrtf(0.2f * 8.0f), 0.02f), "RVDC 0x80 = sqrt(0.2*8) s");
}

// ---- UI delegation: fxGet/fxSet semantics == unified API ----
static void testUiDelegation() {
    FxEngine::FxEngine &fx = FxEngine::FxEngine::GetInstance();
    // The UI path (MixerView::fxSet -> FxEngine::SetParam) must produce the
    // exact same engine state as the previous direct setters.
    fx.SetCompThresholdDb(fl2fp(-24.0f));
    check(fx.GetParam(FX_P_CMP_THR) == fp2fl(fx.GetCompThresholdDb()),
          "GetParam == direct readback (THR)");
    fx.SetParam(FX_P_DLY_TIME, 500.0f);
    check(fx.GetParam(FX_P_DLY_TIME) == fp2fl(fx.GetDelayTimeMs()),
          "GetParam == direct readback (DLY TIME)");
    fx.SetParam(FX_P_EQ_LOW_GAI, 3.0f);   // also engages the band
    check(fx.GetEqBandEnabled(0), "GAI edit engages the band");
    check(!fx.GetEqBypass(), "GAI edit disengages EQ bypass");
}

int main() {
    Audio::Install(new StubAudio());
    System::Install(new StubSystem());
    FileSystem::Install(new NullFileSystem());
    testRoundTripAll();
    testClamp();
    testByteMatchesUiPercent();
    testUiDelegation();
    if (failures == 0) printf("ALL OK (%d checks)\n", checks);
    else printf("%d/%d checks FAILED\n", failures, checks);
    return failures ? 1 : 0;
}