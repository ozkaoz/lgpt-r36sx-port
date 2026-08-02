#ifndef _FX_PARAMETRIC_EQ_H_
#define _FX_PARAMETRIC_EQ_H_

#include "Application/Utils/fixed.h"

/*
 * FxEngine::ParametricEQ -- 3-band master EQ (PLAN_FX_REDESIGN_ES.md, Fase 3).
 *
 * Audio EQ Cookbook biquads (RBJ) in transposed direct form II, Q15 fixed
 * point.  Bands: low shelf / bell (peak) / high shelf.  Per-band Hz, dB, Q
 * (Q maps to the shelf slope for the shelf bands), per-band enable with a
 * smoothed crossfade (click-free) and a global bypass crossfade to dry.
 *
 * Coefficients are computed in the setters (control-rate only, float math);
 * Process() is pure fixed point: zero malloc/new/free, zero syscalls.
 * rtViolations_ counts any would-be violation and must stay 0.
 */

namespace FxEngine {

class ParametricEQ {
public:
    enum Band { LOW = 0, MID = 1, HIGH = 2, kNumBands = 3 };

    ParametricEQ();
    void Reset();

    void SetSampleRate(int rate);
    void SetBandFreq(Band band, fixed hz);    // 20..20000
    void SetBandGainDb(Band band, fixed db);  // -12..+12
    void SetBandQ(Band band, fixed q);        // 0.1..10 (shelf slope for L/H)
    void SetBandEnabled(Band band, bool on);
    void SetBypass(bool on);                  // global crossfade to dry

    // Interleaved stereo in/out (in == out allowed, in-place).
    void Process(const fixed *in, fixed *out, int frames);

    unsigned long GetRtViolations() const { return rtViolations_; }
    static unsigned long StaticMemoryBytes() { return 0; }

private:
    static fixed saturate(fixed x);
    void recompute(Band band);

    struct Biquad {
        fixed b0, b1, b2, a1, a2;  // normalized (a0 = 1)
        fixed s1[2], s2[2];        // transposed DF2 states (L, R)
        fixed mixCur;              // smoothed per-band engage (0..1)
        bool enabled;
        fixed hz, db, q;
    };

    Biquad bands_[kNumBands];
    int rate_;
    bool bypass_;
    fixed bypassMix_;  // smoothed global bypass crossfade
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif
