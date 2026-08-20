/*
 * ANALYZER_MIX (bacon-1.5, item 7, feedback): host test of the
 * SpectrumAnalyzer master-mix-tap contract under ASAN/UBSAN.
 *
 * BACON_1.5_ANALYZER_96BARS (U2.59, feedback #12) / BACON_1.5_ANALYZER_FINE
 * (U2.61, feedback #13) / BACON_1.5_ANALYZER_FINER (U2.62, feedback #14):
 * the analyzer uses a 16384-point FFT over a 16384-frame ring, with 308
 * log bars from 20 Hz to 20 kHz (2.93 Hz/bin at 48 kHz).  Test 4 pins the
 * regression: a 1 kHz tone must NOT light the bars below ~770 Hz (bars
 * 0..161 on the 308-grid).  The plateau boundaries are computed DYNAMICALLY
 * from the grid (logBinIndex of hz/1.3 .. hz/0.7), so the assertions stay
 * correct if the grid changes again.
 *
 * BACON_1.5_ANALYZER_PEAKHIST (U2.62, feedback #14): test 6 pins the
 * historical-peak contract used by the EQ8 L2+R2 marker: the history only
 * grows (the loudest window since the last PeakTrackReset), PeakFrequency()
 * keeps returning the CURRENT window's peak, and resetting clears it.
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
        // b*1.3 in BINS, i.e. symmetric in log): a tone at FFT bin 341.3
        // sits INSIDE every bar whose window spans bin 341 -- the overlap
        // plateau [logBinIndex(1000/1.3) .. logBinIndex(1000/0.7)] all
        // read the same peak (soft log overlap, same design as the
        // 96-bar analyzer).  What the bars must NOT do: read anything
        // below ~770 Hz (bars 0..loLit-1) or above ~1.43 kHz
        // (bars hiLit+1..).
        int peak = 0;
        for (int i = 0; i < sp.BinCount(); i++) {
            if (fp2fl(sp.Bins()[i]) > fp2fl(sp.Bins()[peak])) peak = i;
        }
        int expect = (int)(logBinIndex(1000.0) + 0.5);
        int loLit = (int)logBinIndex(1000.0 / 1.3);
        int hiLit = (int)logBinIndex(1000.0 / 0.7);
        printf("check2: peak bar=%d val=%.4f expect=%d plateau=[%d..%d]\n",
               peak, fp2fl(sp.Bins()[peak]), expect, loLit, hiLit);
        CHECK(fp2fl(sp.Bins()[expect]) > 0.08f);   // the 1 kHz bar is lit
        CHECK(peak >= loLit && peak <= hiLit);     // overlap plateau
        for (int i = 0; i <= loLit - 1; i++) CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
        for (int i = hiLit + 1; i < sp.BinCount(); i++) CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
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
        // Both tones are lit at their log positions (300 Hz -> bar 120,
        // 3 kHz -> bar 223), and the log plateau of one must not spill
        // into the other: the 300 Hz plateau (bins ~102..146) ends at bar
        // ~136, the 3 kHz plateau (bins ~1024) starts at bar ~211 -- the
        // gap bars 137..210 stay dark.
        int lo = (int)(logBinIndex(300.0) + 0.5);
        int hi = (int)(logBinIndex(3000.0) + 0.5);
        int gapLo = (int)logBinIndex(300.0 / 0.7) + 1;
        int gapHi = (int)logBinIndex(3000.0 / 1.3) - 1;
        printf("check3: lo bar=%d hi bar=%d 300Hz=%.4f 3kHz=%.4f gap=[%d..%d]\n",
               lo, hi, fp2fl(sp.Bins()[lo]), fp2fl(sp.Bins()[hi]),
               gapLo, gapHi);
        CHECK(fp2fl(sp.Bins()[lo]) > 0.03f);
        CHECK(fp2fl(sp.Bins()[hi]) > 0.03f);
        for (int i = gapLo; i <= gapHi; i++) CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
    }

    /* --- 4. LF isolation (U2.52.6 regression): a 1 kHz tone must NOT
     * light the bars below ~770 Hz (log bars 0..161, FFT bins ~0..262).
     * With the old 256-point FFT the leakage filled the whole low end. --- */
    {
        settleTone(sp, 1000.0, 0.5f, kF);
        int loLit = (int)logBinIndex(1000.0 / 1.3);
        for (int i = 0; i <= loLit - 1; i++) {
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
    }

    /* --- 6. BACON_1.5_ANALYZER_PEAKHIST (U2.62, feedback #14): the
     * historical peak only grows (the loudest window since the last
     * PeakTrackReset), PeakFrequency() keeps returning the CURRENT
     * window's peak, and resetting clears the history. --- */
    {
        sp.PeakTrackReset();
        CHECK(!sp.PeakHasHistory());

        // 1 kHz at 0.5 -> the history and the current peak both land there
        // (the interpolated peak is within +-3 Hz of the true 1000.0).
        settleTone(sp, 1000.0, 0.5f, kF);
        CHECK(sp.PeakHasHistory());
        printf("hist: cur=%.1f hist=%.1f\n", sp.PeakFrequency(),
               sp.PeakFrequencyHistory());
        CHECK(fabsf(sp.PeakFrequencyHistory() - 1000.0f) < 3.0f);
        CHECK(fabsf(sp.PeakFrequency() - 1000.0f) < 3.0f);

        // a QUIETER 300 Hz window: the current peak moves to 300 Hz, the
        // history stays on the louder 1 kHz (where the energy was).
        settleTone(sp, 300.0, 0.25f, kF);
        CHECK(fabsf(sp.PeakFrequency() - 300.0f) < 3.0f);
        CHECK(fabsf(sp.PeakFrequencyHistory() - 1000.0f) < 3.0f);

        // even a 3 kHz at 0.3 (louder than the 300 Hz but quieter than the
        // 1 kHz, mag^2 0.09 < 0.25) does not move the history, but the
        // current peak follows it.
        settleTone(sp, 3000.0, 0.3f, kF);
        CHECK(fabsf(sp.PeakFrequency() - 3000.0f) < 3.0f);
        CHECK(fabsf(sp.PeakFrequencyHistory() - 1000.0f) < 3.0f);

        // a louder-than-everything 1 kHz re-affirms the history.
        settleTone(sp, 1000.0, 0.9f, kF);
        CHECK(fabsf(sp.PeakFrequencyHistory() - 1000.0f) < 3.0f);

        // reset: the history clears, the current peak still reads.
        sp.PeakTrackReset();
        CHECK(!sp.PeakHasHistory());
        CHECK(fabsf(sp.PeakFrequency() - 1000.0f) < 3.0f);

        // after the reset, a fresh window re-arms the history.
        settleTone(sp, 3000.0, 0.2f, kF);
        CHECK(sp.PeakHasHistory());
        CHECK(fabsf(sp.PeakFrequencyHistory() - 3000.0f) < 3.0f);

        sp.SetArmed(false);
    }

    printf("analyzer_mix_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}