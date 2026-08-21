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
 *   - BACON_1.5_EQ8_SLOPE (U2.62): per-band slope 1..2 (12/24 dB/oct).
 *     Slope 2 cascades the same biquad twice (only LP/HP/BP/NOTCH/shelves;
 *     a bell's shape is its Q), doubling the dB response exactly.
  *   - BACON_1.5_EQ8_SLOPE48 (U2.64, feedback #14 revisado): slope 1..4
 *     (12/24/36/48 dB/oct).  Slope 3/4 cascade 3/4 times the same biquad
 *     (L2+X era el peak, R2+X+UP/DN controla slope).  12 dB suave,
 *     48 dB pared "que corta frecuencias".
 *   - BACON_1.5_EQ8_SLOPE96 (U2.65, feedback #14 revisado): slope 1..8
 *     (12..96 dB/oct, paso 12).  Todos los tipos incluido BELL: cada
 *     etapa cascada el mismo biquad, campana más pronunciada, pared
 *     96 dB corta como ladrillo.  L2+X 1 Hz lineal.
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

    // BACON_1.5_EQ8_LOOPFADE (U2.61, feedback #12): fade ONE channel's
    // biquad states to zero over kFadeFrames samples instead of the hard
    // ResetChannelState() step.  With a HIPASS below 80 Hz the state holds
    // large low-frequency cancellation values, so the instant zero was a
    // click at the loop point ("el kick clipea al final del sonido").
    // The ramp (1.0 -> ~0 over 32 frames) keeps the loop periodic without
    // the discontinuity.  Apply at sample-loop wraps / note retriggers.
    static const int kFadeFrames = 32;
    void FadeChannelState(int channel);

    void SetSampleRate(int rate);

    // BACON_1.5_EQ8_STRUCTURAL: single atomic entry point for every edit.
    // Recomputes the band's RBJ coefficients only when (type, hz, db, q,
    // slope) changed since the last call; toggling `enabled` never touches
    // the float math unless the band must close/reopen (see below).
    void ConfigureBand(int band, BandType type, fixed hz, fixed db, fixed q,
                       int slope, bool enabled);
    void SetBypass(bool on);
    void SetBandEnabled(int band, bool on);
    void SetBandType(int band, BandType type);
    void SetBandFreq(int band, fixed hz);    // 20..20000
    void SetBandGainDb(int band, fixed db);  // -24..+24
    void SetBandQ(int band, fixed q);        // 0.1..10
    void SetBandSlope(int band, int slope);  // 1..8 = 12..96 dB/oct
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
    int GetBandSlope(int band) const { return bandCfg_[band].slope; }
    int GetSampleRate() const { return rate_; }

    // BACON_1.5_EQ8_STRUCTURAL: returns the CURRENT (smoothed) coefficients
    // the Process() path is actually applying to the signal.  The UI curve
    // is drawn from these exact values, so the picture always matches the
    // sound (no duplicated RBJ math in the view).
    // BACON_1.5_EQ8_SLOPE_PRECISION (U2.62): b0..b2 are stored in Q24 (so a
    // 80 Hz LP numerator keeps its exact ~1e-5 values); the readback scales
    // them down to the Q15 the curve expects, truncating toward zero so the
    // values are bit-identical to a direct Q15 emission (Q24 = 2^9 * Q15).
    // BACON_1.5_EQ8_DEN24 (U2.62): a1/a2 are Q24 too (see EqBiquad.h), same
    // >>9 readback.
    void GetBandCoeffs(int band, fixed *b0, fixed *b1, fixed *b2, fixed *a1,
                       fixed *a2) const {
        if (band < 0 || band >= kNumBands) {
            *b0 = i2fp(1); *b1 = 0; *b2 = 0; *a1 = 0; *a2 = 0;
            return;
        }
        fixed v0 = bandCfg_[band].b0;
        fixed v1 = bandCfg_[band].b1;
        fixed v2 = bandCfg_[band].b2;
        fixed va1 = bandCfg_[band].a1;
        fixed va2 = bandCfg_[band].a2;
        *b0 = v0 >= 0 ? (fixed)((v0 + 256) >> 9) : (fixed)(-((-(long long)v0 + 256) >> 9));
        *b1 = v1 >= 0 ? (fixed)((v1 + 256) >> 9) : (fixed)(-((-(long long)v1 + 256) >> 9));
        *b2 = v2 >= 0 ? (fixed)((v2 + 256) >> 9) : (fixed)(-((-(long long)v2 + 256) >> 9));
        *a1 = va1 >= 0 ? (fixed)((va1 + 256) >> 9) : (fixed)(-((-(long long)va1 + 256) >> 9));
        *a2 = va2 >= 0 ? (fixed)((va2 + 256) >> 9) : (fixed)(-((-(long long)va2 + 256) >> 9));
    }

    bool WasEdited() const { return edited_; }
    void ClearEdited() { edited_ = false; }

private:
    void refreshFlat();
    void recomputeBand(int band);
    // BACON_1.5_EQ8_CLICKFREE (U2.62, feedback #14): ramp one band's
    // coefficients to the IDENTITY filter (b0=1, rest 0) instead of the old
    // instant snap/flush.  Used when a band goes to 0 dB (recomputeBand),
    // gets disabled (ConfigureBand) or the EQ bypasses (SetBypass): with a
    // HIPASS below 80 Hz the state holds large low-frequency cancellation
    // values, so removing the filter instantly jumped the output by the
    // whole removed signal ("el clipeo al final al editar").  The band keeps
    // RUNNING while the coefficients blend toward identity, so the output
    // morphs filtered->raw over ~1.3 ms (inaudible); the identity filter
    // then drains the state in 2 samples, so no stale state is left behind
    // when the band comes back (no click on re-activation either).
    void smoothToIdentity(int band);

    struct BandCfg {
        // Active (smoothed) coefficients used by Process().
        fixed b0, b1, b2, a1, a2;   // normalized (a0 = 1)
        // Target coefficients after the last ConfigureBand.
        fixed tB0, tB1, tB2, tA1, tA2;
        bool smoothing;             // cur != tgt, blend each frame
        fixed hz, db, q;
        int slope;                  // 1..8 = 12..96 dB/oct cascade
        bool enabled;
        BandType type;
    };

    struct ChanState {
        // BACON_1.5_EQ8_SLOPE_PRECISION (U2.62): the states run at 2^24
        // (extended precision, 9 fractional bits below the signal scale) so
        // a LP/HP corner below ~500 Hz keeps its exact ~1e-5 numerator:
        // with Q15 states the b0..b2 products truncated to (0,1,0) and the
        // filter became a resonator (any low LP/HP corner "rang"; the slope
        // 2 cascade BOOSTED instead of squaring the cut).
        long long s1L, s2L, s1R, s2R;
    };

    BandCfg bandCfg_[kNumBands];
    // BACON_1.5_EQ8_STRUCTURAL: one INDEPENDENT biquad state per channel AND
    // per band (state_[channel][band][0]).
    // BACON_1.5_EQ8_SLOPE96 (U2.65): stages [1..7] extra passes 12..96
    // dB/oct, todos los tipos incluido BELL (campana más pronunciada).
    ChanState state_[kMaxChannels][kNumBands][8];
    // BACON_1.5_EQ8_LOOPFADE: frames of fade left for each channel
    // (0 = no fade pending).
    int fade_[kMaxChannels];

    int rate_;
    bool bypass_;   // global EQ off -> zero cost
    bool flat_;     // all neutral -> zero cost
    bool edited_;
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif