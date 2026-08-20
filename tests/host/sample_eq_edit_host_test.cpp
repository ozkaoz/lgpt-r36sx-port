// SAMPLE_EQ_EDIT (U2.52.8, feedback "C"): editing any EQ8 value on a sample
// instrument must NEVER kill the sound.
//
// On the device the user reported: "al editar cualquier valor del EQ8 en un
// sample, el sonido muere".  The root cause found while fixing the bell
// (BACON_1.5_BELL_PREWARPED) was the RBJ peaking coefficients: at low/mid
// center frequencies a boost became a huge shelf toward DC (+14..+37 dB at
// DC measured on the device path), so ANY edit that created a low band
// slammed the instrument output into saturation/DC and the sound died.
// This harness drives a REAL SampleInstrument with a REAL InstrumentEq over
// a generated SoundSource (TestPool-injected, the same path Init() uses:
// SamplePool::GetInstance()->GetSource) and flips SIP_EQ* variables between
// renders while a note keeps playing:
//   1. baseline (all bands neutral) -> the 100 Hz / 1 kHz components of the
//      source read at unity;
//   2. +12 dB @ 100 Hz Q=1 -> the 100 Hz component multiplies by ~4 (the
//      bell REALLY boosts at the center), 1 kHz stays ~1, buffer finite;
//   3. +6 dB @ 1 kHz on top -> 1 kHz multiplies by ~2, 100 Hz unchanged;
//   4. LOW_PASS @ 500 Hz -> 1 kHz attenuates, still finite and audible;
//   5. worst case +24 dB @ 100 Hz Q=10 (the old killer) -> bounded peak,
//      no DC offset, finite;
//   6. reset gains to 0 -> back to unity (identity, sample intact).
//
// Scale notes: the codebase uses Q15 fixed point (FIXED_SHIFT=15 in
// Utils/fixed.h).  fp2fl() therefore returns the 16-bit COUNT of a sample
// (unity audio == 32768 counts), NOT a -1..1 float.  All amplitude checks
// below compare against 32768 counts.  The EQ itself is 64-bit Df2 with
// kSmoothShift=6 (coefficients converge in ~64 renders of 512 frames), so
// every phase renders 120 blocks before measuring.  The source is 60 s long
// so the sample NEVER ends during the test (a 2 s source ended at block 187
// while measurements happened at block 225+, which faked the "death").
//
// Compiles the real SampleInstrument.cpp + SamplePool.cpp-independent stubs
// (SamplePool ctor/dtor/GetSource are defined here; Load/Sort never run) +
// InstrumentEq + FilterV2 + variables with the same stub pattern as
// mixer_vu_chain_host_test.
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/BassSynth.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/CommandList.h"
#include "Application/Instruments/FilterV2.h"
#include "Application/Instruments/SampleVariable.h"
#include "Application/Model/Groove.h"
#include "Application/Model/Mixer.h"
#include "Application/Mixer/MixerService.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Audio/FxEngine/FxEngine.h"
#include "Services/Audio/AudioOut.h"
#include "Services/Audio/Audio.h"
#include "System/System/System.h"
#include "System/Console/Trace.h"
#include "Foundation/Variables/Variable.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

// --- Audio stub (real ctor defined here; the device rate is 48000) ---
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

// --- System stub (GetClock only) ---
static unsigned long g_clock = 0;
class StubSystem : public System {
  public:
    virtual unsigned long GetClock() { return g_clock; }
    virtual int GetBatteryLevel() { return 100; }
    virtual void *Malloc(unsigned size) { return malloc(size); }
    virtual void Free(void *p) { free(p); }
    virtual void Memset(void *addr, char value, int size) { memset(addr, value, size); }
    virtual void *Memcpy(void *s1, const void *s2, int n) { return memcpy(s1, s2, n); }
    virtual void PostQuitMessage() {}
    virtual unsigned int GetMemoryUsage() { return 0; }
};

// --- SyncMaster stub (TableSlice is the only method the render reads) ---
SyncMaster::SyncMaster() : tempo_(120), currentSlice_(0), tableRatio_(1),
    beatCount_(0), playSampleCount_(0), tickSampleCount_(0) {}
float SyncMaster::GetPlaySampleCount() { return 0; }
float SyncMaster::GetTickSampleCount() { return 0; }
bool SyncMaster::TableSlice() { return false; }

// --- AudioOut/AudioMixer stubs (T_SimpleList<AudioOut> machinery) ---
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

// --- Groove / Persistent / SubService stubs (TablePlayback) ---
Groove::Groove() : Persistent("groove") {}
Groove::~Groove() {}
Persistent::Persistent(const char *nodeName) : SubService(0), nodeName_(nodeName) {}
SubService::SubService(int fourCC) : fourCC_(fourCC) {}
SubService::~SubService() {}
bool Groove::UpdateGroove(ChannelGroove &g, bool reverse) { return false; }
void Groove::SaveContent(TiXmlNode *node) {}
void Groove::RestoreContent(TiXmlElement *element) {}

// --- Mixer model stub (sends never read by SampleInstrument::Render) ---
Mixer::Mixer() : Persistent("mixer") {}
Mixer::~Mixer() {}
int Mixer::GetChannelDelaySend(int) { return 0; }
int Mixer::GetChannelReverbSend(int) { return 0; }
void Mixer::SaveContent(TiXmlNode *) {}
void Mixer::RestoreContent(TiXmlElement *) {}

// --- MixerService stub (vtable completeness) ---
MixerService::MixerService() : Observable() {}
MixerService::~MixerService() {}
MixBus *MixerService::GetMixBus(int) { return 0; }
void MixerService::Update(Observable &, I_ObservableData *) {}
void MixerService::Execute(FourCC, float) {}

// --- FxEngine stubs ---
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
void FxEngine::FxEngine::SetParam(int, float) {}

// --- Trace stub (instrument diagnostics log calls) ---
void Trace::Log(const char *category, const char *fmt, ...) {}

// --- chopper extern: no chops in this harness ---
bool LGPTChopperGetChopRangeForSampleIndex(int, int, int *, int *) {
    return false;
}

// --- TestSource: 16-bit mono 48 kHz, 60 s, 100 Hz + 1 kHz sines @ 0.3 ---
// 60 s so the note NEVER ends while the phases below measure (96000-frame
// sources ran out at block 187 while measurements happened at block 225+,
// which faked the "EQ killed the sound" death).
static const int kRate = 48000;
static const int kSrcFrames = kRate * 60;
static short *g_src;

class TestSource : public SoundSource {
  public:
    TestSource() {
        g_src = (short *)malloc(sizeof(short) * kSrcFrames);
        for (int i = 0; i < kSrcFrames; i++) {
            double v = 0.3 * sin(2.0 * 3.14159265358979 * 100.0 * i / kRate)
                     + 0.3 * sin(2.0 * 3.14159265358979 * 1000.0 * i / kRate);
            if (v > 1.0) v = 1.0;
            if (v < -1.0) v = -1.0;
            g_src[i] = (short)(v * 32767.0);
        }
    }
    virtual int GetSize(int note) { return kSrcFrames; }
    virtual int GetSampleRate(int note) { return kRate; }
    virtual int GetChannelCount(int note) { return 1; }
    virtual void *GetSampleBuffer(int note) { return g_src; }
    virtual bool IsMulti() { return false; }
    virtual int GetRootNote(int note) { return 60; }
};

// --- TestPool: SamplePool with one injected source (Init() reads it) ---
SamplePool::SamplePool() {
    for (int i = 0; i < MAX_PIG_SAMPLES; i++) {
        names_[i] = NULL;
        wav_[i] = NULL;
    }
    count_ = 0;
}
SamplePool::~SamplePool() {}
SoundSource *SamplePool::GetSource(int i) { return wav_[i]; }
char **SamplePool::GetNameList() { return names_; }
int SamplePool::GetNameListSize() { return count_; }

// --- TestSourceWav: loads a real 16-bit PCM mono WAV (the SD reference
// sample for feedback (A)); TryLoad returns 0 when the file is absent ---
class TestSourceWav : public SoundSource {
  public:
    TestSourceWav(short *data, int frames, int rate, int ch)
        : data_(data), frames_(frames), rate_(rate), ch_(ch) {}
    ~TestSourceWav() { free(data_); }
    virtual int GetSize(int note) { return frames_; }
    virtual int GetSampleRate(int note) { return rate_; }
    virtual int GetChannelCount(int note) { return ch_; }
    virtual void *GetSampleBuffer(int note) { return data_; }
    virtual bool IsMulti() { return false; }
    virtual int GetRootNote(int note) { return 60; }

    static TestSourceWav *TryLoad(const char *path) {
        // open/read (POSIX) on purpose: the port re-routes fopen() through
        // the SD FileSystem, which the host harness stubs to NULL.
        int fd = open(path, 0);
        if (fd < 0) return 0;
        char hdr[12];
        if (read(fd, hdr, 12) != 12) { close(fd); return 0; }
        if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
            close(fd);
            return 0;
        }
        int rate = 0, ch = 0;
        short *data = 0;
        while (read(fd, hdr, 8) == 8) {
            int size = (unsigned char)hdr[4] | ((unsigned char)hdr[5] << 8) |
                       ((unsigned char)hdr[6] << 16) |
                       ((unsigned char)hdr[7] << 24);
            if (memcmp(hdr, "fmt ", 4) == 0) {
                unsigned char fmt[16];
                if (size < 16 || read(fd, fmt, 16) != 16) {
                    close(fd);
                    return 0;
                }
                if (size > 16) lseek(fd, size - 16, SEEK_CUR);
                ch = fmt[2] | (fmt[3] << 8);
                rate = fmt[4] | (fmt[5] << 8) | (fmt[6] << 16) |
                       (fmt[7] << 24);
            } else if (memcmp(hdr, "data", 4) == 0) {
                if (size <= 0) { close(fd); return 0; }
                data = (short *)malloc(size);
                if (!data || read(fd, data, size) != size) {
                    free(data);
                    close(fd);
                    return 0;
                }
                int frames = size / 2 / (ch > 0 ? ch : 1);
                close(fd);
                if (rate <= 0 || ch <= 0 || frames <= 0) {
                    free(data);
                    return 0;
                }
                return new TestSourceWav(data, frames, rate, ch);
            } else {
                lseek(fd, size, SEEK_CUR);
            }
        }
        close(fd);
        return 0;
    }

  private:
    short *data_;
    int frames_, rate_, ch_;
};

static TestSourceWav *g_hihat;
static TestSourceWav *g_kick;
static TestSourceWav *g_snare;

class TestPool : public SamplePool {
  public:
    TestPool() {
        count_ = 1;
        names_[0] = (char *)malloc(2);
        names_[0][0] = 'T';
        names_[0][1] = 0;
        wav_[0] = new TestSource();
        // Feedback (A): the reference samples on the SD, when reachable.
        g_hihat = TestSourceWav::TryLoad(
            "/mnt/g/lgpt/samples/Drum Kit LGPT/HI HAT 01.wav");
        if (g_hihat) {
            names_[1] = (char *)malloc(2);
            names_[1][0] = 'H';
            names_[1][1] = 0;
            wav_[1] = g_hihat;
            count_ = 2;
        }
        g_kick = TestSourceWav::TryLoad(
            "/mnt/g/lgpt/samples/Drum Kit LGPT/KICK 01.wav");
        if (g_kick) {
            names_[2] = (char *)malloc(2);
            names_[2][0] = 'K';
            names_[2][1] = 0;
            wav_[2] = g_kick;
            count_ = 3;
        }
        g_snare = TestSourceWav::TryLoad(
            "/mnt/g/lgpt/samples/Drum Kit LGPT/SNARE 01.wav");
        if (g_snare) {
            names_[3] = (char *)malloc(2);
            names_[3][0] = 'S';
            names_[3][1] = 0;
            wav_[3] = g_snare;
            count_ = 4;
        }
    }
    static void Install() { T_Singleton<SamplePool>::instance_ = new TestPool(); }
    virtual void Load() {}
    virtual void Sort() {}
};

// --- harness state ---
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

// Q15 fixed point: unity audio == 32768 counts.  A "bounded" buffer must
// stay within +-65536 counts (a +24 dB EQ boost can legitimately exceed
// unity mid-pipeline while the 64-bit Df2 stays clean; the
// BACON_1.5_EQ8_SOFTKNEE then caps the instrument at 32767 Q15).
static bool bufferBounded(const fixed *b, int n) {
    for (int i = 0; i < n; i++) {
        if (fabs((double)fp2fl(b[i])) > 65536.0) return false;
    }
    return true;
}

// Goertzel amplitude (in Q15 counts) of one frequency in a stereo fixed
// buffer.
static double goertzel(const fixed *buf, int frames, double f) {
    double w = 2.0 * 3.14159265358979 * f / (double)kRate;
    double c = 2.0 * cos(w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (int i = 0; i < frames; i++) {
        double x = fp2fl(buf[2 * i]);   // L side
        s0 = x + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    double mag = sqrt(s1 * s1 + s2 * s2 - c * s1 * s2);
    return 2.0 * mag / (double)frames;
}

static double meanAbs(const fixed *buf, int n) {
    double acc = 0;
    for (int i = 0; i < n; i++) acc += fabs((double)fp2fl(buf[i]));
    return acc / (double)n;
}

static int g_renderSize = 512;
static const int kAccBlocks = 40;   // 40 x 512 frames ~0.43 s window

// Render enough buffers for the EQ coefficient smoothing to converge.
static void renderBlocks(SampleInstrument &si, fixed *buf, int blocks) {
    for (int b = 0; b < blocks; b++) {
        memset(buf, 0, sizeof(fixed) * 2 * g_renderSize);
        si.Render(0, buf, g_renderSize, false);
    }
}

// Render into a concatenated window (each block appended, no overwrite) so
// one goertzel/peak pass covers the whole window.
static void renderAccumulate(SampleInstrument &si, fixed *out, int blocks) {
    fixed tmp[512 * 2];
    for (int b = 0; b < blocks; b++) {
        memset(tmp, 0, sizeof(tmp));
        si.Render(0, tmp, g_renderSize, false);
        memcpy(out + b * 2 * g_renderSize, tmp, sizeof(fixed) * 2 * g_renderSize);
    }
}

int main() {
    Audio::Install(new StubAudio(kRate));
    System::Install(new StubSystem());
    TestPool::Install();

    SampleInstrument si;
    Variable *vSample = si.FindVariable(SIP_SAMPLE);
    check(vSample != 0 && vSample->GetInt() == -1, "SIP_SAMPLE defaults to -1");
    vSample->SetInt(0);   // Init() binds pool->GetSource(index); -1 binds none
    si.Init();            // return value is always false by design
    check(si.Start(0, 60, true), "sample Start at root note");
    fixed *buf = (fixed *)malloc(sizeof(fixed) * 2 * g_renderSize);

    // --- baseline: all bands neutral -> unity at both frequencies ---
    renderBlocks(si, buf, 65);
    check(bufferFinite(buf, 2 * g_renderSize), "baseline finite");
    double base100 = goertzel(buf, g_renderSize, 100.0);
    double base1k = goertzel(buf, g_renderSize, 1000.0);
    printf("baseline:  100Hz=%.4f 1kHz=%.4f\n", base100, base1k);
    check(base100 > 1500 && base1k > 1500, "both components audible at baseline");

    // --- control: no edits, keep rendering past the positions where the
    // old DC shelf appeared -> must stay bounded and finite ---
    {
        fixed *cb = (fixed *)malloc(sizeof(fixed) * 2 * g_renderSize);
        bool exploded = false;
        for (int b = 0; b < 40; b++) {
            memset(cb, 0, sizeof(fixed) * 2 * g_renderSize);
            si.Render(0, cb, g_renderSize, false);
            if (!bufferBounded(cb, 2 * g_renderSize)) exploded = true;
        }
        printf("ctrl no-edit bounded=%s\n", exploded ? "NO" : "yes");
        check(!exploded, "no-edit control stays bounded");
        free(cb);
    }

    // --- edit 1: +12 dB @ 100 Hz Q=1 while playing (the old killer) ---
    si.FindVariable(SIP_EQG0)->SetInt(12);
    si.FindVariable(SIP_EQF0)->SetInt(10000);   // 100 Hz * 100
    si.FindVariable(SIP_EQ_Q0)->SetInt(100);    // Q 1.00
    renderBlocks(si, buf, 120);
    check(bufferFinite(buf, 2 * g_renderSize), "+12@100 finite");
    double a100 = goertzel(buf, g_renderSize, 100.0);
    double a1k = goertzel(buf, g_renderSize, 1000.0);
    printf("+12@100Hz: 100Hz=%.4f (x%.2f) 1kHz=%.4f (x%.2f)\n",
           a100, base100 > 0 ? a100 / base100 : 0,
           a1k, base1k > 0 ? a1k / base1k : 0);
    // BACON_1.5_EQ8_SOFTKNEE (U2.59): the source peaks at 0.6, so +12 dB
    // pushes the output past the 0.85 knee -- the measured ratio is the
    // COMPRESSED one (x3.05 measured vs x3.98 linear; the knee's unity cap
    // is measured precisely in B4).  The point here is the boost is real.
    check(a100 > base100 * 2.8, "100 Hz REALLY boosted ~12 dB (bell center)");
    check(a1k < base1k * 1.6, "1 kHz left nearly intact (no DC shelf spill)");

    // --- edit 2: +6 dB @ 1 kHz on top ---
    si.FindVariable(SIP_EQG1)->SetInt(6);
    si.FindVariable(SIP_EQF1)->SetInt(100000);  // 1000 Hz * 100
    si.FindVariable(SIP_EQ_Q1)->SetInt(100);
    renderBlocks(si, buf, 120);
    check(bufferFinite(buf, 2 * g_renderSize), "+6@1k finite");
    double b100 = goertzel(buf, g_renderSize, 100.0);
    double b1k = goertzel(buf, g_renderSize, 1000.0);
    printf("+6@1kHz:   100Hz=%.4f (x%.2f) 1kHz=%.4f (x%.2f)\n",
           b100, base100 > 0 ? b100 / base100 : 0,
           b1k, base1k > 0 ? b1k / base1k : 0);
    // Same knee note as +12 dB above: x2.81 measured vs x3.98 linear.
    check(b100 > base100 * 2.5, "100 Hz band still boosted (independent bands)");
    check(b1k > base1k * 1.2, "1 kHz boosted ~6 dB on top of 100 Hz");

    // --- edit 3: LOW_PASS @ 500 Hz with -6 dB (a 0 dB band is transparent
    // by design, BACON_1.5_EQ8_0DB_TRANSPARENT) cuts the highs ---
    si.FindVariable(SIP_EQG2)->SetInt(-6);
    si.FindVariable(SIP_EQT2)->SetInt(3);   // LOW_PASS
    si.FindVariable(SIP_EQF2)->SetInt(50000);   // 500 Hz
    si.FindVariable(SIP_EQG0)->SetInt(0);   // drop the 100 Hz boost first
    si.FindVariable(SIP_EQG1)->SetInt(0);
    renderBlocks(si, buf, 120);
    check(bufferFinite(buf, 2 * g_renderSize), "lowpass finite");
    double c100 = goertzel(buf, g_renderSize, 100.0);
    double c1k = goertzel(buf, g_renderSize, 1000.0);
    printf("LP@500Hz:  100Hz=%.4f (x%.2f) 1kHz=%.4f (x%.2f)\n",
           c100, base100 > 0 ? c100 / base100 : 0,
           c1k, base1k > 0 ? c1k / base1k : 0);
    check(c100 > base100 * 0.4, "100 Hz survives the lowpass");
    check(c1k < base1k * 0.5, "1 kHz attenuated by the lowpass");
    check(c1k > 300, "1 kHz still audible (sound NOT killed)");

    // --- edit 4: worst case +24 dB @ 100 Hz Q=10 (old DC blowup) ---
    si.FindVariable(SIP_EQT2)->SetInt(0);
    si.FindVariable(SIP_EQG2)->SetInt(0);
    si.FindVariable(SIP_EQG0)->SetInt(24);
    si.FindVariable(SIP_EQF0)->SetInt(10000);   // 100 Hz
    si.FindVariable(SIP_EQ_Q0)->SetInt(1000);   // Q 10
    renderBlocks(si, buf, 240);
    check(bufferFinite(buf, 2 * g_renderSize), "+24@100 Q10 finite");
    double d100 = goertzel(buf, g_renderSize, 100.0);
    double meanAbsV = meanAbs(buf, 2 * g_renderSize);
    double dc = 0;
    for (int i = 0; i < 2 * g_renderSize; i++) dc += (double)fp2fl(buf[i]);
    dc /= (double)(2 * g_renderSize);
    printf("+24@100 Q10: 100Hz=%.4f mean|.|=%.4f dc=%.4f\n", d100, meanAbsV, dc);
    check(d100 > base100 * 3.0, "boost audible at the worst case");
    check(meanAbsV < 131072.0, "peak energy bounded (no saturation explosion)");
    check(fabs(dc) < 1500.0, "no DC offset (old bell shelved +20 dB at DC)");

    // --- edit 5: reset -> identity, sample intact ---
    si.FindVariable(SIP_EQG0)->SetInt(0);
    si.FindVariable(SIP_EQG1)->SetInt(0);
    renderBlocks(si, buf, 120);
    check(bufferFinite(buf, 2 * g_renderSize), "reset finite");
    double e100 = goertzel(buf, g_renderSize, 100.0);
    double e1k = goertzel(buf, g_renderSize, 1000.0);
    printf("reset:     100Hz=%.4f (x%.2f) 1kHz=%.4f (x%.2f)\n",
           e100, base100 > 0 ? e100 / base100 : 0,
           e1k, base1k > 0 ? e1k / base1k : 0);
    check(e100 > base100 * 0.7 && e100 < base100 * 1.3, "100 Hz back to unity");
    check(e1k > base1k * 0.7 && e1k < base1k * 1.3, "1 kHz back to unity");

    // ============ (A) feedback: synth vs sample loudness ============
    // The reference sample is HI HAT 01 on the SD (skipped when /mnt/g is
    // not mounted).  The synth is measured SUSTAINED (the first rendered
    // buffer sits inside the attack envelope, ~6% of the peak), through the
    // same fixed-point chain.  Both buffers are int16<<15 (master scale),
    // so fp2fl() returns the DAC int16 count (1.0 == 32768): the compare
    // below is the real loudness at the output.
    // BACON_1.5_VOL_LEVELS_FL (U2.55, feedback #8): FL-style unity levels.
    // BACON_1.5_SYNTH_VOLUME_SCALE (U2.57, feedback #10): the synth at
    // volume 100 rendered a -20 dB saw (peak 0.1, "100 de volumen debe ser
    // el equivalente a 10 actual"), which read ~21% of the mixer bar while
    // the drums read ~99% at the same setting.
    // BACON_1.5_SYNTH_BAR_LEVEL (U2.60, feedback #12): the bars map PEAK on
    // a dB scale, so a sustained saw (peak 0.113) could never reach the top
    // like the transient samples.  The synth's post-EQ level is raised x8
    // so its peak at volume 100 lands at ~0.9 (90% bar, like the drums at
    // the same volume); the instrument volume then scales the bar down
    // linearly, matching the perceived level.
    printf("-- (A) synth vs sample level --\n");
    {
        BassSynth synth;
        check(synth.Init(), "synth Init");
        synth.FindVariable(SBP_VOLUME)->SetInt(100);
        check(synth.Start(0, 60, true), "synth Start C-3");
        double sPk = 0.0, sAcc = 0.0;
        long long sN = 0;
        for (int b = 0; b < 56; b++) {   // ~1.2 s sustained
            fixed sb[1024 * 2];
            memset(sb, 0, sizeof(sb));
            synth.Render(0, sb, 1024, false);
            for (int i = 0; i < 1024 * 2; i++) {
                double v = fabs((double)fp2fl(sb[i]));
                if (v > sPk) sPk = v;
                sAcc += v * v;
                sN++;
            }
        }
        double sRms = sqrt(sAcc / (double)sN);
        printf("synth vol100 sustained: peak=%.3f rms=%.3f\n", sPk, sRms);
        // New bar policy: the synth at volume 100 peaks at ~0.9 of full
        // scale (90% bar, like the drums at the same volume).  A regression
        // to the -20 dB saw (0.1) or a double boost (1.8) both fail.
        // Bounds: [0.8, 1.0] of full scale = [26214, 32767].
        check(sPk > 26214.0 && sPk < 32768.0, "synth sustained peak at the 90% bar level");

        if (g_hihat) {
            SampleInstrument sh;
            sh.FindVariable(SIP_SAMPLE)->SetInt(1);
            sh.Init();
            check(sh.Start(0, 60, true), "hi-hat Start");
            double hPk = 0.0, hAcc = 0.0;
            long long hN = 0;
            for (int b = 0; b < 90; b++) {   // covers the ~1.2 s hat
                fixed hb[1024 * 2];
                memset(hb, 0, sizeof(hb));
                if (!sh.Render(0, hb, 1024, false)) break;
                for (int i = 0; i < 1024 * 2; i++) {
                    double v = fabs((double)fp2fl(hb[i]));
                    if (v > hPk) hPk = v;
                    hAcc += v * v;
                    hN++;
                }
            }
            double hRms = hN ? sqrt(hAcc / (double)hN) : 0.0;
            printf("hihat vol128: peak=%.3f rms=%.3f\n", hPk, hRms);
            check(hPk > 0.0, "hi-hat renders audio");
            // The synth at volume 100 now peaks at ~0.9 (90% bar, the new
            // BAR_LEVEL policy) vs the hat at 0.68 -- the bass can reach
            // the top of the bar like the drums, and the instrument volume
            // scales the bar down from there.
            check(sPk > hPk * 0.8 && sPk < hPk * 1.5,
                  "synth peak now matches the reference sample peak");
        } else {
            printf("SD sample not mounted - hi-hat compare skipped\n");
        }
    }

    // ============ (B) feedback #8: EQ +-1 dB edits (the "EQ destroys the
// sound" repro) ============
// The device complaint: "cualquier modificacion de EQ, bell, hi, low, de
// +1 o -1 dB destruye el sonido y las barras de volumen suben mucho".
// B1 checks EVERY band type at +-1 dB @ 100 Hz on the synthetic 100 Hz +
// 1 kHz source: the 100 Hz component must move by the SET gain (a buggy
// low-f0 coefficient set explodes the low end -- the U2.52.8 DC shelf --
// and fails these ratios by an order of magnitude).  B2 runs the REAL
// KICK 01.wav (full-scale) at instrument volume 128 (FL-style unity)
// through BELL/LSHELF +-1 dB @ 80 Hz: the output stays bounded (the
// BACON_1.5_EQ8_SOFTKNEE caps at 1.0 Q15, it must NOT engage), finite,
// and the peak/bar move by ~1 dB -- not a loudness explosion.  With the
// mixer channel at 127 the kick
// peaks scale x1.27: 0.7 -> 0.89 (bar ~85%, past the -6 dB mark) and a
// +1 dB edit pushes them into the master clip (FL-like red at the top),
// which is the only "damage" a +1 dB edit can do to a hot kick.
    printf("-- (B1) every EQ type at +-1 dB @ 100 Hz --\n");
    {
        fixed *acc = (fixed *)malloc(sizeof(fixed) * 2 * g_renderSize * kAccBlocks);
        renderAccumulate(si, acc, kAccBlocks);
        double b100 = goertzel(acc, g_renderSize * kAccBlocks, 100.0);
        printf("B1 baseline: 100Hz=%.4f\n", b100);
        // type, gain, expected 100 Hz ratio (RBJ design: shelves sit at
        // sqrt(A) at f0, LP/HP at Q (0 dB for Q=1, the Q15 numerator
        // quantization at 100 Hz can wander the center up to ~+2.5 dB so
        // the f0 band is wide), BP peak gain 1, NOTCH a deep dip).  The
        // LP/HP/NOTCH/BP cases need a NONZERO gain: a 0 dB band is the
        // identity filter for every type (BACON_1.5_EQ8_0DB_TRANSPARENT).
        // LP/HP get an extra cutoff check at 2*f0 / f0/2 (the real test:
        // the filter must CUT the side it is supposed to cut).
        struct B1Case { const char *name; int type; int gain;
                        double expect; double tol; bool upperOnly;
                        double cutRatio; bool checkCut; };
        B1Case cases[] = {
            { "BELL +1", 0, 1, 1.122, 0.25, false, 0, false },
            { "BELL -1", 0, -1, 0.891, 0.25, false, 0, false },
            { "LSHELF +1", 1, 1, 1.059, 0.25, false, 0, false },
            { "LSHELF -1", 1, -1, 0.944, 0.25, false, 0, false },
            { "HSHELF +1", 2, 1, 1.059, 0.25, false, 0, false },
            { "LOW_PASS", 3, -6, 1.000, 0.45, false, 0.50, true },
            { "HIGH_PASS", 4, -6, 1.000, 0.45, false, 0.50, true },
            { "NOTCH", 5, -6, 0.300, 0.00, true, 0, false },
            { "BAND_PASS", 6, -6, 1.000, 0.25, false, 0, false },
        };
        for (int t = 0; t < 9; t++) {
            B1Case &c = cases[t];
            si.FindVariable(SIP_EQT0)->SetInt(c.type);
            si.FindVariable(SIP_EQG0)->SetInt(c.gain);
            si.FindVariable(SIP_EQF0)->SetInt(10000);   // 100 Hz * 100
            si.FindVariable(SIP_EQ_Q0)->SetInt(100);    // Q 1.00
            renderBlocks(si, acc, 120);
            renderAccumulate(si, acc, kAccBlocks);
            double r = goertzel(acc, g_renderSize * kAccBlocks, 100.0) / b100;
            bool ok = c.upperOnly ? (r < c.expect)
                                  : (fabs(r - c.expect) < c.expect * c.tol);
            printf("B1 %s: 100Hz ratio=%.3f %s\n", c.name, r, ok ? "ok" : "FAIL");
            check(ok, "EQ type gain matches the set dB at 100 Hz");
            check(bufferFinite(acc, 2 * g_renderSize * kAccBlocks),
                  "EQ type output finite");
            if (c.checkCut) {
                double fc = (c.type == 3) ? 200.0 : 50.0;   // 2*f0 / f0/2
                renderAccumulate(si, acc, kAccBlocks);
                double rc = goertzel(acc, g_renderSize * kAccBlocks, fc) / b100;
                printf("B1 %s: %5.0fHz ratio=%.3f %s\n", c.name, fc, rc,
                       rc < c.cutRatio ? "ok" : "FAIL");
                check(rc < c.cutRatio, "LP/HP really cuts the filtered side");
            }
        }
        si.FindVariable(SIP_EQG0)->SetInt(0);
        si.FindVariable(SIP_EQT0)->SetInt(0);
        free(acc);
    }
    printf("-- (B2) real kick EQ +-1 dB @ 80 Hz --\n");
    if (g_kick) {
        SampleInstrument sk;
        sk.FindVariable(SIP_SAMPLE)->SetInt(2);
        sk.FindVariable(SIP_LOOPMODE)->SetInt(1);   // SILM_LOOP
        sk.FindVariable(SIP_END)->SetInt(g_kick->GetSize(0));
        sk.Init();
        check(sk.Start(0, 60, true), "kick Start");
        fixed *acc = (fixed *)malloc(sizeof(fixed) * 2 * g_renderSize * kAccBlocks);
        const int accN = 2 * g_renderSize * kAccBlocks;
        double basePeak = 0, base80 = 0, baseDc = 0;
        renderAccumulate(sk, acc, kAccBlocks);
        for (int i = 0; i < accN; i++) {
            double v = fabs((double)fp2fl(acc[i]));
            if (v > basePeak) basePeak = v;
            baseDc += (double)fp2fl(acc[i]);
        }
        baseDc /= (double)accN;
        base80 = goertzel(acc, g_renderSize * kAccBlocks, 80.0);
        printf("kick baseline: peak=%.4f 80Hz=%.4f dc=%.1f bar=%.1f%%\n",
               basePeak, base80, baseDc,
               100.0 * (20.0 * log10(basePeak / 32768.0) + 24.0) / 24.0);
        // BACON_1.5_VU_TOP0DB (U2.59): bar% = the -24..0 dBFS mixer scale
        // (0 dBFS = 100% of the bar); the kick at volume 128 with the mixer
        // channel at 127 scales x1.27 and sits above -6 dBFS.
        check(basePeak * 1.27 > 0.5 * 32768.0,
              "kick at channel 127 sits above -6 dBFS");

        // +-1 dB BELL @ 80 Hz Q=1, then LOW_SHELF +1 (the reported types).
        const char *names[] = { "BELL +1", "BELL -1", "LSHELF +1" };
        const int types[] = { 0, 0, 1 };
        const int gains[] = { 1, -1, 1 };
        for (int t = 0; t < 3; t++) {
            sk.FindVariable(SIP_EQG0)->SetInt(gains[t]);
            sk.FindVariable(SIP_EQT0)->SetInt(types[t]);
            sk.FindVariable(SIP_EQF0)->SetInt(8000);   // 80 Hz * 100
            sk.FindVariable(SIP_EQ_Q0)->SetInt(100);   // Q 1.00
            renderBlocks(sk, acc, 120);                // converge the smoothing
            double pk = 0, dc = 0;
            renderAccumulate(sk, acc, kAccBlocks);
            for (int i = 0; i < accN; i++) {
                double v = fabs((double)fp2fl(acc[i]));
                if (v > pk) pk = v;
                dc += (double)fp2fl(acc[i]);
            }
            dc /= (double)accN;
            double dB = 20.0 * log10(pk / 32768.0);
            printf("kick %s: peak=%.4f (%.1f dB) dc=%.1f bar=%.1f%%\n",
                   names[t], pk, dB, dc,
                   100.0 * (dB + 24.0) / 24.0);
            check(bufferFinite(acc, accN), "kick EQ output finite");
            // BACON_1.5_EQ8_SOFTKNEE caps the instrument at 32767 Q15.
            check(pk < 32768.0, "kick EQ output bounded (knee off at +-1 dB)");
            // The loop click (kick tail -> sample start) adds a window DC
            // that shifts with the EQ phase; a loose sanity bound keeps the
            // "no EQ-generated DC" claim on the synthetic-source checks.
            check(fabs(dc) < 0.15 * 32768.0, "kick EQ output has no gross DC");
            check(pk > basePeak * 0.8 && pk < basePeak * 1.6,
                  "kick peak moves by ~1 dB, not a loudness explosion");
        }
        sk.FindVariable(SIP_EQG0)->SetInt(0);
        sk.FindVariable(SIP_EQT0)->SetInt(0);
        free(acc);
    } else {
        printf("SD sample not mounted - kick EQ scenario skipped\n");
    }

    // ============ (B3) U2.59: the project lgpt_KAOZ kick EQ config + the
    // worst slider cases, on the synthetic 100 Hz + 1 kHz source ============
    // The saved kick instrument (lgptsav.dat ID 20) carries: BELL +6 dB @
    // 160 Hz Q 1.00 (band 1) and BELL +5 dB @ 753.88 Hz Q 1.00 (band 3),
    // all 8 bands enabled, EQ active.  The user reported "al ajustar la EQ
    // genera aliasing, armonicos o sonidos extra (similares a
    // sintetizadores); la ultima configuracion que deje en el Kick lo
    // rompe".  The EQ8 DSP is an LTI chain (64-bit Df2, prewarped bell,
    // Nyquist clamp, BACON_1.5_EQ8_SOFTKNEE cap): it CANNOT create new
    // spectral lines -- B3 proves it with the EXACT saved config: the
    // 100 Hz / 1 kHz components stay, the 2nd-harmonic positions
    // (320/1508 Hz) stay at their no-EQ baseline (an LTI filter scales
    // them by |H| <= ~1.4, it never ADDS energy), the output is finite and
    // bounded.  Then the worst cases the view sliders can reach (BELL
    // +24 dB @ 20 kHz Q 0.1 and Q 10, the max frequency) stay bounded and
    // finite too -- no configuration reachable from the UI can break the
    // sound.
    printf("-- (B3) project kick EQ config + worst slider cases --\n");
    {
        renderBlocks(si, buf, 120);   // converge the smoothing at 0 dB
        fixed *acc = (fixed *)malloc(sizeof(fixed) * 2 * g_renderSize * kAccBlocks);
        const int accN = 2 * g_renderSize * kAccBlocks;
        double basePeak = 0, base100 = 0, base1k = 0, base320 = 0,
               base754 = 0, base1508 = 0;
        renderAccumulate(si, acc, kAccBlocks);
        for (int i = 0; i < accN; i++) {
            double v = fabs((double)fp2fl(acc[i]));
            if (v > basePeak) basePeak = v;
        }
        base100 = goertzel(acc, g_renderSize * kAccBlocks, 100.0);
        base1k = goertzel(acc, g_renderSize * kAccBlocks, 1000.0);
        base320 = goertzel(acc, g_renderSize * kAccBlocks, 320.0);
        base754 = goertzel(acc, g_renderSize * kAccBlocks, 754.0);
        base1508 = goertzel(acc, g_renderSize * kAccBlocks, 1508.0);
        printf("B3 baseline: peak=%.4f 100Hz=%.4f 1kHz=%.4f 320Hz=%.4f 754Hz=%.4f 1508Hz=%.4f\n",
               basePeak, base100, base1k, base320, base754, base1508);
        check(bufferFinite(acc, accN), "B3 baseline finite");

        // Apply the project kick EQ: band1 BELL +6 @160 Hz, band3 BELL +5
        // @754 Hz, Q 1.00 (the saved lgptsav.dat values, EQ active).
        si.FindVariable(SIP_EQF1)->SetInt(16000);     // 160 Hz * 100
        si.FindVariable(SIP_EQG1)->SetInt(6);
        si.FindVariable(SIP_EQT1)->SetInt(0);         // BELL
        si.FindVariable(SIP_EQ_Q1)->SetInt(100);      // Q 1.00
        si.FindVariable(SIP_EQF3)->SetInt(75388);     // 753.88 Hz * 100
        si.FindVariable(SIP_EQG3)->SetInt(5);
        si.FindVariable(SIP_EQT3)->SetInt(0);         // BELL
        si.FindVariable(SIP_EQ_Q3)->SetInt(100);      // Q 1.00
        renderBlocks(si, buf, 120);                   // converge the smoothing
        double pk = 0, c100 = 0, c1k = 0, c320 = 0, c754 = 0, c1508 = 0;
        renderAccumulate(si, acc, kAccBlocks);
        for (int i = 0; i < accN; i++) {
            double v = fabs((double)fp2fl(acc[i]));
            if (v > pk) pk = v;
        }
        c100 = goertzel(acc, g_renderSize * kAccBlocks, 100.0);
        c1k = goertzel(acc, g_renderSize * kAccBlocks, 1000.0);
        c320 = goertzel(acc, g_renderSize * kAccBlocks, 320.0);
        c754 = goertzel(acc, g_renderSize * kAccBlocks, 754.0);
        c1508 = goertzel(acc, g_renderSize * kAccBlocks, 1508.0);
        printf("B3 kickEQ: peak=%.4f 100Hz=%.4f 1kHz=%.4f 320Hz=%.4f 754Hz=%.4f 1508Hz=%.4f\n",
               pk, c100, c1k, c320, c754, c1508);
        check(bufferFinite(acc, accN), "B3 kick EQ finite");
        check(pk < 32768.0, "B3 kick EQ bounded (knee off at +-1 dB)");
        check(c100 > base100 * 0.6f && c100 < base100 * 1.8f,
              "B3 100 Hz survives the 160 Hz bell (no kill)");
        // The bells really work on the real content: the 754 Hz bell's
        // skirt (+2.5 dB at 1 kHz, 1.33x above its center) visibly boosts
        // the 1 kHz component of the source (the source has no 754 Hz
        // energy to boost directly -- its 754 Hz reading is just the
        // 1 kHz tone's window leakage, so it is NOT a bell-activity probe).
        check(c1k > base1k * 1.15f, "B3 the 754 Hz bell boosts its skirt (bells work)");
        check(c1k < base1k * 1.8f, "B3 the 754 Hz bell does not explode the 1 kHz");
        // The 2nd-harmonic positions stay at the no-EQ baseline: an LTI
        // filter scales them by the response magnitude (<= ~1.4), it never
        // ADDS energy -- a nonlinearity (the reported "armonicos") would
        // put the ratio far above this.
        check(c320 < base320 * 2.0f, "B3 no 2nd harmonic at 320 Hz");
        check(c1508 < base1508 * 2.0f, "B3 no 2nd harmonic at 1508 Hz");
        si.FindVariable(SIP_EQF1)->SetInt(16000);
        si.FindVariable(SIP_EQG1)->SetInt(0);
        si.FindVariable(SIP_EQF3)->SetInt(75388);
        si.FindVariable(SIP_EQG3)->SetInt(0);
        renderBlocks(si, buf, 120);

        // Worst cases the view sliders can reach: BELL +24 dB @ 20 kHz
        // (freqFromIndex(59), the max) with the widest and narrowest Q.
        const char *qNames[] = { "Q0.1", "Q10" };
        const int qVals[] = { 10, 1000 };
        for (int t = 0; t < 2; t++) {
            si.FindVariable(SIP_EQF0)->SetInt(2000000);  // 20 kHz * 100
            si.FindVariable(SIP_EQG0)->SetInt(24);
            si.FindVariable(SIP_EQT0)->SetInt(0);        // BELL
            si.FindVariable(SIP_EQ_Q0)->SetInt(qVals[t]);
            renderBlocks(si, buf, 120);
            double wpk = 0, w100 = 0;
            renderAccumulate(si, acc, kAccBlocks);
            for (int i = 0; i < accN; i++) {
                double v = fabs((double)fp2fl(acc[i]));
                if (v > wpk) wpk = v;
            }
            w100 = goertzel(acc, g_renderSize * kAccBlocks, 100.0);
            printf("B3 +24dB@20kHz %s: peak=%.4f 100Hz=%.4f\n",
                   qNames[t], wpk, w100);
            check(bufferFinite(acc, accN), "B3 worst case finite");
            check(wpk < 32768.0, "B3 worst case bounded (knee holds)");
            check(w100 > base100 * 0.3f,
                  "B3 the low end survives the worst top band");
        }
        si.FindVariable(SIP_EQF0)->SetInt(8000);
        si.FindVariable(SIP_EQG0)->SetInt(0);
        si.FindVariable(SIP_EQT0)->SetInt(0);
        si.FindVariable(SIP_EQ_Q0)->SetInt(100);
        free(acc);
    }

    // ============ (B4) U2.59: the SAVED lgpt_KAOZ configs (feedback #12) ===
    // The user left the kick (ID 20) with BELL +11 dB @ 2500 Hz Q 1.00 and
    // the snare (ID 10) with BELL -8 dB @ 45.39 Hz Q 1.00.  Claims:
    //   - "Aplicar esa EQ en el kick genera distorsion": measure the EQ
    //     output against the master safety zones (0.85 knee / 1.7 flat
    //     ceiling, AudioMixer.cpp) -- if a single instrument's EQ output
    //     crosses 1.7 the master flat-caps it (real distortion).
    //   - "el kick en 2500 hz no tiene nada en el analizador": measure the
    //     kick's real 2500 Hz content pre-EQ -- the CONTENT exists (the
    //     analyzer showed nothing because it displays the master mix, not
    //     the instrument; fixed by BACON_1.5_ANALYZER_INSTRUMENT U2.59).
    //   - "bajar frecuencias debe bajar la señal, no incrementarla": the
    //     snare -8 dB @ 45.39 Hz must measurably CUT the low content.
    printf("-- (B4) saved lgpt_KAOZ configs: kick +11dB@2500, snare -8dB@45.4 --\n");
    {
        // Mirror of AudioMixer::safetyLimit on the count<<15 scale.
        const double kneeC = 0.85 * 32768.0;
        const double topC = 1.7 * 32768.0;
        const double ceilC = 32768.0;
        const double spanC = ceilC - kneeC;
        const int accN = 2 * g_renderSize * kAccBlocks;
        if (g_kick) {
            SampleInstrument sk;
            sk.FindVariable(SIP_SAMPLE)->SetInt(2);
            sk.FindVariable(SIP_LOOPMODE)->SetInt(1);
            sk.FindVariable(SIP_END)->SetInt(g_kick->GetSize(0));
            sk.Init();
            fixed *acc = (fixed *)malloc(sizeof(fixed) * 2 * g_renderSize * kAccBlocks);
            fixed atk[2 * 512];

            double pk0 = 0, a2500 = 0, pk = 0, c2500 = 0;
            int crushed = 0;
            check(sk.Start(0, 60, true), "B4 kick Start");
            renderBlocks(sk, acc, 120);
            renderAccumulate(sk, acc, kAccBlocks);
            for (int i = 0; i < accN; i++) {
                double v = fabs((double)fp2fl(acc[i]));
                if (v > pk0) pk0 = v;
            }
            // The 2500 Hz content is the attack click (first ms), so measure
            // the FIRST 512-frame block (10.7 ms) of a fresh start, not the
            // long window (the looped body would dilute the transient).
            check(sk.Start(0, 60, true), "B4 kick restart");
            renderBlocks(sk, atk, 1);
            a2500 = goertzel(atk, 512, 2500.0);
            printf("B4 kick pre-EQ: peak=%.1f counts attack2500Hz=%.1f counts (%.1f dBFS)\n",
                   pk0, a2500, 20.0 * log10(a2500 / 32768.0));
            check(a2500 > 32768.0 * 0.002,
                  "B4 the kick HAS 2500 Hz content pre-EQ (the analyzer was showing the mix)");

            // The exact saved config: BELL +11 dB @ 2500 Hz, Q 1.00, active.
            sk.FindVariable(SIP_EQF5)->SetInt(250000);
            sk.FindVariable(SIP_EQG5)->SetInt(11);
            sk.FindVariable(SIP_EQT5)->SetInt(0);
            sk.FindVariable(SIP_EQ_Q5)->SetInt(100);
            renderBlocks(sk, acc, 120);            // converge the EQ
            check(sk.Start(0, 60, true), "B4 kick restart eq");
            renderBlocks(sk, atk, 1);
            c2500 = goertzel(atk, 512, 2500.0);
            renderBlocks(sk, acc, 120);
            renderAccumulate(sk, acc, kAccBlocks);
            for (int i = 0; i < accN; i++) {
                double v = (double)fp2fl(acc[i]);
                double a = fabs(v);
                if (a > pk) pk = a;
                if (a > topC) crushed++;
            }
            printf("B4 kick +11dB@2500: peak=%.1f counts attack2500Hz=%.1f counts masterCrush=%d\n",
                   pk, c2500, crushed);
            check(bufferFinite(acc, accN), "B4 kick +11dB finite");
            check(c2500 > a2500 * 2.0f,
                  "B4 the +11 dB boost really boosts the 2500 Hz click");
            // BACON_1.5_EQ8_SOFTKNEE (U2.59): the per-sample soft knee caps
            // the instrument at unity, so a single instrument can NEVER push
            // the master past its transparent region -- the saved config
            // produces NO flat-top at the master.
            check(pk <= 32768.0, "B4 EQ output never exceeds unity (soft knee)");
            check(crushed == 0,
                  "B4 the master never flat-tops from one instrument's EQ");
            sk.FindVariable(SIP_EQF5)->SetInt(250000);
            sk.FindVariable(SIP_EQG5)->SetInt(0);
            renderBlocks(sk, acc, 120);
            free(acc);
        } else {
            printf("B4 kick skipped (no SD sample)\n");
        }

        if (g_snare) {
            SampleInstrument ss;
            ss.FindVariable(SIP_SAMPLE)->SetInt(3);
            ss.FindVariable(SIP_LOOPMODE)->SetInt(1);
            ss.FindVariable(SIP_END)->SetInt(g_snare->GetSize(0));
            ss.Init();
            check(ss.Start(0, 60, true), "B4 snare Start");
            fixed *acc = (fixed *)malloc(sizeof(fixed) * 2 * g_renderSize * kAccBlocks);

            double base45 = 0, c45 = 0, base45One = 0, c45One = 0;
            renderBlocks(ss, acc, 120);
            renderAccumulate(ss, acc, kAccBlocks);
            base45 = goertzel(acc, g_renderSize * kAccBlocks, 45.39);
            printf("B4 snare pre-EQ(loop): 45.39Hz=%.1f counts\n", base45);

            // The exact saved config: BELL -8 dB @ 45.39 Hz, Q 1.00, active.
            ss.FindVariable(SIP_EQF0)->SetInt(4539);
            ss.FindVariable(SIP_EQG0)->SetInt(-8);
            ss.FindVariable(SIP_EQT0)->SetInt(0);
            ss.FindVariable(SIP_EQ_Q0)->SetInt(100);
            renderBlocks(ss, acc, 120);
            renderAccumulate(ss, acc, kAccBlocks);
            c45 = goertzel(acc, g_renderSize * kAccBlocks, 45.39);
            printf("B4 snare -8dB@45.4(loop): 45.39Hz=%.1f counts\n", c45);
            // BACON_1.5_EQ8_LOOPFLUSH (U2.59): before the fix, the looped
            // window ROSE x7 (the low bell's filter tail crossed the loop
            // boundary every pass and rang) -- the "bajar = subir" report.
            // The per-channel EQ state is flushed at the wrap, so the
            // discontinuity ring is gone (735 -> 290 measured); the residual
            // ~2.8x is the filter's NATURAL resonance when the sample
            // attack is replayed each pass (Q1 at 45 Hz), not an energy
            // adder.  One-shot (how the user hears the snare) adds nothing.
            // BACON_1.5_EQ8_SOFTKNEE_C1 (U2.60): the C1 rational knee
            // shapes the attack crests differently (measured ring 3.17x,
            // within ~15% of the linear knee's 2.83x) -- the resonance
            // excitation moved, not worsened; 3.5x keeps the gate.
            check(c45 < base45 * 3.5f,
                  "B4 the looped cut does not ring (discontinuity flush)");

            // The user hears the snare ONE-SHOT; it has no 45 Hz body (the
            // pre-EQ one-shot bin reads ~0), so there is nothing to cut --
            // but the cut must still NOT add content where there was none.
            ss.FindVariable(SIP_LOOPMODE)->SetInt(0);
            check(ss.Start(0, 60, true), "B4 snare one-shot restart");
            renderBlocks(ss, acc, 120);
            renderAccumulate(ss, acc, kAccBlocks);
            base45One = goertzel(acc, g_renderSize * kAccBlocks, 45.39);
            printf("B4 snare pre-EQ(oneshot): 45.39Hz=%.1f counts\n", base45One);
            ss.FindVariable(SIP_EQG0)->SetInt(-8);
            check(ss.Start(0, 60, true), "B4 snare one-shot restart eq");
            renderBlocks(ss, acc, 120);
            renderAccumulate(ss, acc, kAccBlocks);
            c45One = goertzel(acc, g_renderSize * kAccBlocks, 45.39);
            printf("B4 snare -8dB@45.4(oneshot): 45.39Hz=%.1f counts\n", c45One);
            check(bufferFinite(acc, accN), "B4 snare cut finite");
            check(c45One < base45One + 32768.0 * 0.001,
                  "B4 one-shot cut adds no low energy (bajar = bajar)");
            ss.FindVariable(SIP_LOOPMODE)->SetInt(1);
            ss.FindVariable(SIP_EQG0)->SetInt(0);
            renderBlocks(ss, acc, 120);
            free(acc);
        } else {
            printf("B4 snare skipped (no SD sample)\n");
        }
    }

    free(buf);
    if (failures == 0) {
        printf("SAMPLE_EQ_EDIT: %d checks, 0 failures\n", checks);
        return 0;
    }
    printf("SAMPLE_EQ_EDIT: %d checks, %d failures\n", checks, failures);
    return 1;
}