#ifndef _FX_REVERB_H_
#define _FX_REVERB_H_

#include "Application/Utils/fixed.h"

/*
 * FxEngine::Reverb -- plate/room reverb (PLAN_FX_REDESIGN_ES.md, Fase 2;
 * RC2 wet-only audit, section 3).
 *
 * Simplified Schroeder/Dattorro topology, all buffers static (zero malloc in
 * Process).  Signal path: input HP/LP one-poles (+ fixed -3 dB input
 * headroom) -> predelay -> parallel comb filters (per-comb damping LP in the
 * loop, RT60-driven gains) with a normalized /nCombs sum -> series allpass
 * diffusers (3 in NORMAL, 1 in ECO) -> mid/side width -> DC blocker.
 *
 * RC2 (section 3.1) makes this a true send/return wet-only processor: the
 * dry signal already lives in the master bus, so Process() delivers ONLY the
 * processed (wet) signal.  There is no internal dry*dryMix + wet*wetMix mix
 * anymore.  The legacy RVB MIX value is still read and persisted (mix_) for
 * project compatibility but is inert in the DSP: the reverb runs 100% wet
 * and the audible level is set by the instrument send + the mixer return.
 * While REVERB BYPASS is ON the wet gain glides to zero (the tail keeps
 * running internally and reappears when bypass is released); the dry master
 * signal is never affected by the reverb.
 *
 * RC2 (section 3.2) avoids relying on the hard clamp for level control:
 * the parallel comb sum is normalized by the number of active combs (headroom
 * preserved before the allpasses) and the input carries a -3 dB fixed
 * headroom.  saturate() stays only as final out-of-range protection.
 *
 * Modes:
 *   ECO    -- 2 combs + 1 allpass per channel (cheap, darker)
 *   NORMAL -- 4 combs + 3 allpasses per channel (denser, RC2 diffusion)
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
    static const int kNumAllpass = 6;        // 3 L + 3 R (NORMAL uses all)
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
    void SetBypass(bool on);             // wet gain -> 0 (tail keeps running)
    // RC2 (section 3.1): the legacy dry/wet mix is gone.  SetMix/GetMix still
    // store/return the persisted RVB MIX value (project compatibility) but it
    // is inert in the DSP, which is fixed 100% wet.
    void SetMix(fixed mix);

    // Interleaved stereo in/out (frames frames).
    void Process(const fixed *in, fixed *out, int frames);

    unsigned long GetRtViolations() const { return rtViolations_; }
    static unsigned long StaticMemoryBytes();

    // Control-rate readbacks for the UI (PLAN_FX_REDESIGN_ES.md, Fase 4.3).
    fixed GetPredelayMs() const { return predelayMs_; }
    fixed GetDecayTarget() const { return decayTarget_; }
    fixed GetDecay() const { return decay_; }
    fixed GetSize() const { return size_; }
    fixed GetDamping() const { return damping_; }
    fixed GetWidth() const { return width_; }
    int GetMode() const { return mode_; }
    bool GetBypass() const { return bypass_; }
    fixed GetMix() const { return mix_; }

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
    fixed combNorm_;             // 1/nCombs normalized parallel-sum gain (RC2)

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
    fixed mix_;            // legacy persisted RVB MIX (inert in DSP, RC2)
    fixed mixCur_;         // smoothed wet gain: 1.0 full wet, 0 while bypassed
    unsigned long rtViolations_;
};

} // namespace FxEngine

#endif
