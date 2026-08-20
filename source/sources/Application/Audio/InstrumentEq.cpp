#include "InstrumentEq.h"

#include <math.h>
#include "EqBiquad.h"

static const float kMinGainDb = -24.0f;
static const float kMaxGainDb = 24.0f;
// BACON_1.5_EQ8_SOFTKNEE (U2.59, feedback #12): per-sample soft knee that
// mirrors the master safety limiter (AudioMixer::Render, BACON_1.5_MASTER_
// SAFETY U2.56) but caps at UNITY instead of 1.0-at-the-master:
//   - |x| <= 0.85          -> passes untouched (the master's transparent
//     region, so a normal edit changes nothing);
//   - 0.85 < |x| <= 1.7    -> mapped onto (0.85 .. 1.0];
//   - |x| > 1.7            -> clamped to 1.0 (i2fp(1)-1, the same ceiling
//     the synths clamp to, and a full-scale sample's exact level).
// The old per-block limiter (BACON_1.5_EQ8_BLOCKLIMIT U2.53) scaled a hot
// block down to 2.0, so ONE instrument could still push the master bus
// past its 1.7 flat ceiling -- the saved lgpt_KAOZ kick (BELL +11 dB @
// 2500 Hz, Q 1.0) did exactly that and the whole mix crushed to a square
// wave ("aplicar esa EQ en el kick genera distorsion").  With the knee
// capped at unity an instrument can never exceed full scale on its own,
// so the master stays in its transparent region no matter what the EQ
// does; the 64-bit map handles the full int32 input range (+-2.0).
// BACON_1.5_EQ8_SOFTKNEE_C1 (U2.60, feedback #12): the first soft knee was
// piecewise-LINEAR and its slope JUMPED at 0.85 (1.0 -> 0.176): every
// crest riding the knee got a kink that created "solapa"/click harmonics
// (a boosted kick, or the bass now boosted to ~0.9, both ride it).  The
// map is now a C1 RATIONAL: it joins the transparent region with slope 1
// at 0.85, climbs monotonically, and lands on the unity ceiling with
// slope 0 at 1.7 -- no slope discontinuity anywhere, so a loud crest is
// compressed smoothly and creates no new high-frequency content.
//   f(u) = (u + A*u^2) / (1 + B*u),  u = |x| - 0.85
//   f(0) = 0, f'(0) = 1 (C1 at the knee), f(T) = C, f'(T) = 0 (smooth
//   landing on unity), T = 1.7-0.85, C = 1.0-0.85.
static const int kKneeQ15 = (int)(0.85f * 32768.0f);  // 27852
static const int kTopQ15 = (int)(1.7f * 32768.0f);    // 55705
static const int kUnityQ15 = (1 << 15) - 1;           // 32767
static const int kKneeAQ32 = -27212171;               // A in Q32
static const int kKneeBQ32 = 565359;                  // B in Q32
// BACON_1.5_EQ8_STRUCTURAL: exponential coefficient smoothing step.
// cur += (tgt - cur) >> kSmoothShift per frame -> -3 dB point after
// kSmoothShift frames (~1.3 ms @ 48 kHz with shift 6), within 1 LSB after
// ~7*kSmoothShift frames (~9 ms).  Inaudible, click-free edits.
static const int kSmoothShift = 6;

namespace FxEngine {

// BACON_1.5_EQ8_LOOPFADE (U2.61, feedback #12): linear state-fade ramp.
// At sample n (0..kFadeFrames-1) the biquad states of the fading channel
// are scaled by (kFadeFrames-n)/kFadeFrames * 2^15, so the state reaches
// ~0 at the end of the fade (the last step is 1/32 of the original, a
// jump 32x smaller than the old hard flush).  Q15 fractions of 32.
static const fixed kFadeRampQ15[InstrumentEq::kFadeFrames] = {
    32768, 31744, 30720, 29696, 28672, 27648, 26624, 25600,
    24576, 23552, 22528, 21504, 20480, 19456, 18432, 17408,
    16384, 15360, 14336, 13312, 12288, 11264, 10240,  9216,
     8192,  7168,  6144,  5120,  4096,  3072,  2048,  1024,
};

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
    for (int c = 0; c < kMaxChannels; c++) fade_[c] = 0;
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

// BACON_1.5_EQ8_LOOPFADE (U2.61): see the header.  Sets a per-channel fade
// counter; Process() scales that channel's states to zero over the next
// kFadeFrames samples.  The hard reset (ResetChannelState) stays for the
// init/bypass/flat paths where no audio is playing.
void InstrumentEq::FadeChannelState(int channel) {
    if (channel < 0 || channel >= kMaxChannels) { rtViolations_++; return; }
    fade_[channel] = kFadeFrames;
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

// BACON_1.5_EQ8_SOFTKNEE (U2.59, feedback #12): see the constants above;
// the old per-block scale-to-2.0 limiter is gone.  The knee runs inline
// per sample (64-bit), after the band loop.
void InstrumentEq::Process(int channel, fixed *buffer, int frames) {
    if (frames <= 0 || !buffer) { rtViolations_++; return; }
    if (flat_) return;  // zero cost
    if (channel < 0 || channel >= kMaxChannels) { rtViolations_++; return; }

    // BACON_1.5_EQ8_SOFTKNEE (U2.59, feedback #12): the old block limiter
    // note is gone.  The int32 sample pipeline wraps at +/-2.0 when two
    // full-scale signals meet (the master sum accumulates in 64 bits,
    // AudioMixer::Render), but this knee caps the instrument at UNITY
    // before it ever reaches the bus, so the master safety never engages
    // on account of one instrument's EQ.
    int idx = 0;
    int fade = fade_[channel];
    for (int i = 0; i < frames; i++) {
        // BACON_1.5_EQ8_LOOPFADE (U2.61): scale this channel's states toward
        // zero across a sample-loop wrap.  Runs BEFORE the band loop so the
        // current sample already reads the faded state; the hard zero step
        // of the old loop flush became an audible click at the loop point
        // with a HIPASS below 80 Hz (the state holds large low-frequency
        // cancellation values).
        if (fade > 0) {
            fixed r = kFadeRampQ15[kFadeFrames - (fade--)];
            for (int b = 0; b < kNumBands; b++) {
                ChanState &st = state_[channel][b];
                st.s1L = fp_mul(st.s1L, r);
                st.s2L = fp_mul(st.s2L, r);
                st.s1R = fp_mul(st.s1R, r);
                st.s2R = fp_mul(st.s2R, r);
            }
        }
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
        // BACON_1.5_EQ8_SOFTKNEE_C1: per-sample soft knee (linked L/R is
        // unnecessary: each channel maps independently, same curve).  The
        // map is monotonic, C1-smooth and 64-bit; boosts can push x past
        // +/-2^30 and the master-scale restore <<15 must never see a value
        // over i2fp(1)-1 (32767 Q15), which this guarantees.
        // f(u) = (u + A*u^2) / (1 + B*u) with A in Q32, B in Q32:
        //   n = u - ((Aq32*u^2) >> 32),  d = (1<<32) + Bq32*u,
        //   f = (n << 32) / d.
        {
            int aL = (xL < 0) ? -xL : xL;
            int aR = (xR < 0) ? -xR : xR;
            if (aL > kKneeQ15) {
                int v;
                if (aL >= kTopQ15) {
                    v = kUnityQ15;
                } else {
                    int u = aL - kKneeQ15;
                    long long u2 = (long long)u * u;
                    long long n = u - ((kKneeAQ32 * u2) >> 32);
                    long long d = (1LL << 32) + (long long)kKneeBQ32 * u;
                    long long f = (n << 32) / d;
                    v = kKneeQ15 + (int)f;
                    if (v > kUnityQ15) v = kUnityQ15;
                    if (v < kKneeQ15) v = kKneeQ15;
                }
                xL = (xL < 0) ? -v : v;
            }
            if (aR > kKneeQ15) {
                int v;
                if (aR >= kTopQ15) {
                    v = kUnityQ15;
                } else {
                    int u = aR - kKneeQ15;
                    long long u2 = (long long)u * u;
                    long long n = u - ((kKneeAQ32 * u2) >> 32);
                    long long d = (1LL << 32) + (long long)kKneeBQ32 * u;
                    long long f = (n << 32) / d;
                    v = kKneeQ15 + (int)f;
                    if (v > kUnityQ15) v = kUnityQ15;
                    if (v < kKneeQ15) v = kKneeQ15;
                }
                xR = (xR < 0) ? -v : v;
            }
        }
        buffer[idx] = xL;
        buffer[idx + 1] = xR;
        idx += 2;
    }
    fade_[channel] = fade;
}

} // namespace FxEngine