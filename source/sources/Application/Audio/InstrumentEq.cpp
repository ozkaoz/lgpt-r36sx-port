#include "InstrumentEq.h"

#include <math.h>
#include "EqBiquad.h"

static const float kMinGainDb = -24.0f;
static const float kMaxGainDb = 24.0f;
// BACON_1.5_EQ8_STRUCTURAL: exponential coefficient smoothing step.
// cur += (tgt - cur) >> kSmoothShift per frame -> -3 dB point after
// kSmoothShift frames (~1.3 ms @ 48 kHz with shift 6), within 1 LSB after
// ~7*kSmoothShift frames (~9 ms).  Inaudible, click-free edits.
static const int kSmoothShift = 6;

namespace FxEngine {

float InstrumentEq::DefaultBandHz(int band) {
    static const float kBandHz[kNumBands] = {
        80.0f, 160.0f, 320.0f, 640.0f, 1250.0f, 2500.0f, 5000.0f, 10000.0f,
    };
    if (band < 0) band = 0;
    if (band >= kNumBands) band = kNumBands - 1;
    return kBandHz[band];
}

InstrumentEq::InstrumentEq()
    : rate_(44100), bypass_(false), flat_(true), edited_(false), rtViolations_(0) {
    Reset();
}

void InstrumentEq::Reset() {
    ResetChannelState();
    bypass_ = false;
    for (int b = 0; b < kNumBands; b++) {
        BandCfg &bg = bandCfg_[b];
        bg.b0 = i2fp(1);
        bg.b1 = 0;
        bg.b2 = 0;
        bg.a1 = 0;
        bg.a2 = 0;
        bg.tB0 = bg.b0;
        bg.tB1 = 0;
        bg.tB2 = 0;
        bg.tA1 = 0;
        bg.tA2 = 0;
        bg.smoothing = false;
        bg.hz = fl2fp(DefaultBandHz(b));
        bg.db = 0;
        bg.q = fl2fp(1.0f);
        bg.enabled = false;
        bg.type = TYPE_BELL;
    }
    refreshFlat();
}

void InstrumentEq::ResetChannelState() {
    for (int c = 0; c < kMaxChannels; c++) {
        for (int b = 0; b < kNumBands; b++) {
            ChanState &s = state_[c][b];
            s.s1L = 0; s.s2L = 0;
            s.s1R = 0; s.s2R = 0;
        }
    }
}

void InstrumentEq::SetSampleRate(int rate) {
    if (rate < 8000 || rate > 96000) { rtViolations_++; return; }
    if (rate == rate_) return;
    rate_ = rate;
    // New rate forces a full recompute of every band.
    for (int b = 0; b < kNumBands; b++) {
        recomputeBand(b);
    }
}

// BACON_1.5_EQ8_STRUCTURAL: atomic edit entry point.  The fingerprint
// (type, hz, db, q, rate) gates the float RBJ recompute; the enabled flag
// is a free toggle that never touches the coefficient math.
void InstrumentEq::ConfigureBand(int band, BandType type, fixed hz, fixed db,
                                 fixed q, bool enabled) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    if (type < 0 || type >= (BandType)kTypeCount) { rtViolations_++; return; }

    BandCfg &bg = bandCfg_[band];

    float f = fp2fl(hz);
    if (f < 20.0f) f = 20.0f;
    if (f > 20000.0f) f = 20000.0f;
    hz = fl2fp(f);

    float g = fp2fl(db);
    if (g < kMinGainDb) g = kMinGainDb;
    if (g > kMaxGainDb) g = kMaxGainDb;
    db = fl2fp(g);

    float qf = fp2fl(q);
    if (qf < 0.1f) qf = 0.1f;
    if (qf > 10.0f) qf = 10.0f;
    q = fl2fp(qf);

    bool paramsChanged = (bg.type != type || bg.hz != hz || bg.db != db ||
                          bg.q != q);
    bg.type = type;
    bg.hz = hz;
    bg.db = db;
    bg.q = q;
    bg.enabled = enabled;
    edited_ = true;

    if (paramsChanged) {
        recomputeBand(band);
    }
    refreshFlat();
}

void InstrumentEq::SetBypass(bool on) { bypass_ = on; edited_ = true; refreshFlat(); }

void InstrumentEq::SetBandEnabled(int band, bool on) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    bandCfg_[band].enabled = on; edited_ = true; refreshFlat();
}

void InstrumentEq::SetBandType(int band, BandType t) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    ConfigureBand(band, t, bandCfg_[band].hz, bandCfg_[band].db,
                  bandCfg_[band].q, bandCfg_[band].enabled);
}

void InstrumentEq::SetBandFreq(int band, fixed hz) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    ConfigureBand(band, bandCfg_[band].type, hz, bandCfg_[band].db,
                  bandCfg_[band].q, bandCfg_[band].enabled);
}

void InstrumentEq::SetBandGainDb(int band, fixed db) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    ConfigureBand(band, bandCfg_[band].type, bandCfg_[band].hz, db,
                  bandCfg_[band].q, bandCfg_[band].enabled);
}

void InstrumentEq::SetBandQ(int band, fixed q) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    ConfigureBand(band, bandCfg_[band].type, bandCfg_[band].hz,
                  bandCfg_[band].db, q, bandCfg_[band].enabled);
}

void InstrumentEq::SetAllFlat() {
    for (int b = 0; b < kNumBands; b++) {
        bandCfg_[b].enabled = false;
        bandCfg_[b].type = TYPE_BELL;
        bandCfg_[b].db = 0;
        bandCfg_[b].q = fl2fp(1.0f);
    }
    edited_ = true;
    refreshFlat();
}

void InstrumentEq::refreshFlat() {
    if (bypass_) {
        flat_ = true;
        ResetChannelState();
        return;
    }
    // BACON_1.5_EQ8_0DB_TRANSPARENT (U2.52.6, feedback): a band at 0 dB is
    // transparent for EVERY type.  The RBJ LP/HP/NOTCH/BP filters have no
    // gain, so at 0 dB they were still active (e.g. LOWPA on the default
    // 80 Hz band cut everything above 80 Hz -> "the EQ kills the sound").
    // The band only enters the DSP once the user moves the gain off 0.
    bool any = false;
    for (int b = 0; b < kNumBands; b++) {
        if (!bandCfg_[b].enabled) continue;
        if (bandCfg_[b].db != 0) { any = true; break; }
    }
    flat_ = !any;
    if (flat_) {
        // BACON_1.5_EQ8_STRUCTURAL: entering the flat path leaves stale
        // filter states behind; reset them so the next edit starts clean
        // (no transient from the old filter when the band comes back).
        ResetChannelState();
    }
}

// FXP_INSTRUMENT_EQ_BP (bacon-1.5, item 2): the coefficient math lives in the
// shared EqBiquad primitive (same DSP as FxEngine::ParametricEQ).  Maps the
// persisted BandType (0..6) to the EqBiquad type; BELL default on unknown.
static int mapBandType(int t) {
    switch (t) {
    case InstrumentEq::TYPE_LOW_SHELF:  return EQ_BIQUAD_LOW_SHELF;
    case InstrumentEq::TYPE_HIGH_SHELF: return EQ_BIQUAD_HIGH_SHELF;
    case InstrumentEq::TYPE_LOW_PASS:   return EQ_BIQUAD_LOW_PASS;
    case InstrumentEq::TYPE_HIGH_PASS:  return EQ_BIQUAD_HIGH_PASS;
    case InstrumentEq::TYPE_BAND_PASS:  return EQ_BIQUAD_BAND_PASS;
    case InstrumentEq::TYPE_NOTCH:      return EQ_BIQUAD_NOTCH;
    case InstrumentEq::TYPE_BELL:
    default:                            return EQ_BIQUAD_BELL;
    }
}

void InstrumentEq::recomputeBand(int band) {
    BandCfg &bg = bandCfg_[band];
    // BACON_1.5_EQ8_0DB_TRANSPARENT: a 0 dB band is the identity filter for
    // EVERY type.  Snap the coefficients immediately (no smoothing, no
    // stale LP/HP/NOTCH state left behind while the band is transparent).
    if (bg.db == 0) {
        bg.tB0 = bg.b0 = i2fp(1);
        bg.tB1 = bg.b1 = 0;
        bg.tB2 = bg.b2 = 0;
        bg.tA1 = bg.a1 = 0;
        bg.tA2 = bg.a2 = 0;
        bg.smoothing = false;
        return;
    }
    if (rate_ <= 0) return;
    fixed b0, b1, b2, a1, a2;
    eqBiquadCoeffs(mapBandType((int)bg.type), rate_, fp2fl(bg.hz),
                   fp2fl(bg.db), fp2fl(bg.q), b0, b1, b2, a1, a2);
    // Set the target coefficients; the per-frame loop smooths cur -> tgt.
    bg.tB0 = b0; bg.tB1 = b1; bg.tB2 = b2; bg.tA1 = a1; bg.tA2 = a2;
    if (bg.b0 != bg.tB0 || bg.b1 != bg.tB1 || bg.b2 != bg.tB2 ||
        bg.a1 != bg.tA1 || bg.a2 != bg.tA2) {
        bg.smoothing = true;
    } else {
        bg.smoothing = false;
    }
}

fixed InstrumentEq::saturate(fixed x) {
    if (x > i2fp(1)) return i2fp(1);
    if (x < -i2fp(1)) return -i2fp(1);
    return x;
}

void InstrumentEq::Process(int channel, fixed *buffer, int frames) {
    if (frames <= 0 || !buffer) { rtViolations_++; return; }
    if (flat_) return;  // zero cost
    if (channel < 0 || channel >= kMaxChannels) { rtViolations_++; return; }

    int idx = 0;
    for (int i = 0; i < frames; i++) {
        // BACON_1.5_EQ8_STRUCTURAL: per-frame exponential coefficient blend
        // (only while a band is converging).
        for (int b = 0; b < kNumBands; b++) {
            BandCfg &bg = bandCfg_[b];
            if (!bg.smoothing) continue;
            fixed d0 = bg.tB0 - bg.b0;
            fixed d1 = bg.tB1 - bg.b1;
            fixed d2 = bg.tB2 - bg.b2;
            fixed dA1 = bg.tA1 - bg.a1;
            fixed dA2 = bg.tA2 - bg.a2;
            bg.b0 += d0 >> kSmoothShift;
            bg.b1 += d1 >> kSmoothShift;
            bg.b2 += d2 >> kSmoothShift;
            bg.a1 += dA1 >> kSmoothShift;
            bg.a2 += dA2 >> kSmoothShift;
            // Snap the last sub-2^-6 residual so convergence is EXACT
            // (the (tgt-cur)>>6 step lands on zero while still apart).
            if (bg.b0 != bg.tB0 && (d0 >> kSmoothShift) == 0) bg.b0 = bg.tB0;
            if (bg.b1 != bg.tB1 && (d1 >> kSmoothShift) == 0) bg.b1 = bg.tB1;
            if (bg.b2 != bg.tB2 && (d2 >> kSmoothShift) == 0) bg.b2 = bg.tB2;
            if (bg.a1 != bg.tA1 && (dA1 >> kSmoothShift) == 0) bg.a1 = bg.tA1;
            if (bg.a2 != bg.tA2 && (dA2 >> kSmoothShift) == 0) bg.a2 = bg.tA2;
            if (bg.b0 == bg.tB0 && bg.b1 == bg.tB1 && bg.b2 == bg.tB2 &&
                bg.a1 == bg.tA1 && bg.a2 == bg.tA2) {
                bg.smoothing = false;
            }
        }

        fixed xL = buffer[idx];
        fixed xR = buffer[idx + 1];
        for (int b = 0; b < kNumBands; b++) {
            const BandCfg &bg = bandCfg_[b];
            // BACON_1.5_EQ8_0DB_TRANSPARENT: same rule as refreshFlat() --
            // a 0 dB band (any type) never touches the audio.
            if (!bg.enabled || bg.db == 0) continue;
            ChanState &st = state_[channel][b];
            // BACON_1.5_EQ8_DF2_64BIT: compute the transposed Df2 state update
            // in 64 bits.  With full-scale input and EQ boosts the per-term
            // Q15 values (b1*x, a1*t, s2...) can each approach +/-2^31, so a
            // 32-bit sum overflows (verified under UBSAN; on the device any
            // edit "killed" the sample sound: U2.52.5).  The final value fits
            // in 32 bits, so the truncation is exact.
            fixed tL = (fixed)((long long)fp_mul(bg.b0, xL) + st.s1L);
            st.s1L = (fixed)((long long)fp_mul(bg.b1, xL) -
                             (long long)fp_mul(bg.a1, tL) + st.s2L);
            st.s2L = (fixed)((long long)fp_mul(bg.b2, xL) -
                             (long long)fp_mul(bg.a2, tL));
            fixed tR = (fixed)((long long)fp_mul(bg.b0, xR) + st.s1R);
            st.s1R = (fixed)((long long)fp_mul(bg.b1, xR) -
                             (long long)fp_mul(bg.a1, tR) + st.s2R);
            st.s2R = (fixed)((long long)fp_mul(bg.b2, xR) -
                             (long long)fp_mul(bg.a2, tR));
            xL = tL;
            xR = tR;
        }
        buffer[idx] = saturate(xL);
        buffer[idx + 1] = saturate(xR);
        idx += 2;
    }
}

} // namespace FxEngine