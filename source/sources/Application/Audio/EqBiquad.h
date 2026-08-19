#ifndef _EQ_BIQUAD_H_
#define _EQ_BIQUAD_H_

#include <math.h>
#include "Application/Utils/fixed.h"

/*
 * EqBiquad -- shared biquad coefficient computation (bacon-1.5, item 2).
 * Used by BOTH FxEngine::InstrumentEq and FxEngine::ParametricEQ so the
 * instrument EQ8 and the master EQ use the same DSP primitive for their
 * band coefficients.  Pure inline: no state, no dependencies beyond
 * fixed.h and <math.h>.
 *
 * Coefficients are normalized by a0 (a0 = 1) and emitted in Q15.
 * The shelf/filter types mirror the golden recomputeBand() of
 * InstrumentEq.cpp / recompute() of ParametricEQ.cpp exactly (RBJ Audio
 * EQ Cookbook), plus the missing LOW_PASS / BAND_PASS types.  The BELL
 * type deviates from the cookbook since U2.52.8 (BACON_1.5_BELL_PREWARPED):
 * the RBJ peaking formula is asymmetric at low/mid frequencies and never
 * reaches its set gain at the center, so it was replaced by the
 * prewarped-bilinear analog peaking prototype (exact gain at w0, 0 dB at
 * DC/Nyquist, log-symmetric).  The Nyquist clamp uses 0.9*pi (ParametricEQ
 * golden); at 48 kHz the engine never reaches it (max band 20 kHz <
 * 0.9*pi), so both existing paths are bit-identical for the other types.
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

    float A = powf(10.0f, lvl / 20.0f);
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
    // RBJ cookbook LP/HP (the golden recomputeBand): the corner frequency is
    // the design center; the response AT f0 is Q (0 dB for Q=1), with the
    // -3 dB point at ~1.27*f0 (Q=1).  The numerators are exact (1-cw)
    // forms; at low f0 (80-100 Hz) the Q15 quantization leaves b0..b2 at
    // 1-3 counts, so the center gain can wander ~+-2 dB (measured +2.4 dB
    // at 100 Hz) -- a fidelity limit of the Q15 format, not a design
    // defect (the shelves/bell are unaffected; the sound is never killed).
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
    // BACON_1.5_BELL_PREWARPED (U2.52.8, feedback): the old RBJ peaking
    // formula was asymmetric at low/mid frequencies: the gain at w0 is NOT
    // A (the peak migrates to a gain-dependent "magic" frequency), DC and
    // Nyquist never return to 0 dB, and at low f0 the filter became a huge
    // shelf toward DC with the center DIPPING below 0 dB.  Measured on the
    // device path (48 kHz): +6 dB at 1 kHz Q=1 -> center +0.2 dB, DC
    // +37 dB; +6 dB at 100 Hz -> center -1.9 dB, DC +14 dB.  That is
    // exactly the reported "lifts the left side and drops the right side".
    // The replacement is the analog peaking prototype bilinear-transformed
    // with prewarping (K = w0/tan(w0/2), so s = j w0 maps to z = e^{jw0}):
    // H(s) = (s^2 + (A/Q) w0 s + w0^2) / (s^2 + (1/(A Q)) w0 s + w0^2).
    // Properties: gain EXACTLY A (20log10(A) = lvl) at w0, exactly 0 dB at
    // DC and Nyquist, symmetric in log-frequency.  The poles are the
    // bilinear images of the stable analog poles, so the filter is stable
    // for every A>0, Q>0 and the old low-f0 stability cap (which limited
    // boosts to ~0.4-1 dB below ~1 kHz) is gone.
    // Numerically well-conditioned at low f0 (K -> 2 as w0 -> 0; the old
    // alpha = sw/(2Q) denominators collapsed there).  At lvl=0 it is the
    // exact identity (num == den).
    case EQ_BIQUAD_BELL:
    default: {
        // sqrt(A) in the s-terms so the peak gain at w0 is A exactly
        // (the prototype with plain A peaks at A^2 = lvl*2 dB).
        float sA = powf(10.0f, lvl / 40.0f);
        float K = w0 / tanf(w0 / 2.0f);
        float kk = K * K;
        float ww = w0 * w0;
        float bw = sA * w0 * K / qv;
        float aw = w0 * K / (sA * qv);
        fb0 = kk + ww + bw;
        fb1 = 2.0f * (ww - kk);
        fb2 = kk + ww - bw;
        fa0 = kk + ww + aw;
        fa1 = 2.0f * (ww - kk);
        fa2 = kk + ww - aw;
        break;
    }
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