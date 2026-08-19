// MASTER_SAFETY (U2.56, feedback #9): host test of the REAL AudioMixer
// master path (sum -> volume/damp -> pre-clip meter -> clip loop) with the
// speaker-protection limiter.
//
// Motivation: with the FL-style unity levels (U2.55) the default mix sums
// past 1.0 (instrument vol 128 = unity, mixer channel 127 = +2 dB, 5
// tracks), and the master's DEFAULT path was a pure hard clip at +/-1.0:
// the output became a flat-topped square wave -- the "broken / extremely
// saturated" report.  This test drives the real master (real AudioMixer.cpp,
// NOT the stub the mixer_vu_chain runner uses) and asserts the safety
// properties:
//   1. the output NEVER exceeds +/-1.0 (32767 counts) whatever the sum
//      (DAC and console speaker are safe);
//   2. a moderately hot sum (a couple of tracks at 1.3x) passes through
//      the soft knee with ZERO flat-topped samples (no square waves);
//   3. an extreme sum (6x) still reads <= 1.0 and finite (no int overflow
//      garbage);
//   4. a quiet mix (0.5) is transparent (peak unchanged, no clip);
//   5. the pre-clip meter still reads the TRUE level (can exceed 1.0), so
//      the mixer bars honestly show the red before the limiter catches it;
//   6. a REAL BassSynth at volume 100 through a channel at 127 keeps the
//      output <= 1.0 and finite (the exact case the user tested).
//
// Links the real AudioMixer.cpp with the same stub pattern as
// mixer_vu_chain_host_test (SyncMaster/MixerService/FxEngine/Audio/System
// stubbed; PlayerChannel + BassSynth + InstrumentEq + FilterV2 +
// TablePlayback real).
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
#include "Services/Audio/AudioModule.h"
#include "Services/Audio/AudioMixer.h"
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

static unsigned long g_clock = 0;

class StubSystem : public System {
  public:
    static void Init() {
        if (T_Factory<System>::GetInstance() == 0) {
            T_Factory<System>::Install(new StubSystem());
        }
    }
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

// SyncMaster stub: TableSlice is the only method the chain reads.
SyncMaster::SyncMaster() : tempo_(120), currentSlice_(0), tableRatio_(1),
    beatCount_(0), playSampleCount_(0), tickSampleCount_(0) {}
float SyncMaster::GetPlaySampleCount() { return 0; }
bool SyncMaster::TableSlice() { return false; }

// AudioOut stub: symbols needed by T_SimpleList<AudioOut> / the vtable.
// AudioOut has no Render override (it uses AudioMixer::Render directly).
AudioOut::AudioOut() : AudioMixer("AudioOut"), sampleOffset_(0) {}
AudioOut::~AudioOut() {}

// WavFileWriter stub: AudioMixer.cpp holds a pointer and calls
// Close/AddBuffer only when EnableRendering is set (never in this test).
WavFileWriter::WavFileWriter(const char *path) : file_(0) {}
WavFileWriter::~WavFileWriter() {}
void WavFileWriter::AddBuffer(fixed *, int) {}
void WavFileWriter::Close() {}

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

// Deterministic audio source for the master path: an N-sample stereo sine
// at a fixed amplitude, in the REAL master scale (counts<<15: a track at
// "A full scale" writes A * i2fp(32767), so fp2fl(master) reads DAC counts
// 0..32767 and the hard clip caps at i2fp(32767)).
class SineModule : public AudioModule {
  public:
    SineModule(float amp, float freq, int rate)
        : amp_(amp), freq_(freq), rate_(rate), ph_(0.0f) {}
    virtual bool Render(fixed *buffer, int samplecount) {
        fixed full = i2fp(32767);
        for (int i = 0; i < samplecount * 2; i++) {
            ph_ += freq_ / (float)rate_;
            if (ph_ >= 1.0f) ph_ -= 1.0f;
            float s = amp_ * sinf(2.0f * 3.14159265f * ph_);
            buffer[i] = (fixed)((float)full * s);
        }
        return true;
    }
  private:
    float amp_, freq_, ph_;
    int rate_;
};

// Render the master and report/measure the output.  The output buffer is in
// counts<<15: fp2fl gives DAC counts (32767 = full scale = 0 dBFS), so the
// linear 0..1 level is fp2fl()/32767 and a flat-topped sample is one AT the
// hard-clip cap (within 1 count of i2fp(32767)).
struct MasterMeasure {
    float outPeak;      // max |out| / i2fp(32767), 0..1 linear
    int flatTop;        // samples within 1 count of the +/-32767 cap
    float prePeak;      // master pre-clip meter (true sum), 0..1 linear
    bool finite;
};

static MasterMeasure renderMaster(AudioMixer &master, int frames) {
    static fixed buf[1024 * 2];
    master.ResetPeak();
    MasterMeasure m;
    m.outPeak = 0.0f;
    m.flatTop = 0;
    m.finite = true;
    bool got = master.Render(buf, frames);
    if (!got) {
        m.finite = false;
        return m;
    }
    fixed cap = i2fp(32767);
    for (int i = 0; i < frames * 2; i++) {
        if (!(buf[i] >= -2147483647 && buf[i] <= 2147483647)) {
            m.finite = false;
            continue;
        }
        float v = (float)((double)buf[i] / (double)cap);
        if (v < 0.0f) v = -v;
        if (v > m.outPeak) m.outPeak = v;
        if (buf[i] >= cap - 1 || buf[i] <= -cap + 1) m.flatTop++;
    }
    m.prePeak = master.GetPeakValueL();
    float pR = master.GetPeakValueR();
    if (pR > m.prePeak) m.prePeak = pR;
    return m;
}

int main(int argc, char **argv) {
    StubSystem::Init();
    int rate = 48000;

    printf("-- (1) moderate hot mix: two tracks at 1.3x sum --\n");
    {
        AudioMixer master("master");
        SineModule a(0.65f, 100, rate);
        SineModule b(0.65f, 150, rate);   // sum ~1.24 peak
        master.Insert(&a);
        master.Insert(&b);
        MasterMeasure m = renderMaster(master, 512);
        printf("   outPeak=%.4f flatTop=%d prePeak=%.4f finite=%d\n",
               m.outPeak, m.flatTop, m.prePeak, m.finite);
        check(m.finite, "moderate mix output finite");
        check(m.outPeak <= 1.0005f, "moderate mix output never exceeds 1.0");
        check(m.flatTop == 0,
              "moderate mix: soft knee, ZERO flat-topped samples (no square wave)");
    }

    printf("-- (1b) meter honesty: single track at 1.3x, pre-clip red --\n");
    {
        AudioMixer master("master");
        SineModule s(1.3f, 120, rate);
        master.Insert(&s);
        MasterMeasure m = renderMaster(master, 512);
        printf("   outPeak=%.4f flatTop=%d prePeak=%.4f finite=%d\n",
               m.outPeak, m.flatTop, m.prePeak, m.finite);
        check(m.finite, "1.3 track output finite");
        check(m.outPeak <= 1.0005f, "1.3 track output never exceeds 1.0");
        check(m.prePeak > 1.0f,
              "pre-clip meter honestly shows the hot sum (red above 1.0)");
        check(m.prePeak > 1.2f && m.prePeak < 1.4f,
              "pre-clip meter reads the true ~1.3 level");
    }

    printf("-- (2) default 5-track mix: kick + 4 synths (sum ~3.8x) --\n");
    {
        AudioMixer master("master");
        SineModule k(0.77f, 60, rate);    // kick at vol 100, chan 127
        SineModule s1(1.00f, 120, rate);  // synth at vol 100, chan 127
        SineModule s2(1.00f, 240, rate);
        SineModule s3(0.50f, 360, rate);
        SineModule s4(0.50f, 480, rate);
        master.Insert(&k); master.Insert(&s1); master.Insert(&s2);
        master.Insert(&s3); master.Insert(&s4);
        MasterMeasure m = renderMaster(master, 512);
        printf("   outPeak=%.4f flatTop=%d prePeak=%.4f finite=%d\n",
               m.outPeak, m.flatTop, m.prePeak, m.finite);
        check(m.finite, "default mix output finite");
        check(m.outPeak <= 1.0005f,
              "default mix NEVER exceeds 1.0 (speaker/DAC safe)");
        printf("   note: prePeak=%.3f is the meter after its peak-hold decay\n",
               m.prePeak);
    }

    printf("-- (3) extreme overload: five tracks at 1.0 sum (5x) --\n");
    {
        AudioMixer master("master");
        SineModule s1(1.0f, 200, rate);
        SineModule s2(1.0f, 220, rate);
        SineModule s3(1.0f, 240, rate);
        SineModule s4(1.0f, 260, rate);
        SineModule s5(1.0f, 280, rate);
        master.Insert(&s1); master.Insert(&s2); master.Insert(&s3);
        master.Insert(&s4); master.Insert(&s5);
        MasterMeasure m = renderMaster(master, 512);
        printf("   outPeak=%.4f flatTop=%d prePeak=%.4f finite=%d\n",
               m.outPeak, m.flatTop, m.prePeak, m.finite);
        check(m.finite, "extreme overload output finite (no int overflow)");
        check(m.outPeak <= 1.0005f, "extreme overload NEVER exceeds 1.0");
    }

    printf("-- (3b) pre-clip sum probe (clipBypass): the TRUE mix level --\n");
    {
        AudioMixer master("master");
        SineModule a(0.65f, 100, rate);
        SineModule b(0.65f, 150, rate);
        master.Insert(&a);
        master.Insert(&b);
        master.SetClipBypass(true);   // raw sum out (never wraps: int32 clamp)
        MasterMeasure m = renderMaster(master, 512);
        printf("   outPeak=%.4f flatTop=%d prePeak=%.4f finite=%d\n",
               m.outPeak, m.flatTop, m.prePeak, m.finite);
        check(m.finite, "pre-clip probe output finite");
        check(m.outPeak > 1.20f && m.outPeak < 1.30f,
              "pre-clip probe: the sum really is ~1.3 (two 0.65 tracks)");
    }

    printf("-- (4) quiet mix is transparent --\n");
    {
        AudioMixer master("master");
        SineModule s(0.5f, 100, rate);
        master.Insert(&s);
        MasterMeasure m = renderMaster(master, 512);
        printf("   outPeak=%.4f flatTop=%d prePeak=%.4f finite=%d\n",
               m.outPeak, m.flatTop, m.prePeak, m.finite);
        check(m.finite, "quiet mix output finite");
        check(m.outPeak > 0.49f && m.outPeak < 0.51f,
              "quiet mix passes through transparently (no gain change)");
        check(m.flatTop == 0, "quiet mix has no clipping");
    }

    printf("-- (5) user softclip mode still caps at 1.0 --\n");
    {
        AudioMixer master("master");
        SineModule s1(1.0f, 200, rate);
        SineModule s2(1.0f, 210, rate);
        SineModule s3(1.0f, 220, rate);   // sum ~3.0
        master.Insert(&s1); master.Insert(&s2); master.Insert(&s3);
        master.SetSoftclip(1, 0);   // mode 0 (alpha 1.45, -1.5 dB)
        MasterMeasure m = renderMaster(master, 512);
        printf("   outPeak=%.4f flatTop=%d prePeak=%.4f finite=%d\n",
               m.outPeak, m.flatTop, m.prePeak, m.finite);
        check(m.finite, "softclip mode output finite");
        check(m.outPeak <= 1.0005f, "softclip mode NEVER exceeds 1.0");
    }

    printf("-- (6) real BassSynth vol 100 through channel 127 on the master --\n");
    {
        Audio::Install(new StubAudio(48000));
        AudioMixer master("master");
        BassSynth synth;
        check(synth.Init(), "synth Init");
        synth.FindVariable(SBP_ATTACK)->SetInt(0);
        synth.FindVariable(SBP_DECAY)->SetInt(0);
        check(synth.Start(0, 60, true), "synth Start ch0");
        PlayerChannel pc(0);
        pc.StartInstrument(&synth, 60, true);
        pc.SetVolume(127);
        master.Insert(&pc);
        static fixed buf[1024 * 2];
        bool got = master.Render(buf, 1024);
        check(got, "real synth rendered through the master");
        check(bufferFinite(buf, 1024 * 2), "real synth master output finite");
        fixed cap = i2fp(32767);
        float peak = 0.0f;
        int flat = 0;
        for (int i = 0; i < 1024 * 2; i++) {
            float v = (float)((double)buf[i] / (double)cap);
            if (v < 0.0f) v = -v;
            if (v > peak) peak = v;
            if (buf[i] >= cap - 1 || buf[i] <= -cap + 1) flat++;
        }
        printf("   outPeak=%.4f flatTop=%d prePeak=%.4f\n",
               peak, flat, master.GetPeakValueL());
        check(peak <= 1.0005f,
              "synth at vol 100 / channel 127 NEVER exceeds 1.0");
        check(flat == 0,
              "synth at vol 100 / channel 127 has no square-wave flat top");
    }

    printf("MASTER_SAFETY: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}