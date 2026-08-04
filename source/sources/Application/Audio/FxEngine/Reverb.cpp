#include "Reverb.h"
#include <math.h>

namespace FxEngine {

#define FX_REVERB_MIX_SMOOTH fl2fp(0.001f)
#define FX_REVERB_DECAY_GLIDE fl2fp(0.005f)
#define FX_REVERB_MIN_RT60 0.05f   // seconds
#define FX_REVERB_MAX_RT60 8.0f

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

Reverb::Reverb()
    : predelayWrite_(0), predelayLen_(0),
      combNorm_(i2fp(1)),
      inLpCoeff_(0), inHpCoeff_(0), dcCoeff_(fl2fp(0.995f)),
      rate_(44100), mode_(NORMAL),
      predelayMs_(0), decay_(fl2fp(1.0f)), decayTarget_(fl2fp(1.0f)),
      size_(i2fp(1)), damping_(fl2fp(0.5f)), width_(i2fp(1)),
      bypass_(false), mix_(i2fp(1)), mixCur_(i2fp(1)),
      rtViolations_(0) {
    for (int i = 0; i < 2; i++) {
        inLpState_[i] = inHpState_[i] = dcState_[i] = 0;
    }
    for (int i = 0; i < kNumCombs; i++) {
        combIdx_[i] = 0;
        combLen_[i] = 1;
        combGain_[i] = 0;
        combDamp_[i] = 0;
        combState_[i] = 0;
        for (int j = 0; j < kCombMaxLen; j++) combBuf_[i][j] = 0;
    }
    for (int i = 0; i < kNumAllpass; i++) {
        apIdx_[i] = 0;
        apLen_[i] = 1;
        for (int j = 0; j < kAllpassMaxLen; j++) allpassBuf_[i][j] = 0;
    }
    for (int i = 0; i < kPredelayMax * 2; i++) predelay_[i] = 0;
    SetSampleRate(44100);
    SetMode(NORMAL);
    recomputeCombs();
}

void Reverb::Reset() {
    predelayWrite_ = 0;
    for (int i = 0; i < kPredelayMax * 2; i++) predelay_[i] = 0;
    for (int i = 0; i < kNumCombs; i++) {
        combIdx_[i] = 0;
        combState_[i] = 0;
        for (int j = 0; j < kCombMaxLen; j++) combBuf_[i][j] = 0;
    }
    for (int i = 0; i < kNumAllpass; i++) {
        apIdx_[i] = 0;
        for (int j = 0; j < kAllpassMaxLen; j++) allpassBuf_[i][j] = 0;
    }
    for (int i = 0; i < 2; i++) {
        inLpState_[i] = inHpState_[i] = dcState_[i] = 0;
    }
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
    // Input one-pole coefficients.
    inLpCoeff_ = inHpCoeff_ = 0; // recomputed in SetInputHP/LP if set
    dcCoeff_ = fl2fp(0.995f);
    // Predelay length and comb/allpass lengths depend on rate: keep the same
    // *time* by scaling base lengths to the new rate.
    recomputeCombs();
}

void Reverb::SetPredelayMs(fixed ms) {
    float m = fp2fl(ms);
    if (m < 0.0f) m = 0.0f;
    if (m > 100.0f) m = 100.0f;
    predelayMs_ = fl2fp(m);
    predelayLen_ = (int)(m * (float)rate_ / 1000.0f);
    if (predelayLen_ > kPredelayMax) predelayLen_ = kPredelayMax;
    if (predelayLen_ < 0) predelayLen_ = 0;
}

void Reverb::SetDecay(fixed rt60Seconds) {
    float rt = fp2fl(rt60Seconds);
    if (rt < FX_REVERB_MIN_RT60) rt = FX_REVERB_MIN_RT60;
    if (rt > FX_REVERB_MAX_RT60) rt = FX_REVERB_MAX_RT60;
    decayTarget_ = fl2fp(rt);
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
}

void Reverb::SetInputHP(fixed hz) {
    float f = fp2fl(hz);
    if (f <= 0.0f) {
        inHpCoeff_ = 0;
    } else {
        inHpCoeff_ = fl2fp(expf(-2.0f * 3.14159265f * f / (float)rate_));
    }
}

void Reverb::SetInputLP(fixed hz) {
    float f = fp2fl(hz);
    if (f <= 0.0f) {
        inLpCoeff_ = 0;
    } else {
        inLpCoeff_ = fl2fp(1.0f - expf(-2.0f * 3.14159265f * f / (float)rate_));
    }
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

    // RC2 (section 3.2): normalized parallel-comb sum.  Four combs sum to
    // roughly the level of one (headroom preserved before the allpasses)
    // instead of accumulating 4x and relying on the hard clamp.
    combNorm_ = i2fp(1) / combs;

    // Fill L combs (0..combs-1) and R combs (kNumCombs/2 .. +combs-1).
    for (int c = 0; c < combs; c++) {
        int lenL = (int)(kCombBase[c] * rateScale * sizeScale);
        int lenR = (int)(kCombBaseR[c] * rateScale * sizeScale);
        if (lenL < 8) lenL = 8;
        if (lenR < 8) lenR = 8;
        if (lenL > kCombMaxLen) lenL = kCombMaxLen;
        if (lenR > kCombMaxLen) lenR = kCombMaxLen;
        combLen_[c] = lenL;
        combLen_[kNumCombs / 2 + c] = lenR;
        combDamp_[c] = combDamp_[kNumCombs / 2 + c] = damping_;
        combGain_[c] = combGain_[kNumCombs / 2 + c] = 0; // set from RT60
    }
    // Zero out unused combs.
    for (int c = combs; c < kNumCombs / 2; c++) combLen_[c] = 1;
    for (int c = combs; c < kNumCombs / 2; c++)
        combLen_[kNumCombs / 2 + c] = 1;

    for (int c = 0; c < aps; c++) {
        int lenL = (int)(kAllpassBase[c] * rateScale * sizeScale);
        int lenR = (int)(kAllpassBaseR[c] * rateScale * sizeScale);
        if (lenL < 4) lenL = 4;
        if (lenR < 4) lenR = 4;
        if (lenL > kAllpassMaxLen) lenL = kAllpassMaxLen;
        if (lenR > kAllpassMaxLen) lenR = kAllpassMaxLen;
        apLen_[c] = lenL;
        apLen_[kNumAllpass / 2 + c] = lenR;
    }
    for (int c = aps; c < kNumAllpass / 2; c++) apLen_[c] = 1;
    for (int c = aps; c < kNumAllpass / 2; c++)
        apLen_[kNumAllpass / 2 + c] = 1;

    recomputeGains();
    // Note: this runs at control-rate (parameter changes), not in Process().
}

void Reverb::recomputeGains() {
    float rt = fp2fl(decay_);
    if (rt < FX_REVERB_MIN_RT60) rt = FX_REVERB_MIN_RT60;
    if (rt > FX_REVERB_MAX_RT60) rt = FX_REVERB_MAX_RT60;
    // g = 10^(-3*L / (RT60 * fs))
    for (int i = 0; i < kNumCombs; i++) {
        int L = combLen_[i];
        float g = powf(10.0f, -3.0f * (float)L / (rt * (float)rate_));
        combGain_[i] = fl2fp(g);
    }
}

void Reverb::glideDecay() {
    if (decay_ < decayTarget_) {
        decay_ += FX_REVERB_DECAY_GLIDE;
        if (decay_ > decayTarget_) decay_ = decayTarget_;
    } else if (decay_ > decayTarget_) {
        decay_ -= FX_REVERB_DECAY_GLIDE;
        if (decay_ < decayTarget_) decay_ = decayTarget_;
    }
    recomputeGains();
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

    glideDecay();

    // RC2 (section 3.1): wet-only send/return.  The reverb delivers only the
    // processed (wet) signal; the dry signal already lives in the master bus,
    // so there is no dry*dryMix term.  mixCur_ is the smoothed wet gain: full
    // wet at 1.0, gliding to 0 while bypassed.  The tail keeps running
    // internally and reappears when bypass is released.
    fixed targetGain = bypass_ ? 0 : i2fp(1);
    mixCur_ = mixCur_ + fp_mul(FX_REVERB_MIX_SMOOTH, targetGain - mixCur_);
    fixed wetGain = mixCur_;

    int idx = 0;
    for (int i = 0; i < frames; i++) {
        // RC2 (section 3.2): fixed -3 dB input headroom.
        fixed xL = saturate(fp_mul(saturate(inputFilter(in[idx], 0)),
                                   FX_REVERB_INPUT_HEADROOM));
        fixed xR = saturate(fp_mul(saturate(inputFilter(in[idx + 1], 1)),
                                   FX_REVERB_INPUT_HEADROOM));

        // Predelay write.
        predelay_[predelayWrite_] = xL;
        predelay_[predelayWrite_ + 1] = xR;
        int readPos = predelayWrite_ - 2 * predelayLen_;
        while (readPos < 0) readPos += kPredelayMax * 2;
        fixed pL = predelay_[readPos];
        fixed pR = predelay_[readPos + 1];

        // Parallel combs (L and R banks).
        fixed sumL = 0;
        fixed sumR = 0;
        int nCombs = (mode_ == ECO) ? 2 : 4;
        for (int c = 0; c < nCombs; c++) {
            // L comb
            {
                int iL = combIdx_[c];
                fixed outL = combBuf_[c][iL];
                // damping one-pole in the loop
                combState_[c] = combState_[c] + fp_mul(combDamp_[c], outL - combState_[c]);
                combBuf_[c][iL] = saturate(pL + fp_mul(combGain_[c], combState_[c]));
                iL++;
                if (iL >= combLen_[c]) iL = 0;
                combIdx_[c] = iL;
                sumL += outL;
            }
            // R comb (bank offset kNumCombs/2)
            {
                int iR = combIdx_[kNumCombs / 2 + c];
                fixed outR = combBuf_[kNumCombs / 2 + c][iR];
                combState_[kNumCombs / 2 + c] =
                    combState_[kNumCombs / 2 + c] +
                    fp_mul(combDamp_[kNumCombs / 2 + c],
                           outR - combState_[kNumCombs / 2 + c]);
                combBuf_[kNumCombs / 2 + c][iR] =
                    saturate(pR + fp_mul(combGain_[kNumCombs / 2 + c],
                                         combState_[kNumCombs / 2 + c]));
                iR++;
                if (iR >= combLen_[kNumCombs / 2 + c]) iR = 0;
                combIdx_[kNumCombs / 2 + c] = iR;
                sumR += outR;
            }
        }
        // RC2 (section 3.2): normalized sum (headroom before the allpasses);
        // saturate stays as final protection only.
        sumL = saturate(fp_mul(sumL, combNorm_));
        sumR = saturate(fp_mul(sumR, combNorm_));

        // Series allpass diffusers (L and R banks) on the comb sum.
        int nAps = (mode_ == ECO) ? 1 : 3;
        fixed wetL = sumL;
        fixed wetR = sumR;
        for (int c = 0; c < nAps; c++) {
            // L allpass
            {
                int iL = apIdx_[c];
                fixed bufIn = allpassBuf_[c][iL];
                fixed y = -wetL + bufIn;
                allpassBuf_[c][iL] = wetL + fp_mul(fl2fp(0.5f), bufIn);
                wetL = y;
                iL++;
                if (iL >= apLen_[c]) iL = 0;
                apIdx_[c] = iL;
            }
            // R allpass
            {
                int iR = apIdx_[kNumAllpass / 2 + c];
                fixed bufIn = allpassBuf_[kNumAllpass / 2 + c][iR];
                fixed y = -wetR + bufIn;
                allpassBuf_[kNumAllpass / 2 + c][iR] =
                    wetR + fp_mul(fl2fp(0.5f), bufIn);
                wetR = y;
                iR++;
                if (iR >= apLen_[kNumAllpass / 2 + c]) iR = 0;
                apIdx_[kNumAllpass / 2 + c] = iR;
            }
        }

        // Mid/side width on the wet signal.
        if (width_ != i2fp(1)) {
            fixed mid = fp_mul(wetL + wetR, fl2fp(0.5f));
            fixed side = fp_mul(wetL - wetR, fl2fp(0.5f));
            wetL = mid + fp_mul(side, width_);
            wetR = mid - fp_mul(side, width_);
        }

        // DC blocker on the wet path.
        dcState_[0] = dcState_[0] + fp_mul(dcCoeff_, wetL - dcState_[0]);
        dcState_[1] = dcState_[1] + fp_mul(dcCoeff_, wetR - dcState_[1]);
        wetL = wetL - dcState_[0];
        wetR = wetR - dcState_[1];

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
