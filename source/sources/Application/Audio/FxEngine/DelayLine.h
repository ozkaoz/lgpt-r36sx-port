#ifndef _FX_DELAY_LINE_H_
#define _FX_DELAY_LINE_H_

#include "Application/Utils/fixed.h"

/*
 * FxEngine::DelayLine -- stereo feedback delay (PLAN_FX_REDESIGN_ES.md,
 * Fase 2; bacon-1.5 item 3).
 *
 * Preallocated interleaved circular buffer (zero malloc in Process), free
 * delay time in ms with linear interpolation, smoothing of time changes
 * (glide, per-sample so it is buffer-size independent) to avoid clicks,
 * stable feedback (loop gain clamped < 1.0, max documented), ping-pong
 * mode, stereo width (mid/side), LP/HP one-pole filters in the feedback
 * loop (LOW CUT / HIGH CUT), optional saturation, dry/wet mix with
 * per-sample crossfade smoothing, and independent tail (bypass keeps the
 * buffer running so the echo decays naturally instead of cutting).
 *
 * bacon-1.5 item 3: FREE/SYNC mode.  In SYNC the controller (FxEngine)
 * recomputes SetDelayMs() from the song tempo and a musical division
 * (1/32..1/1, including dotted and triplet) every audio callback using the
 * pure-integer table below (no floats, no trascendentals in Process).
 * While SYNC is on, the user TIME value is overridden by the division
 * (the TIME target keeps the last free value and reappears in FREE mode).
 *
 * Real-time contract: zero malloc/new/free, zero syscalls/file-I/O/logging,
 * all buffers static.  rtViolations_ counts any would-be violation.
 */

namespace FxEngine {

// Musical sync divisions (bacon-1.5 item 3).  Each entry is a fraction
// num/den of a whole note (4 beats): ms = 4*60000/bpm * num/den.  Triplets
// are 2/3 of the straight division, dotted are 3/2 (exact integer math).
enum SyncDivision {
    SDIV_1_32 = 0, SDIV_1_32T, SDIV_1_32D,
    SDIV_1_16, SDIV_1_16T, SDIV_1_16D,
    SDIV_1_8, SDIV_1_8T, SDIV_1_8D,
    SDIV_1_4, SDIV_1_4T, SDIV_1_4D,
    SDIV_1_2, SDIV_1_2T, SDIV_1_2D,
    SDIV_1_1, SDIV_COUNT
};

struct SyncDivisionInfo {
    const char *name;
    int num;
    int den;
};

static const SyncDivisionInfo kSyncDivisions[SDIV_COUNT] = {
    { "1/32",  1, 32 }, { "1/32T", 2, 96 }, { "1/32D", 3, 64 },
    { "1/16",  1, 16 }, { "1/16T", 2, 48 }, { "1/16D", 3, 32 },
    { "1/8",   1,  8 }, { "1/8T",  2, 24 }, { "1/8D",  3, 16 },
    { "1/4",   1,  4 }, { "1/4T",  2, 12 }, { "1/4D",  3,  8 },
    { "1/2",   1,  2 }, { "1/2T",  2,  6 }, { "1/2D",  3,  4 },
    { "1/1",   1,  1 }
};

class DelayLine {
public:
    // Max delay: 2000 ms at 48 kHz -> 96000 samples per channel.
    static const int kMaxMs = 2000;
    static const int kMaxSamplesPerChannel = 48000 * kMaxMs / 1000; // 96000
    static const int kBufferSize = kMaxSamplesPerChannel * 2;       // interleaved L/R

    DelayLine();
    void Reset();

    void SetSampleRate(int rate);      // 44100 or 48000
    void SetDelayMs(fixed ms);         // target time, smoothed (per-sample glide)
    void SetFeedback(fixed fb);        // clamped to [0, 0.98]
    void SetWidth(fixed width);        // 0..1 mid/side width on wet
    void SetPingPong(bool on);
    // bacon-1.5 item 3: LOW CUT / HIGH CUT one-poles in the feedback loop.
    // Frequencies near the open thresholds (>= 19000 Hz for LP, <= 30 Hz
    // for HP) bypass the filter (coeff 0) so the legacy open state is
    // bit-identical.  The current frequency is stored for persistence/UI.
    void SetLoopLPHz(fixed hz);
    void SetLoopHPHz(fixed hz);
    void SetSaturation(bool on);
    void SetBypass(bool on);           // crossfade wet->dry, tail keeps running
    void SetMix(fixed mix);            // dry/wet 0..1, per-sample smoothed
    // bacon-1.5 item 3: FREE/SYNC mode + musical division (SyncDivision).
    void SetSync(bool on);
    void SetDivision(int division);

    // Interleaved stereo in/out (frames frames).  Output = dry/wet mix.
    void Process(const fixed *in, fixed *out, int frames);

    // Sync musical helper (bacon-1.5 item 3): division (SyncDivision index)
    // at bpm -> delay time in ms.  Pure integer math: ms = 240000*num/(bpm*den),
    // clamped to kMaxMs.  The controller (FxEngine::Process) calls
    // SetDelayMs() with the result while SYNC is on.
    static fixed SyncDivisionToMs(int division, int bpm);

    // Control-rate readbacks for the UI (PLAN_FX_REDESIGN_ES.md, Fase 4.3).
    // bacon-1.5 item 5: GetDelayMsTarget returns the time in ms (Q15); the
    // internal glide state is 64-bit fractional samples so the full 2000 ms
    // range (96000 samples at 48 kHz) never overflows 32-bit Q15.
    fixed GetDelayMsTarget() const { return delayTargetMs_; }
    fixed GetDelayMs() const { return fl2fp(fp2fl(delayTargetMs_)); }
    fixed GetFeedback() const { return fb_; }
    fixed GetMix() const { return mix_; }
    fixed GetWidth() const { return width_; }
    bool GetPingPong() const { return pingPong_; }
    bool GetSaturation() const { return sat_; }
    bool GetBypass() const { return bypass_; }
    bool GetSync() const { return syncOn_; }
    int GetDivision() const { return division_; }
    fixed GetLoopLPHz() const { return lpHz_; }
    fixed GetLoopHPHz() const { return hpHz_; }

    unsigned long GetRtViolations() const { return rtViolations_; }
    static unsigned long StaticMemoryBytes() {
        return (unsigned long)kBufferSize * sizeof(fixed);
    }

private:
    static fixed saturate(fixed x);
    void setLoopLPCoeff(float hz);
    void setLoopHPCoeff(float hz);
    fixed loopFilter(fixed v, int ch);

    fixed buf_[kBufferSize];
    int rate_;
    int maxSamples_;
    int writePos_;
    long long delaySamples_;  // current smoothed delay in fractional samples (Q15, 64-bit)
    long long delayTarget_;   // target delay in fractional samples (Q15, 64-bit)
    fixed delayTargetMs_;     // target delay in ms (Q15, for UI/persistence)
    fixed fb_;
    fixed width_;
    bool pingPong_;
    bool sat_;
    bool bypass_;
    fixed mix_;
    fixed mixCur_;           // smoothed mix (per-sample crossfade)
    fixed lpState_[2];       // feedback loop LP state (L,R)
    fixed hpState_[2];       // feedback loop HP state (L,R)
    fixed lpCoeff_;
    fixed hpCoeff_;
    fixed lpHz_;             // current LOW CUT frequency (Hz, Q15)
    fixed hpHz_;             // current HIGH CUT frequency (Hz, Q15)
    bool syncOn_;            // FREE/SYNC mode (bacon-1.5 item 3)
    int division_;           // SyncDivision index while syncOn_
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif
