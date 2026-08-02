#include "FxEngine.h"

namespace FxEngine {

FxEngine::FxEngine()
    : legacyMode_(true), callCount_(0), frames_(0), maxFrames_(0),
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

    // Build send buses (channel 0 = delay, channel 1 = reverb).
    for (int i = 0; i < n; i++) {
        buses_.send_[0][i] = fp_mul(buffer[i], delaySend_);
        buses_.send_[1][i] = fp_mul(buffer[i], reverbSend_);
    }

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

} // namespace FxEngine
