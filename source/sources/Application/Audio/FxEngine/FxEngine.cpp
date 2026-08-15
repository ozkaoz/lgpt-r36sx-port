#include "FxEngine.h"
#include "Application/Player/SyncMaster.h"
#include "Application/Mixer/FxPages.h"

namespace FxEngine {

FxEngine::FxEngine()
    : legacyMode_(true), sendsAccumulated_(false), anyChannelSendActive_(false),
      callCount_(0), frames_(0), maxFrames_(0),
      rtViolations_(0), sampleRate_(44100),
      delaySend_(0), delayReturn_(fl2fp(0.5f)),
      reverbSend_(0), reverbReturn_(fl2fp(0.5f)), scTapValid_(false) {
    delay_.SetSampleRate(sampleRate_);
    delay_.SetMix(i2fp(1)); // send/return: full wet return
    reverb_.SetSampleRate(sampleRate_);
    reverb_.SetMix(i2fp(1));
    eq_.SetSampleRate(sampleRate_);
    eq_.SetBypass(true);   // off by default: master stays dry
    comp_.SetSampleRate(sampleRate_);
    comp_.SetBypass(true); // off by default: master stays dry
}

FxEngine &FxEngine::GetInstance() {
    static FxEngine instance;
    return instance;
}

void FxEngine::Reset() {
    callCount_ = 0;
    frames_ = 0;
    maxFrames_ = 0;
    rtViolations_ = 0;
    sendsAccumulated_ = false;
    scTapValid_ = false;
    delay_.Reset();
    reverb_.Reset();
    eq_.Reset();
    comp_.Reset();
}

void FxEngine::SetSampleRate(int rate) {
    if (rate < 8000 || rate > 96000) {
        ++rtViolations_;
        return;
    }
    sampleRate_ = rate;
    delay_.SetSampleRate(rate);
    reverb_.SetSampleRate(rate);
    eq_.SetSampleRate(rate);
    comp_.SetSampleRate(rate);
}

// Fase 5 auto-engage: legacy bypass is only active while every master FX
// parameter sits at its legacy default AND no per-track send is raised.
// Any user edit (UI page, FX command or channel send) flips it off so the
// DSP actually runs; returning everything to default re-engages bypass.
void FxEngine::RefreshLegacy() {
    legacyMode_ = AllParamsAtLegacyDefault() && !anyChannelSendActive_;
}

bool FxEngine::AllParamsAtLegacyDefault() const {
    if (delaySend_ != 0 || reverbSend_ != 0) return false;
    if (delayReturn_ != fl2fp(0.5f) || reverbReturn_ != fl2fp(0.5f)) return false;
    // Delay line: default time 0 ms, no feedback, width 1, mix 1, no flags.
    if (delay_.GetDelayMsTarget() != 0) return false;
    if (delay_.GetFeedback() != 0) return false;
    if (delay_.GetMix() != i2fp(1)) return false;
    if (delay_.GetWidth() != i2fp(1)) return false;
    if (delay_.GetPingPong()) return false;
    if (delay_.GetSaturation()) return false;
    if (delay_.GetBypass()) return false;
    // bacon-1.5 item 3: defaults are FREE mode, division 1/16 (inert in FREE),
    // LOW CUT 20 Hz and HIGH CUT 20000 Hz (open filters, legacy state).
    if (delay_.GetSync()) return false;
    if (delay_.GetDivision() != SDIV_1_16) return false;
    if (delay_.GetLoopLPHz() != fl2fp(20000.0f)) return false;
    if (delay_.GetLoopHPHz() != fl2fp(20.0f)) return false;
    // Reverb: default predelay 0, decay 1.0 s, size 1, damping 0.5, width 1.
    if (reverb_.GetPredelayMs() != 0) return false;
    if (reverb_.GetDecayTarget() != fl2fp(1.0f)) return false;
    if (reverb_.GetSize() != i2fp(1)) return false;
    if (reverb_.GetDamping() != fl2fp(0.5f)) return false;
    if (reverb_.GetWidth() != i2fp(1)) return false;
    if (reverb_.GetMode() != Reverb::NORMAL) return false;
    if (reverb_.GetMix() != i2fp(1)) return false;
    if (reverb_.GetBypass()) return false;
    // bacon-1.5 item 3: input HP 20 Hz / LP 20000 Hz = open filters (legacy).
    if (reverb_.GetInputHPHz() != fl2fp(20.0f)) return false;
    if (reverb_.GetInputLPHz() != fl2fp(20000.0f)) return false;
    // Master EQ: global bypass on (dry) by default, all bands disabled.
    if (!eq_.GetBypass()) return false;
    // FXP_MASTER_EQ8 (bacon-1.5, item 2): EXT bands (BAND3..BAND7) default to
    // a 2/4/8/16 kHz ladder, 0 dB, type BELL => implicit-disabled, so the
    // legacy-all-defaults check stays true.
    static const fixed kBandFreqDefault[ParametricEQ::kNumBands] = {
        fl2fp(100.0f), fl2fp(1000.0f), fl2fp(10000.0f),
        fl2fp(2000.0f), fl2fp(4000.0f), fl2fp(8000.0f),
        fl2fp(16000.0f), fl2fp(16000.0f) };
    for (int b = 0; b < ParametricEQ::kNumBands; b++) {
        if (eq_.GetBandEnabled((ParametricEQ::Band)b)) return false;
        if (eq_.GetBandFreq((ParametricEQ::Band)b) != kBandFreqDefault[b]) return false;
        if (eq_.GetBandGainDb((ParametricEQ::Band)b) != 0) return false;
        if (eq_.GetBandQ((ParametricEQ::Band)b) != fl2fp(1.0f)) return false;
        // EXT bands must be neutral BELL at defaults; base LOW/HIGH are
        // shelves by design (golden topology).
        if (b >= ParametricEQ::BAND3 &&
            eq_.GetBandType((ParametricEQ::Band)b) != ParametricEQ::BT_BELL) return false;
    }
    // Master compressor: bypass on (dry) by default, others at ctor defaults.
    if (!comp_.GetBypass()) return false;
    if (comp_.GetThresholdDb() != fl2fp(-24.0f)) return false;
    if (comp_.GetRatio() != fl2fp(4.0f)) return false;
    if (comp_.GetKneeDb() != fl2fp(6.0f)) return false;
    if (comp_.GetMakeupDb() != 0) return false;
    if (comp_.GetAttackMs() != 15.0f) return false;
    if (comp_.GetReleaseMs() != 200.0f) return false;
    if (!comp_.GetStereoLink()) return false;
    if (!comp_.GetSoftClip()) return false;
    return true;
}

void FxEngine::Process(fixed *buffer, int samplecount) {
    if (samplecount <= 0 || !buffer) {
        ++rtViolations_;
        return;
    }

    ++callCount_;
    frames_ += (unsigned long)samplecount;
    if ((unsigned long)samplecount > maxFrames_) {
        maxFrames_ = (unsigned long)samplecount;
    }

    // Legacy mode (default) preserves the original master path exactly:
    // no DSP, gain 1.0, buffer untouched.
    if (legacyMode_) {
        return;
    }

    if ((unsigned long)samplecount > FX_ENGINE_MAX_FRAMES) {
        ++rtViolations_;
        return;
    }

    processSendReturns(buffer, samplecount);
}

// Sends/returns: dry passes through; delay/reverb process their send buses
// and their returns are summed back into the master with their return gains.
// All buffers are static (Buses), zero malloc/new/syscalls.
void FxEngine::processSendReturns(fixed *buffer, int samplecount) {
    int n = samplecount * 2;

    // The master bus runs at int16<<15 scale (AudioMixer), but the DSP
    // kernels (delay/reverb/EQ/compressor) are Q15 (range ±1.0 = ±i2fp(1)).
    // Normalize the snapshot into Q15 first, otherwise every master sample
    // (~5e8) collapses into the DSP saturate() clamps and the output is
    // destroyed.  The >>FIXED_SHIFT / <<FIXED_SHIFT round trip is exact.
    for (int i = 0; i < n; i++) buses_.master_[i] = buffer[i] >> FIXED_SHIFT;

    // Build send buses.  Fase 4: per-track sends were accumulated into
    // send_[0]/send_[1] by AccumulateChannelSend() during channel rendering
    // (already normalized to Q15).  If no channel accumulated (direct
    // Process() on a mixed buffer), fall back to the global delaySend_/
    // reverbSend_ gains so the Fase 2/3 send model is preserved.
    if (!sendsAccumulated_) {
        for (int i = 0; i < n; i++) {
            buses_.send_[0][i] = fp_mul(buses_.master_[i], delaySend_);
            buses_.send_[1][i] = fp_mul(buses_.master_[i], reverbSend_);
        }
    }
    sendsAccumulated_ = false;

    // bacon-1.5 item 3: SYNC mode recomputes the delay time from the song
    // tempo and the musical division every callback.  Pure integer math at
    // control rate (no trascendentals); the target glides per-sample inside
    // DelayLine::Process.
    if (delay_.GetSync()) {
        int bpm = SyncMaster::GetInstance()->GetTempo();
        delay_.SetDelayMs(DelayLine::SyncDivisionToMs(delay_.GetDivision(),
                                                      bpm));
    }

    // Process returns (wet).
    delay_.Process(buses_.send_[0], buses_.returnDelay_, samplecount);
    reverb_.Process(buses_.send_[1], buses_.returnReverb_, samplecount);

    // Sum returns back into the master output (Q15).
    for (int i = 0; i < n; i++) {
        buses_.master_[i] = buses_.master_[i]
                            + fp_mul(buses_.returnDelay_[i], delayReturn_)
                            + fp_mul(buses_.returnReverb_[i], reverbReturn_);
    }

    // Master chain (Fase 3): EQ then compressor/limiter, in place.
    eq_.Process(buses_.master_, buses_.master_, samplecount);
    // bacon-1.5 item 4: zero-latency sidechain.  TRACK taps were accumulated
    // during channel rendering; BUS taps are sampled from the returns right
    // now (pre-master-sum).  The tap is single-use: the compressor consumes
    // it inside this Process() call.
    int scSrc = comp_.GetSidechainSource();
    if (scSrc >= Compressor::SC_TRACK_1 && scSrc <= Compressor::SC_TRACK_8) {
        if (scTapValid_) {
            comp_.SetSidechainInput(scTap_, samplecount);
        }
    } else if (scSrc == Compressor::SC_DELAY_RETURN ||
               scSrc == Compressor::SC_REVERB_RETURN) {
        fillSidechainTapFromBus(samplecount);
        if (scTapValid_) {
            comp_.SetSidechainInput(scTap_, samplecount);
        }
    }
    comp_.Process(buses_.master_, buses_.master_, samplecount);
    scTapValid_ = false;

    // Expand back to the int16<<15 scale that AudioMixer / clipToMix expect,
    // with the same hard clip the mixer uses so full-scale DSP output (Q15
    // ±1.0) maps exactly onto a full-scale 16-bit sample.
    const fixed maxPos = i2fp(32767);
    const fixed maxNeg = i2fp(-32768);
    for (int i = 0; i < n; i++) {
        fixed s = buses_.master_[i] << FIXED_SHIFT;
        if (s > maxPos) s = maxPos;
        else if (s < maxNeg) s = maxNeg;
        buffer[i] = s;
    }
}

// Called by each PlayerChannel after it renders its own instrument buffer and
// before the master mix.  RT-safe: pure fixed-point accumulation into the
// static send buses, no allocation/syscalls.
void FxEngine::AccumulateChannelSend(int channel, const fixed *buffer,
                                     int samplecount, fixed delayGain,
                                     fixed reverbGain) {
    if (legacyMode_) {
        return;
    }
    if (samplecount <= 0 || !buffer) {
        ++rtViolations_;
        return;
    }
    if (channel < 0 || channel >= FX_ENGINE_MAX_CHANNELS) {
        ++rtViolations_;
        return;
    }
    if ((unsigned long)samplecount > FX_ENGINE_MAX_FRAMES) {
        ++rtViolations_;
        return;
    }
    int n = samplecount * 2;
    // First accumulator of this frame: the send buses still hold the previous
    // frame's data (static buffers), so zero the part this frame will use.
    if (!sendsAccumulated_) {
        for (int i = 0; i < n; i++) {
            buses_.send_[0][i] = 0;
            buses_.send_[1][i] = 0;
        }
        sendsAccumulated_ = true;
    }
    if (delayGain != 0) {
        fixed *dst = buses_.send_[0];
        for (int i = 0; i < n; i++) {
            // Channel buffer is int16<<15 scale; normalize to Q15 so the send
            // buses match the delay/reverb DSP range.
            dst[i] += fp_mul(buffer[i] >> FIXED_SHIFT, delayGain);
        }
    }
    if (reverbGain != 0) {
        fixed *dst = buses_.send_[1];
        for (int i = 0; i < n; i++) {
            dst[i] += fp_mul(buffer[i] >> FIXED_SHIFT, reverbGain);
        }
    }
    // bacon-1.5 item 4: sidechain tap of the selected track (pre-master, so
    // the compressor sees the source before the master EQ/returns).
    if (comp_.GetSidechainSource() == Compressor::SC_TRACK_1 + channel) {
        accumulateSidechainTap(buffer, samplecount);
    }
}

// bacon-1.5 item 4: accumulate the zero-latency sidechain tap.  The tap is
// the track's rendered audio (int16<<15 scale) normalized to Q15, summed over
// the whole block so every note of the track participates (a track may render
// several voices in one block).  RT-safe: static buffer, pure accumulation.
void FxEngine::accumulateSidechainTap(const fixed *buffer,
                                      int samplecount) {
    if (!scTapValid_) {
        for (int i = 0; i < samplecount * 2; i++) scTap_[i] = 0;
        scTapValid_ = true;
    }
    for (int i = 0; i < samplecount * 2; i++) {
        scTap_[i] += buffer[i] >> FIXED_SHIFT;
    }
}

// bacon-1.5 item 4: the compressor can also duck from a bus tap: the delay or
// reverb return (post-return-gain, pre-master-sum).  Kept in Q15.
void FxEngine::fillSidechainTapFromBus(int samplecount) {
    int src = comp_.GetSidechainSource();
    const fixed *srcBuf = 0;
    if (src == Compressor::SC_DELAY_RETURN) {
        srcBuf = buses_.returnDelay_;
    } else if (src == Compressor::SC_REVERB_RETURN) {
        srcBuf = buses_.returnReverb_;
    }
    if (!srcBuf) {
        scTapValid_ = false;
        return;
    }
    for (int i = 0; i < samplecount * 2; i++) scTap_[i] = srcBuf[i];
    scTapValid_ = true;
}

// bacon-1.5 item 5: API unificada de parametros FX.  Unico punto de
// escritura/lectura del motor por id de kFxParams_ (UI, automatizacion
// Phrase/Table y persistencia FXMASTER comparten la misma conversion y el
// mismo clamp).  El switch fue movido de MixerView::fxSet/fxGet sin cambiar
// ninguna conversion ni efecto lateral (p. ej. el enable/desbypass automatico
// al editar ganancia de banda EQ).
void FxEngine::SetParam(int id, float v) {
    if (id < 0 || id >= FX_PARAM_COUNT) return;
    const FxParamSpec &spec = kFxParams_[id];
    if (v < spec.vmin) v = spec.vmin;
    if (v > spec.vmax) v = spec.vmax;
    switch (id) {
    case FX_P_DLY_TIME: SetDelayTimeMs(fl2fp(v)); break;
    case FX_P_DLY_FBK:  SetDelayFeedback(fl2fp(v)); break;
    case FX_P_DLY_MIX:  SetDelayMix(fl2fp(v)); break;
    case FX_P_DLY_WID:  SetDelayWidth(fl2fp(v)); break;
    case FX_P_DLY_PP:   SetDelayPingPong(v >= 0.5f); break;
    case FX_P_DLY_SAT:  SetDelaySaturation(v >= 0.5f); break;
    case FX_P_DLY_BYP:  SetDelayBypass(v >= 0.5f); break;
    case FX_P_RVB_PRE:  SetReverbPredelayMs(fl2fp(v)); break;
    case FX_P_RVB_DEC:  SetReverbDecay(fl2fp(v)); break;
    case FX_P_RVB_SIZ:  SetReverbSize(fl2fp(v)); break;
    case FX_P_RVB_DMP:  SetReverbDamping(fl2fp(v)); break;
    case FX_P_RVB_WID:  SetReverbWidth(fl2fp(v)); break;
    case FX_P_RVB_MODE: SetReverbMode((int)v); break;
    case FX_P_RVB_BYP:  SetReverbBypass(v >= 0.5f); break;
    case FX_P_EQ_BYP:   SetEqBypass(v >= 0.5f); break;
    case FX_P_EQ_LOW_FRQ: SetEqBandFreq(0, fl2fp(v)); break;
    case FX_P_EQ_LOW_GAI: SetEqBandGainDb(0, fl2fp(v));
        SetEqBandEnabled(0, true); SetEqBypass(false); break;
    case FX_P_EQ_LOW_Q:   SetEqBandQ(0, fl2fp(v)); break;
    case FX_P_EQ_LOW_EN:  SetEqBandEnabled(0, v >= 0.5f); break;
    case FX_P_EQ_MID_FRQ: SetEqBandFreq(1, fl2fp(v)); break;
    case FX_P_EQ_MID_GAI: SetEqBandGainDb(1, fl2fp(v));
        SetEqBandEnabled(1, true); SetEqBypass(false); break;
    case FX_P_EQ_MID_Q:   SetEqBandQ(1, fl2fp(v)); break;
    case FX_P_EQ_MID_EN:  SetEqBandEnabled(1, v >= 0.5f); break;
    case FX_P_EQ_HI_FRQ:  SetEqBandFreq(2, fl2fp(v)); break;
    case FX_P_EQ_HI_GAI:  SetEqBandGainDb(2, fl2fp(v));
        SetEqBandEnabled(2, true); SetEqBypass(false); break;
    case FX_P_EQ_HI_Q:    SetEqBandQ(2, fl2fp(v)); break;
    case FX_P_EQ_HI_EN:   SetEqBandEnabled(2, v >= 0.5f); break;
    case FX_P_EQX_BYP:    SetEqExtBypass(v >= 0.5f); break;
    case FX_P_EQX_B3_FRQ: SetEqBandFreq(3, fl2fp(v)); break;
    case FX_P_EQX_B3_GAI: SetEqBandGainDb(3, fl2fp(v)); SetEqBypass(false); break;
    case FX_P_EQX_B3_Q:   SetEqBandQ(3, fl2fp(v)); break;
    case FX_P_EQX_B3_TYP: SetEqBandType(3, (int)v); break;
    case FX_P_EQX_B4_FRQ: SetEqBandFreq(4, fl2fp(v)); break;
    case FX_P_EQX_B4_GAI: SetEqBandGainDb(4, fl2fp(v)); SetEqBypass(false); break;
    case FX_P_EQX_B4_Q:   SetEqBandQ(4, fl2fp(v)); break;
    case FX_P_EQX_B4_TYP: SetEqBandType(4, (int)v); break;
    case FX_P_EQX_B5_FRQ: SetEqBandFreq(5, fl2fp(v)); break;
    case FX_P_EQX_B5_GAI: SetEqBandGainDb(5, fl2fp(v)); SetEqBypass(false); break;
    case FX_P_EQX_B5_Q:   SetEqBandQ(5, fl2fp(v)); break;
    case FX_P_EQX_B5_TYP: SetEqBandType(5, (int)v); break;
    case FX_P_EQX_B6_FRQ: SetEqBandFreq(6, fl2fp(v)); break;
    case FX_P_EQX_B6_GAI: SetEqBandGainDb(6, fl2fp(v)); SetEqBypass(false); break;
    case FX_P_EQX_B6_Q:   SetEqBandQ(6, fl2fp(v)); break;
    case FX_P_EQX_B6_TYP: SetEqBandType(6, (int)v); break;
    case FX_P_EQX_B7_FRQ: SetEqBandFreq(7, fl2fp(v)); break;
    case FX_P_EQX_B7_GAI: SetEqBandGainDb(7, fl2fp(v)); SetEqBypass(false); break;
    case FX_P_EQX_B7_Q:   SetEqBandQ(7, fl2fp(v)); break;
    case FX_P_EQX_B7_TYP: SetEqBandType(7, (int)v); break;
    case FX_P_DLY_SYNC:   SetDelaySync(v >= 0.5f); break;
    case FX_P_DLY_DIV:    SetDelayDivision((int)v); break;
    case FX_P_DLY_LOW:    SetDelayLowCutHz(fl2fp(v)); break;
    case FX_P_DLY_HIG:    SetDelayHighCutHz(fl2fp(v)); break;
    case FX_P_RVB_HP:     SetReverbInputHPHz(fl2fp(v)); break;
    case FX_P_RVB_LP:     SetReverbInputLPHz(fl2fp(v)); break;
    case FX_P_CMP_THR:    SetCompThresholdDb(fl2fp(v)); break;
    case FX_P_CMP_RAT:    SetCompRatio(fl2fp(v)); break;
    case FX_P_CMP_KNE:    SetCompKneeDb(fl2fp(v)); break;
    case FX_P_CMP_ATK:    SetCompAttackMs(fl2fp(v)); break;
    case FX_P_CMP_REL:    SetCompReleaseMs(fl2fp(v)); break;
    case FX_P_CMP_MKU:    SetCompMakeupDb(fl2fp(v)); break;
    case FX_P_CMP_LINK:   SetCompStereoLink(v >= 0.5f); break;
    case FX_P_CMP_SC:     SetCompSoftClip(v >= 0.5f); break;
    case FX_P_CMP_BYP:    SetCompBypass(v >= 0.5f); break;
    case FX_P_CMP_MIX:    SetCompMix(fl2fp(v)); break;
    case FX_P_CMP_SCSRC:  SetCompSidechainSource((int)v); break;
    case FX_P_CMP_SCFLT:  SetCompSidechainHpfHz(fl2fp(v)); break;
    case FX_P_CMP_SCAMT:  SetCompSidechainAmount(fl2fp(v)); break;
    }
}

float FxEngine::GetParam(int id) const {
    switch (id) {
    case FX_P_DLY_TIME: return fp2fl(GetDelayTimeMs());
    case FX_P_DLY_FBK:  return fp2fl(GetDelayFeedback());
    case FX_P_DLY_MIX:  return fp2fl(GetDelayMix());
    case FX_P_DLY_WID:  return fp2fl(GetDelayWidth());
    case FX_P_DLY_PP:   return GetDelayPingPong() ? 1.0f : 0.0f;
    case FX_P_DLY_SAT:  return GetDelaySaturation() ? 1.0f : 0.0f;
    case FX_P_DLY_BYP:  return GetDelayBypass() ? 1.0f : 0.0f;
    case FX_P_RVB_PRE:  return fp2fl(GetReverbPredelayMs());
    case FX_P_RVB_DEC:  return fp2fl(GetReverbDecay());
    case FX_P_RVB_SIZ:  return fp2fl(GetReverbSize());
    case FX_P_RVB_DMP:  return fp2fl(GetReverbDamping());
    case FX_P_RVB_WID:  return fp2fl(GetReverbWidth());
    case FX_P_RVB_MODE: return (float)GetReverbMode();
    case FX_P_RVB_BYP:  return GetReverbBypass() ? 1.0f : 0.0f;
    case FX_P_EQ_BYP:   return GetEqBypass() ? 1.0f : 0.0f;
    case FX_P_EQ_LOW_FRQ: return fp2fl(GetEqBandFreq(0));
    case FX_P_EQ_LOW_GAI: return fp2fl(GetEqBandGainDb(0));
    case FX_P_EQ_LOW_Q:   return fp2fl(GetEqBandQ(0));
    case FX_P_EQ_LOW_EN:  return GetEqBandEnabled(0) ? 1.0f : 0.0f;
    case FX_P_EQ_MID_FRQ: return fp2fl(GetEqBandFreq(1));
    case FX_P_EQ_MID_GAI: return fp2fl(GetEqBandGainDb(1));
    case FX_P_EQ_MID_Q:   return fp2fl(GetEqBandQ(1));
    case FX_P_EQ_MID_EN:  return GetEqBandEnabled(1) ? 1.0f : 0.0f;
    case FX_P_EQ_HI_FRQ:  return fp2fl(GetEqBandFreq(2));
    case FX_P_EQ_HI_GAI:  return fp2fl(GetEqBandGainDb(2));
    case FX_P_EQ_HI_Q:    return fp2fl(GetEqBandQ(2));
    case FX_P_EQ_HI_EN:   return GetEqBandEnabled(2) ? 1.0f : 0.0f;
    case FX_P_EQX_BYP:    return GetEqExtBypass() ? 1.0f : 0.0f;
    case FX_P_EQX_B3_FRQ: return fp2fl(GetEqBandFreq(3));
    case FX_P_EQX_B3_GAI: return fp2fl(GetEqBandGainDb(3));
    case FX_P_EQX_B3_Q:   return fp2fl(GetEqBandQ(3));
    case FX_P_EQX_B3_TYP: return (float)GetEqBandType(3);
    case FX_P_EQX_B4_FRQ: return fp2fl(GetEqBandFreq(4));
    case FX_P_EQX_B4_GAI: return fp2fl(GetEqBandGainDb(4));
    case FX_P_EQX_B4_Q:   return fp2fl(GetEqBandQ(4));
    case FX_P_EQX_B4_TYP: return (float)GetEqBandType(4);
    case FX_P_EQX_B5_FRQ: return fp2fl(GetEqBandFreq(5));
    case FX_P_EQX_B5_GAI: return fp2fl(GetEqBandGainDb(5));
    case FX_P_EQX_B5_Q:   return fp2fl(GetEqBandQ(5));
    case FX_P_EQX_B5_TYP: return (float)GetEqBandType(5);
    case FX_P_EQX_B6_FRQ: return fp2fl(GetEqBandFreq(6));
    case FX_P_EQX_B6_GAI: return fp2fl(GetEqBandGainDb(6));
    case FX_P_EQX_B6_Q:   return fp2fl(GetEqBandQ(6));
    case FX_P_EQX_B6_TYP: return (float)GetEqBandType(6);
    case FX_P_EQX_B7_FRQ: return fp2fl(GetEqBandFreq(7));
    case FX_P_EQX_B7_GAI: return fp2fl(GetEqBandGainDb(7));
    case FX_P_EQX_B7_Q:   return fp2fl(GetEqBandQ(7));
    case FX_P_EQX_B7_TYP: return (float)GetEqBandType(7);
    case FX_P_DLY_SYNC:   return GetDelaySync() ? 1.0f : 0.0f;
    case FX_P_DLY_DIV:    return (float)GetDelayDivision();
    case FX_P_DLY_LOW:    return fp2fl(GetDelayLowCutHz());
    case FX_P_DLY_HIG:    return fp2fl(GetDelayHighCutHz());
    case FX_P_RVB_HP:     return fp2fl(GetReverbInputHPHz());
    case FX_P_RVB_LP:     return fp2fl(GetReverbInputLPHz());
    case FX_P_CMP_THR:    return fp2fl(GetCompThresholdDb());
    case FX_P_CMP_RAT:    return fp2fl(GetCompRatio());
    case FX_P_CMP_KNE:    return fp2fl(GetCompKneeDb());
    case FX_P_CMP_ATK:    return GetCompAttackMs();
    case FX_P_CMP_REL:    return GetCompReleaseMs();
    case FX_P_CMP_MKU:    return fp2fl(GetCompMakeupDb());
    case FX_P_CMP_LINK:   return GetCompStereoLink() ? 1.0f : 0.0f;
    case FX_P_CMP_SC:     return GetCompSoftClip() ? 1.0f : 0.0f;
    case FX_P_CMP_BYP:    return GetCompBypass() ? 1.0f : 0.0f;
    case FX_P_CMP_MIX:    return fp2fl(GetCompMix());
    case FX_P_CMP_SCSRC:  return (float)GetCompSidechainSource();
    case FX_P_CMP_SCFLT:  return fp2fl(GetCompSidechainHpfHz());
    case FX_P_CMP_SCAMT:  return fp2fl(GetCompSidechainAmount());
    }
    return 0.0f;
}

} // namespace FxEngine
