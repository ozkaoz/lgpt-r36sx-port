#ifndef _INSTRUMENT_EQ_H_
#define _INSTRUMENT_EQ_H_

#include "Application/Utils/fixed.h"

/*
 * InstrumentEq -- 8-band graphic EQ applied per sample instrument BEFORE the
 * FX sends (dry instrument output).  RBJ Audio EQ Cookbook biquads in Q15
 * fixed point (same kernels as FxEngine::ParametricEQ).
 *
 * BACON_1.5_EQ8_STRUCTURAL (bacon-1.5, item 4): every band owns its OWN
 * biquad state per channel (state_[channel][band]).  The previous layout
 * (one ChanState per channel shared by all 8 bands) chained every band on
 * the same s1/s2 registers: band N+1 was computed from band N's state
 * values, so the filter states of band N were destroyed the moment band N+1
 * ran.  With per-band states each band filters independently and the
 * composite response matches the sum of the individual band responses.
 *
 * Performance contract (R36SX port, no lag):
 *   - All bands flat/disabled or global bypass -> Process() returns with one
 *     branch, zero per-sample work.  Instruments that never use the EQ pay
 *     nothing.
 *   - Only enabled bands run; disabled bands skipped per frame.
 *   - Pure fixed point inside Process(): no allocation, no syscalls.
 *   - Coefficients recomputed only when a parameter changes (control rate,
 *     float math, same as ParametricEQ).  ConfigureBand() is atomic: a
 *     fingerprint per band caches (type, freq, gain, q, rate) and the RBJ
 *     recompute only runs when one of them actually changed.
 *   - BACON_1.5_EQ8_STRUCTURAL: coefficient smoothing.  After an edit the
 *     per-frame path blends the active coefficients toward the new target
 *     with a fixed-point exponential (no clicks, no zipper noise).  The
 *     smoothing runs only while a band is converging.
 */

namespace FxEngine {

class InstrumentEq {
public:
    enum BandType {
        TYPE_BELL = 0,
        TYPE_LOW_SHELF,
        TYPE_HIGH_SHELF,
        TYPE_LOW_PASS,
        TYPE_HIGH_PASS,
        TYPE_NOTCH,
        // FXP_INSTRUMENT_EQ_BP (bacon-1.5, item 2): band-pass added at the
        // end so persisted eqType_ values 0..5 are unchanged.
        TYPE_BAND_PASS,
        kTypeCount
    };
    static const int kNumBands = 8;
    static const int kMaxChannels = 8;  // sample instrument polyphony

    static float DefaultBandHz(int band);  // octave ladder

    InstrumentEq();
    void Reset();
    void ResetChannelState();

    void SetSampleRate(int rate);

    // BACON_1.5_EQ8_STRUCTURAL: single atomic entry point for every edit.
    // Recomputes the band's RBJ coefficients only when (type, hz, db, q)
    // changed since the last call; toggling `enabled` never touches the
    // float math.
    void ConfigureBand(int band, BandType type, fixed hz, fixed db, fixed q,
                       bool enabled);
    void SetBypass(bool on);
    void SetBandEnabled(int band, bool on);
    void SetBandType(int band, BandType type);
    void SetBandFreq(int band, fixed hz);    // 20..20000
    void SetBandGainDb(int band, fixed db);  // -24..+24
    void SetBandQ(int band, fixed q);        // 0.1..10
    void SetAllFlat();

    // Interleaved stereo in-place processing (channel 0..kMaxChannels-1).
    void Process(int channel, fixed *buffer, int frames);

    // Control-rate readbacks.
    bool IsFlat() const { return flat_; }
    bool GetBypass() const { return bypass_; }
    bool GetBandEnabled(int band) const { return bandCfg_[band].enabled; }
    BandType GetBandType(int band) const { return bandCfg_[band].type; }
    fixed GetBandFreq(int band) const { return bandCfg_[band].hz; }
    fixed GetBandGainDb(int band) const { return bandCfg_[band].db; }
    fixed GetBandQ(int band) const { return bandCfg_[band].q; }
    int GetSampleRate() const { return rate_; }

    // BACON_1.5_EQ8_STRUCTURAL: returns the CURRENT (smoothed) coefficients
    // the Process() path is actually applying to the signal.  The UI curve
    // is drawn from these exact values, so the picture always matches the
    // sound (no duplicated RBJ math in the view).
    void GetBandCoeffs(int band, fixed *b0, fixed *b1, fixed *b2, fixed *a1,
                       fixed *a2) const {
        if (band < 0 || band >= kNumBands) {
            *b0 = i2fp(1); *b1 = 0; *b2 = 0; *a1 = 0; *a2 = 0;
            return;
        }
        *b0 = bandCfg_[band].b0; *b1 = bandCfg_[band].b1;
        *b2 = bandCfg_[band].b2; *a1 = bandCfg_[band].a1;
        *a2 = bandCfg_[band].a2;
    }

    bool WasEdited() const { return edited_; }
    void ClearEdited() { edited_ = false; }

private:
    static fixed saturate(fixed x);
    void refreshFlat();
    void recomputeBand(int band);

    struct BandCfg {
        // Active (smoothed) coefficients used by Process().
        fixed b0, b1, b2, a1, a2;   // normalized (a0 = 1)
        // Target coefficients after the last ConfigureBand.
        fixed tB0, tB1, tB2, tA1, tA2;
        bool smoothing;             // cur != tgt, blend each frame
        fixed hz, db, q;
        bool enabled;
        BandType type;
    };

    struct ChanState {
        fixed s1L, s2L, s1R, s2R;
    };

    BandCfg bandCfg_[kNumBands];
    // BACON_1.5_EQ8_STRUCTURAL: one INDEPENDENT biquad state per channel AND
    // per band (state_[channel][band]).
    ChanState state_[kMaxChannels][kNumBands];

    int rate_;
    bool bypass_;   // global EQ off -> zero cost
    bool flat_;     // all neutral -> zero cost
    bool edited_;
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif