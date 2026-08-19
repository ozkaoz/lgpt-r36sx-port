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

class TestPool : public SamplePool {
  public:
    TestPool() {
        count_ = 1;
        names_[0] = (char *)malloc(2);
        names_[0][0] = 'T';
        names_[0][1] = 0;
        wav_[0] = new TestSource();
        // Feedback (A): the reference sample on the SD, when reachable.
        g_hihat = TestSourceWav::TryLoad(
            "/mnt/g/lgpt/samples/Drum Kit LGPT/HI HAT 01.wav");
        if (g_hihat) {
            names_[1] = (char *)malloc(2);
            names_[1][0] = 'H';
            names_[1][1] = 0;
            wav_[1] = g_hihat;
            count_ = 2;
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
// unity while the 64-bit Df2 stays clean; BACON_1.5_EQ8_BLOCKLIMIT caps the
// per-block output at 65535 = just under 2.0, the int32 pipeline limit).
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

// Render enough buffers for the EQ coefficient smoothing to converge.
static void renderBlocks(SampleInstrument &si, fixed *buf, int blocks) {
    for (int b = 0; b < blocks; b++) {
        memset(buf, 0, sizeof(fixed) * 2 * g_renderSize);
        si.Render(0, buf, g_renderSize, false);
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
    check(a100 > base100 * 3.4, "100 Hz REALLY boosted ~12 dB (bell center)");
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
    check(b100 > base100 * 3.0, "100 Hz band still boosted (independent bands)");
    check(b1k > base1k * 1.6, "1 kHz boosted ~6 dB on top of 100 Hz");

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
    // same fixed-point chain, both at instrument volume 100.  Both buffers
    // are int16<<15 (master scale), so fp2fl() returns the DAC int16 count
    // (1.0 == 32768): the compare below is the real loudness at the output.
    // Before BACON_1.5_VOL_SYNTHS_FIX the synth rendered Q15 and its fp2fl
    // peak read ~1.1 while the sample read 11122 counts -- the check caught
    // the ~90 dB gap.  BACON_1.5_VOL_SYNTHS_PAD (U2.52.9, feedback #6) then
    // padded the master-scale write by -20 dB (x0.1): a full-scale saw at
    // volume 100 measured peak 1.0 / rms 0.452 vs the hat's 0.339 / 0.051
    // (~19 dB RMS louder on the device), so the pad brings the synth to the
    // kit level (measured here: peak 3276 counts = 0.1, rms 1482 = 0.0452).
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
        // Padded range in int16 counts (fp2fl of the master-scale buffer =
        // the DAC count; 1.0 == 32768): the pad x0.1 maps the full-scale saw
        // to a peak of 3276.8 counts (0.1) -- a regression to the un-padded
        // 32768, or a double pad at 328, both fail.  Bounds: [0.05, 0.5] of
        // full scale = [1638.4, 16384].
        check(sPk > 1638.4 && sPk < 16384.0, "synth sustained peak in padded range");

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
            printf("hihat vol100: peak=%.3f rms=%.3f\n", hPk, hRms);
            check(hPk > 0.0, "hi-hat renders audio");
            // Within ~[-12, +3.5] dB of the reference peak and ~[-6, +9.5]
            // dB of its rms (the expected 0.113/0.045 sit at -9.5 / -1 dB
            // from the hat; the un-padded 1.0/0.452 fail the upper band).
            check(sPk > hPk * 0.25 && sPk < hPk * 1.5,
                  "synth peak near the reference sample peak");
            check(sRms > hRms * 0.5 && sRms < hRms * 3.0,
                  "synth rms near the reference sample rms");
        } else {
            printf("SD sample not mounted - hi-hat compare skipped\n");
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