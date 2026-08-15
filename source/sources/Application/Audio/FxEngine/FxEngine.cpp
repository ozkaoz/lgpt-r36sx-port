#include "FxEngine.h"
#include "Application/Player/SyncMaster.h"

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
        accumulateSidechainTap(channel, buffer, samplecount);
    }
}

// bacon-1.5 item 4: accumulate the zero-latency sidechain tap.  The tap is
// the track's rendered audio (int16<<15 scale) normalized to Q15, summed over
// the whole block so every note of the track participates (a track may render
// several voices in one block).  RT-safe: static buffer, pure accumulation.
void FxEngine::accumulateSidechainTap(int channel, const fixed *buffer,
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

} // namespace FxEngine
