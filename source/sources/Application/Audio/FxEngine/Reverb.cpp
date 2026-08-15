#include "Reverb.h"
#include <math.h>

namespace FxEngine {

#define FX_REVERB_MIX_SMOOTH fl2fp(0.0005f)
// Per-sample comb gain glide (bacon-1.5 item 3): RT60 changes step at control
// rate (8 powf max per change event, never per audio buffer) and the applied
// gain glides here, so the tail evolves smoothly without trascendentals in
// the audio callback.
#define FX_REVERB_GAIN_SMOOTH fl2fp(0.0005f)
// Per-sample decay glide for the readback value (fast: 0.05..8 s covered in
// ~30 ms; the audible transition is driven by the gain glide above).
#define FX_REVERB_DECAY_GLIDE fl2fp(0.005f)
// Predelay / comb / allpass length glide per sample (bacon-1.5 item 3):
// click-free size and predelay changes.
#define FX_REVERB_LEN_GLIDE fl2fp(1.0f)
#define FX_REVERB_MIN_RT60 0.05f   // seconds
#define FX_REVERB_MAX_RT60 8.0f
// Modulation LFO: ~0.2 Hz, +/-2 samples on the comb read lengths.
#define FX_REVERB_MOD_DEPTH fl2fp(2.0f)
#define FX_REVERB_LFO_PHASE_BITS 24
// Fixed ring headroom (samples) so the comb read pointer (length glide +
// LFO modulation) always reaches a written slot before the write pointer
// wraps over it.
#define FX_REVERB_RING_HEADROOM 4

// RC2 (section 3.2): fixed -3 dB input headroom so the network never relies
// on the hard clamp for level control.
#define FX_REVERB_INPUT_HEADROOM fl2fp(0.70710678f)

// Comb lengths (in samples at ~44.1k/48k base) for L and R channels.
// Slightly de-correlated L/R lengths give a wide stereo image.
static const int kCombBase[4] = {1116, 1188, 1277, 1356};
static const int kCombBaseR[4] = {1131, 1203, 1293, 1377};
// RC2 (section 3.3): a third series allpass per channel (Dattorro-inspired
// short diffuser) thickens the tail and reduces metallic ringing in NORMAL.
static const int kAllpassBase[3] = {556, 441, 225};
static const int kAllpassBaseR[3] = {561, 445, 231};

// bacon-1.5 item 3: 64-entry Q15 sine table (modulation LFO).  Generated once
// (sin(2*pi*k/64)*32768); the LFO phase is a Q24 accumulator and the table is
// linearly interpolated with integer math, so Process() has zero
// trascendentals.
static const fixed kLfoTable[64] = {
    0, 3212, 6393, 9512, 12540, 15447, 18205, 20788,
    23170, 25330, 27246, 28899, 30274, 31357, 32138, 32610,
    32768, 32610, 32138, 31357, 30274, 28899, 27246, 25330,
    23170, 20788, 18205, 15447, 12540, 9512, 6393, 3212,
    0, -3212, -6393, -9512, -12540, -15447, -18205, -20788,
    -23170, -25330, -27246, -28899, -30274, -31357, -32138, -32610,
    -32768, -32610, -32138, -31357, -30274, -28899, -27246, -25330,
    -23170, -20788, -18205, -15447, -12540, -9512, -6393, -3212
};

Reverb::Reverb()
    : predelayWrite_(0), predelayLenF_(0), predelayTargetF_(0),
      combNorm_(i2fp(1)),
      inLpCoeff_(0), inHpCoeff_(0), inputLpHz_(fl2fp(20000.0f)),
      inputHpHz_(fl2fp(20.0f)), dcCoeff_(fl2fp(0.9995f)),
      lfoPhase_(0), lfoInc_(70),
      rate_(44100), mode_(NORMAL),
      predelayMs_(0), decay_(fl2fp(1.0f)), decayTarget_(fl2fp(1.0f)),
      decayDirty_(false),
      size_(i2fp(1)), damping_(fl2fp(0.5f)), width_(i2fp(1)),
      bypass_(false), mix_(i2fp(1)), mixCur_(i2fp(1)),
      rtViolations_(0) {
    for (int i = 0; i < 2; i++) {
        inLpState_[i] = inHpState_[i] = dcState_[i] = 0;
    }
    for (int i = 0; i < kNumCombs; i++) {
        combIdx_[i] = 0;
        combLenF_[i] = fl2fp(1);
        combLenTargetF_[i] = fl2fp(1);
        combGain_[i] = 0;
        combGainTarget_[i] = 0;
        combDamp_[i] = 0;
        combState_[i] = 0;
        for (int j = 0; j < kCombMaxLen; j++) combBuf_[i][j] = 0;
    }
    for (int i = 0; i < kNumAllpass; i++) {
        apIdx_[i] = 0;
        apLenF_[i] = fl2fp(1);
        apLenTargetF_[i] = fl2fp(1);
        for (int j = 0; j < kAllpassMaxLen; j++) allpassBuf_[i][j] = 0;
    }
    for (int i = 0; i < kPredelayMax * 2; i++) predelay_[i] = 0;
    SetSampleRate(44100);
    SetMode(NORMAL);
    recomputeCombs();
}

void Reverb::Reset() {
    predelayWrite_ = 0;
    predelayLenF_ = predelayTargetF_;
    for (int i = 0; i < kPredelayMax * 2; i++) predelay_[i] = 0;
    for (int i = 0; i < kNumCombs; i++) {
        combIdx_[i] = 0;
        combLenF_[i] = combLenTargetF_[i];
        combGain_[i] = combGainTarget_[i];
        combState_[i] = 0;
        for (int j = 0; j < kCombMaxLen; j++) combBuf_[i][j] = 0;
    }
    for (int i = 0; i < kNumAllpass; i++) {
        apIdx_[i] = 0;
        apLenF_[i] = apLenTargetF_[i];
        for (int j = 0; j < kAllpassMaxLen; j++) allpassBuf_[i][j] = 0;
    }
    for (int i = 0; i < 2; i++) {
        inLpState_[i] = inHpState_[i] = dcState_[i] = 0;
    }
    lfoPhase_ = 0;
    // Wet-only (RC2): the smoothed wet gain starts at full wet; the legacy
    // persisted mix_ no longer shapes the output.
    mixCur_ = i2fp(1);
    decay_ = decayTarget_;
}

void Reverb::SetSampleRate(int rate) {
    if (rate < 8000 || rate > 96000) {
        ++rtViolations_;
        return;
    }
    rate_ = rate;
    // bacon-1.5 item 3: recompute the input filter coefficients from the
    // stored frequencies (legacy bug: they were zeroed here and stayed dead
    // until the user re-set the filters).
    setInputHPCoeff(fp2fl(inputHpHz_));
    setInputLPCoeff(fp2fl(inputLpHz_));
    dcCoeff_ = fl2fp(0.9995f);
    // LFO rate: ~0.2 Hz regardless of the sample rate.
    lfoInc_ = (int)(0.2f * 16777216.0f / (float)rate_);
    if (lfoInc_ < 1) lfoInc_ = 1;
    // Predelay length and comb/allpass lengths depend on rate: keep the same
    // *time* by scaling base lengths to the new rate.
    recomputeCombs();
}

void Reverb::SetPredelayMs(fixed ms) {
    float m = fp2fl(ms);
    if (m < 0.0f) m = 0.0f;
    if (m > 100.0f) m = 100.0f;
    predelayMs_ = fl2fp(m);
    float len = m * (float)rate_ / 1000.0f;
    if (len > (float)kPredelayMax) len = (float)kPredelayMax;
    if (len < 0.0f) len = 0.0f;
    predelayTargetF_ = fl2fp(len);
}

void Reverb::SetDecay(fixed rt60Seconds) {
    float rt = fp2fl(rt60Seconds);
    if (rt < FX_REVERB_MIN_RT60) rt = FX_REVERB_MIN_RT60;
    if (rt > FX_REVERB_MAX_RT60) rt = FX_REVERB_MAX_RT60;
    decayTarget_ = fl2fp(rt);
    // bacon-1.5 item 3: the comb gains are recomputed once here (control
    // rate, 8 powf) instead of every Process() call while gliding.
    decayDirty_ = true;
}

void Reverb::SetSize(fixed size) {
    float s = fp2fl(size);
    if (s < 0.5f) s = 0.5f;
    if (s > 1.5f) s = 1.5f;
    size_ = fl2fp(s);
    recomputeCombs();
}

void Reverb::SetDamping(fixed damping) {
    float d = fp2fl(damping);
    if (d < 0.0f) d = 0.0f;
    if (d > 1.0f) d = 1.0f;
    damping_ = fl2fp(d);
    // bacon-1.5 item 3: propagate to the per-comb loop one-poles so damping
    // changes apply immediately (and glide-free: the comb state is bounded).
    // The knob is "how much lowpass" (0 = brightest), so the one-pole
    // coefficient is (1 - damping): the loop must keep full amplitude when
    // the knob is at 0 (a raw 0 would freeze the state and kill the tail).
    for (int i = 0; i < kNumCombs; i++) combDamp_[i] = i2fp(1) - damping_;
}

void Reverb::setInputHPCoeff(float hz) {
    // Open threshold: <= 30 Hz disables the HP (legacy open state).
    if (hz <= 0.0f || hz <= 30.0f) {
        inHpCoeff_ = 0;
    } else {
        inHpCoeff_ = fl2fp(expf(-2.0f * 3.14159265f * hz / (float)rate_));
    }
}

void Reverb::setInputLPCoeff(float hz) {
    // Open threshold: >= 19000 Hz disables the LP (legacy open state).
    if (hz <= 0.0f || hz >= 19000.0f) {
        inLpCoeff_ = 0;
    } else {
        inLpCoeff_ = fl2fp(1.0f - expf(-2.0f * 3.14159265f * hz / (float)rate_));
    }
}

void Reverb::SetInputHP(fixed hz) {
    float f = fp2fl(hz);
    if (f < 0.0f) f = 0.0f;
    if (f > 20000.0f) f = 20000.0f;
    inputHpHz_ = fl2fp(f);
    setInputHPCoeff(f);
}

void Reverb::SetInputLP(fixed hz) {
    float f = fp2fl(hz);
    if (f < 0.0f) f = 0.0f;
    if (f > 20000.0f) f = 20000.0f;
    inputLpHz_ = fl2fp(f);
    setInputLPCoeff(f);
}

void Reverb::SetWidth(fixed width) {
    float w = fp2fl(width);
    if (w < 0.0f) w = 0.0f;
    if (w > 1.0f) w = 1.0f;
    width_ = fl2fp(w);
}

void Reverb::SetMode(Mode mode) {
    mode_ = (mode == ECO) ? ECO : NORMAL;
    recomputeCombs();
}

void Reverb::SetBypass(bool on) { bypass_ = on; }

void Reverb::SetMix(fixed mix) {
    float m = fp2fl(mix);
    if (m < 0.0f) m = 0.0f;
    if (m > 1.0f) m = 1.0f;
    // RC2 (section 3.1): stored for persistence/migration only; the DSP is
    // fixed 100% wet and never reintroduces dry into the return.
    mix_ = fl2fp(m);
}

void Reverb::recomputeCombs() {
    // Base lengths scaled by rate (relative to 44100) and by size_.
    float rateScale = (float)rate_ / 44100.0f;
    float sizeScale = fp2fl(size_);

    // ECO: 2 combs + 1 allpass per channel; NORMAL: 4 combs + 3 allpasses
    // (RC2, section 3.3: denser diffusion).
    int combs = (mode_ == ECO) ? 2 : 4;
    int aps = (mode_ == ECO) ? 1 : 3;

    // RC2 (section 3.2): normalized parallel-comb sum.
    combNorm_ = i2fp(1) / combs;

    // Fill L combs (0..combs-1) and R combs (kNumCombs/2 .. +combs-1).
    // bacon-1.5 item 3: lengths become targets; Process() glides per sample.
    for (int c = 0; c < combs; c++) {
        int lenL = (int)(kCombBase[c] * rateScale * sizeScale);
        int lenR = (int)(kCombBaseR[c] * rateScale * sizeScale);
        if (lenL < 8) lenL = 8;
        if (lenR < 8) lenR = 8;
        if (lenL > kCombMaxLen - FX_REVERB_RING_HEADROOM) lenL = kCombMaxLen - FX_REVERB_RING_HEADROOM;
        if (lenR > kCombMaxLen - FX_REVERB_RING_HEADROOM) lenR = kCombMaxLen - FX_REVERB_RING_HEADROOM;
        combLenTargetF_[c] = fl2fp((float)lenL);
        combLenTargetF_[kNumCombs / 2 + c] = fl2fp((float)lenR);
        // Damping one-pole coefficient is (1 - damping): the knob means "how
        // much lowpass" (0 = brightest), the one-pole state must track the
        // comb output fully when damping is 0 (a coefficient of 0 would
        // freeze the state and kill the feedback loop entirely).
        combDamp_[c] = combDamp_[kNumCombs / 2 + c] = i2fp(1) - damping_;
        combGainTarget_[c] = combGainTarget_[kNumCombs / 2 + c] = 0;
    }
    // Zero out unused combs (inactive: the Process loop only walks nCombs).
    for (int c = combs; c < kNumCombs / 2; c++) {
        combLenTargetF_[c] = fl2fp(1);
        combLenTargetF_[kNumCombs / 2 + c] = fl2fp(1);
    }

    for (int c = 0; c < aps; c++) {
        int lenL = (int)(kAllpassBase[c] * rateScale * sizeScale);
        int lenR = (int)(kAllpassBaseR[c] * rateScale * sizeScale);
        if (lenL < 4) lenL = 4;
        if (lenR < 4) lenR = 4;
        if (lenL > kAllpassMaxLen) lenL = kAllpassMaxLen;
        if (lenR > kAllpassMaxLen) lenR = kAllpassMaxLen;
        apLenTargetF_[c] = fl2fp((float)lenL);
        apLenTargetF_[kNumAllpass / 2 + c] = fl2fp((float)lenR);
    }
    for (int c = aps; c < kNumAllpass / 2; c++) {
        apLenTargetF_[c] = fl2fp(1);
        apLenTargetF_[kNumAllpass / 2 + c] = fl2fp(1);
    }

    recomputeGains();
    // Note: this runs at control-rate (parameter changes), not in Process().
}

void Reverb::recomputeGains() {
    // Uses decayTarget_ (not the gliding readback decay_): SetDecay() marks
    // dirty and recomputes at control rate; the comb gain glide handles the
    // audible transition, so the target must be exact from the first sample.
    float rt = fp2fl(decayTarget_);
    if (rt < FX_REVERB_MIN_RT60) rt = FX_REVERB_MIN_RT60;
    if (rt > FX_REVERB_MAX_RT60) rt = FX_REVERB_MAX_RT60;
    // g = 10^(-3*L / (RT60 * fs)) -- the only powf() in the module, called
    // at control rate (decay/size/mode/sample-rate changes), never per audio
    // buffer (bacon-1.5 item 3).  Uses the TARGET comb length (the gliding
    // current length would under-compute the gain while the length is still
    // moving from its 1-sample reset value).
    for (int i = 0; i < kNumCombs; i++) {
        int L = fp2i(combLenTargetF_[i]);
        if (L < 1) L = 1;
        float g = powf(10.0f, -3.0f * (float)L / (rt * (float)rate_));
        combGainTarget_[i] = fl2fp(g);
    }
}

fixed Reverb::saturate(fixed x) {
    if (x > i2fp(1)) return i2fp(1);
    if (x < -i2fp(1)) return -i2fp(1);
    return x;
}

fixed Reverb::inputFilter(fixed x, int ch) {
    if (inLpCoeff_ != 0) {
        inLpState_[ch] = inLpState_[ch] + fp_mul(inLpCoeff_, x - inLpState_[ch]);
        x = inLpState_[ch];
    }
    if (inHpCoeff_ != 0) {
        fixed xm = inHpState_[ch];
        inHpState_[ch] = xm + fp_mul(inHpCoeff_, x - xm);
        x = x - inHpState_[ch];
    }
    return x;
}

void Reverb::Process(const fixed *in, fixed *out, int frames) {
    if (frames <= 0 || !in || !out) {
        ++rtViolations_;
        return;
    }

    // bacon-1.5 item 3: RT60 gains recompute once per decay/size/mode change
    // (control rate), never while gliding; the applied gains glide per-sample
    // below, so Process() contains zero powf()/trascendentals.
    if (decayDirty_) {
        recomputeGains();
        decayDirty_ = false;
    }
    // Per-sample decay glide (readback only; the audible motion comes from
    // the comb gain glide).
    if (decay_ < decayTarget_) {
        decay_ += FX_REVERB_DECAY_GLIDE;
        if (decay_ > decayTarget_) decay_ = decayTarget_;
    } else if (decay_ > decayTarget_) {
        decay_ -= FX_REVERB_DECAY_GLIDE;
        if (decay_ < decayTarget_) decay_ = decayTarget_;
    }

    int idx = 0;
    // RC2 (section 3.1): wet-only send/return.
    fixed targetGain = bypass_ ? 0 : i2fp(1);
    for (int i = 0; i < frames; i++) {
        // Per-sample wet gain glide (must be inside the frame loop: a
        // per-chunk glide would need ~2000 Process() calls to mute the wet).
        // The Q15 fp_mul rounds deltas < ~2048 units to 0, which would stall
        // the glide ~3% short of the target (audible leak while bypassed); a
        // minimum step of 1 unit walks the gain all the way to the target.
        fixed mixStep = fp_mul(FX_REVERB_MIX_SMOOTH, targetGain - mixCur_);
        if (mixStep == 0) {
            if (mixCur_ < targetGain) mixStep = 1;
            else if (mixCur_ > targetGain) mixStep = -1;
        }
        mixCur_ = mixCur_ + mixStep;
        fixed wetGain = mixCur_;
        // Per-sample length glides (predelay/comb/allpass): click-free
        // predelay and size changes (bacon-1.5 item 3).
        if (predelayLenF_ < predelayTargetF_) {
            predelayLenF_ += FX_REVERB_LEN_GLIDE;
            if (predelayLenF_ > predelayTargetF_) predelayLenF_ = predelayTargetF_;
        } else if (predelayLenF_ > predelayTargetF_) {
            predelayLenF_ -= FX_REVERB_LEN_GLIDE;
            if (predelayLenF_ < predelayTargetF_) predelayLenF_ = predelayTargetF_;
        }

        // Modulation LFO (Q24 phase accumulator + 64-entry Q15 sine table,
        // integer interpolation -- zero trascendentals).
        lfoPhase_ += lfoInc_;
        lfoPhase_ &= (1 << FX_REVERB_LFO_PHASE_BITS) - 1;
        int lfoIdx = lfoPhase_ >> (FX_REVERB_LFO_PHASE_BITS - 6);
        int lfoFrac = (lfoPhase_ >> (FX_REVERB_LFO_PHASE_BITS - 12)) & 0x3F;
        fixed lfo = kLfoTable[lfoIdx];
        fixed lfoNext = kLfoTable[(lfoIdx + 1) & 63];
        lfo += (lfoNext - lfo) * lfoFrac >> 6;

        // RC2 (section 3.2): fixed -3 dB input headroom.
        fixed xL = saturate(fp_mul(saturate(inputFilter(in[idx], 0)),
                                   FX_REVERB_INPUT_HEADROOM));
        fixed xR = saturate(fp_mul(saturate(inputFilter(in[idx + 1], 1)),
                                   FX_REVERB_INPUT_HEADROOM));

        // Predelay write + fractional read.
        predelay_[predelayWrite_] = xL;
        predelay_[predelayWrite_ + 1] = xR;
        int pLen = fp2i(predelayLenF_);
        fixed pFrac = predelayLenF_ - i2fp(pLen);
        int readPos = predelayWrite_ - 2 * pLen;
        while (readPos < 0) readPos += kPredelayMax * 2;
        int readPosB = readPos - 2;
        if (readPosB < 0) readPosB += kPredelayMax * 2;
        fixed pL = predelay_[readPos]
                   + fp_mul(predelay_[readPosB] - predelay_[readPos], pFrac);
        fixed pR = predelay_[readPos + 1]
                   + fp_mul(predelay_[readPosB + 1] - predelay_[readPos + 1], pFrac);

        // Parallel combs (L and R banks) with fractional reads, per-sample
        // gain glide and length glide + LFO shimmer (opposite phase L/R).
        fixed sumL = 0;
        fixed sumR = 0;
        int nCombs = (mode_ == ECO) ? 2 : 4;
        for (int c = 0; c < nCombs; c++) {
            // L comb
            {
                // Glide length toward target.
                if (combLenF_[c] < combLenTargetF_[c]) {
                    combLenF_[c] += FX_REVERB_LEN_GLIDE;
                    if (combLenF_[c] > combLenTargetF_[c]) combLenF_[c] = combLenTargetF_[c];
                } else if (combLenF_[c] > combLenTargetF_[c]) {
                    combLenF_[c] -= FX_REVERB_LEN_GLIDE;
                    if (combLenF_[c] < combLenTargetF_[c]) combLenF_[c] = combLenTargetF_[c];
                }
                // Glide applied gain toward RT60 target.  The Q15 fp_mul
                // rounds to 0 for deltas < ~2048 units, which would stall the
                // gain ~6% below the target; a minimum step of 1 unit walks
                // the gain all the way to the target.
                fixed gainStep = fp_mul(FX_REVERB_GAIN_SMOOTH,
                                        combGainTarget_[c] - combGain_[c]);
                if (gainStep == 0) {
                    if (combGain_[c] < combGainTarget_[c]) gainStep = 1;
                    else if (combGain_[c] > combGainTarget_[c]) gainStep = -1;
                }
                combGain_[c] = combGain_[c] + gainStep;
                // Effective modulated length (L bank +lfo, R bank -lfo).
                fixed mod = fp_mul(lfo, FX_REVERB_MOD_DEPTH);
                fixed eff = combLenF_[c] + mod;
                // The ring wraps at TARGET + headroom (constant while the
                // length glides).  The read pointer = combIdx - len0 must be
                // able to reach a slot before the write pointer wraps over
                // it; the LFO modulation (+/-MOD_DEPTH) delays the read by a
                // couple of samples, so a fixed 4-sample headroom keeps the
                // first arrival alive instead of being overwritten unread.
                int ring = fp2i(combLenTargetF_[c]) + FX_REVERB_RING_HEADROOM;
                if (ring < 8) ring = 8;
                fixed effClamped = eff;
                if (effClamped < fl2fp(1)) effClamped = fl2fp(1);
                if (effClamped > fl2fp(ring - 1)) effClamped = fl2fp(ring - 1);
                int len0 = fp2i(effClamped);
                fixed frac = effClamped - i2fp(len0);

                int iL = combIdx_[c] - len0;
                while (iL < 0) iL += ring;
                int iLb = iL - 1;
                if (iLb < 0) iLb += ring;
                fixed outL = combBuf_[c][iL]
                             + fp_mul(combBuf_[c][iLb] - combBuf_[c][iL], frac);
                // damping one-pole in the loop
                combState_[c] = combState_[c]
                                + fp_mul(combDamp_[c], outL - combState_[c]);
                int wL = combIdx_[c];
                combBuf_[c][wL] = saturate(pL + fp_mul(combGain_[c], combState_[c]));
                wL++;
                if (wL >= ring) wL = 0;
                combIdx_[c] = wL;
                sumL += outL;
            }
            // R comb (bank offset kNumCombs/2), LFO in counter-phase
            {
                int rc = kNumCombs / 2 + c;
                if (combLenF_[rc] < combLenTargetF_[rc]) {
                    combLenF_[rc] += FX_REVERB_LEN_GLIDE;
                    if (combLenF_[rc] > combLenTargetF_[rc]) combLenF_[rc] = combLenTargetF_[rc];
                } else if (combLenF_[rc] > combLenTargetF_[rc]) {
                    combLenF_[rc] -= FX_REVERB_LEN_GLIDE;
                    if (combLenF_[rc] < combLenTargetF_[rc]) combLenF_[rc] = combLenTargetF_[rc];
                }
                fixed gainStepR = fp_mul(FX_REVERB_GAIN_SMOOTH,
                                         combGainTarget_[rc] - combGain_[rc]);
                if (gainStepR == 0) {
                    if (combGain_[rc] < combGainTarget_[rc]) gainStepR = 1;
                    else if (combGain_[rc] > combGainTarget_[rc]) gainStepR = -1;
                }
                combGain_[rc] = combGain_[rc] + gainStepR;
                fixed mod = -fp_mul(lfo, FX_REVERB_MOD_DEPTH);
                fixed eff = combLenF_[rc] + mod;
                int ring = fp2i(combLenTargetF_[rc]) + FX_REVERB_RING_HEADROOM;
                if (ring < 8) ring = 8;
                fixed effClamped = eff;
                if (effClamped < fl2fp(1)) effClamped = fl2fp(1);
                if (effClamped > fl2fp(ring - 1)) effClamped = fl2fp(ring - 1);
                int len0 = fp2i(effClamped);
                fixed frac = effClamped - i2fp(len0);

                int iR = combIdx_[rc] - len0;
                while (iR < 0) iR += ring;
                int iRb = iR - 1;
                if (iRb < 0) iRb += ring;
                fixed outR = combBuf_[rc][iR]
                             + fp_mul(combBuf_[rc][iRb] - combBuf_[rc][iR], frac);
                combState_[rc] = combState_[rc]
                                 + fp_mul(combDamp_[rc], outR - combState_[rc]);
                int wR = combIdx_[rc];
                combBuf_[rc][wR] = saturate(pR + fp_mul(combGain_[rc], combState_[rc]));
                wR++;
                if (wR >= ring) wR = 0;
                combIdx_[rc] = wR;
                sumR += outR;
            }
        }
        // RC2 (section 3.2): normalized sum (headroom before the allpasses).
        sumL = saturate(fp_mul(sumL, combNorm_));
        sumR = saturate(fp_mul(sumR, combNorm_));

        // Series allpass diffusers (L and R banks) on the comb sum, with
        // fractional reads (length glide stays click-free).
        int nAps = (mode_ == ECO) ? 1 : 3;
        fixed wetL = sumL;
        fixed wetR = sumR;
        for (int c = 0; c < nAps; c++) {
            // L allpass
            {
                if (apLenF_[c] < apLenTargetF_[c]) {
                    apLenF_[c] += FX_REVERB_LEN_GLIDE;
                    if (apLenF_[c] > apLenTargetF_[c]) apLenF_[c] = apLenTargetF_[c];
                } else if (apLenF_[c] > apLenTargetF_[c]) {
                    apLenF_[c] -= FX_REVERB_LEN_GLIDE;
                    if (apLenF_[c] < apLenTargetF_[c]) apLenF_[c] = apLenTargetF_[c];
                }
                int iL = apIdx_[c];
                int len = fp2i(apLenF_[c]);
                if (len < 1) len = 1;
                fixed frac = apLenF_[c] - i2fp(fp2i(apLenF_[c]));
                int iLb = iL - 1;
                if (iLb < 0) iLb += len;
                fixed bufIn = allpassBuf_[c][iL]
                              + fp_mul(allpassBuf_[c][iLb] - allpassBuf_[c][iL], frac);
                fixed y = -wetL + bufIn;
                allpassBuf_[c][iL] = wetL + fp_mul(fl2fp(0.5f), bufIn);
                wetL = y;
                iL++;
                if (iL >= len) iL = 0;
                apIdx_[c] = iL;
            }
            // R allpass
            {
                int rc = kNumAllpass / 2 + c;
                if (apLenF_[rc] < apLenTargetF_[rc]) {
                    apLenF_[rc] += FX_REVERB_LEN_GLIDE;
                    if (apLenF_[rc] > apLenTargetF_[rc]) apLenF_[rc] = apLenTargetF_[rc];
                } else if (apLenF_[rc] > apLenTargetF_[rc]) {
                    apLenF_[rc] -= FX_REVERB_LEN_GLIDE;
                    if (apLenF_[rc] < apLenTargetF_[rc]) apLenF_[rc] = apLenTargetF_[rc];
                }
                int iR = apIdx_[rc];
                int len = fp2i(apLenF_[rc]);
                if (len < 1) len = 1;
                fixed frac = apLenF_[rc] - i2fp(fp2i(apLenF_[rc]));
                int iRb = iR - 1;
                if (iRb < 0) iRb += len;
                fixed bufIn = allpassBuf_[rc][iR]
                              + fp_mul(allpassBuf_[rc][iRb] - allpassBuf_[rc][iR], frac);
                fixed y = -wetR + bufIn;
                allpassBuf_[rc][iR] = wetR + fp_mul(fl2fp(0.5f), bufIn);
                wetR = y;
                iR++;
                if (iR >= len) iR = 0;
                apIdx_[rc] = iR;
            }
        }

        // Mid/side width on the wet signal.
        if (width_ != i2fp(1)) {
            fixed mid = fp_mul(wetL + wetR, fl2fp(0.5f));
            fixed side = fp_mul(wetL - wetR, fl2fp(0.5f));
            wetL = mid + fp_mul(side, width_);
            wetR = mid - fp_mul(side, width_);
        }

        // Classic DC blocker on the wet path (unit gain at high frequencies,
        // unlike the former one-pole high-pass which had ~2*(1-R) gain and
        // attenuated the whole wet signal):  y = x + s;  s' = R*s - (1-R)*x,
        // R = 0.9995 (cutoff ~3.5 Hz, removes DC buildup without eating the
        // low-frequency body of the tail).
        dcState_[0] = fp_mul(dcCoeff_, dcState_[0])
                      - fp_mul(i2fp(1) - dcCoeff_, wetL);
        wetL = wetL + dcState_[0];
        dcState_[1] = fp_mul(dcCoeff_, dcState_[1])
                      - fp_mul(i2fp(1) - dcCoeff_, wetR);
        wetR = wetR + dcState_[1];

        out[idx] = saturate(fp_mul(saturate(wetL), wetGain));
        out[idx + 1] = saturate(fp_mul(saturate(wetR), wetGain));

        predelayWrite_ += 2;
        if (predelayWrite_ >= kPredelayMax * 2) predelayWrite_ = 0;
        idx += 2;
    }
}

unsigned long Reverb::StaticMemoryBytes() {
    unsigned long n = 0;
    n += (unsigned long)kPredelayMax * 2 * sizeof(fixed);
    n += (unsigned long)kNumCombs * kCombMaxLen * sizeof(fixed);
    n += (unsigned long)kNumAllpass * kAllpassMaxLen * sizeof(fixed);
    return n;
}

} // namespace FxEngine
