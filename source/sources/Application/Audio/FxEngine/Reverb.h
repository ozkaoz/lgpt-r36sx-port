#ifndef _FX_REVERB_H_
#define _FX_REVERB_H_

#include "Application/Utils/fixed.h"

/*
 * FxEngine::Reverb -- plate/room reverb (PLAN_FX_REDESIGN_ES.md, Fase 2).
 *
 * Simplified Schroeder/Dattorro topology, all buffers static (zero malloc in
 * Process).  Signal path: input HP/LP one-poles -> predelay -> parallel comb
 * filters (with per-comb damping LP in the loop, gain derived from RT60) ->
 * series allpass diffusers -> mid/side width -> DC blocker.
 *
 * Modes:
 *   ECO    -- 2 combs + 1 allpass per channel (cheap, darker)
 *   NORMAL -- 4 combs + 2 allpasses per channel (denser)
 *
 * Protection: per-stage clamp to +/-1 (no runaway), RT60-driven comb gains
 * < 1 always (no feedback blow-up), output DC blocker (no DC buildup).
 * Real-time contract: zero malloc/new/free, zero syscalls/file-I/O/logging.
 */

namespace FxEngine {

class Reverb {
public:
    enum Mode { ECO = 0, NORMAL = 1 };

    static const int kPredelayMax = 4800;    // 100 ms @ 48 kHz, per channel
    static const int kNumCombs = 8;          // 4 L + 4 R (NORMAL uses all)
    static const int kCombMaxLen = 2048;     // covers size up to ~1.5x
    static const int kNumAllpass = 4;        // 2 L + 2 R
    static const int kAllpassMaxLen = 1024;

    Reverb();
    void Reset();

    void SetSampleRate(int rate);
    void SetPredelayMs(fixed ms);        // 0..100
    void SetDecay(fixed rt60Seconds);    // RT60 target, smoothed
    void SetSize(fixed size);            // 0.5..1.5 comb length scale
    void SetDamping(fixed damping);      // 0..1 LP in comb loops
    void SetInputHP(fixed hz);
    void SetInputLP(fixed hz);
    void SetWidth(fixed width);          // 0..1 mid/side
    void SetMode(Mode mode);
    void SetBypass(bool on);             // crossfade to dry, tail runs on
    void SetMix(fixed mix);              // dry/wet 0..1, smoothed

    // Interleaved stereo in/out (frames frames).
    void Process(const fixed *in, fixed *out, int frames);

    unsigned long GetRtViolations() const { return rtViolations_; }
    static unsigned long StaticMemoryBytes();

private:
    static fixed saturate(fixed x);
    void recomputeCombs();
    void recomputeGains();
    void glideDecay();
    fixed inputFilter(fixed x, int ch);

    fixed predelay_[kPredelayMax * 2];
    int predelayWrite_;
    int predelayLen_;

    fixed combBuf_[kNumCombs][kCombMaxLen];
    int combIdx_[kNumCombs];
    int combLen_[kNumCombs];
    fixed combGain_[kNumCombs];
    fixed combDamp_[kNumCombs];
    fixed combState_[kNumCombs];

    fixed allpassBuf_[kNumAllpass][kAllpassMaxLen];
    int apIdx_[kNumAllpass];
    int apLen_[kNumAllpass];

    fixed inLpState_[2];
    fixed inHpState_[2];
    fixed inLpCoeff_;
    fixed inHpCoeff_;

    fixed dcState_[2];
    fixed dcCoeff_;

    int rate_;
    int mode_;
    fixed predelayMs_;
    fixed decay_;          // current smoothed RT60 (Q15 seconds*scale)
    fixed decayTarget_;
    fixed size_;
    fixed damping_;
    fixed width_;
    bool bypass_;
    fixed mix_;
    fixed mixCur_;
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif
