#include "DelayLine.h"
#include <math.h>

namespace FxEngine {

// Loop gain must stay < 1.0 for a stable feedback loop.  Documented max.
#define FX_DELAY_MAX_FEEDBACK fl2fp(0.98f)
// Time-change glide: max delay movement per sample (Q15).  ~1 sample / sample
// keeps the interpolation smooth and click-free.
#define FX_DELAY_GLIDE_PER_SAMPLE fl2fp(0.5f)
// One-pole mix crossfade coefficient per sample (Q15).
#define FX_DELAY_MIX_SMOOTH fl2fp(0.001f)

DelayLine::DelayLine()
    : rate_(44100), maxSamples_(0), writePos_(0), delaySamples_(0),
      delayTarget_(0), fb_(0), width_(i2fp(1)), pingPong_(false),
      sat_(false), bypass_(false), mix_(i2fp(1)), mixCur_(i2fp(1)),
      lpCoeff_(0), hpCoeff_(0), rtViolations_(0) {
    lpState_[0] = lpState_[1] = 0;
    hpState_[0] = hpState_[1] = 0;
    maxSamples_ = (kMaxMs * rate_) / 1000;
    if (maxSamples_ > kMaxSamplesPerChannel) maxSamples_ = kMaxSamplesPerChannel;
    if (maxSamples_ < 2) maxSamples_ = 2;
    for (int i = 0; i < kBufferSize; i++) buf_[i] = 0;
}

void DelayLine::Reset() {
    writePos_ = 0;
    delaySamples_ = 0;
    delayTarget_ = 0;
    mixCur_ = mix_;
    lpState_[0] = lpState_[1] = 0;
    hpState_[0] = hpState_[1] = 0;
    for (int i = 0; i < kBufferSize; i++) buf_[i] = 0;
}

void DelayLine::SetSampleRate(int rate) {
    if (rate < 8000 || rate > 96000) {
        ++rtViolations_;
        return;
    }
    rate_ = rate;
    maxSamples_ = (kMaxMs * rate_) / 1000;
    if (maxSamples_ > kMaxSamplesPerChannel) maxSamples_ = kMaxSamplesPerChannel;
    if (maxSamples_ < 2) maxSamples_ = 2;
}

void DelayLine::SetDelayMs(fixed ms) {
    // ms in [0, kMaxMs]; convert to fractional samples (Q15).
    float m = fp2fl(ms);
    if (m < 0.0f) m = 0.0f;
    if (m > (float)kMaxMs) m = (float)kMaxMs;
    float samples = m * (float)rate_ / 1000.0f;
    if (samples > (float)maxSamples_ - 1.0f) samples = (float)maxSamples_ - 1.0f;
    delayTarget_ = fl2fp(samples);
}

void DelayLine::SetFeedback(fixed fb) {
    float f = fp2fl(fb);
    if (f < 0.0f) f = 0.0f;
    if (f > 0.98f) f = 0.98f; // keep loop gain < 1 (documented)
    fb_ = fl2fp(f);
}

void DelayLine::SetWidth(fixed width) {
    float w = fp2fl(width);
    if (w < 0.0f) w = 0.0f;
    if (w > 1.0f) w = 1.0f;
    width_ = fl2fp(w);
}

void DelayLine::SetPingPong(bool on) { pingPong_ = on; }
void DelayLine::SetSaturation(bool on) { sat_ = on; }
void DelayLine::SetBypass(bool on) { bypass_ = on; }

void DelayLine::SetMix(fixed mix) {
    float m = fp2fl(mix);
    if (m < 0.0f) m = 0.0f;
    if (m > 1.0f) m = 1.0f;
    mix_ = fl2fp(m);
}

void DelayLine::SetLoopLPHz(fixed hz) {
    float f = fp2fl(hz);
    if (f <= 0.0f) {
        lpCoeff_ = 0;
    } else {
        // one-pole LP: a = 1 - exp(-2*pi*fc/fs)
        float a = 1.0f - expf(-2.0f * 3.14159265f * f / (float)rate_);
        lpCoeff_ = fl2fp(a);
    }
}

void DelayLine::SetLoopHPHz(fixed hz) {
    float f = fp2fl(hz);
    if (f <= 0.0f) {
        hpCoeff_ = 0;
    } else {
        // one-pole HP: a = exp(-2*pi*fc/fs) (complement of LP)
        float a = expf(-2.0f * 3.14159265f * f / (float)rate_);
        hpCoeff_ = fl2fp(a);
    }
}

fixed DelayLine::saturate(fixed x) {
    if (x > i2fp(1)) return i2fp(1);
    if (x < -i2fp(1)) return -i2fp(1);
    return x;
}

void DelayLine::glideDelay() {
    if (delaySamples_ < delayTarget_) {
        delaySamples_ += FX_DELAY_GLIDE_PER_SAMPLE;
        if (delaySamples_ > delayTarget_) delaySamples_ = delayTarget_;
    } else if (delaySamples_ > delayTarget_) {
        delaySamples_ -= FX_DELAY_GLIDE_PER_SAMPLE;
        if (delaySamples_ < delayTarget_) delaySamples_ = delayTarget_;
    }
}

// Applies the optional one-pole LP then HP in the feedback loop (series).
fixed DelayLine::loopFilter(fixed v, int ch) {
    if (lpCoeff_ != 0) {
        // low-pass state: y = y + a*(x - y)
        fixed diff = v - lpState_[ch];
        lpState_[ch] = lpState_[ch] + fp_mul(lpCoeff_, diff);
        v = lpState_[ch];
    }
    if (hpCoeff_ != 0) {
        // high-pass: y = x - xm; xm = xm + a*(x - xm)  (complement of LP)
        fixed xm = hpState_[ch];
        hpState_[ch] = xm + fp_mul(hpCoeff_, v - xm);
        v = v - hpState_[ch];
    }
    return v;
}

void DelayLine::Process(const fixed *in, fixed *out, int frames) {
    if (frames <= 0 || !in || !out) {
        ++rtViolations_;
        return;
    }

    glideDelay();

    int delay = fp2i(delaySamples_);
    fixed frac = delaySamples_ - i2fp(delay);

    // Delay must leave room for one interpolation tap ahead.
    if (delay < 0) delay = 0;
    if (delay > maxSamples_ - 1) delay = maxSamples_ - 1;

    // Effective wet mix: bypass crossfades to dry but the tail keeps running.
    fixed targetMix = bypass_ ? 0 : mix_;
    // Smooth mix toward target (one-pole) for click-free bypass/mix changes.
    mixCur_ = mixCur_ + fp_mul(FX_DELAY_MIX_SMOOTH, targetMix - mixCur_);
    fixed wetMix = mixCur_;
    fixed dryMix = i2fp(1) - wetMix;

    int idx = 0;
    for (int i = 0; i < frames; i++) {
        fixed xL = in[idx];
        fixed xR = in[idx + 1];

        // Read delayed samples with linear interpolation (stereo interleaved).
        // Buffer is interleaved L/R; delay is in frames, so step 2*delay.
        int readPos = writePos_ - 2 * delay;
        while (readPos < 0) readPos += kBufferSize;
        int readPosB = readPos + 2;
        if (readPosB >= kBufferSize) readPosB -= kBufferSize;

        fixed d0 = buf_[readPos];
        fixed d1 = buf_[readPos + 1];
        fixed d0B = buf_[readPosB];
        fixed d1B = buf_[readPosB + 1];

        fixed delayedL = d0 + fp_mul(d1 - d0, frac);
        fixed delayedR = d0B + fp_mul(d1B - d0B, frac);

        // Feedback path (ping-pong crosses L/R taps).
        fixed fL = loopFilter(delayedL, 0);
        fixed fR = loopFilter(delayedR, 1);
        fixed feedL = pingPong_ ? fR : fL;
        fixed feedR = pingPong_ ? fL : fR;

        fixed wetL = saturate(xL + fp_mul(fb_, feedL));
        fixed wetR = saturate(xR + fp_mul(fb_, feedR));

        if (sat_) {
            wetL = saturate(wetL);
            wetR = saturate(wetR);
        }

        // Write processed feedback into the circular buffer.
        buf_[writePos_] = wetL;
        buf_[writePos_ + 1] = wetR;

        // Width (mid/side) on the delayed taps for the wet output.
        fixed wetOutL = delayedL;
        fixed wetOutR = delayedR;
        if (width_ != i2fp(1)) {
            // mid = (L+R)/2 ; side = (L-R)/2*width
            fixed mid = fp_mul(delayedL + delayedR, fl2fp(0.5f));
            fixed side = fp_mul(delayedL - delayedR, fl2fp(0.5f));
            wetOutL = mid + fp_mul(side, width_);
            wetOutR = mid - fp_mul(side, width_);
        }

        out[idx] = fp_mul(xL, dryMix) + fp_mul(wetOutL, wetMix);
        out[idx + 1] = fp_mul(xR, dryMix) + fp_mul(wetOutR, wetMix);

        writePos_ += 2;
        if (writePos_ >= kBufferSize) writePos_ = 0;
        idx += 2;
    }
}

fixed DelayLine::SyncDivisionToMs(int division, int bpm) {
    if (division <= 0) division = 1;
    if (bpm <= 0) bpm = 120;
    // 1 whole note = 4 beats = 4 * (60/bpm) seconds.
    float wholeMs = 4.0f * 60000.0f / (float)bpm;
    float divMs = wholeMs / (float)division;
    return fl2fp(divMs);
}

} // namespace FxEngine
