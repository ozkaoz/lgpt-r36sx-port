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

    // Auto-engage (PLAN_FX_REDESIGN_ES.md, Fase 5): editing any FX master
    // parameter, or raising any per-track send, disables legacy bypass so the
    // DSP actually runs.  Returning every parameter to its legacy default
    // (and clearing all sends) re-engages bypass.  Called at the end of every
    // public setter and by Mixer whenever a channel send changes.
    void NotifyChannelSendActive(bool on) { anyChannelSendActive_ = on; RefreshLegacy(); }
    void RefreshLegacy();
    bool AllParamsAtLegacyDefault() const;

    // Sample rate (used by delay/reverb; control-rate only).
    void SetSampleRate(int rate);

    // --- Delay send/return (Fase 2) ---
    void SetDelaySend(fixed send) { delaySend_ = send; RefreshLegacy(); }
    void SetDelayReturn(fixed ret) { delayReturn_ = ret; RefreshLegacy(); }
    void SetDelayTimeMs(fixed ms) { delay_.SetDelayMs(ms); RefreshLegacy(); }
    void SetDelayFeedback(fixed fb) { delay_.SetFeedback(fb); RefreshLegacy(); }
    void SetDelayPingPong(bool on) { delay_.SetPingPong(on); RefreshLegacy(); }
    void SetDelayWidth(fixed w) { delay_.SetWidth(w); RefreshLegacy(); }
    void SetDelayMix(fixed mix) { delay_.SetMix(mix); RefreshLegacy(); }
    void SetDelayBypass(bool on) { delay_.SetBypass(on); RefreshLegacy(); }
    void SetDelaySaturation(bool on) { delay_.SetSaturation(on); RefreshLegacy(); }
    void SetDelayLoopLPHz(fixed hz) { delay_.SetLoopLPHz(hz); RefreshLegacy(); }
    void SetDelayLoopHPHz(fixed hz) { delay_.SetLoopHPHz(hz); RefreshLegacy(); }
    // bacon-1.5 item 3: FREE/SYNC + musical division + LOW CUT / HIGH CUT.
    void SetDelaySync(bool on) { delay_.SetSync(on); RefreshLegacy(); }
    void SetDelayDivision(int div) { delay_.SetDivision(div); RefreshLegacy(); }
    void SetDelayLowCutHz(fixed hz) { delay_.SetLoopLPHz(hz); RefreshLegacy(); }
    void SetDelayHighCutHz(fixed hz) { delay_.SetLoopHPHz(hz); RefreshLegacy(); }

    // --- Reverb send/return (Fase 2) ---
    void SetReverbSend(fixed send) { reverbSend_ = send; RefreshLegacy(); }
    void SetReverbReturn(fixed ret) { reverbReturn_ = ret; RefreshLegacy(); }
    void SetReverbPredelayMs(fixed ms) { reverb_.SetPredelayMs(ms); RefreshLegacy(); }
    void SetReverbDecay(fixed rt60) { reverb_.SetDecay(rt60); RefreshLegacy(); }
    void SetReverbSize(fixed size) { reverb_.SetSize(size); RefreshLegacy(); }
    void SetReverbDamping(fixed d) { reverb_.SetDamping(d); RefreshLegacy(); }
    void SetReverbWidth(fixed w) { reverb_.SetWidth(w); RefreshLegacy(); }
    void SetReverbMix(fixed mix) { reverb_.SetMix(mix); RefreshLegacy(); }
    void SetReverbBypass(bool on) { reverb_.SetBypass(on); RefreshLegacy(); }
    void SetReverbInputHPHz(fixed hz) { reverb_.SetInputHP(hz); RefreshLegacy(); }
    void SetReverbInputLPHz(fixed hz) { reverb_.SetInputLP(hz); RefreshLegacy(); }
    void SetReverbMode(int mode) { reverb_.SetMode((Reverb::Mode)mode); RefreshLegacy(); }

    // Control-rate readbacks for the UI (PLAN_FX_REDESIGN_ES.md, Fase 4.3).
    fixed GetDelaySend() const { return delaySend_; }
    fixed GetDelayReturn() const { return delayReturn_; }
    fixed GetDelayTimeMs() const { return delay_.GetDelayMsTarget(); }
    fixed GetDelayFeedback() const { return delay_.GetFeedback(); }
    fixed GetDelayMix() const { return delay_.GetMix(); }
    fixed GetDelayWidth() const { return delay_.GetWidth(); }
    bool GetDelayPingPong() const { return delay_.GetPingPong(); }
    bool GetDelayBypass() const { return delay_.GetBypass(); }
    bool GetDelaySaturation() const { return delay_.GetSaturation(); }
    // bacon-1.5 item 3 readbacks (FREE/SYNC, division, LOW/HIGH CUT).
    bool GetDelaySync() const { return delay_.GetSync(); }
    int GetDelayDivision() const { return delay_.GetDivision(); }
    fixed GetDelayLowCutHz() const { return delay_.GetLoopLPHz(); }
    fixed GetDelayHighCutHz() const { return delay_.GetLoopHPHz(); }
    fixed GetReverbInputHPHz() const { return reverb_.GetInputHPHz(); }
    fixed GetReverbInputLPHz() const { return reverb_.GetInputLPHz(); }
    fixed GetReverbSend() const { return reverbSend_; }
    fixed GetReverbReturn() const { return reverbReturn_; }
    fixed GetReverbPredelayMs() const { return reverb_.GetPredelayMs(); }
    fixed GetReverbDecay() const { return reverb_.GetDecayTarget(); }
    fixed GetReverbSize() const { return reverb_.GetSize(); }
    fixed GetReverbDamping() const { return reverb_.GetDamping(); }
    fixed GetReverbWidth() const { return reverb_.GetWidth(); }
    fixed GetReverbMix() const { return reverb_.GetMix(); }
    int GetReverbMode() const { return reverb_.GetMode(); }
    bool GetReverbBypass() const { return reverb_.GetBypass(); }

    // --- Master EQ 3 bandas (Fase 3) ---
    void SetEqBypass(bool on) { eq_.SetBypass(on); RefreshLegacy(); }
    void SetEqBandFreq(int band, fixed hz) { eq_.SetBandFreq((ParametricEQ::Band)band, hz); RefreshLegacy(); }
    void SetEqBandGainDb(int band, fixed db) { eq_.SetBandGainDb((ParametricEQ::Band)band, db); RefreshLegacy(); }
    void SetEqBandQ(int band, fixed q) { eq_.SetBandQ((ParametricEQ::Band)band, q); RefreshLegacy(); }
    void SetEqBandEnabled(int band, bool on) { eq_.SetBandEnabled((ParametricEQ::Band)band, on); RefreshLegacy(); }
    // FXP_MASTER_EQ8 (bacon-1.5, item 2): per-band type + EXT chain bypass.
    void SetEqBandType(int band, int type) { eq_.SetBandType((ParametricEQ::Band)band, (ParametricEQ::BandType)type); RefreshLegacy(); }
    void SetEqExtBypass(bool on) { eq_.SetExtBypass(on); RefreshLegacy(); }

    // --- Compresor/limitador master (Fase 3) ---
    void SetCompBypass(bool on) { comp_.SetBypass(on); RefreshLegacy(); }
    void SetCompThresholdDb(fixed db) { comp_.SetThresholdDb(db); RefreshLegacy(); }
    void SetCompRatio(fixed ratio) { comp_.SetRatio(ratio); RefreshLegacy(); }
    void SetCompKneeDb(fixed db) { comp_.SetKneeDb(db); RefreshLegacy(); }
    void SetCompAttackMs(fixed ms) { comp_.SetAttackMs(ms); RefreshLegacy(); }
    void SetCompReleaseMs(fixed ms) { comp_.SetReleaseMs(ms); RefreshLegacy(); }
    void SetCompMakeupDb(fixed db) { comp_.SetMakeupDb(db); RefreshLegacy(); }
    void SetCompStereoLink(bool on) { comp_.SetStereoLink(on); RefreshLegacy(); }
    void SetCompSoftClip(bool on) { comp_.SetSoftClip(on); RefreshLegacy(); }
    // bacon-1.5 item 4 (V2): sidechain + dry/wet mix.
    void SetCompSidechainSource(int src) { comp_.SetSidechainSource(src); RefreshLegacy(); }
    void SetCompSidechainHpfHz(fixed hz) { comp_.SetSidechainHpfHz(hz); RefreshLegacy(); }
    void SetCompSidechainAmount(fixed amt) { comp_.SetSidechainAmount(amt); RefreshLegacy(); }
    void SetCompMix(fixed mix) { comp_.SetMix(mix); RefreshLegacy(); }
    fixed GetCompGainReductionDb() const { return comp_.GetGainReductionDb(); }

    // Control-rate readbacks for the UI (PLAN_FX_REDESIGN_ES.md, Fase 4.3).
    fixed GetEqBandFreq(int band) const { return eq_.GetBandFreq((ParametricEQ::Band)band); }
    fixed GetEqBandGainDb(int band) const { return eq_.GetBandGainDb((ParametricEQ::Band)band); }
    fixed GetEqBandQ(int band) const { return eq_.GetBandQ((ParametricEQ::Band)band); }
    bool GetEqBandEnabled(int band) const { return eq_.GetBandEnabled((ParametricEQ::Band)band); }
    int GetEqBandType(int band) const { return (int)eq_.GetBandType((ParametricEQ::Band)band); }
    bool GetEqExtBypass() const { return eq_.GetExtBypass(); }
    bool GetEqBypass() const { return eq_.GetBypass(); }
    fixed GetCompThresholdDb() const { return comp_.GetThresholdDb(); }
    fixed GetCompRatio() const { return comp_.GetRatio(); }
    fixed GetCompKneeDb() const { return comp_.GetKneeDb(); }
    fixed GetCompMakeupDb() const { return comp_.GetMakeupDb(); }
    float GetCompAttackMs() const { return comp_.GetAttackMs(); }
    float GetCompReleaseMs() const { return comp_.GetReleaseMs(); }
    fixed GetCompAttackMsFixed() const { return comp_.GetAttackMsFixed(); }
    fixed GetCompReleaseMsFixed() const { return comp_.GetReleaseMsFixed(); }
    bool GetCompStereoLink() const { return comp_.GetStereoLink(); }
    bool GetCompSoftClip() const { return comp_.GetSoftClip(); }
    bool GetCompBypass() const { return comp_.GetBypass(); }
    // bacon-1.5 item 4 readbacks.
    int GetCompSidechainSource() const { return comp_.GetSidechainSource(); }
    fixed GetCompSidechainHpfHz() const { return comp_.GetSidechainHpfHz(); }
    fixed GetCompSidechainAmount() const { return comp_.GetSidechainAmount(); }
    fixed GetCompMix() const { return comp_.GetMix(); }

    // bacon-1.5 item 5: API unificada de parametros FX.  Cualquier
    // escritura/lectura del motor pasa por aqui (UI via MixerView::fxGet/
    // fxSet, automatizacion Phrase/Table via fxParamFromByte y persistencia
    // FXMASTER).  El id es un FX_P_* de kFxParams_ y el valor se expresa en
    // unidades naturales (ms, dB, Hz, s, ratio, 0/1).  SetParam clampa al
    // rango de la tabla; GetParam devuelve 0.0f para ids fuera de rango.
    void SetParam(int id, float v);
    float GetParam(int id) const;

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
    // bacon-1.5 item 4: accumulate the sidechain tap for the selected track
    // during channel rendering (pre-master, Q15).
    void accumulateSidechainTap(const fixed *buffer, int samplecount);
    // Fill scTap_ from the selected bus (delay/reverb return), pre-master.
    void fillSidechainTapFromBus(int samplecount);

    Buses buses_;
    bool legacyMode_;
    bool sendsAccumulated_;
    bool anyChannelSendActive_;
    unsigned long callCount_;
    unsigned long frames_;
    unsigned long maxFrames_;
    unsigned long rtViolations_;
    int sampleRate_;
    fixed delaySend_;
    fixed delayReturn_;
    fixed reverbSend_;
    fixed reverbReturn_;
    // bacon-1.5 item 4: zero-latency sidechain tap for the compressor
    // (static, single-use per Process() call).
    fixed scTap_[FX_ENGINE_MAX_FIXED];
    bool scTapValid_;
    DelayLine delay_;
    Reverb reverb_;
    ParametricEQ eq_;
    Compressor comp_;
};

} // namespace FxEngine

#endif
