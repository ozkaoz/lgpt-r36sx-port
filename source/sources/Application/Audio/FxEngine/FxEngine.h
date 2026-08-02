#ifndef _FX_ENGINE_H_
#define _FX_ENGINE_H_

#include "Application/Utils/fixed.h"
#include "DelayLine.h"
#include "Reverb.h"
#include "ParametricEQ.h"
#include "Compressor.h"

/*
 * FxEngine -- post-mix master effects stage (PLAN_FX_REDESIGN_ES.md, Fase 1).
 *
 * Phase 1 goal: a real, callable skeleton wired into the master output path
 * that is a pure bypass by default (gain 1.0, DSP off) so playback stays
 * bit-for-bit identical to the legacy LGPT path.  It owns the static buses
 * (dry / send / return / master) that later phases fill with delay, reverb,
 * EQ and compressor processing.
 *
 * Real-time contract (validated by tests/test_fx_phase1_fxengine_bypass.py):
 *   - zero malloc/new/free in Process()
 *   - zero syscalls / file I/O / logging in Process()
 *   - all buffers are static (Buses), no dynamic allocation
 *   - rtViolations_ counts any would-be violation and must stay 0
 *
 * NOTE: System::GetClock() is wall-clock based (a syscall) and therefore must
 * NOT be used inside Process().  CPU measurement happens on the host bench
 * instead; on device the only telemetry is the counters below.
 */

// Static bus geometry.  Stereo interleaved, FIXED_ONE = 1.0.  These will be
// re-dimensioned when the real DSP (Fase 2+) defines its needs; in Phase 1
// bypass mode the buffers are never written, so no overflow can occur.
#define FX_ENGINE_MAX_CHANNELS 8
#define FX_ENGINE_MAX_FRAMES   2048
#define FX_ENGINE_MAX_FIXED    (FX_ENGINE_MAX_FRAMES * 2)

namespace FxEngine {

// Buses estáticos, preasignados, cero malloc.
struct Buses {
    fixed dry_[FX_ENGINE_MAX_CHANNELS][FX_ENGINE_MAX_FIXED];
    fixed send_[FX_ENGINE_MAX_CHANNELS][FX_ENGINE_MAX_FIXED];
    fixed returnDelay_[FX_ENGINE_MAX_FIXED];
    fixed returnReverb_[FX_ENGINE_MAX_FIXED];
    fixed master_[FX_ENGINE_MAX_FIXED];
};

// Orchestrator.  Phase 1: pure bypass, no DSP.  Phase 2+: sends/returns for
// delay and reverb once legacyMode_ is disabled.
class FxEngine {
public:
    static FxEngine &GetInstance();

    void Reset();

    // Master-stage post-mix processor.  Interleaved stereo fixed buffer.
    // In legacy mode (default) it is a pure bypass: the buffer is untouched,
    // so the output is bit-for-bit identical to the original path.
    void Process(fixed *buffer, int samplecount);

    // --- Per-track sends (Fase 4) ---
    // Each PlayerChannel accumulates its own rendered audio into the delay and
    // reverb send buses with the track's send gains (read from the Mixer
    // model), BEFORE the master mix.  Process() then uses the accumulated
    // buses.  When no channel accumulates (e.g. bench calls Process() directly
    // on a single mixed buffer), Process() falls back to the global
    // delaySend_/reverbSend_ sends so the Fase 2/3 behaviour is preserved.
    void AccumulateChannelSend(int channel, const fixed *buffer,
                               int samplecount, fixed delayGain,
                               fixed reverbGain);

    bool IsLegacyMode() const { return legacyMode_; }
    void SetLegacyMode(bool on) { legacyMode_ = on; }

    // Sample rate (used by delay/reverb; control-rate only).
    void SetSampleRate(int rate);

    // --- Delay send/return (Fase 2) ---
    void SetDelaySend(fixed send) { delaySend_ = send; }
    void SetDelayReturn(fixed ret) { delayReturn_ = ret; }
    void SetDelayTimeMs(fixed ms) { delay_.SetDelayMs(ms); }
    void SetDelayFeedback(fixed fb) { delay_.SetFeedback(fb); }
    void SetDelayPingPong(bool on) { delay_.SetPingPong(on); }
    void SetDelayWidth(fixed w) { delay_.SetWidth(w); }
    void SetDelayMix(fixed mix) { delay_.SetMix(mix); }
    void SetDelayBypass(bool on) { delay_.SetBypass(on); }
    void SetDelayLoopLPHz(fixed hz) { delay_.SetLoopLPHz(hz); }
    void SetDelayLoopHPHz(fixed hz) { delay_.SetLoopHPHz(hz); }

    // --- Reverb send/return (Fase 2) ---
    void SetReverbSend(fixed send) { reverbSend_ = send; }
    void SetReverbReturn(fixed ret) { reverbReturn_ = ret; }
    void SetReverbPredelayMs(fixed ms) { reverb_.SetPredelayMs(ms); }
    void SetReverbDecay(fixed rt60) { reverb_.SetDecay(rt60); }
    void SetReverbSize(fixed size) { reverb_.SetSize(size); }
    void SetReverbDamping(fixed d) { reverb_.SetDamping(d); }
    void SetReverbWidth(fixed w) { reverb_.SetWidth(w); }
    void SetReverbMix(fixed mix) { reverb_.SetMix(mix); }
    void SetReverbBypass(bool on) { reverb_.SetBypass(on); }
    void SetReverbInputHPHz(fixed hz) { reverb_.SetInputHP(hz); }
    void SetReverbInputLPHz(fixed hz) { reverb_.SetInputLP(hz); }
    void SetReverbMode(int mode) { reverb_.SetMode((Reverb::Mode)mode); }

    // --- Master EQ 3 bandas (Fase 3) ---
    void SetEqBypass(bool on) { eq_.SetBypass(on); }
    void SetEqBandFreq(int band, fixed hz) { eq_.SetBandFreq((ParametricEQ::Band)band, hz); }
    void SetEqBandGainDb(int band, fixed db) { eq_.SetBandGainDb((ParametricEQ::Band)band, db); }
    void SetEqBandQ(int band, fixed q) { eq_.SetBandQ((ParametricEQ::Band)band, q); }
    void SetEqBandEnabled(int band, bool on) { eq_.SetBandEnabled((ParametricEQ::Band)band, on); }

    // --- Compresor/limitador master (Fase 3) ---
    void SetCompBypass(bool on) { comp_.SetBypass(on); }
    void SetCompThresholdDb(fixed db) { comp_.SetThresholdDb(db); }
    void SetCompRatio(fixed ratio) { comp_.SetRatio(ratio); }
    void SetCompKneeDb(fixed db) { comp_.SetKneeDb(db); }
    void SetCompAttackMs(fixed ms) { comp_.SetAttackMs(ms); }
    void SetCompReleaseMs(fixed ms) { comp_.SetReleaseMs(ms); }
    void SetCompMakeupDb(fixed db) { comp_.SetMakeupDb(db); }
    void SetCompStereoLink(bool on) { comp_.SetStereoLink(on); }
    void SetCompSoftClip(bool on) { comp_.SetSoftClip(on); }
    fixed GetCompGainReductionDb() const { return comp_.GetGainReductionDb(); }

    // RT telemetry.  rtViolations_ must stay 0; it is incremented only if
    // Process() would have needed a dynamic allocation or a syscall.
    unsigned long GetCallCount() const { return callCount_; }
    unsigned long GetFramesProcessed() const { return frames_; }
    unsigned long GetMaxFrames() const { return maxFrames_; }
    unsigned long GetRtViolations() const { return rtViolations_; }

    static unsigned long StaticMemoryBytes() {
        return sizeof(Buses) + DelayLine::StaticMemoryBytes()
               + Reverb::StaticMemoryBytes() + ParametricEQ::StaticMemoryBytes()
               + Compressor::StaticMemoryBytes();
    }

private:
    FxEngine();
    FxEngine(const FxEngine &);
    FxEngine &operator=(const FxEngine &);

    void processSendReturns(fixed *buffer, int samplecount);

    Buses buses_;
    bool legacyMode_;
    bool sendsAccumulated_;
    unsigned long callCount_;
    unsigned long frames_;
    unsigned long maxFrames_;
    unsigned long rtViolations_;
    int sampleRate_;
    fixed delaySend_;
    fixed delayReturn_;
    fixed reverbSend_;
    fixed reverbReturn_;
    DelayLine delay_;
    Reverb reverb_;
    ParametricEQ eq_;
    Compressor comp_;
};

} // namespace FxEngine

#endif
