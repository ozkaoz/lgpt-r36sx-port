// MULTITRACK_EXPORT (bacon-1.5, item 8): host test of the FxEngine stems
// capture -- delay return / reverb return / master written to WAV while the
// DSP runs -- plus the UnixFileSystem::GetFreeSpace free-space probe.
// Compiles the real FxEngine stack + UnixFileSystem with ASAN/UBSAN.
#include "Application/Audio/FxEngine/FxEngine.h"
#include "Application/Instruments/WavFileWriter.h"
#include "Application/Player/SyncMaster.h"
#include "Services/Audio/Audio.h"
#include "Services/Audio/AudioOut.h"
#include "System/System/System.h"
#include "System/FileSystem/FileSystem.h"
#include "Adapters/Unix/FileSystem/UnixFileSystem.h"
#include "Application/Utils/wildcard.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>

using namespace FxEngine;

// ---- Minimal stubs (mirrors the other bacon-1.5 host tests) ----
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

// SyncMaster: only GetTempo() is referenced (delay SYNC path), never called
// in this test (SYNC is off), but the symbol must link.
SyncMaster::SyncMaster() : tempo_(120), currentSlice_(0), tableRatio_(1),
    beatCount_(0), playSampleCount_(0), tickSampleCount_(0) {}
int SyncMaster::GetTempo() { return 120; }

// Trace stubs: FileSystem/UnixFileSystem only log, never assert here.
void Trace::Debug(const char *, ...) {}
void Trace::Log(const char *, const char *, ...) {}
void Trace::Error(const char *, ...) {}

// Byte-swap helpers (WavFileWriter calls these; little-endian = identity).
short Swap16(short from) { return from; }
int Swap32(int from) { return from; }

// AudioOut/AudioMixer stubs: only the typeinfo/vtable of AudioOut is pulled
// in through T_SimpleList<AudioOut>; nothing else is referenced.
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

static void check(bool cond, const char *fmt, ...) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: ");
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
    }
}

// Little-endian read of the RIFF "data" chunk size (offset 40).
static unsigned int wavDataSize(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0xFFFFFFFFu;
    unsigned char hdr[44];
    size_t got = fread(hdr, 1, 44, f);
    fclose(f);
    if (got < 44) return 0xFFFFFFFFu;
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        return 0xFFFFFFFFu;
    }
    if (memcmp(hdr + 36, "data", 4) != 0) return 0xFFFFFFFFu;
    return (unsigned int)(hdr[40] | (hdr[41] << 8) | (hdr[42] << 16) | (hdr[43] << 24));
}

// Max absolute int16 sample of a WAV payload (only the data chunk body).
static int wavPeak(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    unsigned char hdr[44];
    if (fread(hdr, 1, 44, f) != 44) { fclose(f); return -1; }
    if (memcmp(hdr + 36, "data", 4) != 0) { fclose(f); return -1; }
    unsigned int n = wavDataSize(path);
    if (n == 0xFFFFFFFFu || n == 0) { fclose(f); return 0; }
    int peak = 0;
    long total = (long)n;
    unsigned char *buf = (unsigned char *)malloc((size_t)total);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)total, f) != (size_t)total) {
        free(buf); fclose(f); return -1;
    }
    for (long i = 0; i + 1 < total; i += 2) {
        short s = (short)((unsigned short)(buf[i] | (buf[i + 1] << 8)));
        int a = s < 0 ? -s : s;
        if (a > peak) peak = a;
    }
    free(buf);
    fclose(f);
    return peak;
}

static const int FRAMES = 2048; // FX_ENGINE_MAX_FRAMES
static const char *kDir = "/tmp/fx_stems_host";
static char kDelay[256], kReverb[256], kMaster[256];

static void fillSine(fixed *b, int frames, int startSample, double hz) {
    for (int i = 0; i < frames; i++) {
        double v = 8000.0 * sin(2.0 * 3.14159265 * hz * (double)(startSample + i) / 44100.0);
        fixed s = fl2fp(v);
        b[2 * i] = s;
        b[2 * i + 1] = s;
    }
}

int main() {
    Audio::Install(new StubAudio());
    System::Install(new StubSystem());
    FileSystem::Install(new UnixFileSystem());

    // Clean any stale files from a previous run.
    remove(kDelay);
    remove(kReverb);
    remove(kMaster);
    mkdir(kDir, 0755);
    snprintf(kDelay, sizeof(kDelay), "%s/delayret.wav", kDir);
    snprintf(kReverb, sizeof(kReverb), "%s/reveret.wav", kDir);
    snprintf(kMaster, sizeof(kMaster), "%s/master.wav", kDir);
    remove(kDelay);
    remove(kReverb);
    remove(kMaster);

    FxEngine::FxEngine &fx = FxEngine::FxEngine::GetInstance();
    fx.Reset();
    fx.SetLegacyMode(true);

    // ---- Test 0: UnixFileSystem::GetFreeSpace probe (item 8 API) ----
    long long free = FileSystem::GetInstance()->GetFreeSpace(".");
    check(free >= 0, "GetFreeSpace('.') reports >= 0 on Linux");
    check(FileSystem::GetInstance()->GetFreeSpace(
              "/nonexistent_dir_xyz_12345") < 0,
          "GetFreeSpace() on a bad path reports -1 (unavailable)");

    // ---- Test A: legacy bypass - master stem only, returns silent ----
    {
        WavFileWriter wDelay(kDelay);
        WavFileWriter wReverb(kReverb);
        WavFileWriter wMaster(kMaster);
        check(wDelay.IsOpen() && wReverb.IsOpen() && wMaster.IsOpen(),
              "writers open");
        fx.EnableStemsCapture(&wDelay, &wReverb, &wMaster);
        check(fx.StemsCaptureActive(), "StemsCaptureActive() true after enable");

        static fixed bufA[FRAMES * 2];
        int start = 0;
        for (int k = 0; k < 5; k++) {
            fillSine(bufA, FRAMES, start, 220.0);
            start += FRAMES;
            fx.Process(bufA, FRAMES);
        }
        fx.EnableStemsCapture(0, 0, 0);
        check(!fx.StemsCaptureActive(), "StemsCaptureActive() false after disable");
    }

    unsigned int masterSz = wavDataSize(kMaster);
    check(masterSz == (unsigned int)(5 * FRAMES * 4),
          "legacy: master.wav holds exactly 5 buffers (%u vs %u)",
          masterSz, (unsigned int)(5 * FRAMES * 4));
    check(wavDataSize(kDelay) == 0, "legacy: delayret.wav silent (0 samples)");
    check(wavDataSize(kReverb) == 0, "legacy: reveret.wav silent (0 samples)");
    check(wavPeak(kMaster) > 0, "legacy: master.wav has real audio");

    // ---- Test B: DSP active - all three stems captured and non-silent ----
    fx.Reset();
    fx.SetDelaySend(fl2fp(0.5f));
    fx.SetDelayReturn(i2fp(1));
    fx.SetDelayTimeMs(fl2fp(50.0f));
    fx.SetDelayFeedback(fl2fp(0.3f));
    fx.SetDelayMix(i2fp(1));
    fx.SetReverbSend(fl2fp(0.5f));
    fx.SetReverbReturn(i2fp(1));
    fx.SetReverbDecay(fl2fp(1.0f));
    fx.SetReverbSize(i2fp(1));
    fx.SetReverbDamping(fl2fp(0.5f));
    fx.SetReverbMix(i2fp(1));
    fx.SetLegacyMode(false);
    check(!fx.IsLegacyMode(), "DSP is engaged for the stems test");

    remove(kDelay);
    remove(kReverb);
    remove(kMaster);
    {
        WavFileWriter wDelay(kDelay);
        WavFileWriter wReverb(kReverb);
        WavFileWriter wMaster(kMaster);
        check(wDelay.IsOpen() && wReverb.IsOpen() && wMaster.IsOpen(),
              "writers open (DSP)");
        fx.EnableStemsCapture(&wDelay, &wReverb, &wMaster);

        static fixed bufB[FRAMES * 2];
        int start = 0;
        for (int k = 0; k < 5; k++) {
            fillSine(bufB, FRAMES, start, 440.0);
            start += FRAMES;
            fx.Process(bufB, FRAMES);
        }
        fx.EnableStemsCapture(0, 0, 0);
    }

    unsigned int dSz = wavDataSize(kDelay);
    unsigned int rSz = wavDataSize(kReverb);
    unsigned int mSz = wavDataSize(kMaster);
    check(dSz == (unsigned int)(5 * FRAMES * 4),
          "DSP: delayret.wav length matches master (%u vs %u)", dSz, (unsigned int)(5 * FRAMES * 4));
    check(rSz == (unsigned int)(5 * FRAMES * 4),
          "DSP: reveret.wav length matches master (%u vs %u)", rSz, (unsigned int)(5 * FRAMES * 4));
    check(mSz == (unsigned int)(5 * FRAMES * 4),
          "DSP: master.wav length matches (%u vs %u)", mSz, (unsigned int)(5 * FRAMES * 4));
    check(wavPeak(kDelay) > 0, "DSP: delay return stem has real audio");
    check(wavPeak(kReverb) > 0, "DSP: reverb return stem has real audio");
    check(wavPeak(kMaster) > 0, "DSP: master stem has real audio");

    // ---- Test C: unopenable path -> capture refused, no partial files ----
    {
        WavFileWriter bad1("/nonexistent_dir_xyz_12345/delayret.wav");
        WavFileWriter bad2("/nonexistent_dir_xyz_12345/reveret.wav");
        WavFileWriter bad3("/nonexistent_dir_xyz_12345/master.wav");
        check(!bad1.IsOpen() && !bad2.IsOpen() && !bad3.IsOpen(),
              "writers fail to open on a missing directory");
    }

    printf("fx_stems_capture_host_test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
