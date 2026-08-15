#ifndef _FX_COMPRESSOR_H_
#define _FX_COMPRESSOR_H_

#include "Application/Utils/fixed.h"

/*
 * FxEngine::Compressor -- stereo feed-forward compressor + soft limiter
 * (PLAN_FX_REDESIGN_ES.md, Fase 3; bacon-1.5 item 4: V2 sidechain).
 *
 * Feed-forward detector (peak, stereo link option), threshold/ratio/soft knee,
 * attack/release one-pole envelope, makeup gain, gain-reduction meter, and a
 * final cubic soft clip (limiter) so the master never exceeds +/-1.
 *
 * bacon-1.5 item 4 (V2): true zero-latency sidechain.  The detector uses the
 * maximum of the program envelope and the external sidechain tap envelope
 * (SC AMOUNT scales the sidechain contribution, SC HPF removes boom).  The
 * tap is provided by FxEngine::Compressor::SetSidechainInput() from the
 * selected TRACK or BUS (delay/reverb return) BEFORE the master mix, so the
 * compression reacts to the kick/whatever while the program keeps its own
 * attack/release.  No look-ahead: the tap is the same block the program is
 * being compressed against, so the sidechain is sample-accurate with zero
 * latency.  A dry/wet MIX crossfades program vs compressed output.
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

    // bacon-1.5 item 4: sidechain source selectors.  Values are persisted in
    // the FXMASTER node (CMPSCR), so they are a stable public contract:
    //   0 = OFF (program detector only)
    //   1..8 = TRACK 1..8 pre-master tap (accumulated in AccumulateChannelSend)
    //   9 = DELAY return bus tap
    //   10 = REVERB return bus tap
    enum SidechainSource {
        SC_OFF = 0,
        SC_TRACK_1 = 1,       // ... SC_TRACK_8 = 8
        SC_DELAY_RETURN = 9,
        SC_REVERB_RETURN = 10,
        SC_SOURCE_COUNT = 11
    };

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

    // bacon-1.5 item 4 (V2): sidechain.
    void SetSidechainSource(int src);     // 0..SC_SOURCE_COUNT-1
    void SetSidechainHpfHz(fixed hz);     // SC HPF: 30..20000, <=30 = open
    void SetSidechainAmount(fixed amt);   // 0..1, Q15
    void SetMix(fixed mix);               // 0..1 dry/wet (Q15), smoothed
    // Zero-latency tap: caller (FxEngine) supplies the sidechain signal for
    // this Process() call (interleaved stereo Q15, same frame count).  May be
    // 0/NULL to use only the program detector.
    void SetSidechainInput(const fixed *tap, int frames);

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
    fixed GetAttackMsFixed() const { return fl2fp(attackMs_); }
    fixed GetReleaseMsFixed() const { return fl2fp(releaseMs_); }
    bool GetStereoLink() const { return stereoLink_; }
    bool GetBypass() const { return bypass_; }
    bool GetSoftClip() const { return softClip_; }
    // bacon-1.5 item 4 readbacks.
    int GetSidechainSource() const { return scSource_; }
    fixed GetSidechainHpfHz() const { return scHpfHz_; }
    fixed GetSidechainAmount() const { return scAmount_; }
    fixed GetMix() const { return mix_; }

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

    // bacon-1.5 item 4 (V2): sidechain state.  Zero-latency tap, SC HPF
    // one-pole state, envelope (shares attK_/relK_), amount, dry/wet mix.
    int scSource_;                 // SidechainSource
    fixed scHpfHz_;                // Q15 Hz, <=30 Hz = open
    fixed scHpfCoeff_;             // one-pole coefficient (control-rate)
    fixed scHpfState_[2];          // per-channel HPF state
    fixed scLevel_[2];             // SC envelope (Q15 [0,1])
    fixed scAmount_;               // Q15 0..1
    fixed mix_;                    // Q15 0..1 dry/wet
    fixed mixCur_;                 // smoothed wet mix (Q15)
    const fixed *scTap_;           // valid only during Process()
    int scFrames_;
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif
