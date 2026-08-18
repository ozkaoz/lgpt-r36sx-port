/*
 * ANALYZER_MIX (bacon-1.5, item 7, feedback): host test of the
 * SpectrumAnalyzer master-mix-tap contract under ASAN/UBSAN.
 *
 * The RT thread feeds the analyzer the FINAL master buffer (the exact
 * signal that reaches the speakers) via FeedMix().  The tap records only
 * when armed (a view is open and listening); there is no per-instrument
 * targeting anymore: the spectrum shows the whole mix.
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

static void feedSine(SpectrumAnalyzer &sp, double hz, int frames) {
    fixed buf[2 * 512];
    for (int i = 0; i < frames && i < 512; i++) {
        fixed s = fl2fp((float)(0.5 * sin(2.0 * 3.14159265 * hz *
                                          (double)i / 48000.0)));
        buf[i * 2] = s;
        buf[i * 2 + 1] = s;
    }
    sp.FeedMix(buf, frames);
}

int main() {
    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();

    /* --- 1. not armed: feeding is a no-op (Compute says "no new data") --- */
    {
        feedSine(sp, 1000.0, 512);
        CHECK(sp.Compute() == false);
        for (int i = 0; i < sp.BinCount(); i++) CHECK(sp.Bins()[i] == 0);
    }

    /* --- 2. armed: the 1 kHz sine reaches the bins --- */
    {
        sp.SetArmed(true);
        feedSine(sp, 1000.0, 512);
        CHECK(sp.Compute() == true);

        // expected log-bin index for 1 kHz over 30..20000 Hz, 24 bins
        double idx = log(1000.0 / 30.0) / log(20000.0 / 30.0) * 23.0;
        int expect = (int)(idx + 0.5);
        int peak = 0;
        for (int i = 0; i < sp.BinCount(); i++) {
            if (fp2fl(sp.Bins()[i]) > fp2fl(sp.Bins()[peak])) peak = i;
        }
        CHECK(fp2fl(sp.Bins()[peak]) > 0.05f);
        CHECK(peak >= expect - 2 && peak <= expect + 2);
    }

    /* --- 3. a MIX of several frequencies peaks in the right places --- */
    {
        fixed buf[2 * 512];
        for (int i = 0; i < 512; i++) {
            fixed s = fl2fp((float)(0.3 * sin(2.0 * 3.14159265 * 300.0 *
                                              (double)i / 48000.0) +
                                    0.3 * sin(2.0 * 3.14159265 * 3000.0 *
                                              (double)i / 48000.0)));
            buf[i * 2] = s;
            buf[i * 2 + 1] = s;
        }
        sp.FeedMix(buf, 512);
        CHECK(sp.Compute() == true);
        int peak = 0;
        for (int i = 0; i < sp.BinCount(); i++) {
            if (fp2fl(sp.Bins()[i]) > fp2fl(sp.Bins()[peak])) peak = i;
        }
        double idx300 = log(300.0 / 30.0) / log(20000.0 / 30.0) * 23.0;
        double idx3000 = log(3000.0 / 30.0) / log(20000.0 / 30.0) * 23.0;
        int lo = (int)(idx300 + 0.5);
        int hi = (int)(idx3000 + 0.5);
        CHECK(peak >= lo - 2 && peak <= hi + 2);
    }

    /* --- 4. silence still produces fresh bins (all ~0) --- */
    {
        fixed buf[2 * 512];
        memset(buf, 0, sizeof(buf));
        sp.FeedMix(buf, 512);
        CHECK(sp.Compute() == true);
        for (int i = 0; i < sp.BinCount(); i++) {
            CHECK(fp2fl(sp.Bins()[i]) < 0.001f);
        }
        sp.SetArmed(false);
    }

    printf("analyzer_mix_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}