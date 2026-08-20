/*
 * ANALYZER_MIX (bacon-1.5, item 7, feedback): host test of the
 * SpectrumAnalyzer master-mix-tap contract under ASAN/UBSAN.
 *
 * BACON_1.5_ANALYZER_96BARS (U2.59, feedback #12) / BACON_1.5_ANALYZER_FINE
 * (U2.61, feedback #13): the analyzer now uses an 8192-point FFT over an
 * 8192-frame ring, with 154 log bars from 20 Hz to 20 kHz (5.86 Hz/bin at
 * 48 kHz).  Test 4 pins the regression: a 1 kHz tone must NOT light the
 * bars below ~500 Hz (bars 0..69 on the 154-grid).
 *
 * BACON_1.5_ANALYZER_SCALE (U2.52.9, feedback #6): FeedMix takes the
 * MASTER bus, which is int16 DAC counts shifted <<15 (count<<15).  The
 * mono average is taken in counts ((l>>16)+(r>>16)), so fp2fl() on the
 * ring yields the true -1..1 audio: a 0 dBFS sine peaks at ~0.25 in the
 * bins (Hann window), and a quiet hat lights its bars low while a kick
 * lights the lows.  The harness feeds master scale (i2fp(count)), the
 * same byte layout the real AudioMixer/recorder path writes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Application/Audio/SpectrumAnalyzer.h"

static int checks = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL line %d: %s\n", __LINE__, #cond);              \
            exit(1);                                                    \
        }                                                               \
        checks++;                                                       \
    } while (0)

static void feedTone(SpectrumAnalyzer &sp, double hz, float amp,
                     int frames) {
    static fixed buf[2 * SpectrumAnalyzer::kRingFrames];
    if (frames > SpectrumAnalyzer::kRingFrames)
        frames = SpectrumAnalyzer::kRingFrames;
    for (int i = 0; i < frames; i++) {
        // BACON_1.5_ANALYZER_SCALE: the master mix is int16 DAC counts <<15,
        // so the harness feeds i2fp(count) -- fp2fl() on the ring must read
        // back the real 0..1 audio (feeding fl2fp(amp) here would vanish
        // through the (l>>16)+(r>>16) count average).
        int c = (int)(amp * 32767.0 * sin(2.0 * 3.14159265 * hz *
                                          (double)i / 48000.0));
        fixed s = i2fp(c);
        buf[i * 2] = s;
        buf[i * 2 + 1] = s;
    }
    sp.FeedMix(buf, frames);
}

// BACON_1.5_ANALYZER_INSTANT_PEAK (U2.57b, feedback #10): bins are the
// instantaneous per-window peak (no temporal smoothing); a single
// feed/compute reaches steady state, so "settling" loops are unnecessary.
static void settleTone(SpectrumAnalyzer &sp, double hz, float amp,
                       int frames) {
    feedTone(sp, hz, amp, frames);
    sp.Compute();
}

// log-bin index of a frequency over the 20..20000 Hz, kLogBins grid
static double logBinIndex(double hz) {
    return log(hz / 20.0) / log(20000.0 / 20.0) *
           (double)(SpectrumAnalyzer::kLogBins - 1);
}

int main() {
    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();
    const int kF = SpectrumAnalyzer::kRingFrames;

    /* --- 1. not armed: feeding is a no-op (Compute says "no new data") --- */
    {
        feedTone(sp, 1000.0, 0.5f, kF);
        CHECK(sp.Compute() == false);
        for (int i = 0; i < sp.BinCount(); i++) CHECK(sp.Bins()[i] == 0);
    }

    /* --- 2. armed: the 1 kHz sine reaches the bins (settled) --- */
    {
        sp.SetArmed(true);
        settleTone(sp, 1000.0, 0.5f, kF);

        // The log bars are +/-30% band windows (binLo = b*0.7, binHi =
        // b*1.3 in BINS, i.e. symmetric in log): a tone at FFT bin 170.7
        // sits INSIDE every bar whose window spans bin 170 -- the overlap
        // plateau bars 81..95 all read the same peak (soft log overlap,
        // same design as the 96-bar analyzer).  What the bars must NOT do:
        // read anything below ~500 Hz (bars 0..69) or above ~1.7 kHz
        // (bars 103..153).
        int peak = 0;
        for (int i = 0; i < sp.BinCount(); i++) {
            if (fp2fl(sp.Bins()[i]) > fp2fl(sp.Bins()[peak])) peak = i;
        }
        int expect = (int)(logBinIndex(1000.0) + 0.5);
        printf("check2: peak bar=%d val=%.4f expect=%d\n", peak,
               fp2fl(sp.Bins()[peak]), expect);
        CHECK(fp2fl(sp.Bins()[expect]) > 0.08f);   // the 1 kHz bar is lit
        CHECK(peak >= 81 && peak <= 95);           // overlap plateau of bin 170
        for (int i = 0; i <= 69; i++) CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
        for (int i = 103; i < sp.BinCount(); i++) CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
    }

    /* --- 3. a MIX of several frequencies peaks in the right places --- */
    {
        static fixed buf[2 * SpectrumAnalyzer::kRingFrames];
        for (int i = 0; i < SpectrumAnalyzer::kRingFrames; i++) {
            int c = (int)(32767.0 *
                          (0.3 * sin(2.0 * 3.14159265 * 300.0 *
                                     (double)i / 48000.0) +
                           0.3 * sin(2.0 * 3.14159265 * 3000.0 *
                                     (double)i / 48000.0)));
            fixed s = i2fp(c);
            buf[i * 2] = s;
            buf[i * 2 + 1] = s;
        }
        sp.FeedMix(buf, SpectrumAnalyzer::kRingFrames);
        sp.Compute();
        // Both tones are lit at their log positions (300 Hz -> bar 60,
        // 3 kHz -> bar 111), and the log plateau of one must not spill
        // into the other: the 300 Hz plateau (bins 49..53) ends at bar
        // ~69, the 3 kHz plateau (bins 512) starts at bar ~105 -- the
        // gap bars 70..104 stay dark.
        int lo = (int)(logBinIndex(300.0) + 0.5);
        int hi = (int)(logBinIndex(3000.0) + 0.5);
        printf("check3: lo bar=%d hi bar=%d 300Hz=%.4f 3kHz=%.4f\n", lo, hi,
               fp2fl(sp.Bins()[lo]), fp2fl(sp.Bins()[hi]));
        CHECK(fp2fl(sp.Bins()[lo]) > 0.03f);
        CHECK(fp2fl(sp.Bins()[hi]) > 0.03f);
        for (int i = 70; i <= 104; i++) CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
    }

    /* --- 4. LF isolation (U2.52.6 regression): a 1 kHz tone must NOT
     * light the bars below ~500 Hz (log bars 0..69, FFT bins ~0..120).
     * With the old 256-point FFT the leakage filled the whole low end. --- */
    {
        settleTone(sp, 1000.0, 0.5f, kF);
        for (int i = 0; i <= 69; i++) {
            CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
        }
        // and the 1 kHz bar itself is still well lit
        int expect = (int)(logBinIndex(1000.0) + 0.5);
        CHECK(fp2fl(sp.Bins()[expect]) > 0.08f);
    }

    /* --- 5. silence still produces fresh bins (all ~0; INSTANT_PEAK
     * reads a silent window directly) --- */
    {
        static fixed buf[2 * SpectrumAnalyzer::kRingFrames];
        memset(buf, 0, sizeof(buf));
        sp.FeedMix(buf, SpectrumAnalyzer::kRingFrames);
        sp.Compute();
        for (int i = 0; i < sp.BinCount(); i++) {
            CHECK(fp2fl(sp.Bins()[i]) < 0.001f);
        }
        sp.SetArmed(false);
    }

    printf("analyzer_mix_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}