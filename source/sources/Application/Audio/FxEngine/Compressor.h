#ifndef _FX_COMPRESSOR_H_
#define _FX_COMPRESSOR_H_

#include "Application/Utils/fixed.h"

/*
 * FxEngine::Compressor -- stereo feed-forward compressor + soft limiter
 * (PLAN_FX_REDESIGN_ES.md, Fase 3).
 *
 * Feed-forward detector (peak, stereo link option), threshold/ratio/soft knee,
 * attack/release one-pole envelope, makeup gain, gain-reduction meter, and a
 * final cubic soft clip (limiter) so the master never exceeds +/-1.
 *
 * Real-time contract: the compression curve (level -> gain) is precomputed
 * into a static table at control-rate (parameter changes only); Process() is
 * pure table lookup + fixed point: zero malloc/new/free, zero syscalls.
 * rtViolations_ must stay 0.
 */

namespace FxEngine {

class Compressor {
public:
    static const int kTableBits = 12;         // 4096 entries
    static const int kTableSize = 1 << kTableBits;

    Compressor();
    void Reset();

    void SetSampleRate(int rate);
    void SetThresholdDb(fixed db);   // -60..0
    void SetRatio(fixed ratio);      // 1..20 (>= 1)
    void SetKneeDb(fixed db);        // 0..12
    void SetAttackMs(fixed ms);      // 0.1..500
    void SetReleaseMs(fixed ms);     // 1..2000
    void SetMakeupDb(fixed db);      // 0..24
    void SetStereoLink(bool on);     // link L/R detector
    void SetBypass(bool on);         // crossfade to dry (smoothed)
    void SetSoftClip(bool on);       // final cubic limiter

    // Interleaved stereo in/out (in == out allowed, in-place).
    void Process(const fixed *in, fixed *out, int frames);

    // Gain reduction in dB (Q15), smoothed for the UI meter.
    fixed GetGainReductionDb() const { return grMeter_; }
    unsigned long GetRtViolations() const { return rtViolations_; }
    static unsigned long StaticMemoryBytes() {
        return (unsigned long)kTableSize * sizeof(fixed) * 2; // gain + gr tables
    }

    // Control-rate readbacks for the UI (PLAN_FX_REDESIGN_ES.md, Fase 4.3).
    fixed GetThresholdDb() const { return threshDb_; }
    fixed GetRatio() const { return ratio_; }
    fixed GetKneeDb() const { return kneeDb_; }
    fixed GetMakeupDb() const { return makeupDb_; }
    float GetAttackMs() const { return attackMs_; }
    float GetReleaseMs() const { return releaseMs_; }
    bool GetStereoLink() const { return stereoLink_; }
    bool GetBypass() const { return bypass_; }
    bool GetSoftClip() const { return softClip_; }

private:
    static fixed saturate(fixed x);
    static fixed cubicClip(fixed x);
    void recomputeTable();
    void recomputeSmoothing();

    fixed gainTable_[kTableSize];  // level (Q15 [0,1]) -> gain (Q15)
    fixed grTable_[kTableSize];    // level -> gain reduction (Q15 dB)

    fixed level_[2];               // smoothed detector envelope (Q15 [0,1])
    fixed attK_, relK_;            // one-pole coefficients (Q15)
    fixed grMeter_;                // smoothed GR for UI (Q15 dB)
    fixed threshDb_, ratio_, kneeDb_, makeupDb_;  // Q15 dB / linear
    float attackMs_, releaseMs_;   // control-rate only
    int rate_;
    bool stereoLink_;
    bool bypass_;
    bool softClip_;
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif
