/*
 * ANALYZER_MIX (bacon-1.5, item 7, feedback): host test of the
 * SpectrumAnalyzer master-mix-tap contract under ASAN/UBSAN.
 *
 * BACON_1.5_ANALYZER_96BARS (U2.59, feedback #12): the analyzer now uses a
 * 4096-point FFT over a 4096-frame ring, with 96 log bars from 20 Hz to
 * 20 kHz (11.719 Hz/bin at 48 kHz).  Test 4 pins the regression: a 1 kHz
 * tone must NOT light the bars below ~300 Hz.
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
    static fixed buf[2 * 4096];
    if (frames > 4096) frames = 4096;
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

// log-bin index of a frequency over the 20..20000 Hz, 96-bin grid
static double logBinIndex(double hz) {
    return log(hz / 20.0) / log(20000.0 / 20.0) *
           (double)(SpectrumAnalyzer::kLogBins - 1);
}

int main() {
    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();

    /* --- 1. not armed: feeding is a no-op (Compute says "no new data") --- */
    {
        feedTone(sp, 1000.0, 0.5f, 4096);
        CHECK(sp.Compute() == false);
        for (int i = 0; i < sp.BinCount(); i++) CHECK(sp.Bins()[i] == 0);
    }

    /* --- 2. armed: the 1 kHz sine reaches the bins (settled) --- */
    {
        sp.SetArmed(true);
        settleTone(sp, 1000.0, 0.5f, 4096);

        // The log bars are +/-30% band windows (binLo = b*0.7, binHi =
        // b*1.3 in BINS, i.e. symmetric in log): a tone at FFT bin 85.3
        // sits INSIDE every bar whose window spans bin 85 -- the overlap
        // plateau bars 51..59 all read the same peak (soft log overlap,
        // same design as the original 24-bin analyzer).  What the bars
        // must NOT do: read anything below ~500 Hz or above ~1.7 kHz.
        int peak = 0;
        for (int i = 0; i < sp.BinCount(); i++) {
            if (fp2fl(sp.Bins()[i]) > fp2fl(sp.Bins()[peak])) peak = i;
        }
        int expect = (int)(logBinIndex(1000.0) + 0.5);
        printf("check2: peak bar=%d val=%.4f expect=%d\n", peak,
               fp2fl(sp.Bins()[peak]), expect);
        CHECK(fp2fl(sp.Bins()[expect]) > 0.08f);   // the 1 kHz bar is lit
        CHECK(peak >= 51 && peak <= 59);           // overlap plateau of bin 85
        for (int i = 0; i <= 46; i++) CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
        for (int i = 64; i < sp.BinCount(); i++) CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
    }

    /* --- 3. a MIX of several frequencies peaks in the right places --- */
    {
        static fixed buf[2 * 4096];
        for (int i = 0; i < 4096; i++) {
            int c = (int)(32767.0 *
                          (0.3 * sin(2.0 * 3.14159265 * 300.0 *
                                     (double)i / 48000.0) +
                           0.3 * sin(2.0 * 3.14159265 * 3000.0 *
                                     (double)i / 48000.0)));
            fixed s = i2fp(c);
            buf[i * 2] = s;
            buf[i * 2 + 1] = s;
        }
        sp.FeedMix(buf, 4096);
        sp.Compute();
        // Both tones are lit at their log positions (300 Hz -> bar 37,
        // 3 kHz -> bar 69), and the log plateau of one must not spill
        // into the other: the 300 Hz plateau ends at bar 46 (binHi 46.6),
        // the 3 kHz plateau starts at bar 66 (binLo > 197 bins -> 65.3).
        int lo = (int)(logBinIndex(300.0) + 0.5);
        int hi = (int)(logBinIndex(3000.0) + 0.5);
        printf("check3: lo bar=%d hi bar=%d 300Hz=%.4f 3kHz=%.4f\n", lo, hi,
               fp2fl(sp.Bins()[lo]), fp2fl(sp.Bins()[hi]));
        CHECK(fp2fl(sp.Bins()[lo]) > 0.03f);
        CHECK(fp2fl(sp.Bins()[hi]) > 0.03f);
        for (int i = 47; i <= 65; i++) CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
    }

    /* --- 4. LF isolation (U2.52.6 regression): a 1 kHz tone must NOT
     * light the bars below ~500 Hz (log bars 0..46, FFT bins ~0..30).
     * With the old 256-point FFT the leakage filled the whole low end. --- */
    {
        settleTone(sp, 1000.0, 0.5f, 4096);
        for (int i = 0; i <= 46; i++) {
            CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
        }
        // and the 1 kHz bar itself is still well lit
        int expect = (int)(logBinIndex(1000.0) + 0.5);
        CHECK(fp2fl(sp.Bins()[expect]) > 0.08f);
    }

    /* --- 5. silence still produces fresh bins (all ~0; INSTANT_PEAK
     * reads a silent window directly) --- */
    {
        static fixed buf[2 * 4096];
        memset(buf, 0, sizeof(buf));
        sp.FeedMix(buf, 4096);
        sp.Compute();
        for (int i = 0; i < sp.BinCount(); i++) {
            CHECK(fp2fl(sp.Bins()[i]) < 0.001f);
        }
        sp.SetArmed(false);
    }

    printf("analyzer_mix_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}