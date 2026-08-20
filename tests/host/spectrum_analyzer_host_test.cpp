// EQ8_SPECTRUM_VERIFY (U2.57b, feedback #10 + U2.59, feedback #12): objective
// host check that the SpectrumAnalyzer bins ARE a faithful representation of
// the audio fed from the master mix.
//
// The analyzer (SpectrumAnalyzer.cpp) is fed the final master buffer in DAC
// counts <<15; FeedMix stores the mono average as (l>>16)+(r>>16), which for
// two identical full-scale channels is ~1.0 after fp2fl().  An 8192-point
// Hann-windowed FFT (BACON_1.5_ANALYZER_FINE, U2.61 -- 5.86 Hz/bin) then
// maps a 0 dBFS sine at an exact FFT bin to a peak of N*A/4 / N = 0.25;
// the EQ8 view scales x4 to a full bar (0 dBFS = full).
//
// U2.57b perceptual model (feedback #10: "las barras no se dibujan en
// sonidos altos; en los graves (kick) si se dibujan"):
//   - DCBLOCK: the window mean is subtracted before the FFT.  Percussive
//     attacks carry a DC transient (the kit's hi-hat swings -4000..+9000
//     counts during the first 20 ms, dc_attack.py); the Hann FFT mapped it
//     onto bins 1-3, so bars 20..120 Hz pinned at full on EVERY percussive
//     hit.  The ear hears the AC content, not the DC step.  (A linear
//     detrend was tried in U2.57b and REJECTED: it cost -5.3 dB on a
//     46.875 Hz 1-cycle tone, i.e. real kick bodies.)
//   - INSTANT_PEAK: bins are the instantaneous per-window peak, NO temporal
//     smoothing.
//
// U2.61 grid: 154 log bars over 20 Hz..20 kHz, FFT 8192 (5.86 Hz/bin at
// 48 kHz).  The window covers the newest 8192 of an 8192-frame ring, so a
// full feed fills the window exactly.  Reference bar indices (log grid
// 20*1000^(i/153)):
//   - 46.875 Hz (bin 8)   -> bar 19 (fc ~47.9 Hz)
//   - 100 Hz (bin 17)     -> bar 36 (fc ~100 Hz)
//   - 984.375 Hz (bin 168)-> bar 86 (fc ~1015 Hz)
//   - 16 kHz (bin 2731)   -> bar 148 (fc ~16.1 kHz)
//
// Scenarios:
//   1. silence -> every bin is 0.
//   2. 0 dBFS sine at bin 84 (984.375 Hz): the peak lands on bar 54 at
//      ~0.25, the +-30% log overlap lights bars 51..55 at the same height,
//      and the far bars stay ~0.
//   3. same sine at -12 dBFS (amp 0.25): bar 54 at ~0.0625, exactly 4x less
//      -- the bars reflect the real dynamics.
//   4. low-bass resolution: a sine exactly at bin 4 (46.875 Hz) lights
//      bar 12 at ~0.25; a 20 Hz sine falls below bin 2 and reads the lobe
//      minus the window DC (documented 20..70 Hz floor); a 100 Hz sine
//      lights bar 22 WITHOUT reaching the 1 kHz bars.
//   5. 16 kHz sine lands on bar 92, not the mid bars.
//   6. full-scale DC (the hi-hat DC transient, ~4300 counts): the DCBLOCK
//      removes it -- all bars ~0.
//   7. a full-scale 1 ms pulse (broadband, like a kick transient): it must
//      NOT pin any bar to full -- the per-bar peak stays < 0.06.
#include "Application/Audio/SpectrumAnalyzer.h"
#include "Application/Utils/fixed.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

static int checks = 0;
static int failures = 0;

static void check(bool cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static int logBarIndex(double hz) {
    double r = log(hz / 20.0) / log(20000.0 / 20.0);
    return (int)(r * (SpectrumAnalyzer::kLogBins - 1) + 0.5);
}

// The master mix arrives in DAC counts << 15 (AudioOutDriver scale):
// a count of 32767 (0 dBFS) is the fixed value 32767 << 15.  FeedMix
// downsamples with (l>>16)+(r>>16), so the feeder must use that scale.
static void feedSine(float ampHz, int ampCounts) {
    const int kFrames = SpectrumAnalyzer::kRingFrames;
    fixed buf[2 * kFrames];
    for (int i = 0; i < kFrames; i++) {
        float v = (float)ampCounts *
                  sinf(2.0f * 3.14159265f * ampHz * i / 48000.0f);
        fixed s = ((fixed)(long long)v) << 15;
        buf[i * 2] = s;
        buf[i * 2 + 1] = s;
    }
    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();
    sp.SetArmed(true);
    sp.FeedMix(buf, kFrames);
}

static void feedDC(int ampCounts) {
    const int kFrames = SpectrumAnalyzer::kRingFrames;
    fixed buf[2 * kFrames];
    fixed s = ((fixed)ampCounts) << 15;
    for (int i = 0; i < kFrames; i++) {
        buf[i * 2] = s;
        buf[i * 2 + 1] = s;
    }
    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();
    sp.SetArmed(true);
    sp.FeedMix(buf, kFrames);
}

// One feed + compute: with INSTANT_PEAK (U2.57b) a sustained tone reaches
// its steady-state value in a single frame (the ring holds the full window).
static void oneShot(float ampHz, int ampCounts) {
    feedSine(ampHz, ampCounts);
    SpectrumAnalyzer::Get().Compute();
}

int main() {
    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();

    // 1. silence
    sp.SetArmed(true);
    static fixed silence[SpectrumAnalyzer::kRingFrames * 2];
    memset(silence, 0, sizeof(silence));
    sp.FeedMix(silence, SpectrumAnalyzer::kRingFrames);
    check(sp.Compute(), "compute on silence");
    bool allZero = true;
    for (int i = 0; i < sp.BinCount(); i++) {
        if (fp2fl(sp.Bins()[i]) > 0.001f) allZero = false;
    }
    check(allZero, "silence: all bins 0");

    // 2. 0 dBFS sine at bin 84 (984.375 Hz) -> bar 54 (fc ~1015 Hz) ~0.25.
    //    Design finding: the bands are +/-30% LOG bands, so the +-30%
    //    ranges of bars 51..55 all contain bin 84 -- a pure tone lights
    //    ~5 contiguous bars at the same height (soft overlap, not
    //    leakage).  What must NOT happen: the tone reaching the far ends.
    oneShot(984.375f, 32767);
    {
        int bar = logBarIndex(984.375);
        float peakBar = fp2fl(sp.Bins()[bar]);
        printf("1kHz 0dBFS: bar%d=%.4f bar%d=%.4f\n", bar, peakBar, bar + 1,
               fp2fl(sp.Bins()[bar + 1]));
        check(peakBar > 0.22f && peakBar < 0.28f,
              "1 kHz 0 dBFS: bar 54 ~0.25 (Hann N/4)");
        check(fp2fl(sp.Bins()[bar + 1]) > 0.15f,
              "overlap design: bar 55 shares bin 84");
        float bar0 = fp2fl(sp.Bins()[0]);
        float bar92 = fp2fl(sp.Bins()[logBarIndex(16000.0)]);
        printf("1kHz 0dBFS: bar0=%.4f bar92=%.4f\n", bar0, bar92);
        check(bar0 < 0.02f && bar92 < 0.02f, "1 kHz stays out of the far bars");
    }

    // 3. same sine at -12 dBFS: exactly 4x less (instantaneous peak)
    oneShot(984.375f, 8192);
    {
        float peakBar = fp2fl(sp.Bins()[logBarIndex(984.375)]);
        printf("1kHz -12dBFS: bar54=%.4f\n", peakBar);
        check(peakBar > 0.055f && peakBar < 0.075f,
              "-12 dBFS is 4x below 0 dBFS (0.0625)");
    }

    // 4. low-bass floor: bin 4 (46.875 Hz) lights bar 12 at ~0.25.  A
    //    20 Hz sine (bin 1.7) falls BELOW bin 2 -- its Hann main lobe
    //    lands on bins 1-2, so bar 0 still reads part of it (documented
    //    20..70 Hz floor).  A 100 Hz sine (bin 8.5) must light the low
    //    bars WITHOUT reaching the 1 kHz bars.
    oneShot(46.875f, 32767);
    {
        int bar = logBarIndex(46.875);
        float b = fp2fl(sp.Bins()[bar]);
        printf("46.875Hz 0dBFS: bar%d=%.4f\n", bar, b);
        check(b > 0.22f && b < 0.28f, "46.875 Hz (bin 4) lights bar 12 at ~0.25");
    }
    oneShot(20.0f, 32767);
    {
        float bar0 = fp2fl(sp.Bins()[0]);
        printf("20Hz 0dBFS: bar0=%.4f (lobe minus window DC)\n", bar0);
        // The window covers 1.7 cycles of a 20 Hz tone, so the DC-block
        // removes only a small fraction of the tone (vs the 0.43 cycles of
        // the old 1024 window) and bar 0 reads most of the lobe.
        check(bar0 > 0.12f && bar0 < 0.30f,
              "20 Hz: bar 0 reads the lobe minus the window DC (documented floor)");
    }
    oneShot(100.0f, 32767);
    {
        float barLow = fp2fl(sp.Bins()[logBarIndex(100.0)]);
        float barHigh = fp2fl(sp.Bins()[logBarIndex(1000.0)]);
        printf("100Hz 0dBFS: bar22=%.4f bar54=%.4f\n", barLow, barHigh);
        check(barLow > 0.05f, "100 Hz lights the low bars");
        check(barHigh < 0.05f, "100 Hz stays out of the 1 kHz bar");
    }

    // 5. 16 kHz sine -> bar 92 (fc ~16.1 kHz)
    oneShot(16000.0f, 32767);
    {
        int bar = logBarIndex(16000.0);
        float b = fp2fl(sp.Bins()[bar]);
        printf("16kHz 0dBFS: bar%d=%.4f\n", bar, b);
        check(b > 0.10f, "16 kHz lands on the top bar 92");
        float mid = fp2fl(sp.Bins()[logBarIndex(1000.0)]);
        check(mid < 0.05f, "16 kHz stays out of the mid bars");
    }

    // 6. DCBLOCK: a sustained DC of 4300 counts (the measured hi-hat DC
    //    transient swing) must read ~0 everywhere -- the mean subtraction
    //    removes it before the FFT.
    feedDC(4300);
    check(sp.Compute(), "compute on DC 4300");
    {
        float worst = 0.0f;
        for (int i = 0; i < sp.BinCount(); i++) {
            float v = fp2fl(sp.Bins()[i]);
            if (v > worst) worst = v;
        }
        printf("DC 4300: worst bar=%.4f\n", worst);
        check(worst < 0.01f, "DC transient is blocked (bars ~0)");
    }

    // 7. INSTANT_PEAK: a full-scale 1 ms pulse (48 samples, like a kick
    //    transient) at the FFT window center must NOT pin any bar to full:
    //    the detrended per-bar peak stays < 0.06 while a real sustained
    //    tone reads 0.25.  The ring holds the NEWEST 4096 frames (ringPos_
    //    wraps to 0 after the feed, so the window is the whole feed; its
    //    center is 2048).
    {
        const int kFrames = SpectrumAnalyzer::kRingFrames;
        fixed buf[2 * kFrames];
        memset(buf, 0, sizeof(buf));
        fixed s = ((fixed)32767) << 15;
        for (int i = 2024; i < 2072; i++) {
            buf[i * 2] = s;
            buf[i * 2 + 1] = s;
        }
        sp.SetArmed(true);
        sp.FeedMix(buf, kFrames);
    }
    check(sp.Compute(), "compute on 1 ms pulse");
    {
        float worst = 0.0f;
        for (int i = 0; i < sp.BinCount(); i++) {
            float v = fp2fl(sp.Bins()[i]);
            if (v > worst) worst = v;
        }
        printf("1ms pulse: worst bar=%.4f\n", worst);
        check(worst < 0.06f, "a 1 ms transient never pins a bar (sustained tones read 0.25)");
    }

    printf("EQ8_SPECTRUM_VERIFY: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}