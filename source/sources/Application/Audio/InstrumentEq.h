#ifndef _INSTRUMENT_EQ_H_
#define _INSTRUMENT_EQ_H_

#include "Application/Utils/fixed.h"

/*
 * InstrumentEq -- 8-band graphic EQ applied per sample instrument BEFORE the
 * FX sends (dry instrument output).  RBJ Audio EQ Cookbook biquads in Q15
 * fixed point (same kernels as FxEngine::ParametricEQ).
 *
 * Performance contract (R36SX port, no lag):
 *   - All bands flat/disabled or global bypass -> Process() returns with one
 *     branch, zero per-sample work.  Instruments that never use the EQ pay
 *     nothing.
 *   - Only enabled bands run; disabled bands skipped per frame.
 *   - Pure fixed point inside Process(): no allocation, no syscalls.
 *   - Coefficients recomputed only when a parameter changes (control rate,
 *     float math, same as ParametricEQ).
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
        kTypeCount
    };
    static const int kNumBands = 8;
    static const int kMaxChannels = 8;  // sample instrument polyphony

    static float DefaultBandHz(int band);  // octave ladder

    InstrumentEq();
    void Reset();
    void ResetChannelState();

    void SetSampleRate(int rate);
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

    bool WasEdited() const { return edited_; }
    void ClearEdited() { edited_ = false; }

private:
    static fixed saturate(fixed x);
    void refreshFlat();
    void recomputeBand(int band);

    struct BandCfg {
        fixed b0, b1, b2, a1, a2;   // normalized (a0 = 1)
        fixed hz, db, q;
        bool enabled;
        BandType type;
    };

    struct ChanState {
        fixed s1L, s2L, s1R, s2R;
    };

    BandCfg bandCfg_[kNumBands];
    ChanState state_[kMaxChannels];

    int rate_;
    bool bypass_;   // global EQ off -> zero cost
    bool flat_;     // all neutral -> zero cost
    bool edited_;
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif