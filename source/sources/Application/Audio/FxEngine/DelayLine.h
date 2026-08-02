#ifndef _FX_DELAY_LINE_H_
#define _FX_DELAY_LINE_H_

#include "Application/Utils/fixed.h"

/*
 * FxEngine::DelayLine -- stereo feedback delay (PLAN_FX_REDESIGN_ES.md, Fase 2).
 *
 * Preallocated interleaved circular buffer (zero malloc in Process), free
 * delay time in ms with linear interpolation, smoothing of time changes
 * (glide) to avoid clicks, stable feedback (loop gain clamped < 1.0, max
 * documented), ping-pong mode, stereo width (mid/side), optional HP/LP
 * one-pole filters in the feedback loop, optional saturation, dry/wet mix
 * with crossfade smoothing, and independent tail (bypass keeps the buffer
 * running so the echo decays naturally instead of cutting).
 *
 * Real-time contract: zero malloc/new/free, zero syscalls/file-I/O/logging,
 * all buffers static.  rtViolations_ counts any would-be violation.
 */

namespace FxEngine {

class DelayLine {
public:
    // Max delay: 2000 ms at 48 kHz -> 96000 samples per channel.
    static const int kMaxMs = 2000;
    static const int kMaxSamplesPerChannel = 48000 * kMaxMs / 1000; // 96000
    static const int kBufferSize = kMaxSamplesPerChannel * 2;       // interleaved L/R

    DelayLine();
    void Reset();

    void SetSampleRate(int rate);      // 44100 or 48000
    void SetDelayMs(fixed ms);         // target time, smoothed (glide)
    void SetFeedback(fixed fb);        // clamped to [0, 0.98]
    void SetWidth(fixed width);        // 0..1 mid/side width on wet
    void SetPingPong(bool on);
    void SetLoopLPHz(fixed hz);        // optional one-pole LP in feedback loop
    void SetLoopHPHz(fixed hz);        // optional one-pole HP in feedback loop
    void SetSaturation(bool on);
    void SetBypass(bool on);           // crossfade wet->dry, tail keeps running
    void SetMix(fixed mix);            // dry/wet 0..1, smoothed

    // Interleaved stereo in/out (frames frames).  Output = dry/wet mix.
    void Process(const fixed *in, fixed *out, int frames);

    // Sync musical helper: division (1=whole,2=half,4=quarter,8=8th,...) at
    // bpm -> delay time in ms.  Controller (Fase 4) calls SetDelayMs with it.
    static fixed SyncDivisionToMs(int division, int bpm);

    unsigned long GetRtViolations() const { return rtViolations_; }
    static unsigned long StaticMemoryBytes() {
        return (unsigned long)kBufferSize * sizeof(fixed);
    }

private:
    static fixed saturate(fixed x);
    void glideDelay();
    fixed loopFilter(fixed v, int ch);

    fixed buf_[kBufferSize];
    int rate_;
    int maxSamples_;
    int writePos_;
    fixed delaySamples_;     // current smoothed delay in samples (Q15)
    fixed delayTarget_;      // target delay in samples (Q15)
    fixed fb_;
    fixed width_;
    bool pingPong_;
    bool sat_;
    bool bypass_;
    fixed mix_;
    fixed mixCur_;           // smoothed mix (crossfade)
    fixed lpState_[2];       // feedback loop LP state (L,R)
    fixed hpState_[2];       // feedback loop HP state (L,R)
    fixed lpCoeff_;
    fixed hpCoeff_;
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif
