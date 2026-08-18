#ifndef _EQ_BIQUAD_H_
#define _EQ_BIQUAD_H_

#include "Application/Utils/fixed.h"

/*
 * EqBiquad -- shared RBJ Audio EQ Cookbook coefficient computation
 * (bacon-1.5, item 2).  Used by BOTH FxEngine::InstrumentEq and
 * FxEngine::ParametricEQ so the instrument EQ8 and the master EQ use the
 * same DSP primitive for their band coefficients.  Pure inline: no state,
 * no dependencies beyond fixed.h and <math.h>.
 *
 * Coefficients are normalized by a0 (a0 = 1) and emitted in Q15.
 * The math mirrors the golden recomputeBand() of InstrumentEq.cpp /
 * recompute() of ParametricEQ.cpp exactly (RBJ Audio EQ Cookbook), plus the
 * missing LOW_PASS / BAND_PASS types.  The Nyquist clamp uses 0.9*pi
 * (ParametricEQ golden); at 48 kHz the engine never reaches it (max band
 * 20 kHz < 0.9*pi), so both existing paths are bit-identical.
 */

namespace FxEngine {

enum EqBiquadType {
    EQ_BIQUAD_BELL = 0,
    EQ_BIQUAD_LOW_SHELF,
    EQ_BIQUAD_HIGH_SHELF,
    EQ_BIQUAD_LOW_PASS,
    EQ_BIQUAD_HIGH_PASS,
    EQ_BIQUAD_BAND_PASS,
    EQ_BIQUAD_NOTCH,
    EQ_BIQUAD_TYPECOUNT
};

// Computes normalized (a0 = 1) coefficients for one band.
//   type : EqBiquadType
//   rate : sample rate in Hz
//   f0   : center frequency in Hz
//   lvl  : band gain in dB
//   qv   : Q (0.1 .. 10)
inline void eqBiquadCoeffs(int type, int rate, float f0, float lvl, float qv,
                           fixed &b0, fixed &b1, fixed &b2,
                           fixed &a1, fixed &a2) {
    float w0 = 2.0f * 3.14159265f * f0 / (float)rate;
    if (w0 > 3.14159265f * 0.9f) w0 = 3.14159265f * 0.9f;
    if (w0 < 1e-6f) w0 = 1e-6f;
    float cw = cosf(w0);
    float sw = sinf(w0);

    // RBJ_BELL_STABILITY (bacon-1.5): the RBJ peaking filter is UNSTABLE
    // for boosts at low normalized frequencies (a real pole exits the unit
    // circle: P(1) = 1 + a1 + a2 < 0).  Verified divergent settings include
    // +6 dB at 1 kHz Q=1 and +2 dB at 250 Hz Q=1 (44.1/48 kHz).  The
    // marginal boost is A = sw/(sw - 4*Q*(1-cw)); cap the gain at 90% of it
    // so the filter always stays bounded.  Cuts (A < 1) and shelves are
    // unaffected.
    if (type == EQ_BIQUAD_BELL && lvl > 0.0f) {
        float denom = sw - 4.0f * qv * (1.0f - cw);
        if (denom > 0.0f) {
            float cap = (sw / denom) * 0.9f;
            if (cap < 1.0f) cap = 1.0f;
            float lvlCap = 40.0f * log10f(cap);
            if (lvl > lvlCap) lvl = lvlCap;
        }
    }

    float A = powf(10.0f, lvl / 40.0f);
    float alpha = sw / (2.0f * qv);

    float fb0, fb1, fb2, fa0, fa1, fa2;
    switch (type) {
    case EQ_BIQUAD_LOW_SHELF: {
        float S = qv; if (S < 0.5f) S = 0.5f; if (S > 2.0f) S = 2.0f;
        float sqA = sqrtf(A);
        float ac = (sw / 2.0f) * sqrtf((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);
        fb0 = A * ((A + 1.0f) - (A - 1.0f) * cw + 2.0f * sqA * ac);
        fb1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw);
        fb2 = A * ((A + 1.0f) - (A - 1.0f) * cw - 2.0f * sqA * ac);
        fa0 = (A + 1.0f) + (A - 1.0f) * cw + 2.0f * sqA * ac;
        fa1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw);
        fa2 = (A + 1.0f) + (A - 1.0f) * cw - 2.0f * sqA * ac;
        break;
    }
    case EQ_BIQUAD_HIGH_SHELF: {
        float S = qv; if (S < 0.5f) S = 0.5f; if (S > 2.0f) S = 2.0f;
        float sqA = sqrtf(A);
        float as = (sw / 2.0f) * sqrtf((A + 1.0f / A) * (1.0f / S - 1.0f) + 2.0f);
        fb0 = A * ((A + 1.0f) + (A - 1.0f) * cw + 2.0f * sqA * as);
        fb1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw);
        fb2 = A * ((A + 1.0f) + (A - 1.0f) * cw - 2.0f * sqA * as);
        fa0 = (A + 1.0f) - (A - 1.0f) * cw + 2.0f * sqA * as;
        fa1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cw);
        fa2 = (A + 1.0f) - (A - 1.0f) * cw - 2.0f * sqA * as;
        break;
    }
    case EQ_BIQUAD_LOW_PASS:
        fb0 = (1.0f - cw) / 2.0f;
        fb1 = 1.0f - cw;
        fb2 = fb0;
        fa0 = 1.0f + alpha;
        fa1 = -2.0f * cw;
        fa2 = 1.0f - alpha;
        break;
    case EQ_BIQUAD_HIGH_PASS:
        fb0 = (1.0f + cw) / 2.0f;
        fb1 = -(1.0f + cw);
        fb2 = fb0;
        fa0 = 1.0f + alpha;
        fa1 = -2.0f * cw;
        fa2 = 1.0f - alpha;
        break;
    case EQ_BIQUAD_BAND_PASS:
        // constant 0 dB peak gain
        fb0 = alpha;
        fb1 = 0.0f;
        fb2 = -alpha;
        fa0 = 1.0f + alpha;
        fa1 = -2.0f * cw;
        fa2 = 1.0f - alpha;
        break;
    case EQ_BIQUAD_NOTCH:
        fb0 = 1.0f;
        fb1 = -2.0f * cw;
        fb2 = 1.0f;
        fa0 = 1.0f + alpha;
        fa1 = -2.0f * cw;
        fa2 = 1.0f - alpha;
        break;
    case EQ_BIQUAD_BELL:
    default:
        fb0 = 1.0f + alpha * A;
        fb1 = -2.0f * cw;
        fb2 = 1.0f - alpha * A;
        fa0 = 1.0f + alpha / A;
        fa1 = -2.0f * cw;
        fa2 = 1.0f - alpha;
        break;
    }
    if (fa0 != 0.0f) {
        b0 = fl2fp(fb0 / fa0);
        b1 = fl2fp(fb1 / fa0);
        b2 = fl2fp(fb2 / fa0);
        a1 = fl2fp(fa1 / fa0);
        a2 = fl2fp(fa2 / fa0);
    } else {
        b0 = i2fp(1); b1 = 0; b2 = 0; a1 = 0; a2 = 0;
    }
}

} // namespace FxEngine

#endif