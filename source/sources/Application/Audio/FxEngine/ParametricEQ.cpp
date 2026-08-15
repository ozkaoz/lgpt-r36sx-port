#include "ParametricEQ.h"
#include <math.h>
#include "Application/Audio/EqBiquad.h"

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
#define FX_EQ_MIN_DB (-24.0f)
#define FX_EQ_MAX_DB (24.0f)
// Shelf slope when SetBandQ is used on LOW/HIGH (mapped, clamped).
#define FX_EQ_SHELF_Q_MIN 0.5f
#define FX_EQ_SHELF_Q_MAX 2.0f

// FXP_MASTER_EQ8 (bacon-1.5, item 2): persisted BandType -> EqBiquad type.
static int mapBandType(ParametricEQ::BandType t) {
    switch (t) {
    case ParametricEQ::BT_LOW_SHELF:  return EQ_BIQUAD_LOW_SHELF;
    case ParametricEQ::BT_HIGH_SHELF: return EQ_BIQUAD_HIGH_SHELF;
    case ParametricEQ::BT_LOW_PASS:   return EQ_BIQUAD_LOW_PASS;
    case ParametricEQ::BT_HIGH_PASS:  return EQ_BIQUAD_HIGH_PASS;
    case ParametricEQ::BT_BAND_PASS:  return EQ_BIQUAD_BAND_PASS;
    case ParametricEQ::BT_NOTCH:      return EQ_BIQUAD_NOTCH;
    case ParametricEQ::BT_BELL:
    default:                          return EQ_BIQUAD_BELL;
    }
}

ParametricEQ::ParametricEQ()
    : rate_(44100), bypass_(false), extBypass_(true), bypassMix_(0),
      extBypassMix_(0), rtViolations_(0) {
    for (int b = 0; b < kNumBands; b++) {
        Biquad &bg = bands_[b];
        bg.b0 = i2fp(1); bg.b1 = bg.b2 = bg.a1 = bg.a2 = 0;
        bg.s1[0] = bg.s1[1] = bg.s2[0] = bg.s2[1] = 0;
        bg.mixCur = 0;
        bg.enabled = false;
        bg.hz = 0; bg.db = 0; bg.q = fl2fp(1.0f);
        bg.type = BT_BELL;
    }
    // Golden defaults: low shelf 100 Hz, bell 1 kHz, high shelf 10 kHz, all
    // 0 dB.  EXT bands (FX_PAGE_EQ_EXT) default to a 2/4/8/16 kHz ladder,
    // all 0 dB, so the EXT page is silent until edited (and the legacy
    // AllParamsAtLegacyDefault bypass stays true).
    bands_[LOW].type = BT_LOW_SHELF;
    bands_[HIGH].type = BT_HIGH_SHELF;
    SetBandFreq(LOW, fl2fp(100.0f));
    SetBandFreq(MID, fl2fp(1000.0f));
    SetBandFreq(HIGH, fl2fp(10000.0f));
    SetBandFreq(BAND3, fl2fp(2000.0f));
    SetBandFreq(BAND4, fl2fp(4000.0f));
    SetBandFreq(BAND5, fl2fp(8000.0f));
    SetBandFreq(BAND6, fl2fp(16000.0f));
    SetBandFreq(BAND7, fl2fp(16000.0f));
    SetBandQ(LOW, fl2fp(1.0f));
    SetBandQ(MID, fl2fp(1.0f));
    SetBandQ(HIGH, fl2fp(1.0f));
    for (int b = 0; b < kNumBands; b++) recompute(b);
}

void ParametricEQ::Reset() {
    for (int b = 0; b < kNumBands; b++) {
        Biquad &bg = bands_[b];
        bg.s1[0] = bg.s1[1] = bg.s2[0] = bg.s2[1] = 0;
        bg.mixCur = bg.enabled ? i2fp(1) : 0;
    }
    bypassMix_ = bypass_ ? i2fp(1) : 0;
    extBypassMix_ = extBypass_ ? i2fp(1) : 0;
}

void ParametricEQ::SetSampleRate(int rate) {
    if (rate < 8000 || rate > 96000) {
        ++rtViolations_;
        return;
    }
    rate_ = rate;
    for (int b = 0; b < kNumBands; b++) recompute(b);
}

void ParametricEQ::SetBandFreq(int band, fixed hz) {
    if (band < LOW || band >= kNumBands) {
        ++rtViolations_;
        return;
    }
    float f = fp2fl(hz);
    if (f < 20.0f) f = 20.0f;
    if (f > 20000.0f) f = 20000.0f;
    bands_[band].hz = fl2fp(f);
    recompute(band);
}

void ParametricEQ::SetBandGainDb(int band, fixed db) {
    if (band < LOW || band >= kNumBands) {
        ++rtViolations_;
        return;
    }
    float g = fp2fl(db);
    if (g < FX_EQ_MIN_DB) g = FX_EQ_MIN_DB;
    if (g > FX_EQ_MAX_DB) g = FX_EQ_MAX_DB;
    bands_[band].db = fl2fp(g);
    refreshBandEnabled(band);
    recompute(band);
}

void ParametricEQ::SetBandQ(int band, fixed q) {
    if (band < LOW || band >= kNumBands) {
        ++rtViolations_;
        return;
    }
    float qf = fp2fl(q);
    if (qf < 0.1f) qf = 0.1f;
    if (qf > 10.0f) qf = 10.0f;
    bands_[band].q = fl2fp(qf);
    recompute(band);
}

void ParametricEQ::SetBandType(int band, BandType type) {
    if (band < LOW || band >= kNumBands) {
        ++rtViolations_;
        return;
    }
    if (type < BT_LOW_SHELF || type >= BT_TYPECOUNT) type = BT_BELL;
    bands_[band].type = type;
    refreshBandEnabled(band);
    recompute(band);
}

void ParametricEQ::SetBandEnabled(int band, bool on) {
    // EXT bands are enabled implicitly from their settings; only the golden
    // base bands carry an explicit enable.
    if (band < LOW || band >= BAND3) {
        ++rtViolations_;
        return;
    }
    bands_[band].enabled = on;
}

void ParametricEQ::refreshBandEnabled(int band) {
    if (band < BAND3) return; // base bands: explicit enable only
    Biquad &bg = bands_[band];
    bg.enabled = (bg.db != 0) || (bg.type != BT_BELL);
}

fixed ParametricEQ::saturate(fixed x) {
    if (x > i2fp(1)) return i2fp(1);
    if (x < -i2fp(1)) return -i2fp(1);
    return x;
}

// Audio EQ Cookbook (RBJ) biquads via the shared EqBiquad primitive.
// Computed at control-rate (parameter changes only).
void ParametricEQ::recompute(int band) {
    Biquad &bg = bands_[band];
    float f0 = fp2fl(bg.hz);
    if (f0 <= 0.0f) f0 = 1.0f;
    fixed b0, b1, b2, a1, a2;
    eqBiquadCoeffs(mapBandType(bg.type), rate_, f0, fp2fl(bg.db),
                   fp2fl(bg.q), b0, b1, b2, a1, a2);
    bg.b0 = b0; bg.b1 = b1; bg.b2 = b2; bg.a1 = a1; bg.a2 = a2;
}

void ParametricEQ::Process(const fixed *in, fixed *out, int frames) {
    if (frames <= 0 || !in || !out) {
        ++rtViolations_;
        return;
    }

    // Global bypass crossfade (smoothed).
    fixed targetBypass = bypass_ ? 0 : i2fp(1);
    FX_EQ_SNAP(FX_EQ_MIX_SMOOTH, bypassMix_, targetBypass);
    // EXT chain crossfade (smoothed).
    fixed targetExt = extBypass_ ? 0 : i2fp(1);
    FX_EQ_SNAP(FX_EQ_MIX_SMOOTH, extBypassMix_, targetExt);
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

        // Base chain: golden bands LOW/MID/HIGH.
        for (int b = LOW; b < BAND3; b++) {
            Biquad &bg = bands_[b];
            if (bg.mixCur == 0) continue; // fully off: leave dry
            // Transposed direct form II (L).
            fixed tL = fp_mul(bg.b0, xL) + bg.s1[0];
            bg.s1[0] = fp_mul(bg.b1, xL) - fp_mul(bg.a1, tL) + bg.s2[0];
            bg.s2[0] = fp_mul(bg.b2, xL) - fp_mul(bg.a2, tL);
            fixed tR = fp_mul(bg.b0, xR) + bg.s1[1];
            bg.s1[1] = fp_mul(bg.b1, xR) - fp_mul(bg.a1, tR) + bg.s2[1];
            bg.s2[1] = fp_mul(bg.b2, xR) - fp_mul(bg.a2, tR);
            yL = xL + fp_mul(tL - xL, bg.mixCur);
            yR = xR + fp_mul(tR - xR, bg.mixCur);
            xL = yL; // cascade: next band sees this band's output
            xR = yR;
        }
        // Snapshot the base chain for the EXT bypass crossfade.
        fixed yBaseL = yL;
        fixed yBaseR = yR;

        // EXT chain: bands BAND3..BAND7 (FX_PAGE_EQ_EXT).
        for (int b = BAND3; b < kNumBands; b++) {
            Biquad &bg = bands_[b];
            if (bg.mixCur == 0) continue;
            // Transposed direct form II (L).
            fixed tL = fp_mul(bg.b0, xL) + bg.s1[0];
            bg.s1[0] = fp_mul(bg.b1, xL) - fp_mul(bg.a1, tL) + bg.s2[0];
            bg.s2[0] = fp_mul(bg.b2, xL) - fp_mul(bg.a2, tL);
            fixed tR = fp_mul(bg.b0, xR) + bg.s1[1];
            bg.s1[1] = fp_mul(bg.b1, xR) - fp_mul(bg.a1, tR) + bg.s2[1];
            bg.s2[1] = fp_mul(bg.b2, xR) - fp_mul(bg.a2, tR);
            yL = xL + fp_mul(tL - xL, bg.mixCur);
            yR = xR + fp_mul(tR - xR, bg.mixCur);
            xL = yL;
            xR = yR;
        }

        // EXT bypass: crossfade between base chain and full 8-band chain.
        if (extBypassMix_ != i2fp(1)) {
            yL = yBaseL + fp_mul(yL - yBaseL, extBypassMix_);
            yR = yBaseR + fp_mul(yR - yBaseR, extBypassMix_);
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