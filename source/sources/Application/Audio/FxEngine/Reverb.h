#ifndef _FX_REVERB_H_
#define _FX_REVERB_H_

#include "Application/Utils/fixed.h"

/*
 * FxEngine::Reverb -- plate/room reverb (PLAN_FX_REDESIGN_ES.md, Fase 2;
 * RC2 wet-only audit; bacon-1.5 item 3).
 *
 * Simplified Schroeder/Dattorro topology, all buffers static (zero malloc in
 * Process).  Signal path: input HP/LP one-poles (+ fixed -3 dB input
 * headroom) -> fractional predelay -> parallel comb filters (per-comb
 * damping LP in the loop, RT60-driven gains) with a normalized /nCombs sum
 * -> series allpass diffusers (3 in NORMAL, 1 in ECO) -> mid/side width ->
 * DC blocker.
 *
 * bacon-1.5 item 3 (DSP hygiene):
 *   - powf() is NEVER called inside Process(): RT60 comb gains are
 *     recomputed only when the decay target changes (decayDirty_, control
 *     rate) and glide per-sample toward their targets, so the tail evolves
 *     smoothly without trascendental work in the audio callback.
 *   - Predelay, comb and allpass delays use fractional reads with linear
 *     interpolation and per-sample length glide: predelay/size changes and
 *     the slow comb-length modulation are click-free.
 *   - Subtle built-in modulation (LFO shimmer): a 64-entry Q15 sine table
 *     (zero trascendentals per sample) modulates the comb read lengths by
 *     +/-2 samples at ~0.2 Hz, opposite phase on L/R for a wide tail.
 *   - SetSampleRate() recomputes the input filter coefficients from the
 *     stored frequencies (legacy bug: it zeroed them on rate change).
 *
 * RC2 (section 3.1) makes this a true send/return wet-only processor: the
 * dry signal already lives in the master bus, so Process() delivers ONLY the
 * processed (wet) signal.  The legacy RVB MIX value is still read and
 * persisted (mix_) for project compatibility but is inert in the DSP.
 * While REVERB BYPASS is ON the wet gain glides to zero (the tail keeps
 * running internally and reappears when bypass is released).
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
    void SetPredelayMs(fixed ms);        // 0..100, per-sample glide
    void SetDecay(fixed rt60Seconds);    // RT60 target (control-rate recompute)
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
    // bacon-1.5 item 3: current input filter frequencies (for persistence/UI).
    fixed GetInputHPHz() const { return inputHpHz_; }
    fixed GetInputLPHz() const { return inputLpHz_; }

private:
    static fixed saturate(fixed x);
    void recomputeCombs();
    void recomputeGains();
    void setInputHPCoeff(float hz);
    void setInputLPCoeff(float hz);
    fixed inputFilter(fixed x, int ch);

    fixed predelay_[kPredelayMax * 2];
    int predelayWrite_;
    fixed predelayLenF_;         // current fractional predelay (samples, Q15)
    fixed predelayTargetF_;      // target fractional predelay (samples, Q15)

    fixed combBuf_[kNumCombs][kCombMaxLen];
    int combIdx_[kNumCombs];
    fixed combLenF_[kNumCombs];      // current fractional comb length (Q15)
    fixed combLenTargetF_[kNumCombs];// target (set by recomputeCombs)
    fixed combGain_[kNumCombs];      // current applied comb gain (glided)
    fixed combGainTarget_[kNumCombs];// target from RT60 (control-rate)
    fixed combDamp_[kNumCombs];
    fixed combState_[kNumCombs];
    fixed combNorm_;             // 1/nCombs normalized parallel-sum gain (RC2)

    fixed allpassBuf_[kNumAllpass][kAllpassMaxLen];
    int apIdx_[kNumAllpass];
    fixed apLenF_[kNumAllpass];
    fixed apLenTargetF_[kNumAllpass];

    fixed inLpState_[2];
    fixed inHpState_[2];
    fixed inLpCoeff_;
    fixed inHpCoeff_;
    fixed inputLpHz_;            // current input LP frequency (Hz, Q15)
    fixed inputHpHz_;            // current input HP frequency (Hz, Q15)

    fixed dcState_[2];
    fixed dcCoeff_;

    // bacon-1.5 item 3: modulation LFO (64-entry Q15 sine, phase Q24).
    int lfoPhase_;
    int lfoInc_;

    int rate_;
    int mode_;
    fixed predelayMs_;
    fixed decay_;          // current smoothed RT60 (Q15 seconds*scale)
    fixed decayTarget_;
    bool decayDirty_;      // RT60 target changed: recompute gains once
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
