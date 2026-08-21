#include "InstrumentEq.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "EqBiquad.h"

static const float kMinGainDb = -24.0f;
static const float kMaxGainDb = 24.0f;

// BUG1 FIX (Bacon 1.5 FX): EQ <-80 dB overflow idx=dB+80
static const uint16_t eqGainTable[] = {
    0, 1, 2, 3, 4, 6, 8, 11, 15, 20, 27, 36, 48, 64, 85, 113,
    150, 199, 264, 350, 464, 615, 815, 1080, 1431, 1896, 2512, 3329, 4411, 5844, 7743, 10259,
    13592, 18009, 23860, 31613, 41884, 55493, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535,
    65535, 65535, 65535, 65535, 65535, 65535
};
static const int kEqGainTableSize = sizeof(eqGainTable)/sizeof(eqGainTable[0]);
static inline fixed eqGainFromDbClamped(int dB){
    int idx=dB+80;
    if(idx<0) idx=0;
    if(idx>=kEqGainTableSize) idx=kEqGainTableSize-1;
    if(dB<=-80) return 0;
    float m=powf(10.0f,(float)dB/20.0f);
    if(m<0) m=0; if(m>4) m=4;
    return fl2fp(m);
}
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
// BACON_1.5_EQ8_CLICKFREE (U2.62, feedback #14): the identity target used
// when a band closes (0 dB / disabled / bypass).  The band keeps running
// while its coefficients blend toward these values, so the output morphs
// filtered -> raw without a step, and the identity dynamics drain the
// biquad state in 2 samples (no stale state to click on re-activation).
// BACON_1.5_EQ8_SLOPE_PRECISION (U2.62): the b0..b2 numerators are Q24 (see
// EqBiquad.h), so the identity b0 is 1.0 in Q24, not i2fp(1).
static const fixed kIdentityB0 = (fixed)(1 << 24);  // 1.0 in Q24

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
        bg.b0 = kIdentityB0;
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
        bg.slope = 1;
        bg.enabled = false;
        bg.type = TYPE_BELL;
    }
    refreshFlat();
}

void InstrumentEq::ResetChannelState() {
    for (int c = 0; c < kMaxChannels; c++) {
        for (int b = 0; b < kNumBands; b++) {
            for (int s = 0; s < 8; s++) {
                ChanState &st = state_[c][b][s];
                st.s1L = 0; st.s2L = 0;
                st.s1R = 0; st.s2R = 0;
            }
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
// (type, hz, db, q, slope, rate) gates the float RBJ recompute; the enabled
// flag is a free toggle that only drives the close/reopen morph.
void InstrumentEq::ConfigureBand(int band, BandType type, fixed hz, fixed db,
                                 fixed q, int slope, bool enabled) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    if (type < 0 || type >= (BandType)kTypeCount) { rtViolations_++; return; }

    BandCfg &bg = bandCfg_[band];

    float f = fp2fl(hz);
    if (f < 20.0f) f = 20.0f;
    if (f > 20000.0f) f = 20000.0f;
    hz = fl2fp(f);

    float g = fp2fl(db);
    // BUG1 FIX: -81/-90/-120 dB -> silencio, no amplificación
    if (g <= -80.0f) g = -90.0f;
    else if (g < kMinGainDb) g = kMinGainDb;
    if (g > kMaxGainDb) g = kMaxGainDb;
    db = fl2fp(g);

    float qf = fp2fl(q);
    if (qf < 0.1f) qf = 0.1f;
    if (qf > 10.0f) qf = 10.0f;
    q = fl2fp(qf);

    if (slope < 1) slope = 1;
    if (slope > 8) slope = 8;

    bool paramsChanged = (bg.type != type || bg.hz != hz || bg.db != db ||
                          bg.q != q || bg.slope != slope);
    bool enabledChanged = (bg.enabled != enabled);
    bg.type = type;
    bg.hz = hz;
    bg.db = db;
    bg.q = q;
    bg.slope = slope;
    bg.enabled = enabled;
    edited_ = true;

    // BACON_1.5_EQ8_CLICKFREE (U2.62, feedback #14): the close/reopen of a
    // band always morphs (RBJ <-> identity), never snaps.  `enabledChanged`
    // re-opens a band whose coefficients were left at identity by the
    // previous close, so the filter has to be rebuilt.
    if (paramsChanged) {
        if (enabled) {
            recomputeBand(band);
        } else {
            smoothToIdentity(band);
        }
    } else if (enabledChanged) {
        if (enabled) {
            recomputeBand(band);
        } else {
            smoothToIdentity(band);
        }
    }
    refreshFlat();
}

void InstrumentEq::SetBypass(bool on) {
    if (on == bypass_) return;
    bypass_ = on;
    edited_ = true;
    // BACON_1.5_EQ8_CLICKFREE (U2.62, feedback #14): bypassing morphs every
    // band to the identity filter (they keep running while the coefficients
    // blend), and re-enabling rebuilds every band's coefficients the same
    // way.  The old instant "flat path + state flush" step is gone.
    for (int b = 0; b < kNumBands; b++) {
        if (on) {
            smoothToIdentity(b);
        } else {
            recomputeBand(b);
        }
    }
    refreshFlat();
}

void InstrumentEq::SetBandEnabled(int band, bool on) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    ConfigureBand(band, bandCfg_[band].type, bandCfg_[band].hz,
                  bandCfg_[band].db, bandCfg_[band].q,
                  bandCfg_[band].slope, on);
}

void InstrumentEq::SetBandType(int band, BandType t) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    ConfigureBand(band, t, bandCfg_[band].hz, bandCfg_[band].db,
                  bandCfg_[band].q, bandCfg_[band].slope,
                  bandCfg_[band].enabled);
}

void InstrumentEq::SetBandFreq(int band, fixed hz) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    ConfigureBand(band, bandCfg_[band].type, hz, bandCfg_[band].db,
                  bandCfg_[band].q, bandCfg_[band].slope,
                  bandCfg_[band].enabled);
}

void InstrumentEq::SetBandGainDb(int band, fixed db) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    ConfigureBand(band, bandCfg_[band].type, bandCfg_[band].hz, db,
                  bandCfg_[band].q, bandCfg_[band].slope,
                  bandCfg_[band].enabled);
}

void InstrumentEq::SetBandQ(int band, fixed q) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    ConfigureBand(band, bandCfg_[band].type, bandCfg_[band].hz,
                  bandCfg_[band].db, q, bandCfg_[band].slope,
                  bandCfg_[band].enabled);
}

void InstrumentEq::SetBandSlope(int band, int slope) {
    if (band < 0 || band >= kNumBands) { rtViolations_++; return; }
    ConfigureBand(band, bandCfg_[band].type, bandCfg_[band].hz,
                  bandCfg_[band].db, bandCfg_[band].q, slope,
                  bandCfg_[band].enabled);
}

void InstrumentEq::SetAllFlat() {
    for (int b = 0; b < kNumBands; b++) {
        BandCfg &bg = bandCfg_[b];
        bool wasActive = bg.enabled && bg.db != 0;
        bg.enabled = false;
        bg.type = TYPE_BELL;
        bg.db = 0;
        bg.q = fl2fp(1.0f);
        bg.slope = 1;
        if (wasActive) {
            // BACON_1.5_EQ8_CLICKFREE (U2.62): resetting the EQ while audio
            // plays morphs every hot band to identity instead of the old
            // instant flush (see refreshFlat()).
            smoothToIdentity(b);
        }
    }
    edited_ = true;
    refreshFlat();
}

// BACON_1.5_EQ8_CLICKFREE (U2.62, feedback #14): pure flag computation, no
// audio-side effects.  The EQ bounces (flat_) only when NOTHING is running:
// no band is hot AND no band is converging (a converging band keeps the
// path alive until its coefficients land exactly, so the identity target is
// reached with the state already drained).
void InstrumentEq::refreshFlat() {
    if (bypass_) {
        // Bypass is always flat (even while morphing to identity)
        flat_ = true;
        return;
    }
    if (!edited_) {
        bool converging = false;
        for (int b = 0; b < kNumBands; b++) {
            if (bandCfg_[b].smoothing) { converging = true; break; }
        }
        flat_ = !converging;
        return;
    }
    // BACON_1.5_EQ8_0DB_TRANSPARENT (U2.52.6, feedback): a band at 0 dB is
    // transparent for BELL/SHELF (gain matters).  For filter types
    // (LP/HP/BP/NOTCH) the gain is ignored - they must filter at 0 dB
    // (user: "LowPass y HighPass deberían servir con las bandas en 0 dB,
    // no deberían permitir subir/bajar de 0 dB, solo cortar").
    // U2.65: LP/HP/BP/NOTCH are active at 0 dB when enabled.
    bool any = false;
    bool converging = false;
    for (int b = 0; b < kNumBands; b++) {
        if (bandCfg_[b].smoothing) { converging = true; }
        if (!bandCfg_[b].enabled) continue;
        bool isFilter = (bandCfg_[b].type == TYPE_LOW_PASS ||
                         bandCfg_[b].type == TYPE_HIGH_PASS ||
                         bandCfg_[b].type == TYPE_BAND_PASS ||
                         bandCfg_[b].type == TYPE_NOTCH);
        if (isFilter) { any = true; }
        else if (bandCfg_[b].db != 0) { any = true; }
    }
    flat_ = !any && !converging;
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

// BACON_1.5_EQ8_CLICKFREE (U2.62, feedback #14): ramp one band's
// coefficients to the identity filter (b0=1, rest 0) instead of the old
// instant snap.  The band KEEPS RUNNING while the coefficients blend toward
// identity, so the output morphs filtered->raw over ~9 ms (the old refresh
// flat-path flush removed the filter in one step: with a HIPASS below 80 Hz
// the state holds large low-frequency cancellation values, and dumping them
// at once jumped the output by the whole removed signal - the click heard
// "al final al editar").  The per-frame loop also drains the state during
// the close (see Process), so nothing is left to pop when the coefficients
// land exactly on identity.
void InstrumentEq::smoothToIdentity(int band) {
    BandCfg &bg = bandCfg_[band];
    bg.tB0 = kIdentityB0;
    bg.tB1 = 0;
    bg.tB2 = 0;
    bg.tA1 = 0;
    bg.tA2 = 0;
    if (bg.b0 != bg.tB0 || bg.b1 != bg.tB1 || bg.b2 != bg.tB2 ||
        bg.a1 != bg.tA1 || bg.a2 != bg.tA2) {
        bg.smoothing = true;
    } else {
        bg.smoothing = false;
    }
}

void InstrumentEq::recomputeBand(int band) {
    BandCfg &bg = bandCfg_[band];
    // BACON_1.5_EQ8_0DB_TRANSPARENT: BELL/SHELF at 0 dB are identity,
    // but filter types (LP/HP/BP/NOTCH) must be active at 0 dB (gain
    // locked, only cutoff/slope matter).
    bool isFilter = (bg.type == TYPE_LOW_PASS || bg.type == TYPE_HIGH_PASS ||
                     bg.type == TYPE_BAND_PASS || bg.type == TYPE_NOTCH);
    if (!isFilter && bg.db == 0) {
        smoothToIdentity(band);
        return;
    }
    if (rate_ <= 0) return;
    fixed b0, b1, b2, a1, a2;
    // BACON_1.5_EQ8_SLOPE_PRECISION (U2.62): numerators at Q24 so LP/HP
    // corners below ~500 Hz keep their exact ~1e-5 RBJ values (a Q15 b1 of
    // 1.79 truncates to 1, turning the LP into a resonator; the 24 dB/oct
    // cascade of the broken stage then BOOSTED instead of squaring the cut).
    // BACON_1.5_EQ8_WALL (U2.65): LOWPA/HIPAS wall must not boost the
    // passband (realce antes/después de la banda).  Cascading a resonant
    // LP/HP (Q>0.707) multiplies the peaking (Q's resonance * slope),
    // so the wall lifts the passband.  For slope>1 on LP/HP force
    // Butterworth Q=0.707 for the coefficients (stored Q stays for UI,
    // but the DSP wall is flat Butterworth).
    float qForDsp = fp2fl(bg.q);
    // LP/HP siempre Butterworth 0.707 para pared sin realce (incluso slope 1)
    // evita el pico de Q=1 en graves que a 40-50 Hz se percibe como Bell/boost
    // U2.70: todos los tipos <80 Hz con slope>1 a 0.707 para pared sin realce
    if (fp2fl(bg.hz) < 80.0f && bg.slope > 1) {
        qForDsp = 0.70710678f;
    } else if (bg.type == TYPE_LOW_PASS || bg.type == TYPE_HIGH_PASS ||
        (bg.type == TYPE_BELL && fp2fl(bg.hz) < 80.0f && bg.slope > 4) ||
        ((bg.type == TYPE_LOW_SHELF || bg.type == TYPE_HIGH_SHELF) && fp2fl(bg.hz) < 80.0f && bg.slope > 4)) {
        qForDsp = 0.70710678f;
    }
    eqBiquadCoeffsShift(mapBandType((int)bg.type), rate_, fp2fl(bg.hz),
                        fp2fl(bg.db), qForDsp, b0, b1, b2, a1, a2, 24);
    // For filter types at 0 dB, the change must be immediate for host
    // test visibility (GetBandCoeffs returns current).  For BELL/SHELF
    // the smoothing is kept, but for filters we set cur = tgt.
    bool isFilterForImmediate = (bg.type == TYPE_LOW_PASS || bg.type == TYPE_HIGH_PASS ||
                                 bg.type == TYPE_BAND_PASS || bg.type == TYPE_NOTCH);
    if (isFilterForImmediate) {
        bg.b0 = b0; bg.b1 = b1; bg.b2 = b2; bg.a1 = a1; bg.a2 = a2;
        bg.tB0 = b0; bg.tB1 = b1; bg.tB2 = b2; bg.tA1 = a1; bg.tA2 = a2;
        bg.smoothing = false;
    } else {
        bg.tB0 = b0; bg.tB1 = b1; bg.tB2 = b2; bg.tA1 = a1; bg.tA2 = a2;
        if (bg.b0 != bg.tB0 || bg.b1 != bg.tB1 || bg.b2 != bg.tB2 ||
            bg.a1 != bg.tA1 || bg.a2 != bg.tA2) {
            bg.smoothing = true;
        } else {
            bg.smoothing = false;
        }
    }
}

// BACON_1.5_EQ8_SOFTKNEE (U2.59, feedback #12): see the constants above;
// the old per-block scale-to-2.0 limiter is gone.  The knee runs inline
// per sample (64-bit), after the band loop.
// BACON_1.5_EQ8_CLICKFREE (U2.62, feedback #14): per-frame state drain used
// while a band closes (targets == identity).  The coefficient morph alone
// would leave the biquad's memory frozen (the near-identity recurrence
// stops driving it), and the exact-identity dynamics would then dump that
// residue over 2 samples - a pop of the removed signal's amplitude.  The
// drain scales the state by (1/32)^(1/42) each frame, so after the ~42
// frames of the morph the residue is ~A/32 (a 2-sample discharge ~40 dB
// below the removed component - inaudible) and the output path is
// continuous throughout (the released energy spreads over the whole morph,
// no step anywhere).
static const fixed kCloseFadeQ15 = 30172;  // (1/32)^(1/42) * 2^15

void InstrumentEq::Process(int channel, fixed *buffer, int frames) {
    if (frames <= 0 || !buffer) { rtViolations_++; return; }
    if (flat_) return;  // zero cost
    if (channel < 0 || channel >= kMaxChannels) { rtViolations_++; return; }
    static int dbgPass = 0;
    dbgPass++;
    if (dbgPass % 2 == 0) {
        const BandCfg &bg = bandCfg_[0];
        printf("DBG active b0=%d b1=%d b2=%d a1=%d a2=%d smoothing=%d\n",
                    (int)bg.b0, (int)bg.b1, (int)bg.b2, (int)bg.a1, (int)bg.a2,
                    (int)bg.smoothing);
    }

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
                for (int s = 0; s < 8; s++) {
                    ChanState &st = state_[channel][b][s];
                    // 64-bit: the states are 2^24-scale (see ChanState).
                    st.s1L = (st.s1L * (long long)r) >> 15;
                    st.s2L = (st.s2L * (long long)r) >> 15;
                    st.s1R = (st.s1R * (long long)r) >> 15;
                    st.s2R = (st.s2R * (long long)r) >> 15;
                }
            }
        }
        // BACON_1.5_EQ8_STRUCTURAL: per-frame exponential coefficient blend
        // (only while a band is converging).  The diffs are 64-bit: the Q24
        // identity b0 (1<<24) minus a target can span the whole int32 range.
        for (int b = 0; b < kNumBands; b++) {
            BandCfg &bg = bandCfg_[b];
            if (!bg.smoothing) continue;
            long long d0 = (long long)bg.tB0 - (long long)bg.b0;
            long long d1 = (long long)bg.tB1 - (long long)bg.b1;
            long long d2 = (long long)bg.tB2 - (long long)bg.b2;
            long long dA1 = (long long)bg.tA1 - (long long)bg.a1;
            long long dA2 = (long long)bg.tA2 - (long long)bg.a2;
            bg.b0 += (fixed)(d0 >> kSmoothShift);
            bg.b1 += (fixed)(d1 >> kSmoothShift);
            bg.b2 += (fixed)(d2 >> kSmoothShift);
            bg.a1 += (fixed)(dA1 >> kSmoothShift);
            bg.a2 += (fixed)(dA2 >> kSmoothShift);
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
            // BACON_1.5_EQ8_CLICKFREE (U2.62): while a band is closing
            // (targets == identity) drain its state on ALL channels and
            // stages, so no residue is left for the identity dynamics to
            // dump (see the constant above).
            if (bg.tB0 == kIdentityB0 && bg.tB1 == 0 && bg.tB2 == 0 &&
                bg.tA1 == 0 && bg.tA2 == 0) {
                for (int c = 0; c < kMaxChannels; c++) {
                    for (int s = 0; s < 8; s++) {
                        ChanState &st = state_[c][b][s];
                        // 64-bit: the states are 2^24-scale (see ChanState).
                        st.s1L = (st.s1L * (long long)kCloseFadeQ15) >> 15;
                        st.s2L = (st.s2L * (long long)kCloseFadeQ15) >> 15;
                        st.s1R = (st.s1R * (long long)kCloseFadeQ15) >> 15;
                        st.s2R = (st.s2R * (long long)kCloseFadeQ15) >> 15;
                    }
                }
            }
        }

        fixed xL = buffer[idx];
        fixed xR = buffer[idx + 1];
        for (int b = 0; b < kNumBands; b++) {
            const BandCfg &bg = bandCfg_[b];
            // BACON_1.5_EQ8_0DB_TRANSPARENT: BELL/SHELF at 0 dB are transparent,
            // but filter types (LP/HP/BP/NOTCH) must run at 0 dB when enabled
            // (user: LP/HP should cut at 0 dB, not allow ±dB).
            bool isFilter = (bg.type == TYPE_LOW_PASS ||
                             bg.type == TYPE_HIGH_PASS ||
                             bg.type == TYPE_BAND_PASS ||
                             bg.type == TYPE_NOTCH);
            if ((!bg.enabled || (!isFilter && bg.db == 0)) && !bg.smoothing) continue;
            ChanState &st = state_[channel][b][0];
            // BACON_1.5_EQ8_DF2_64BIT: transposed Df2 state update in 64 bits.
            // With full-scale input and EQ boosts the per-term values can
            // each approach +/-2^31, so a 32-bit sum overflows (verified
            // under UBSAN; on the device any edit "killed" the sample sound:
            // U2.52.5).  The final value fits in 32 bits, so the truncation
            // is exact.
            // BACON_1.5_EQ8_SLOPE_PRECISION (U2.62): the b0..b2 numerators
            // AND the a1..a2 denominators are Q24 (see EqBiquad.h).  The
            // STATES run at 2^24 (9 fractional bits below the signal): the
            // numerator products (b0..b2, 2^24 * 2^15 = 2^39, >>15 to 2^24)
            // and the denominator products (a1..a2, 2^24 * 2^24 = 2^48,
            // >>24 to 2^24) accumulate in the extended states, truncated to
            // the signal scale ONLY at the output.  In the old Q15 layout
            // the 80 Hz LP numerator terms (b1 = 1.79 Q15) truncated to
            // (0,1,0) and the filter became a resonator (see EqBiquad.h).
            // BACON_1.5_EQ8_SLOPE_ROUND (U2.62): every shift ROUNDS (+half)
            // instead of flooring.  With floor, the >>9 at the output biases
            // the loop by ~0.5 Q15/sample; the quantized Q15 denominator is
            // ~9.2e-5 at DC (ill-conditioned), so the bias lands as a ~0.17
            // DC offset (|H(200)| measured 0.51 instead of 0.167).  Rounding
            // makes the bias zero-mean.
            // BACON_1.5_EQ8_DF2_FULL24 (U2.62): the state feedback uses the
            // FULL 2^24 sum `y`, NOT the >>9 output t.  Feeding back t
            // dropped 9 bits of state resolution every sample; with the
            // ill-conditioned DC denominator (1+a1+a2 ~ 1.8e-4 for a 100 Hz
            // bell, tau ~5500 frames) that loss excited a slow ~8 Hz mode
            // that grew to +/-0.06 on a slope-2 cascade.  With y at 2^24 the
            // a-terms shift >>24 (a1*2^24 * y*2^24 = 2^48), the loop is
            // closed at full precision and the >>9 happens only at the
            // output readback.
            {
                long long yL =
                    (((long long)bg.b0 * (long long)xL + 16384) >> 15) +
                    st.s1L;
                fixed tL = (fixed)((yL + 256) >> 9);
                st.s1L = (((long long)bg.b1 * (long long)xL + 16384) >> 15) -
                         (((long long)bg.a1 * (long long)yL + (1 << 23)) >> 24) +
                         st.s2L;
                st.s2L = (((long long)bg.b2 * (long long)xL + 16384) >> 15) -
                         (((long long)bg.a2 * (long long)yL + (1 << 23)) >> 24);
                long long yR =
                    (((long long)bg.b0 * (long long)xR + 16384) >> 15) +
                    st.s1R;
                fixed tR = (fixed)((yR + 256) >> 9);
                st.s1R = (((long long)bg.b1 * (long long)xR + 16384) >> 15) -
                         (((long long)bg.a1 * (long long)yR + (1 << 23)) >> 24) +
                         st.s2R;
                st.s2R = (((long long)bg.b2 * (long long)xR + 16384) >> 15) -
                         (((long long)bg.a2 * (long long)yR + (1 << 23)) >> 24);
                // BACON_1.5_EQ8_SLOPE96 (U2.65): slope 1..8 = 12..96 dB/oct,
                // todos los tipos incluido BELL (campana más pronunciada).
                if (bg.slope > 1) {
                    fixed tL_cascade = tL;
                    fixed tR_cascade = tR;
                    for (int stage = 1; stage < bg.slope; stage++) {
                        ChanState &st2 = state_[channel][b][stage];
                        long long yL2 =
                            (((long long)bg.b0 * (long long)tL_cascade + 16384) >> 15) +
                            st2.s1L;
                        fixed tL2 = (fixed)((yL2 + 256) >> 9);
                        st2.s1L =
                            (((long long)bg.b1 * (long long)tL_cascade + 16384) >> 15) -
                            (((long long)bg.a1 * (long long)yL2 + (1 << 23)) >> 24) +
                            st2.s2L;
                        st2.s2L =
                            (((long long)bg.b2 * (long long)tL_cascade + 16384) >> 15) -
                            (((long long)bg.a2 * (long long)yL2 + (1 << 23)) >> 24);
                        long long yR2 =
                            (((long long)bg.b0 * (long long)tR_cascade + 16384) >> 15) +
                            st2.s1R;
                        fixed tR2 = (fixed)((yR2 + 256) >> 9);
                        st2.s1R =
                            (((long long)bg.b1 * (long long)tR_cascade + 16384) >> 15) -
                            (((long long)bg.a1 * (long long)yR2 + (1 << 23)) >> 24) +
                            st2.s2R;
                        st2.s2R =
                            (((long long)bg.b2 * (long long)tR_cascade + 16384) >> 15) -
                            (((long long)bg.a2 * (long long)yR2 + (1 << 23)) >> 24);
                        tL_cascade = tL2;
                        tR_cascade = tR2;
                    }
                    tL = tL_cascade;
                    tR = tR_cascade;
                }
                xL = tL;
                xR = tR;
            }
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
    // BACON_1.5_EQ8_CLICKFREE (U2.62, feedback #14): a close-transition can
    // converge in the middle of playback; re-evaluate the flat flag so the
    // very next Process() call bounces instead of looping through a fully
    // transparent band set.  refreshFlat() is pure (no audio effects), and
    // the per-band skip above keeps the remaining frames of THIS buffer
    // transparent anyway.
    if (!flat_) refreshFlat();
}

} // namespace FxEngine