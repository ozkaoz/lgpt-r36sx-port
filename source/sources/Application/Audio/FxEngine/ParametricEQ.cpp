#include "ParametricEQ.h"
#include <math.h>

namespace FxEngine {

// One-pole crossfade per sample (Q15) for click-free band enable / global
// bypass changes.  ~1000 samples to fully open/close.
#define FX_EQ_MIX_SMOOTH fl2fp(0.001f)
// Q15 one-pole step rounds to 0 when |target-cur| < ~1024, stalling short of
// the target.  Snap to the target when the step would truncate to zero so the
// band fully engages (avoids the Q15 truncation stall).
#define FX_EQ_SNAP(smoother, cur, target) \
    do { \
        fixed __st = fp_mul(smoother, (target) - (cur)); \
        if (__st == 0 && (cur) != (target)) (cur) = (target); \
        else (cur) += __st; \
    } while (0)
// Gain limits (dB).
#define FX_EQ_MIN_DB (-12.0f)
#define FX_EQ_MAX_DB (12.0f)
// Shelf slope when SetBandQ is used on LOW/HIGH (mapped, clamped).
#define FX_EQ_SHELF_Q_MIN 0.5f
#define FX_EQ_SHELF_Q_MAX 2.0f

ParametricEQ::ParametricEQ()
    : rate_(44100), bypass_(false), bypassMix_(0), rtViolations_(0) {
    for (int b = 0; b < kNumBands; b++) {
        Biquad &bg = bands_[b];
        bg.b0 = i2fp(1); bg.b1 = bg.b2 = bg.a1 = bg.a2 = 0;
        bg.s1[0] = bg.s1[1] = bg.s2[0] = bg.s2[1] = 0;
        bg.mixCur = 0;
        bg.enabled = false;
        bg.hz = 0; bg.db = 0; bg.q = fl2fp(1.0f);
    }
    // Defaults: low shelf 100 Hz, bell 1 kHz, high shelf 10 kHz, all 0 dB.
    SetBandFreq(LOW, fl2fp(100.0f));
    SetBandFreq(MID, fl2fp(1000.0f));
    SetBandFreq(HIGH, fl2fp(10000.0f));
    SetBandQ(LOW, fl2fp(1.0f));
    SetBandQ(MID, fl2fp(1.0f));
    SetBandQ(HIGH, fl2fp(1.0f));
    for (int b = 0; b < kNumBands; b++) recompute((Band)b);
}

void ParametricEQ::Reset() {
    for (int b = 0; b < kNumBands; b++) {
        Biquad &bg = bands_[b];
        bg.s1[0] = bg.s1[1] = bg.s2[0] = bg.s2[1] = 0;
        bg.mixCur = bg.enabled ? i2fp(1) : 0;
    }
    bypassMix_ = bypass_ ? i2fp(1) : 0;
}

void ParametricEQ::SetSampleRate(int rate) {
    if (rate < 8000 || rate > 96000) {
        ++rtViolations_;
        return;
    }
    rate_ = rate;
    for (int b = 0; b < kNumBands; b++) recompute((Band)b);
}

void ParametricEQ::SetBandFreq(Band band, fixed hz) {
    if (band < LOW || band > HIGH) {
        ++rtViolations_;
        return;
    }
    float f = fp2fl(hz);
    if (f < 20.0f) f = 20.0f;
    if (f > 20000.0f) f = 20000.0f;
    bands_[band].hz = fl2fp(f);
    recompute(band);
}

void ParametricEQ::SetBandGainDb(Band band, fixed db) {
    if (band < LOW || band > HIGH) {
        ++rtViolations_;
        return;
    }
    float g = fp2fl(db);
    if (g < FX_EQ_MIN_DB) g = FX_EQ_MIN_DB;
    if (g > FX_EQ_MAX_DB) g = FX_EQ_MAX_DB;
    bands_[band].db = fl2fp(g);
    recompute(band);
}

void ParametricEQ::SetBandQ(Band band, fixed q) {
    if (band < LOW || band > HIGH) {
        ++rtViolations_;
        return;
    }
    float qf = fp2fl(q);
    if (qf < 0.1f) qf = 0.1f;
    if (qf > 10.0f) qf = 10.0f;
    bands_[band].q = fl2fp(qf);
    recompute(band);
}

void ParametricEQ::SetBandEnabled(Band band, bool on) {
    if (band < LOW || band > HIGH) {
        ++rtViolations_;
        return;
    }
    bands_[band].enabled = on;
}

void ParametricEQ::SetBypass(bool on) { bypass_ = on; }

fixed ParametricEQ::saturate(fixed x) {
    if (x > i2fp(1)) return i2fp(1);
    if (x < -i2fp(1)) return -i2fp(1);
    return x;
}

// Audio EQ Cookbook (RBJ) biquads.  LOW = low shelf, MID = bell (peak),
// HIGH = high shelf.  Computed at control-rate (parameter changes only).
void ParametricEQ::recompute(Band band) {
    Biquad &bg = bands_[band];
    float f0 = fp2fl(bg.hz);
    float gainDb = fp2fl(bg.db);
    float q = fp2fl(bg.q);

    if (f0 <= 0.0f) f0 = 1.0f;
    float w0 = 2.0f * 3.14159265f * f0 / (float)rate_;
    if (w0 > 3.14159265f * 0.9f) w0 = 3.14159265f * 0.9f;
    float cw = cosf(w0);
    float sw = sinf(w0);
    float A = powf(10.0f, gainDb / 40.0f);      // A = 10^(G/40)
    float sqrtA = sqrtf(A);

    float b0, b1, b2, a0, a1, a2;
    if (band == MID) {
        float alpha = sw / (2.0f * q);
        // b0 = 1 + alpha*A ; b1 = -2*cos ; b2 = 1 - alpha*A
        b0 = 1.0f + alpha * A;
        b1 = -2.0f * cw;
        b2 = 1.0f - alpha * A;
        a0 = 1.0f + alpha / A;
        a1 = -2.0f * cw;
        a2 = 1.0f - alpha / A;
    } else {
        // Shelf slope S from Q (clamped).
        float S = (band == LOW) ? q : q;
        if (S < FX_EQ_SHELF_Q_MIN) S = FX_EQ_SHELF_Q_MIN;
        if (S > FX_EQ_SHELF_Q_MAX) S = FX_EQ_SHELF_Q_MAX;
        float alpha = (sw / 2.0f) * sqrtf((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);
        if (band == LOW) {
            // low shelf
            b0 = A * ((A + 1.0f) - (A - 1.0f) * cw + 2.0f * sqrtA * alpha);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw);
            b2 = A * ((A + 1.0f) - (A - 1.0f) * cw - 2.0f * sqrtA * alpha);
            a0 = (A + 1.0f) + (A - 1.0f) * cw + 2.0f * sqrtA * alpha;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw);
            a2 = (A + 1.0f) + (A - 1.0f) * cw - 2.0f * sqrtA * alpha;
        } else {
            // high shelf
            b0 = A * ((A + 1.0f) + (A - 1.0f) * cw + 2.0f * sqrtA * alpha);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw);
            b2 = A * ((A + 1.0f) + (A - 1.0f) * cw - 2.0f * sqrtA * alpha);
            a0 = (A + 1.0f) - (A - 1.0f) * cw + 2.0f * sqrtA * alpha;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cw);
            a2 = (A + 1.0f) - (A - 1.0f) * cw - 2.0f * sqrtA * alpha;
        }
    }

    // Normalize by a0.
    if (a0 != 0.0f) {
        bg.b0 = fl2fp(b0 / a0);
        bg.b1 = fl2fp(b1 / a0);
        bg.b2 = fl2fp(b2 / a0);
        bg.a1 = fl2fp(a1 / a0);
        bg.a2 = fl2fp(a2 / a0);
    } else {
        bg.b0 = i2fp(1); bg.b1 = bg.b2 = bg.a1 = bg.a2 = 0;
    }
}

void ParametricEQ::Process(const fixed *in, fixed *out, int frames) {
    if (frames <= 0 || !in || !out) {
        ++rtViolations_;
        return;
    }

    // Global bypass crossfade (smoothed).
    fixed targetBypass = bypass_ ? 0 : i2fp(1);
    FX_EQ_SNAP(FX_EQ_MIX_SMOOTH, bypassMix_, targetBypass);
    // Smooth each band's engage mix.
    for (int b = 0; b < kNumBands; b++) {
        Biquad &bg = bands_[b];
        fixed target = bg.enabled ? i2fp(1) : 0;
        FX_EQ_SNAP(FX_EQ_MIX_SMOOTH, bg.mixCur, target);
    }

    int idx = 0;
    for (int i = 0; i < frames; i++) {
        fixed xL = in[idx];
        fixed xR = in[idx + 1];
        fixed yL = xL;
        fixed yR = xR;

        for (int b = 0; b < kNumBands; b++) {
            Biquad &bg = bands_[b];
            if (bg.mixCur == 0) continue; // fully off: leave dry

            // Transposed direct form II (L).
            fixed tL = fp_mul(bg.b0, xL) + bg.s1[0];
            bg.s1[0] = fp_mul(bg.b1, xL) - fp_mul(bg.a1, tL) + bg.s2[0];
            bg.s2[0] = fp_mul(bg.b2, xL) - fp_mul(bg.a2, tL);
            // (R)
            fixed tR = fp_mul(bg.b0, xR) + bg.s1[1];
            bg.s1[1] = fp_mul(bg.b1, xR) - fp_mul(bg.a1, tR) + bg.s2[1];
            bg.s2[1] = fp_mul(bg.b2, xR) - fp_mul(bg.a2, tR);

            yL = xL + fp_mul(tL - xL, bg.mixCur);
            yR = xR + fp_mul(tR - xR, bg.mixCur);
            xL = yL; // cascade: next band sees this band's output
            xR = yR;
        }

        // Global bypass: dry (or whatever) crossfade.
        if (bypassMix_ != i2fp(1)) {
            fixed dryL = in[idx];
            fixed dryR = in[idx + 1];
            yL = dryL + fp_mul(yL - dryL, bypassMix_);
            yR = dryR + fp_mul(yR - dryR, bypassMix_);
        }

        out[idx] = saturate(yL);
        out[idx + 1] = saturate(yR);
        idx += 2;
    }
}

} // namespace FxEngine
