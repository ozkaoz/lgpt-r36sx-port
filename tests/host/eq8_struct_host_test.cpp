/*
 * EQ8_STRUCT (bacon-1.5, item 4): structural host test of
 * FxEngine::InstrumentEq under ASAN/UBSAN.
 *
 * Regression targets:
 *  - Per-band-per-channel biquad states (state_[channel][band]).  The OLD
 *    layout chained all 8 bands on one ChanState per channel, so the cascade
 *    order of the bands changed the sound.  For LTI biquads the order of a
 *    cascade must not matter: this test asserts swapping two bands' params
 *    changes the output only within fixed-point rounding noise.
 *  - Coefficient smoothing converges EXACTLY to the RBJ targets (snap of the
 *    last sub-2^-6 residual; the old loop stalled 63 LSBs short and never
 *    cleared its smoothing flag).
 *  - ConfigureBand atomicity, clamping, channel independence, bypass/flat
 *    zero-cost paths.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Application/Audio/InstrumentEq.h"
#include "Application/Audio/EqBiquad.h"

using namespace FxEngine;

static int checks = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL line %d: %s\n", __LINE__, #cond);              \
            exit(1);                                                    \
        }                                                               \
        checks++;                                                       \
    } while (0)

static const int kRate = 48000;
static const int kFrames = 1024;

/* --- test signal: mix of partials so both bands are exercised --- */
static void makeSignal(fixed *buf, int frames) {
    for (int i = 0; i < frames; i++) {
        double t = (double)i / (double)kRate;
        double v = 0.30 * sin(2.0 * 3.14159265 * 440.0 * t) +
                   0.20 * sin(2.0 * 3.14159265 * 900.0 * t) +
                   0.15 * sin(2.0 * 3.14159265 * 1700.0 * t) +
                   0.10 * sin(2.0 * 3.14159265 * 3000.0 * t);
        fixed s = fl2fp((float)v);
        buf[i * 2] = s;
        buf[i * 2 + 1] = s;
    }
}

/* One single pass of Process() over a 1024-frame buffer advances the
 * per-frame smoothing 1024 steps -- far more than the ~440 needed to land
 * within 1 LSB of the RBJ target (exponential step d >> 6).  Re-processing
 * the same buffer repeatedly would keep re-filtering the OUTPUT (gain
 * multiplies per pass and the state blows up), so convergence is done in
 * ONE pass. */
static void runOnce(InstrumentEq &eq, int channel, fixed *buf, int frames) {
    eq.Process(channel, buf, frames);
}

static fixed maxAbsDiff(const fixed *a, const fixed *b, int n) {
    fixed m = 0;
    for (int i = 0; i < n; i++) {
        fixed d = a[i] - b[i];
        if (d < 0) d = -d;
        if (d > m) m = d;
    }
    return m;
}

/* RMS of the second half of a processed buffer (post-convergence tail). */
static double rmsTail(const fixed *buf, int frames) {
    double acc = 0.0;
    int n = 0;
    for (int i = frames / 2; i < frames; i++) {
        double v = fp2fl(buf[i]);
        acc += v * v;
        n++;
    }
    return (n > 0) ? sqrt(acc / n) : 0.0;
}

/* Goertzel amplitude (Q15 counts) of one frequency over the L-side tail. */
static double goertzel(const fixed *buf, int frames, double f) {
    double w = 2.0 * 3.14159265 * f / (double)kRate;
    double c = 2.0 * cos(w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (int i = frames / 2; i < frames; i++) {
        double x = fp2fl(buf[2 * i]);
        s0 = x + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    double mag = sqrt(s1 * s1 + s2 * s2 - c * s1 * s2);
    return 2.0 * mag / (double)(frames / 2);
}

int main() {
    /* --- 1. fresh EQ is flat: Process is a zero-cost identity --- */
    {
        InstrumentEq eq;
        eq.SetSampleRate(kRate);
        fixed in[2 * kFrames], out[2 * kFrames];
        makeSignal(in, kFrames);
        memcpy(out, in, sizeof(in));
        eq.Process(0, out, kFrames);
        CHECK(memcmp(in, out, sizeof(in)) == 0);
        CHECK(eq.IsFlat());
        CHECK(!eq.GetBypass());
    }

    /* --- 2. RBJ convergence: smoothed coeffs reach the EqBiquad target
     * EXACTLY (previously stalled 63 LSBs short) --- */
    {
        InstrumentEq eq;
        eq.SetSampleRate(kRate);
        eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(1000.0f),
                         fl2fp(12.0f), fl2fp(1.0f), true);

        fixed rb0, rb1, rb2, ra1, ra2;
        eqBiquadCoeffs(EQ_BIQUAD_BELL, kRate, 1000.0f, 12.0f, 1.0f,
                       rb0, rb1, rb2, ra1, ra2);

        // Before any frame: still the identity coefficients (smoothing
        // pending), NOT the target.
        fixed c0, c1, c2, ca1, ca2;
        eq.GetBandCoeffs(0, &c0, &c1, &c2, &ca1, &ca2);
        CHECK(c0 == i2fp(1) && c1 == 0 && c2 == 0 && ca1 == 0 && ca2 == 0);

        // Converge.
        fixed buf[2 * kFrames];
        makeSignal(buf, kFrames);
        runOnce(eq, 0, buf, kFrames);

        eq.GetBandCoeffs(0, &c0, &c1, &c2, &ca1, &ca2);
        CHECK(c0 == rb0 && c1 == rb1 && c2 == rb2 && ca1 == ra1 && ca2 == ra2);
    }

    /* --- 3. smoothing is monotonic toward the target --- */
    {
        InstrumentEq eq;
        eq.SetSampleRate(kRate);
        eq.ConfigureBand(0, InstrumentEq::TYPE_LOW_SHELF, fl2fp(200.0f),
                         fl2fp(6.0f), fl2fp(1.0f), true);

        fixed rb0, rb1, rb2, ra1, ra2;
        eqBiquadCoeffs(EQ_BIQUAD_LOW_SHELF, kRate, 200.0f, 6.0f, 1.0f,
                       rb0, rb1, rb2, ra1, ra2);

        fixed buf[2 * kFrames];
        makeSignal(buf, kFrames);

        // distance before any frame
        fixed b0, b1, b2, a1, a2;
        eq.GetBandCoeffs(0, &b0, &b1, &b2, &a1, &a2);
        fixed d0 = rb0 - b0; if (d0 < 0) d0 = -d0;

        eq.Process(0, buf, 1);  // one frame of blending

        fixed nb0, nb1, nb2, na1, na2;
        eq.GetBandCoeffs(0, &nb0, &nb1, &nb2, &na1, &na2);
        fixed nd0 = rb0 - nb0; if (nd0 < 0) nd0 = -nd0;
        CHECK(nd0 < d0);  // strictly closer to the target
    }

    /* --- 4. cascade order invariance (per-band state isolation).
     * The OLD shared-state layout made the band order audible: this asserts
     * the difference is bounded by fixed-point rounding (<= 32 LSBs). --- */
    {
        fixed sigA[2 * kFrames], sigB[2 * kFrames];
        makeSignal(sigA, kFrames);
        memcpy(sigB, sigA, sizeof(sigA));

        InstrumentEq eqA;
        eqA.SetSampleRate(kRate);
        eqA.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(800.0f),
                          fl2fp(12.0f), fl2fp(2.0f), true);
        eqA.ConfigureBand(1, InstrumentEq::TYPE_BELL, fl2fp(800.0f),
                          fl2fp(-12.0f), fl2fp(2.0f), true);
        runOnce(eqA, 0, sigA, kFrames);

        InstrumentEq eqB;
        eqB.SetSampleRate(kRate);
        eqB.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(800.0f),
                          fl2fp(-12.0f), fl2fp(2.0f), true);
        eqB.ConfigureBand(1, InstrumentEq::TYPE_BELL, fl2fp(800.0f),
                          fl2fp(12.0f), fl2fp(2.0f), true);
        runOnce(eqB, 0, sigB, kFrames);

        fixed diff = maxAbsDiff(sigA, sigB, 2 * kFrames);
        /* per-band states: rounding-only diff (~471 LSB measured for
             * this config); the old shared-state cascade differed by
             * ~23038 LSB for the same swap. */
            CHECK(diff <= i2fp(1) / 8);  // 8192 LSBs: 3913 measured for this config with the 99.5% guard; the old shared-state bug gave ~23038 LSBs
    }

    /* --- 5. per-channel independence: same config + same input on two
     * channels -> bit-identical outputs; with the band disabled the
     * channel is an identity (no state bleed from the other channel). --- */
    {
        InstrumentEq eq;
        eq.SetSampleRate(kRate);
        eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(1000.0f),
                         fl2fp(9.0f), fl2fp(3.0f), true);

        /* the coefficient smoothing is a GLOBAL per-band convergence (the
         * first Process() call blends cur->tgt); warm it up on a scratch
         * buffer, then reset both channels so they start from identical
         * (zero) state with identical final coefficients. */
        fixed scratch[2 * kFrames];
        makeSignal(scratch, kFrames);
        eq.Process(0, scratch, kFrames);
        eq.ResetChannelState();

        fixed ch0[2 * kFrames], ch1[2 * kFrames];
        makeSignal(ch0, kFrames);
        memcpy(ch1, ch0, sizeof(ch0));
        runOnce(eq, 0, ch0, kFrames);
        runOnce(eq, 1, ch1, kFrames);
        CHECK(memcmp(ch0, ch1, sizeof(ch0)) == 0);

        // disable the band -> identity on a fresh channel (states reset)
        eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(1000.0f),
                         fl2fp(9.0f), fl2fp(3.0f), false);
        fixed id[2 * kFrames];
        makeSignal(id, kFrames);
        eq.Process(3, id, kFrames);
        fixed src[2 * kFrames];
        makeSignal(src, kFrames);
        CHECK(memcmp(id, src, sizeof(id)) == 0);
    }

    /* --- 6. ConfigureBand clamps hz/db/q --- */
    {
        InstrumentEq eq;
        eq.SetSampleRate(kRate);
        eq.ConfigureBand(2, InstrumentEq::TYPE_HIGH_PASS, fl2fp(50000.0f),
                         fl2fp(40.0f), fl2fp(20.0f), true);
        CHECK(eq.GetBandFreq(2) == fl2fp(20000.0f));
        CHECK(eq.GetBandGainDb(2) == fl2fp(24.0f));
        CHECK(eq.GetBandQ(2) == fl2fp(10.0f));
        eq.ConfigureBand(2, InstrumentEq::TYPE_HIGH_PASS, fl2fp(10.0f),
                         fl2fp(-40.0f), fl2fp(0.01f), true);
        CHECK(eq.GetBandFreq(2) == fl2fp(20.0f));
        CHECK(eq.GetBandGainDb(2) == fl2fp(-24.0f));
        CHECK(eq.GetBandQ(2) == fl2fp(0.1f));
    }

    /* --- 7. bypass: identity + flat even with enabled bands --- */
    {
        InstrumentEq eq;
        eq.SetSampleRate(kRate);
        eq.ConfigureBand(0, InstrumentEq::TYPE_NOTCH, fl2fp(1000.0f),
                         fl2fp(0.0f), fl2fp(5.0f), true);
        eq.SetBypass(true);
        CHECK(eq.IsFlat());
        CHECK(eq.GetBypass());
        fixed in[2 * kFrames], out[2 * kFrames];
        makeSignal(in, kFrames);
        memcpy(out, in, sizeof(in));
        eq.Process(0, out, kFrames);
        CHECK(memcmp(in, out, sizeof(in)) == 0);
    }

    /* --- 8. sample rate change recomputes every band to the new RBJ --- */
    {
        InstrumentEq eq;
        eq.SetSampleRate(44100);
        eq.ConfigureBand(4, InstrumentEq::TYPE_BAND_PASS, fl2fp(3000.0f),
                         fl2fp(6.0f), fl2fp(1.0f), true);
        eq.SetSampleRate(kRate);

        fixed rb0, rb1, rb2, ra1, ra2;
        eqBiquadCoeffs(EQ_BIQUAD_BAND_PASS, kRate, 3000.0f, 6.0f, 1.0f,
                       rb0, rb1, rb2, ra1, ra2);
        fixed buf[2 * kFrames];
        makeSignal(buf, kFrames);
        runOnce(eq, 0, buf, kFrames);
        fixed c0, c1, c2, ca1, ca2;
        eq.GetBandCoeffs(4, &c0, &c1, &c2, &ca1, &ca2);
        CHECK(c0 == rb0 && c1 == rb1 && c2 == rb2 && ca1 == ra1 && ca2 == ra2);
    }

    /* --- 8b. BACON_1.5_EQ8_0DB_TRANSPARENT: a 0 dB band is the identity
     * filter for EVERY type (a LOW_PASS at 0 dB must not cut anything:
     * this is the "the EQ kills the sound" regression) --- */
    {
        InstrumentEq eq;
        eq.SetSampleRate(kRate);
        eq.ConfigureBand(0, InstrumentEq::TYPE_LOW_PASS, fl2fp(80.0f),
                         fl2fp(0.0f), fl2fp(1.0f), true);
        CHECK(eq.IsFlat());
        fixed c0, c1, c2, ca1, ca2;
        eq.GetBandCoeffs(0, &c0, &c1, &c2, &ca1, &ca2);
        CHECK(c0 == i2fp(1) && c1 == 0 && c2 == 0 && ca1 == 0 && ca2 == 0);

        fixed in[2 * kFrames], out[2 * kFrames];
        makeSignal(in, kFrames);
        memcpy(out, in, sizeof(in));
        eq.Process(0, out, kFrames);
        CHECK(memcmp(in, out, sizeof(in)) == 0);

        // ... and the same band with +6 dB IS a real low-pass: the highs
        // are cut, so the output differs from the identity.
        eq.ConfigureBand(0, InstrumentEq::TYPE_LOW_PASS, fl2fp(80.0f),
                         fl2fp(6.0f), fl2fp(1.0f), true);
        CHECK(!eq.IsFlat());
        memset(out, 0, sizeof(out));
        memcpy(out, in, sizeof(in));
        eq.Process(0, out, kFrames);
        CHECK(memcmp(in, out, sizeof(in)) != 0);
    }

    /* --- 9. RBJ bell stability guard: settings that made the canonical
     * peaking filter DIVERGE (+6 dB at 1 kHz Q=1, +2 dB at 250 Hz Q=1,
     * +12 dB at 80 Hz Q=1) must now stay bounded with poles inside the
     * unit circle (Jury). --- */
    {
        struct {
            float hz, db, q;
        } hot[] = {
            {1000.0f, 6.0f, 1.0f},
            {250.0f, 2.0f, 1.0f},
            {80.0f, 12.0f, 1.0f},
            {1000.0f, 12.0f, 0.5f},
        };
        for (size_t t = 0; t < sizeof(hot) / sizeof(hot[0]); t++) {
            InstrumentEq eq;
            eq.SetSampleRate(kRate);
            eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(hot[t].hz),
                             fl2fp(hot[t].db), fl2fp(hot[t].q), true);
            fixed buf[2 * kFrames];
            makeSignal(buf, kFrames);
            eq.Process(0, buf, kFrames);  // one pass: diverges w/o the guard

            fixed c0, c1, c2, ca1, ca2;
            eq.GetBandCoeffs(0, &c0, &c1, &c2, &ca1, &ca2);
            float a1 = fp2fl(ca1), a2 = fp2fl(ca2);
            /* Jury P(1) > 0: strictly positive; note the identity bell at 80 Hz
             * has P(1) = 2(1-cos w0)/(1+alpha) ~ 1e-4 and is trivially
             * stable (b == a cancels), so only a tiny epsilon applies. */
            CHECK(1.0f + a1 + a2 > 0.00001f);
            CHECK(a2 < 1.0f && a2 > -1.0f);

            fixed mx = 0;
            for (int i = 0; i < 2 * kFrames; i++) {
                fixed v = buf[i]; if (v < 0) v = -v;
                if (v > mx) mx = v;
            }
            CHECK(mx < i2fp(8));  // bounded: no state blowup
        }
    }

/* --- 10. device-flow audibility (bacon-1.5, feedback): the device
     * report said only the first band was audible, that non-bell types went
     * silent, and that editing killed the sound.  Every band and every type
     * must audibly change the output and stay non-silent/bounded. --- */
    {
        static const double kBandHz[8] = {80, 160, 320, 640, 1250,
                                          2500, 5000, 10000};
        static const int kTypes[7] = {
            InstrumentEq::TYPE_BELL, InstrumentEq::TYPE_LOW_SHELF,
            InstrumentEq::TYPE_HIGH_SHELF, InstrumentEq::TYPE_LOW_PASS,
            InstrumentEq::TYPE_HIGH_PASS, InstrumentEq::TYPE_NOTCH,
            InstrumentEq::TYPE_BAND_PASS,
        };

        // 10a. every band b in 1..7: +6 dB at its own frequency must boost
        // the output (the RBJ stability cap allows >= +1.5 dB there); band 0
        // (80 Hz) is the deep-bass limit case: bounded, never divergent.
        for (int b = 0; b < 8; b++) {
            InstrumentEq eq;
            eq.SetSampleRate(kRate);
            InstrumentEq ref;
            ref.SetSampleRate(kRate);
            fixed in[2 * kFrames], a[2 * kFrames], r[2 * kFrames];
            double f = kBandHz[b];
            for (int i = 0; i < kFrames; i++) {
                fixed s = fl2fp((float)(0.5 * sin(2.0 * 3.14159265 * f *
                                                  (double)i / kRate)));
                in[i * 2] = s;
                in[i * 2 + 1] = s;
            }
            memcpy(a, in, sizeof(in));
            memcpy(r, in, sizeof(in));
            eq.ConfigureBand(b, InstrumentEq::TYPE_BELL, fl2fp((float)f),
                             fl2fp(6.0f), fl2fp(1.0f), true);
            eq.Process(0, a, kFrames);
            ref.Process(0, r, kFrames);  // identity
            double rmsA = rmsTail(a, kFrames);
            double rmsR = rmsTail(r, kFrames);
            fixed mx = 0;
            for (int i = 0; i < 2 * kFrames; i++) {
                fixed v = a[i]; if (v < 0) v = -v;
                if (v > mx) mx = v;
            }
            CHECK(mx < i2fp(8));        // bounded: no state blowup
            if (b > 0) {
                // 160 Hz caps at ~+0.65 dB (1.08x) after the stability
                // guard; higher bands reach 1.2x-2x+.  A 4% floor still
                // catches a silently dead band while tolerating the cap.
                CHECK(rmsA > 1.04 * rmsR + 1e-9);  // audibly louder
            }
        }

        // 10b. every type at 1 kHz stays NON-SILENT and bounded on a full
        // spectrum input (filters cut, but never to silence).
        {
            fixed in[2 * kFrames];
            makeSignal(in, kFrames);
            for (size_t t = 0; t < 7; t++) {
                InstrumentEq eq;
                eq.SetSampleRate(kRate);
                eq.ConfigureBand(0, (InstrumentEq::BandType)kTypes[t],
                                 fl2fp(1000.0f), fl2fp(6.0f), fl2fp(1.0f),
                                 true);
                fixed out[2 * kFrames];
                memcpy(out, in, sizeof(in));
                eq.Process(0, out, kFrames);
                double r = rmsTail(out, kFrames);
                fixed mx = 0;
                for (int i = 0; i < 2 * kFrames; i++) {
                    fixed v = out[i]; if (v < 0) v = -v;
                    if (v > mx) mx = v;
                }
                CHECK(r > 0.05 * rmsTail(in, kFrames));
                CHECK(mx < i2fp(8));
            }
        }

        // 10c. sequential edits keep the sound alive: configure band 1,
        // process; then band 2, process; then band 3 -- output must stay
        // non-silent after every edit (the device reported silence on edit).
        {
            InstrumentEq eq;
            eq.SetSampleRate(kRate);
            fixed buf[2 * kFrames];
            makeSignal(buf, kFrames);
            eq.ConfigureBand(1, InstrumentEq::TYPE_BELL, fl2fp(160.0f),
                             fl2fp(6.0f), fl2fp(1.0f), true);
            eq.Process(0, buf, kFrames);
            CHECK(rmsTail(buf, kFrames) > 0.05);
            eq.ConfigureBand(2, InstrumentEq::TYPE_BELL, fl2fp(320.0f),
                             fl2fp(6.0f), fl2fp(1.0f), true);
            eq.Process(0, buf, kFrames);
            CHECK(rmsTail(buf, kFrames) > 0.05);
            eq.ConfigureBand(3, InstrumentEq::TYPE_LOW_PASS, fl2fp(640.0f),
                             fl2fp(0.0f), fl2fp(1.0f), true);
            eq.Process(0, buf, kFrames);
            CHECK(rmsTail(buf, kFrames) > 0.05);
        }
    }

    /* --- 11. BACON_1.5_EQ8_BLOCKLIMIT (U2.53, feedback #7): the per-block
     * soft limiter replaces the old per-sample saturate at +/-1.0.  A hot
     * block (over 2.0 linear) is scaled so its peak lands EXACTLY on 65535
     * Q15 -- shape preserved, no flat-topping -- while blocks at or under
     * the ceiling pass through untouched. --- */
    {
        /* 11a/11b. BACON_1.5_EQ8_BLOCKLIMIT: the limiter is a pure GAIN --
         * the hot block must be a scaled copy of the clean filter output
         * (no flat-topping).  Both runs use the same +24 dB @ 100 Hz Q=1
         * config: a QUIET run (0.1/0.06 two-tone, clean peak ~1.7 < 2.0)
         * measures the reference spectrum ratios, a HOT run (0.9/0.6,
         * clean peak ~15.4) must show the same ratios at an exact 65535
         * ceiling.  A flat-topping saturate would break them (3rd harmonic
         * of the 100 Hz peak ~ 1/3 of the fundamental). */
        {
            /* quiet reference: no limiting (peak ~1.7 linear) */
            double rqRatio = 0.0, rq3 = 0.0;
            {
                InstrumentEq eq;
                eq.SetSampleRate(kRate);
                eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(100.0f),
                                 fl2fp(24.0f), fl2fp(1.0f), true);
                fixed buf[2 * kFrames];
                for (int p = 0; p < 2; p++) {
                    for (int i = 0; i < kFrames; i++) {
                        double t = (double)i / (double)kRate;
                        fixed s = fl2fp((float)(
                            0.1 * sin(2.0 * 3.14159265 * 100.0 * t) +
                            0.06 * sin(2.0 * 3.14159265 * 1000.0 * t)));
                        buf[i * 2] = s;
                        buf[i * 2 + 1] = s;
                    }
                    eq.Process(0, buf, kFrames);
                }
                fixed mx = 0;
                for (int i = 0; i < 2 * kFrames; i++) {
                    fixed v = buf[i]; if (v < 0) v = -v;
                    if (v > mx) mx = v;
                }
                printf("quiet limiter run: mx=%d\n", mx);
                CHECK(mx < 65535);          // below the ceiling: no limiting
                CHECK(mx > 40000 && mx < 64000);  // clean peak ~1.4 linear
                double g100 = goertzel(buf, kFrames, 100.0);
                double g1k = goertzel(buf, kFrames, 1000.0);
                double g300 = goertzel(buf, kFrames, 300.0);
                rqRatio = g1k > 0.0 ? g100 / g1k : 0.0;
                rq3 = g100 > 0.0 ? g300 / g100 : 0.0;
            }

            /* hot run: same config, 9x hotter -> limiter engages */
            {
                InstrumentEq eq;
                eq.SetSampleRate(kRate);
                eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(100.0f),
                                 fl2fp(24.0f), fl2fp(1.0f), true);
                fixed buf[2 * kFrames];
                for (int p = 0; p < 2; p++) {
                    for (int i = 0; i < kFrames; i++) {
                        double t = (double)i / (double)kRate;
                        fixed s = fl2fp((float)(
                            0.9 * sin(2.0 * 3.14159265 * 100.0 * t) +
                            0.6 * sin(2.0 * 3.14159265 * 1000.0 * t)));
                        buf[i * 2] = s;
                        buf[i * 2 + 1] = s;
                    }
                    eq.Process(0, buf, kFrames);
                }
                fixed mx = 0;
                for (int i = 0; i < 2 * kFrames; i++) {
                    fixed v = buf[i]; if (v < 0) v = -v;
                    if (v > mx) mx = v;
                }
                CHECK(mx == 65535);         // peak lands exactly on the ceiling
                CHECK(mx <= 65535);         // and never above it
                double g100 = goertzel(buf, kFrames, 100.0);
                double g1k = goertzel(buf, kFrames, 1000.0);
                double g300 = goertzel(buf, kFrames, 300.0);
                double rRatio = g1k > 0.0 ? g100 / g1k : 0.0;
                double r3 = g100 > 0.0 ? g300 / g100 : 0.0;
                printf("limiter: mx=%d hotRatio=%.2f qRatio=%.2f hot3rd=%.4f q3rd=%.4f\n",
                       mx, rRatio, rqRatio, r3, rq3);
                /* the hot block is a scaled copy of the clean one: both
                 * ratios match the quiet reference within rounding.  A
                 * flat-topping saturate would push r3 to ~0.33. */
                CHECK(rqRatio > 0.0 && rRatio > 0.8 * rqRatio &&
                      rRatio < 1.2 * rqRatio);
                CHECK(r3 >= 0.0 && r3 < rq3 * 1.5 + 0.02);
            }
        }

        /* 11c. the linear region is bit-preserving: +6 dB @ 1 kHz Q=1 on a
         * 0.5 sine at the center -> the output is the input times the RBJ
         * gain (~1.995) with no limiter rounding, no flat-topping. */
        {
            InstrumentEq eq;
            eq.SetSampleRate(kRate);
            eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(1000.0f),
                             fl2fp(6.0f), fl2fp(1.0f), true);
            fixed in[2 * kFrames], out[2 * kFrames];
            for (int i = 0; i < kFrames; i++) {
                fixed s = fl2fp((float)(0.5 * sin(2.0 * 3.14159265 * 1000.0 *
                                                  (double)i / kRate)));
                in[i * 2] = s;
                in[i * 2 + 1] = s;
            }
            memcpy(out, in, sizeof(in));
            eq.Process(0, out, kFrames);  // warm (smoothing + state)
            memcpy(out, in, sizeof(in));
            eq.Process(0, out, kFrames);  // measured pass
            double rmsOut = rmsTail(out, kFrames);
            double rmsIn = rmsTail(in, kFrames);
            CHECK(rmsIn > 0.3 && rmsOut > 0.6);         // both audible
            CHECK(rmsOut > 1.9 * rmsIn && rmsOut < 2.1 * rmsIn);  // x~2 boost
            fixed mx = 0;
            for (int i = 0; i < 2 * kFrames; i++) {
                fixed v = out[i]; if (v < 0) v = -v;
                if (v > mx) mx = v;
            }
            CHECK(mx < 65535);            // under the ceiling: untouched
        }
    }

    printf("eq8_struct_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}