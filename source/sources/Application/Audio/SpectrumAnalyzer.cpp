#include "SpectrumAnalyzer.h"

#include "Application/Instruments/I_Instrument.h"
#include <math.h>
#include <string.h>

SpectrumAnalyzer &SpectrumAnalyzer::Get() {
    static SpectrumAnalyzer instance;
    return instance;
}

SpectrumAnalyzer::SpectrumAnalyzer()
    : ringPos_(0), armed_(false), generation_(0),
      lastSeenGeneration_(0) {
    memset(ring_, 0, sizeof(ring_));
    memset(wre_, 0, sizeof(wre_));
    memset(wim_, 0, sizeof(wim_));
    memset(bins_, 0, sizeof(bins_));

    // BACON_1.5_ANALYZER_20HZ (U2.52.6, feedback): log bars from 20 Hz to
    // 20 kHz.  The FFT is 1024 points so the low bars resolve real bass
    // (46.9 Hz/bin at 48 kHz instead of 187.5 Hz/bin): with 256 points the
    // bottom nine bars all collapsed onto a single FFT bin and any sample
    // lit them all through spectral leakage ("las barras no miden
    // 20 Hz-20 kHz").
    static const float fLo = 20.0f;
    static const float fHi = 20000.0f;
    const float step = logf(fHi / fLo) / (kLogBins - 1);
    const float hzPerBin = (float)kRate / (float)kFftSize;
    for (int i = 0; i < kLogBins; i++) {
        float fc = fLo * expf(step * i);
        float b = fc / hzPerBin;
        binLo_[i] = (int)(b - b * 0.30f);
        binHi_[i] = (int)(b + b * 0.30f);
        if (binLo_[i] < 1) binLo_[i] = 1;
        if (binHi_[i] >= kFftSize / 2) binHi_[i] = kFftSize / 2 - 1;
        if (binHi_[i] < binLo_[i]) binHi_[i] = binLo_[i];
    }
}

void SpectrumAnalyzer::SetArmed(bool armed) { armed_ = armed; }

// BACON_1.5_ANALYZER_MIX (bacon-1.5, item 7, feedback): the analyzer is fed
// from the final master mix (the buffer that reaches the speakers after the
// master bus and the master FxEngine stage).  No instrument filtering: the
// spectrum shows the whole mix so the EQ8 view reflects what is actually
// sounding.
// BACON_1.5_ANALYZER_SCALE (U2.52.9, feedback #6): the master mix is int16
// DAC counts shifted <<15, so the mono average must be taken in COUNTS
// ((l>>16)+(r>>16)): fp2fl() on the ring then yields the true -1..1 audio
// and the FFT bins map 0 dBFS sine -> peak ~0.25 (Hann window), which the
// view scales x4 to a full bar.  The old (l>>1)+(r>>1) stored the raw
// count<<15 bus value, so fp2fl() returned up to 32767 and EVERY bin
// clamped at 1.0: kick/snare/hat all lit every bar ("las barras no
// reflejan la dinamica real", bars 100% full for any signal).
void SpectrumAnalyzer::FeedMix(const fixed *stereo, int frames) {
    if (!armed_) return;                     // zero cost
    if (!stereo || frames <= 0) return;
    for (int i = 0; i < frames; i++) {
        fixed l = stereo[i * 2];
        fixed r = stereo[i * 2 + 1];
        ring_[ringPos_] = (l >> 16) + (r >> 16);
        if (++ringPos_ >= kRingFrames) ringPos_ = 0;
    }
    generation_++;
}

void SpectrumAnalyzer::runFft() {
    // Copy newest 256 samples of the ring into the float window.
    for (int i = 0; i < kFftSize; i++) {
        int idx = ringPos_ - kFftSize + i;
        if (idx < 0) idx += kRingFrames;
        float s = fp2fl(ring_[idx]);
        // Hann window
        float w = 0.5f - 0.5f * cosf(2.0f * 3.14159265f * i / (float)(kFftSize - 1));
        wre_[i] = s * w;
        wim_[i] = 0.0f;
    }

    // Radix-2 iterative FFT, in place.
    const int n = kFftSize;
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            float t = wre_[i]; wre_[i] = wre_[j]; wre_[j] = t;
            t = wim_[i]; wim_[i] = wim_[j]; wim_[j] = t;
        }
        int m = n >> 1;
        while (j >= m) { j -= m; m >>= 1; }
        j += m;
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * 3.14159265f / (float)len;
        float wR = cosf(ang);
        float wI = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cR = 1.0f, cI = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k;
                int b2 = a + len / 2;
                float tR = cR * wre_[b2] - cI * wim_[b2];
                float tI = cR * wim_[b2] + cI * wre_[b2];
                wre_[b2] = wre_[a] - tR;
                wim_[b2] = wim_[a] - tI;
                wre_[a] += tR;
                wim_[a] += tI;
                float nR = cR * wR - cI * wI;
                float nI = cR * wI + cI * wR;
                cR = nR; cI = nI;
            }
        }
    }
}

bool SpectrumAnalyzer::Compute() {
    if (generation_ == lastSeenGeneration_) return false;
    lastSeenGeneration_ = generation_;

    runFft();

    for (int i = 0; i < kLogBins; i++) {
        float peak2 = 0.0f;
        for (int b = binLo_[i]; b <= binHi_[i]; b++) {
            float mag2 = wre_[b] * wre_[b] + wim_[b] * wim_[b];
            if (mag2 > peak2) peak2 = mag2;
        }
        float peak = sqrtf(peak2) / (float)kFftSize;
        if (peak > 1.0f) peak = 1.0f;
        bins_[i] = fl2fp(peak);
    }
    return true;
}