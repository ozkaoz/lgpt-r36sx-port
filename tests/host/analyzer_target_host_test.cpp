/*
 * ANALYZER_MIX (bacon-1.5, item 7, feedback): host test of the
 * SpectrumAnalyzer master-mix-tap contract under ASAN/UBSAN.
 *
 * BACON_1.5_ANALYZER_20HZ (U2.52.6, feedback): the analyzer now uses a
 * 1024-point FFT over a 2048-frame ring, with log bars from 20 Hz to
 * 20 kHz.  With the old 256-point FFT the bottom nine bars collapsed
 * onto a single FFT bin and any sample lit them all through spectral
 * leakage ("las barras no miden 20 Hz-20 kHz").  Test 4 pins the
 * regression: a 1 kHz tone must NOT light the bars below ~300 Hz.
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
    static fixed buf[2 * 2048];
    if (frames > 2048) frames = 2048;
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

// log-bin index of a frequency over the 20..20000 Hz, 24-bin grid
static double logBinIndex(double hz) {
    return log(hz / 20.0) / log(20000.0 / 20.0) * 23.0;
}

int main() {
    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();

    /* --- 1. not armed: feeding is a no-op (Compute says "no new data") --- */
    {
        feedTone(sp, 1000.0, 0.5f, 2048);
        CHECK(sp.Compute() == false);
        for (int i = 0; i < sp.BinCount(); i++) CHECK(sp.Bins()[i] == 0);
    }

    /* --- 2. armed: the 1 kHz sine reaches the bins --- */
    {
        sp.SetArmed(true);
        feedTone(sp, 1000.0, 0.5f, 2048);
        CHECK(sp.Compute() == true);

        // expected log-bin index for 1 kHz over 20..20000 Hz, 24 bins
        int expect = (int)(logBinIndex(1000.0) + 0.5);
        int peak = 0;
        for (int i = 0; i < sp.BinCount(); i++) {
            if (fp2fl(sp.Bins()[i]) > fp2fl(sp.Bins()[peak])) peak = i;
        }
        CHECK(fp2fl(sp.Bins()[peak]) > 0.05f);
        CHECK(peak >= expect - 2 && peak <= expect + 2);
    }

    /* --- 3. a MIX of several frequencies peaks in the right places --- */
    {
        static fixed buf[2 * 2048];
        for (int i = 0; i < 2048; i++) {
            int c = (int)(32767.0 *
                          (0.3 * sin(2.0 * 3.14159265 * 300.0 *
                                     (double)i / 48000.0) +
                           0.3 * sin(2.0 * 3.14159265 * 3000.0 *
                                     (double)i / 48000.0)));
            fixed s = i2fp(c);
            buf[i * 2] = s;
            buf[i * 2 + 1] = s;
        }
        sp.FeedMix(buf, 2048);
        CHECK(sp.Compute() == true);
        int peak = 0;
        for (int i = 0; i < sp.BinCount(); i++) {
            if (fp2fl(sp.Bins()[i]) > fp2fl(sp.Bins()[peak])) peak = i;
        }
        int lo = (int)(logBinIndex(300.0) + 0.5);
        int hi = (int)(logBinIndex(3000.0) + 0.5);
        CHECK(peak >= lo - 2 && peak <= hi + 2);
    }

    /* --- 4. LF isolation (U2.52.6 regression): a 1 kHz tone must NOT
     * light the bars below ~300 Hz (log bins 0..6, FFT bins 0..6).  With
     * the old 256-point FFT the leakage filled the whole low end. --- */
    {
        feedTone(sp, 1000.0, 0.5f, 2048);
        CHECK(sp.Compute() == true);
        for (int i = 0; i < 7; i++) {
            CHECK(fp2fl(sp.Bins()[i]) < 0.02f);
        }
        // and the 1 kHz bar itself is still well lit
        int expect = (int)(logBinIndex(1000.0) + 0.5);
        CHECK(fp2fl(sp.Bins()[expect]) > 0.05f);
    }

    /* --- 5. silence still produces fresh bins (all ~0) --- */
    {
        static fixed buf[2 * 2048];
        memset(buf, 0, sizeof(buf));
        sp.FeedMix(buf, 2048);
        CHECK(sp.Compute() == true);
        for (int i = 0; i < sp.BinCount(); i++) {
            CHECK(fp2fl(sp.Bins()[i]) < 0.001f);
        }
        sp.SetArmed(false);
    }

    printf("analyzer_mix_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}