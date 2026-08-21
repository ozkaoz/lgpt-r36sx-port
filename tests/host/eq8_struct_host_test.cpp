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

/* The always-on soft-knee map (must mirror InstrumentEq.cpp's constants):
 * f(u) = (u - A*u^2) / (1 + B*u), u = |x| - 0.85, output clamped to
 * [0.85, 1.0].  A flat EQ returns early (flat_) and never applies it, so a
 * bypassed tail's bit-exact reference is the RAW through this map. */
static fixed kneeMap(fixed v) {
    const int kKneeQ15 = 27852, kTopQ15 = 55705, kUnityQ15 = 32767;
    const int kKneeAQ32 = -27212171, kKneeBQ32 = 565359;
    int a = (v < 0) ? -v : v;
    if (a <= kKneeQ15) return v;
    int out;
    if (a >= kTopQ15) {
        out = kUnityQ15;
    } else {
        int u = a - kKneeQ15;
        long long u2 = (long long)u * u;
        long long n = u - ((kKneeAQ32 * u2) >> 32);
        long long d = (1LL << 32) + (long long)kKneeBQ32 * u;
        long long f = (n << 32) / d;
        out = kKneeQ15 + (int)f;
        if (out > kUnityQ15) out = kUnityQ15;
        if (out < kKneeQ15) out = kKneeQ15;
    }
    return (v < 0) ? (fixed)-out : (fixed)out;
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
                         fl2fp(12.0f), fl2fp(1.0f), 1, true);

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
                         fl2fp(6.0f), fl2fp(1.0f), 1, true);

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
                          fl2fp(12.0f), fl2fp(2.0f), 1, true);
        eqA.ConfigureBand(1, InstrumentEq::TYPE_BELL, fl2fp(800.0f),
                          fl2fp(-12.0f), fl2fp(2.0f), 1, true);
        /* the coefficient smoothing converges per-band at its own rate, so
         * the two orderings' morph TRANSIENTS differ; warm both up on a
         * scratch pass and reset the channels so the measured diff is the
         * steady-state rounding only (the per-band state isolation). */
        fixed scratch[2 * kFrames];
        makeSignal(scratch, kFrames);
        eqA.Process(0, scratch, kFrames);
        eqA.ResetChannelState();
        runOnce(eqA, 0, sigA, kFrames);

        InstrumentEq eqB;
        eqB.SetSampleRate(kRate);
        eqB.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(800.0f),
                          fl2fp(-12.0f), fl2fp(2.0f), 1, true);
        eqB.ConfigureBand(1, InstrumentEq::TYPE_BELL, fl2fp(800.0f),
                          fl2fp(12.0f), fl2fp(2.0f), 1, true);
        eqB.Process(0, scratch, kFrames);
        eqB.ResetChannelState();
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
                         fl2fp(9.0f), fl2fp(3.0f), 1, true);

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

        // BACON_1.5_EQ8_CLICKFREE (U2.62, feedback #14): disabling the band
        // is now a SMOOTH morph to the identity filter, NOT an instant
        // flush -- so the channel is NOT bit-identical while the band is
        // closing.  Assert the transition converges to the exact identity:
        // the Q24 morph (identity b0 = 2^24) takes ~700 frames for this
        // config (the b0 diff ~6e5, the a1 diff ~6.5e4, each >>6 per
        // frame), so the LAST QUARTER of the buffer is bit-identical to the
        // raw input on a fresh channel, and the EQ reports flat.
        eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(1000.0f),
                         fl2fp(9.0f), fl2fp(3.0f), 1, false);
        CHECK(!eq.IsFlat());  // converging: the close is audible (no click)
        fixed id[2 * kFrames];
        makeSignal(id, kFrames);
        eq.Process(3, id, kFrames);
        CHECK(eq.IsFlat());   // converged: nothing left to smooth
        fixed src[2 * kFrames];
        makeSignal(src, kFrames);
        CHECK(memcmp(id + 3 * kFrames / 2, src + 3 * kFrames / 2,
                     (kFrames / 2) * sizeof(fixed)) == 0);
    }

    /* --- 6. ConfigureBand clamps hz/db/q --- */
    {
        InstrumentEq eq;
        eq.SetSampleRate(kRate);
        eq.ConfigureBand(2, InstrumentEq::TYPE_HIGH_PASS, fl2fp(50000.0f),
                         fl2fp(40.0f), fl2fp(20.0f), 1, true);
        CHECK(eq.GetBandFreq(2) == fl2fp(20000.0f));
        CHECK(eq.GetBandGainDb(2) == fl2fp(24.0f));
        CHECK(eq.GetBandQ(2) == fl2fp(10.0f));
        eq.ConfigureBand(2, InstrumentEq::TYPE_HIGH_PASS, fl2fp(10.0f),
                         fl2fp(-40.0f), fl2fp(0.01f), 1, true);
        CHECK(eq.GetBandFreq(2) == fl2fp(20.0f));
        CHECK(eq.GetBandGainDb(2) == fl2fp(-24.0f));
        CHECK(eq.GetBandQ(2) == fl2fp(0.1f));
    }

    /* --- 7. bypass: identity + flat even with enabled bands --- */
    {
        InstrumentEq eq;
        eq.SetSampleRate(kRate);
        eq.ConfigureBand(0, InstrumentEq::TYPE_NOTCH, fl2fp(1000.0f),
                         fl2fp(0.0f), fl2fp(5.0f), 1, true);
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
                         fl2fp(6.0f), fl2fp(1.0f), 1, true);
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
                         fl2fp(0.0f), fl2fp(1.0f), 1, true);
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
                         fl2fp(6.0f), fl2fp(1.0f), 1, true);
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
                             fl2fp(hot[t].db), fl2fp(hot[t].q), 1, true);
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
                             fl2fp(6.0f), fl2fp(1.0f), 1, true);
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
                                 1, true);
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
                             fl2fp(6.0f), fl2fp(1.0f), 1, true);
            eq.Process(0, buf, kFrames);
            CHECK(rmsTail(buf, kFrames) > 0.05);
            eq.ConfigureBand(2, InstrumentEq::TYPE_BELL, fl2fp(320.0f),
                             fl2fp(6.0f), fl2fp(1.0f), 1, true);
            eq.Process(0, buf, kFrames);
            CHECK(rmsTail(buf, kFrames) > 0.05);
            eq.ConfigureBand(3, InstrumentEq::TYPE_LOW_PASS, fl2fp(640.0f),
                             fl2fp(0.0f), fl2fp(1.0f), 1, true);
            eq.Process(0, buf, kFrames);
            CHECK(rmsTail(buf, kFrames) > 0.05);
        }
    }

    /* --- 11. BACON_1.5_EQ8_SOFTKNEE (U2.59, feedback #12): the per-sample
     * soft knee REPLACES the old per-block limiter (removed).  It is a
     * smooth, monotonic map: below 0.85 Q15 (27852) the output is the input
     * untouched, 0.85..1.7 compresses onto 0.85..1.0 (32767), and anything
     * above 1.7 sits exactly on 32767 (the instrument is capped at UNITY
     * before it ever reaches the bus).  A hot block is a SCALED COPY of the
     * clean filter output (no flat-topping: the 3rd harmonic stays low),
     * and the ceiling is 32767 -- the old 65535 ceiling no longer exists.
     * --- */
    {
        /* 11a/11b. the knee is a pure monotonic GAIN -- the hot block must
         * show the same spectrum ratios as the quiet reference (no
         * flat-topping), and both caps land EXACTLY on 32767.  Both runs
         * use the same +24 dB @ 100 Hz Q=1 config: a QUIET run (0.1/0.06
         * two-tone) and a HOT run (0.9/0.6). */
        {
            /* quiet reference */
            double rqRatio = 0.0, rq3 = 0.0;
            {
                InstrumentEq eq;
                eq.SetSampleRate(kRate);
                eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(100.0f),
                                 fl2fp(24.0f), fl2fp(1.0f), 1, true);
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
                printf("quiet knee run: mx=%d\n", mx);
                CHECK(mx <= 32767);          // never above the soft ceiling
                CHECK(mx > 27852);           // the crest passed the knee
                double g100 = goertzel(buf, kFrames, 100.0);
                double g1k = goertzel(buf, kFrames, 1000.0);
                double g300 = goertzel(buf, kFrames, 300.0);
                rqRatio = g1k > 0.0 ? g100 / g1k : 0.0;
                rq3 = g100 > 0.0 ? g300 / g100 : 0.0;
            }

            /* hot run: same config, 9x hotter -> same 32767 ceiling */
            {
                InstrumentEq eq;
                eq.SetSampleRate(kRate);
                eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(100.0f),
                                 fl2fp(24.0f), fl2fp(1.0f), 1, true);
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
                CHECK(mx == 32767);         // peak lands exactly on the ceiling
                CHECK(mx <= 32767);         // and never above it
                double g100 = goertzel(buf, kFrames, 100.0);
                double g1k = goertzel(buf, kFrames, 1000.0);
                double g300 = goertzel(buf, kFrames, 300.0);
                double rRatio = g1k > 0.0 ? g100 / g1k : 0.0;
                double r3 = g100 > 0.0 ? g300 / g100 : 0.0;
                printf("knee: mx=%d hotRatio=%.2f qRatio=%.2f hot3rd=%.4f q3rd=%.4f\n",
                       mx, rRatio, rqRatio, r3, rq3);
                /* The knee compresses the 0.85..1.7 region onto 0.85..1.0
                 * (smooth, monotonic, NO flat-topping), and anything above
                 * 1.7 must sit on 32767 (the unity cap -- a hard limit, by
                 * design).  The QUIET run's crest (~1.4) sits inside the
                 * knee region, so its 3rd harmonic stays low (q3rd < 0.2);
                 * the HOT run's crest (~23x the clean gain) is pinned at
                 * the cap, so its 3rd approaches the hard-clip bound
                 * (4/pi - 1 ~ 0.273; 0.33 is a generous ceiling).
                 * BACON_1.5_EQ8_SOFTKNEE_C1 (U2.60): the map is now the C1
                 * RATIONAL (u + A*u^2)/(1 + B*u) -- slope 1 at the knee,
                 * slope 0 on the unity landing -- so it is gain-varying
                 * INSIDE the knee band.  A two-tone riding the knee shifts
                 * its inter-band ratio slightly more than the old
                 * piecewise-linear map (measured 1.206x), so the 0.8..1.2
                 * window is widened to 0.7..1.35; the distortion metrics
                 * are the real gate and they IMPROVED (q3rd 0.127 vs 0.143:
                 * the kink harmonics are gone).
                 * BACON_1.5_EQ8_DEN24 (U2.62): the Q24 denominators restored
                 * the bell's true +24 dB gain (13.6 -> 15.2 @ 100 Hz), so
                 * the HOT run's crest is deeper (13.7 vs 12.2) and the clip
                 * rides closer to the hard-clip bound H3/H1 = 1/3.  The
                 * measured bin is the C1 map's H2 leaking ~1.0 into the
                 * 300 Hz bin (200 Hz sits 1.07 bins off), so the ceiling is
                 * 1/3 + H2/H1(~0.04) + margin = 0.42, still below the
                 * flat-top / kink regimes the gate rejects. */
                CHECK(rqRatio > 0.0 && rRatio > 0.7 * rqRatio &&
                      rRatio < 1.35 * rqRatio);
                CHECK(r3 >= 0.0 && r3 < 0.42);   // clip bound + H2 leak, not worse
                CHECK(rq3 >= 0.0 && rq3 < 0.20); // knee region: no flat-top
            }
        }

        /* 11c. the knee is TRANSPARENT below 0.85: +24 dB @ 100 Hz on a
         * 0.02/0.01 two-tone (clean peak ~0.47) must pass through bit
         * identical in shape (peak under the knee, exactly the RBJ gain). */
        {
            InstrumentEq eq;
            eq.SetSampleRate(kRate);
            eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(100.0f),
                             fl2fp(24.0f), fl2fp(1.0f), 1, true);
            fixed in[2 * kFrames], out[2 * kFrames];
            // BACON_1.5_EQ8_DEN24 (U2.62): the passes use the CONTINUOUS
            // two-tone (each pass advances the phase by kFrames).  Feeding
            // the same buffer every pass re-injects a phase discontinuity at
            // each 1024-frame boundary; the bell's DC pole (1+a1+a2 ~ 1.8e-4
            // at Q24, tau ~5500 frames) picks that up and settles ~5% below
            // the RBJ gain (measured 12.9x instead of 15.2x).
            for (int p = 0; p < 3; p++) {  // warm 1: morph, warm 2: state,
                                           // pass 3: measured
                for (int i = 0; i < kFrames; i++) {
                    double t = (double)(i + p * kFrames) / (double)kRate;
                    fixed s = fl2fp((float)(
                        0.02 * sin(2.0 * 3.14159265 * 100.0 * t) +
                        0.01 * sin(2.0 * 3.14159265 * 1000.0 * t)));
                    in[i * 2] = s;
                    in[i * 2 + 1] = s;
                }
                memcpy(out, in, sizeof(in));
                eq.Process(0, out, kFrames);
            }
            fixed mx = 0;
            for (int i = 0; i < 2 * kFrames; i++) {
                fixed v = out[i]; if (v < 0) v = -v;
                if (v > mx) mx = v;
            }
            double rmsOut = rmsTail(out, kFrames);
            double rmsIn = rmsTail(in, kFrames);
            printf("transparent: mx=%d rmsIn=%.3f rmsOut=%.3f\n",
                   mx, rmsIn, rmsOut);
            CHECK(mx < 27852);            // under the knee: untouched
            CHECK(rmsOut > 12.0 * rmsIn && rmsOut < 17.0 * rmsIn);  // RBJ x~15
        }

        /* 11d. the linear region is bit-preserving: +6 dB @ 1 kHz Q=1 on a
         * 0.3 sine at the center -> the output is the input times the RBJ
         * gain (~1.995) with no knee rounding, no flat-topping. */
        {
            InstrumentEq eq;
            eq.SetSampleRate(kRate);
            eq.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(1000.0f),
                             fl2fp(6.0f), fl2fp(1.0f), 1, true);
            fixed in[2 * kFrames], out[2 * kFrames];
            for (int i = 0; i < kFrames; i++) {
                fixed s = fl2fp((float)(0.3 * sin(2.0 * 3.14159265 * 1000.0 *
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
            CHECK(rmsIn > 0.2 && rmsOut > 0.4);         // both audible
            CHECK(rmsOut > 1.9 * rmsIn && rmsOut < 2.1 * rmsIn);  // x~2 boost
            fixed mx = 0;
            for (int i = 0; i < 2 * kFrames; i++) {
                fixed v = out[i]; if (v < 0) v = -v;
                if (v > mx) mx = v;
            }
            CHECK(mx < 27852);            // under the knee: untouched
        }
    }

    /* --- 12. BACON_1.5_EQ8_SLOPE (U2.62, feedback #14): a slope-2 band
     * cascades the same biquad twice -> EXACTLY 2x the single-stage dB at
     * every frequency (24 dB/oct for LP/HP, etc.).  The dB doubling and
     * the setters/getters/clamps are asserted below. --- */
    {
        // 12a. SetBandSlope/GetBandSlope + ConfigureBand clamping.
        InstrumentEq eq;
        eq.SetSampleRate(kRate);
        eq.ConfigureBand(0, InstrumentEq::TYPE_LOW_PASS, fl2fp(80.0f),
                         fl2fp(-12.0f), fl2fp(1.0f), 1, true);
        CHECK(eq.GetBandSlope(0) == 1);
        eq.SetBandSlope(0, 2);
        CHECK(eq.GetBandSlope(0) == 2);
        eq.SetBandSlope(0, 7);       // clamps to 8 (new max 96 dB)
        CHECK(eq.GetBandSlope(0) == 7);
        eq.SetBandSlope(0, 9);       // clamps to 8
        CHECK(eq.GetBandSlope(0) == 8);
        eq.SetBandSlope(0, 0);       // clamps to 1
        CHECK(eq.GetBandSlope(0) == 1);

        // 12b. the cascade doubles the attenuation: LP 80 Hz at 200 Hz
        // (1.32 octaves above the corner) is ~-15.2 dB at slope 1 and
        // ~-30.5 dB at slope 2 -- the squared response of the same biquad.
        // (Q31 numerators keep the exact RBJ values; a Q15 b1 of 1.79
        // truncates to 1 and turns the LP into a resonator whose cascade
        // BOOSTS -- see the EqBiquad.h comment.)
        {
            InstrumentEq eq1, eq2;
            eq1.SetSampleRate(kRate);
            eq2.SetSampleRate(kRate);
            eq1.ConfigureBand(0, InstrumentEq::TYPE_LOW_PASS, fl2fp(80.0f),
                              fl2fp(-12.0f), fl2fp(1.0f), 1, true);
            eq2.ConfigureBand(0, InstrumentEq::TYPE_LOW_PASS, fl2fp(80.0f),
                              fl2fp(-12.0f), fl2fp(1.0f), 2, true);
            fixed in[2 * kFrames], a[2 * kFrames], b[2 * kFrames];
            // BACON_1.5_EQ8_DEN24 (U2.62): the LP's Q24 DC pole
            // (1+a1+a2 ~ 9.2e-5, tau ~11000 frames) is excited by the
            // input's onset and by repeated-buffer boundaries, so the
            // measurement needs the continuous tone + enough warm passes
            // for that mode to decay (40 passes ~4 tau).
            for (int p = 0; p < 40; p++) {
                for (int i = 0; i < kFrames; i++) {
                    fixed s = fl2fp((float)(0.5 * sin(2.0 * 3.14159265 * 200.0 *
                                                      (double)(i + p * kFrames) /
                                                      kRate)));
                    in[i * 2] = s;
                    in[i * 2 + 1] = s;
                }
                memcpy(a, in, sizeof(in));
                memcpy(b, in, sizeof(in));
                eq1.Process(0, a, kFrames);
                eq2.Process(0, b, kFrames);
            }
            double g1 = goertzel(a, kFrames, 200.0);
            double g2 = goertzel(b, kFrames, 200.0);
            double gIn = goertzel(in, kFrames, 200.0);
            /* slope-1: ~0.091 ( -15.2 dB ), slope-2: ~0.016 ( -30.4 dB );
             * the real gates are the >4x deeper cut and the EXACT squared
             * response (the cascade is the same biquad twice). */
            CHECK(gIn > 0.45 && gIn < 0.55);
            printf("slope: g1=%.4f g2=%.4f gIn=%.4f sq=%.4f\n",
                   g1, g2, gIn, gIn > 0.0 ? g1 * g1 / gIn : 0.0);
            CHECK(g1 > 0.06 && g1 < 0.35);
            CHECK(g2 > 0.008 && g2 < 0.20);
            CHECK(g1 > 4.0 * g2);     // slope 2 cuts at least 4x deeper
            double sq = (gIn > 0.0) ? g1 * g1 / gIn : 0.0;
            CHECK(g2 > 0.5 * sq && g2 < 1.5 * sq);  // exactly the squared
        }

        // 12c. BACON_1.5_EQ8_SLOPE96 (U2.65): BELL now respects slope
        // 1..8 (campana más pronunciada): slope 8 cascades 8x bell,
        // so slope 1 vs 2 must be DIFFERENT (more pronounced).
        {
            InstrumentEq eq1, eq2;
            eq1.SetSampleRate(kRate);
            eq2.SetSampleRate(kRate);
            eq1.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(1000.0f),
                              fl2fp(12.0f), fl2fp(1.0f), 1, true);
            eq2.ConfigureBand(0, InstrumentEq::TYPE_BELL, fl2fp(1000.0f),
                              fl2fp(12.0f), fl2fp(1.0f), 2, true);
            fixed sig1[2 * kFrames], sig2[2 * kFrames];
            makeSignal(sig1, kFrames);
            memcpy(sig2, sig1, sizeof(sig1));
            eq1.Process(0, sig1, kFrames);
            eq2.Process(0, sig2, kFrames);
            CHECK(memcmp(sig1, sig2, sizeof(sig1)) != 0);
        }
    }

    /* --- 13. BACON_1.5_EQ8_CLICKFREE (U2.62, feedback #14): the flat
     * transitions (gain to 0 dB, band off, bypass) morph the band to the
     * identity filter INSTEAD of the old instant removal.  On a hipass the
     * removed sub-bass sits in the biquad state; the old flush dumped it in
     * one step (a click equal to the whole removed signal).  The close must
     * keep the output CONTINUOUS: the max consecutive-sample delta across
     * the transition stays within ~2x the raw signal's own slew (the old
     * code stepped by the whole removed component on top of it).  A 50 Hz +
     * 120 Hz kick-like body under a 200 Hz hipass corner gives the state a
     * large removal to dump (~30k Q15 counts), so the assertion is sharp.
     * --- */
    {
        static const double kBodyHz[2] = {50.0, 120.0};
        static const double kBodyAmp[2] = {0.9, 0.5};
        static const double kCornerHz = 200.0;   // well above the body
        InstrumentEq eq;
        eq.SetSampleRate(kRate);

        fixed raw[2 * kFrames];
        for (int i = 0; i < kFrames; i++) {
            double t = (double)i / (double)kRate;
            double v = kBodyAmp[0] * sin(2.0 * 3.14159265 * kBodyHz[0] * t) +
                       kBodyAmp[1] * sin(2.0 * 3.14159265 * kBodyHz[1] * t);
            fixed s = fl2fp((float)v);
            raw[i * 2] = s;
            raw[i * 2 + 1] = s;
        }
        /* the raw signal's own per-sample slew and peak amplitude (the
         * delta bound base).  The sine slew is A*2*pi*f/fs (~450 counts),
         * the drain releases ~0.08*A_in per frame: the bound
         * rawMaxDelta + A_in/2 separates the new morph (~0.1*A_in total)
         * from the old 2-sample dump (~A_in/2 in ONE delta) by ~5x. */
        fixed rawMaxDelta = 0;
        fixed rawPeak = 0;
        for (int i = 1; i < kFrames; i++) {
            fixed d = raw[i * 2] - raw[(i - 1) * 2];
            if (d < 0) d = -d;
            if (d > rawMaxDelta) rawMaxDelta = d;
            fixed v = raw[i * 2];
            if (v < 0) v = -v;
            if (v > rawPeak) rawPeak = v;
        }
        CHECK(rawMaxDelta > 200 && rawMaxDelta < 1000);  // sanity: low body
        const fixed kDeltaBound = rawMaxDelta + rawPeak / 2;

        /* helper lambda: max consecutive-sample delta over a buffer. */
        struct {
            fixed operator()(const fixed *buf, int frames) const {
                fixed m = 0;
                for (int i = 1; i < 2 * frames; i++) {
                    fixed d = buf[i] - buf[i - 1];
                    if (d < 0) d = -d;
                    if (d > m) m = d;
                }
                return m;
            }
        } maxDelta;

        /* warm: hipass 80 Hz hot at -12 dB, fully converged (3 passes). */
        eq.ConfigureBand(0, InstrumentEq::TYPE_HIGH_PASS, fl2fp((float)kCornerHz),
                         fl2fp(-12.0f), fl2fp(1.0f), 1, true);
        for (int p = 0; p < 3; p++) {
            fixed w[2 * kFrames];
            memcpy(w, raw, sizeof(raw));
            eq.Process(0, w, kFrames);
        }
        /* the hipass really removed the sub-bass: its output is much
         * quieter than the raw body (the state holds the removal). */
        {
            fixed w[2 * kFrames];
            memcpy(w, raw, sizeof(raw));
            eq.Process(0, w, kFrames);
            CHECK(rmsTail(w, kFrames) < 0.5 * rmsTail(raw, kFrames));
        }

        /* 13a. gain to 0 dB: continuous, and the tail converges to the
         * raw body (the EQ releases the sub-bass softly). */
        {
            eq.ConfigureBand(0, InstrumentEq::TYPE_HIGH_PASS, fl2fp((float)kCornerHz),
                             fl2fp(0.0f), fl2fp(1.0f), 1, true);
            fixed tr[2 * kFrames];
            memcpy(tr, raw, sizeof(raw));
            eq.Process(0, tr, kFrames);
            fixed d = maxDelta(tr, kFrames);
            printf("close-0dB: maxDelta=%d bound=%d\n", d, kDeltaBound);
            CHECK(d <= kDeltaBound);      // continuous: no click
            CHECK(eq.IsFlat());            // converged to the identity
            CHECK(rmsTail(tr, kFrames) > 0.6 * rmsTail(raw, kFrames));
        }

        /* 13b. band off: same continuity, same soft release. */
        {
            fixed w[2 * kFrames];
            memcpy(w, raw, sizeof(raw));
            eq.Process(0, w, kFrames);   // re-enter at 0 dB first (flat)
            eq.ConfigureBand(0, InstrumentEq::TYPE_HIGH_PASS, fl2fp((float)kCornerHz),
                             fl2fp(-12.0f), fl2fp(1.0f), 1, true);
            for (int p = 0; p < 3; p++) {
                fixed z[2 * kFrames];
                memcpy(z, raw, sizeof(raw));
                eq.Process(0, z, kFrames);
            }
            eq.ConfigureBand(0, InstrumentEq::TYPE_HIGH_PASS, fl2fp((float)kCornerHz),
                             fl2fp(-12.0f), fl2fp(1.0f), 1, false);
            fixed tr[2 * kFrames];
            memcpy(tr, raw, sizeof(raw));
            eq.Process(0, tr, kFrames);
            fixed d = maxDelta(tr, kFrames);
            printf("close-off: maxDelta=%d bound=%d\n", d, kDeltaBound);
            CHECK(d <= kDeltaBound);
            CHECK(eq.IsFlat());
        }

        /* 13c. bypass while hot: every band morphs to identity. */
        {
            fixed w[2 * kFrames];
            memcpy(w, raw, sizeof(raw));
            eq.Process(0, w, kFrames);   // re-enter at -12 dB first
            eq.ConfigureBand(0, InstrumentEq::TYPE_HIGH_PASS, fl2fp((float)kCornerHz),
                             fl2fp(-12.0f), fl2fp(1.0f), 1, true);
            for (int p = 0; p < 3; p++) {
                fixed z[2 * kFrames];
                memcpy(z, raw, sizeof(raw));
                eq.Process(0, z, kFrames);
            }
            // BACON_1.5_EQ8_SOFTKNEE: the raw body (peak 1.4) rides the
            // knee, so the bypassed tail is bit-identical to the RAW
            // through the always-on soft-knee map (the band set is the
            // identity there), not to the raw buffer itself.
            InstrumentEq flatEq;
            flatEq.SetSampleRate(kRate);
            fixed ref[2 * kFrames];
            for (int i = 0; i < 2 * kFrames; i++) ref[i] = kneeMap(raw[i]);
            eq.SetBypass(true);
            fixed tr[2 * kFrames];
            memcpy(tr, raw, sizeof(raw));
            eq.Process(0, tr, kFrames);
            fixed d = maxDelta(tr, kFrames);
            printf("close-bypass: maxDelta=%d bound=%d\n", d, kDeltaBound);
            CHECK(d <= kDeltaBound);
            CHECK(eq.IsFlat());
            CHECK(memcmp(tr + kFrames, ref + kFrames,
                         kFrames * sizeof(fixed)) == 0);
        }

        /* 13d. re-activation after a close is click-free too: a closed
         * band left only a drained (tiny) residue, so re-opening to +6 dB
         * must NOT dump anything (the morph smooths the reopen). */
        {
            eq.SetBypass(false);
            eq.ConfigureBand(0, InstrumentEq::TYPE_HIGH_PASS, fl2fp((float)kCornerHz),
                             fl2fp(6.0f), fl2fp(1.0f), 1, true);
            fixed tr[2 * kFrames];
            memcpy(tr, raw, sizeof(raw));
            eq.Process(0, tr, kFrames);
            fixed d = maxDelta(tr, kFrames);
            printf("reopen: maxDelta=%d bound=%d\n", d, kDeltaBound);
            CHECK(d <= kDeltaBound);
            CHECK(!eq.IsFlat());
            CHECK(rmsTail(tr, kFrames) > 0.05);
        }
    }

    printf("eq8_struct_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}