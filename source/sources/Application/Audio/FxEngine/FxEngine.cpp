#include "FxEngine.h"

namespace FxEngine {

FxEngine::FxEngine()
    : legacyMode_(true), sendsAccumulated_(false), anyChannelSendActive_(false),
      callCount_(0), frames_(0), maxFrames_(0),
      rtViolations_(0), sampleRate_(44100),
      delaySend_(0), delayReturn_(fl2fp(0.5f)),
      reverbSend_(0), reverbReturn_(fl2fp(0.5f)) {
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
    // Reverb: default predelay 0, decay 1.0 s, size 1, damping 0.5, width 1.
    if (reverb_.GetPredelayMs() != 0) return false;
    if (reverb_.GetDecayTarget() != fl2fp(1.0f)) return false;
    if (reverb_.GetSize() != i2fp(1)) return false;
    if (reverb_.GetDamping() != fl2fp(0.5f)) return false;
    if (reverb_.GetWidth() != i2fp(1)) return false;
    if (reverb_.GetMode() != Reverb::NORMAL) return false;
    if (reverb_.GetMix() != i2fp(1)) return false;
    if (reverb_.GetBypass()) return false;
    // Master EQ: global bypass on (dry) by default, all bands disabled.
    if (!eq_.GetBypass()) return false;
    static const fixed kBandFreqDefault[ParametricEQ::kNumBands] = {
        fl2fp(100.0f), fl2fp(1000.0f), fl2fp(10000.0f) };
    for (int b = 0; b < ParametricEQ::kNumBands; b++) {
        if (eq_.GetBandEnabled((ParametricEQ::Band)b)) return false;
        if (eq_.GetBandFreq((ParametricEQ::Band)b) != kBandFreqDefault[b]) return false;
        if (eq_.GetBandGainDb((ParametricEQ::Band)b) != 0) return false;
        if (eq_.GetBandQ((ParametricEQ::Band)b) != fl2fp(1.0f)) return false;
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

    // Snapshot dry master.
    for (int i = 0; i < n; i++) buses_.master_[i] = buffer[i];

    // Build send buses.  Fase 4: per-track sends were accumulated into
    // send_[0]/send_[1] by AccumulateChannelSend() during channel rendering.
    // If no channel accumulated (direct Process() on a mixed buffer), fall
    // back to the global delaySend_/reverbSend_ gains so the Fase 2/3 send
    // model is preserved.
    if (!sendsAccumulated_) {
        for (int i = 0; i < n; i++) {
            buses_.send_[0][i] = fp_mul(buffer[i], delaySend_);
            buses_.send_[1][i] = fp_mul(buffer[i], reverbSend_);
        }
    }
    sendsAccumulated_ = false;

    // Process returns (wet).
    delay_.Process(buses_.send_[0], buses_.returnDelay_, samplecount);
    reverb_.Process(buses_.send_[1], buses_.returnReverb_, samplecount);

    // Sum returns back into the master output.
    for (int i = 0; i < n; i++) {
        buffer[i] = buses_.master_[i]
                    + fp_mul(buses_.returnDelay_[i], delayReturn_)
                    + fp_mul(buses_.returnReverb_[i], reverbReturn_);
    }

    // Master chain (Fase 3): EQ then compressor/limiter, in place.
    eq_.Process(buffer, buffer, samplecount);
    comp_.Process(buffer, buffer, samplecount);
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
            dst[i] += fp_mul(buffer[i], delayGain);
        }
    }
    if (reverbGain != 0) {
        fixed *dst = buses_.send_[1];
        for (int i = 0; i < n; i++) {
            dst[i] += fp_mul(buffer[i], reverbGain);
        }
    }
}

} // namespace FxEngine
