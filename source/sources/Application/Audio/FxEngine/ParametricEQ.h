#ifndef _PARAMETRIC_EQ_H_
#define _PARAMETRIC_EQ_H_

#include "Application/Utils/fixed.h"

/*
 * ParametricEQ -- master parametric equalizer (3 golden bands + 5 EXT bands,
 * bacon-1.5 item 2).  The EXT bands (BAND3..BAND7) back the FX_PAGE_EQ_EXT
 * page: FRQ/GAI/Q/TYPE per band, enabled iff non-neutral
 * (gain != 0 dB or type != BELL), with an independent EXT bypass.
 *
 * RBJ Audio EQ Cookbook biquads in Q15 fixed point (coefficients via the
 * shared EqBiquad primitive, same DSP as InstrumentEq).  Per-band engage
 * mixes are smoothed per sample (click-free enable), the EXT bypass
 * crossfades between the base (LOW/MID/HIGH) chain and the full 8-band
 * chain.
 *
 * Performance contract (R36SX port):
 *   - Process() runs all 8 bands only when needed; bands with mixCur == 0
 *     are skipped per frame.
 *   - Pure fixed point inside Process(): no allocation, no syscalls.
 *   - Coefficients recomputed only on parameter changes (control rate).
 */

namespace FxEngine {

class ParametricEQ {
public:
    // FXP_MASTER_EQ8 (bacon-1.5, item 2): 3 golden bands + 5 EXT bands.
    enum Band {
        LOW = 0,
        MID,
        HIGH,
        BAND3,
        BAND4,
        BAND5,
        BAND6,
        BAND7,
        kNumBands
    };

    enum BandType {
        BT_LOW_SHELF = 0,
        BT_BELL,
        BT_HIGH_SHELF,
        BT_LOW_PASS,
        BT_HIGH_PASS,
        BT_BAND_PASS,
        BT_NOTCH,
        BT_TYPECOUNT
    };

    ParametricEQ();
    void Reset();
    void SetSampleRate(int rate);

    void SetBypass(bool on) { bypass_ = on; }
    void SetExtBypass(bool on) { extBypass_ = on; }

    void SetBandEnabled(int band, bool on);
    void SetBandType(int band, BandType type);
    void SetBandFreq(int band, fixed hz);
    void SetBandGainDb(int band, fixed db);
    void SetBandQ(int band, fixed q);

    bool GetBypass() const { return bypass_; }
    bool GetExtBypass() const { return extBypass_; }
    bool GetBandEnabled(int band) const { return (band >= LOW && band < kNumBands) ? bands_[band].enabled : false; }
    BandType GetBandType(int band) const { return (band >= LOW && band < kNumBands) ? bands_[band].type : BT_BELL; }
    fixed GetBandFreq(int band) const { return (band >= LOW && band < kNumBands) ? bands_[band].hz : 0; }
    fixed GetBandGainDb(int band) const { return (band >= LOW && band < kNumBands) ? bands_[band].db : 0; }
    fixed GetBandQ(int band) const { return (band >= LOW && band < kNumBands) ? bands_[band].q : fl2fp(1.0f); }

    // Interleaved stereo in-place processing (no channel dimension: master).
    void Process(const fixed *in, fixed *out, int frames);

    static unsigned long StaticMemoryBytes() {
        return sizeof(Biquad) * kNumBands + 64;
    }

    unsigned long GetRtViolations() const { return rtViolations_; }

private:
    struct Biquad {
        fixed b0, b1, b2, a1, a2;   // normalized (a0 = 1)
        fixed s1[2], s2[2];         // transposed DF2 state (L/R)
        fixed mixCur;               // engage crossfade 0..1
        fixed hz, db, q;
        bool enabled;
        BandType type;
    };

    static fixed saturate(fixed x);
    void recompute(int band);
    // EXT bands (>= BAND3) are enabled iff non-neutral; base bands use the
    // explicit SetBandEnabled flag.
    void refreshBandEnabled(int band);

    Biquad bands_[kNumBands];

    int rate_;
    bool bypass_;   // global EQ off (dry)
    bool extBypass_;  // EXT chain off (base 3-band only)
    fixed bypassMix_;   // global bypass crossfade
    fixed extBypassMix_; // EXT bypass crossfade
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif