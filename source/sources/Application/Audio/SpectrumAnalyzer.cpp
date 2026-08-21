#include "SpectrumAnalyzer.h"

#include "Application/Instruments/I_Instrument.h"
#include <math.h>
#include <string.h>

SpectrumAnalyzer &SpectrumAnalyzer::Get() {
    static SpectrumAnalyzer instance;
    return instance;
}

SpectrumAnalyzer::SpectrumAnalyzer()
    : ringPos_(0), armed_(false), targetInstr_(0), generation_(0),
      lastSeenGeneration_(0), windowReady_(false), amplitudeScale_(0.0f),
      peakHz_(0.0f), peakMag2_(0.0f), peakHzHist_(0.0f), peakMag2Hist_(0.0f) {
    memset(ring_, 0, sizeof(ring_));
    // wre_/wim_ will be fully overwritten in runFft before reading, no need to clear
    memset(bins_, 0, sizeof(bins_));
    // Build log centers and exclusive intervals
    const float hzPerBin = (float)kRate / (float)kFftSize;
    for (int i = 0; i < kLogBins; i++) {
        float fc = 20.0f * expf(logf(20000.0f / 20.0f) * i / (kLogBins - 1));
        binFreq_[i] = fc;
    }
    for (int i = 0; i < kLogBins; i++) {
        float edgeLow, edgeHigh;
        if (i == 0) edgeLow = 20.0f;
        else edgeLow = sqrtf(binFreq_[i-1] * binFreq_[i]);
        if (i == kLogBins - 1) edgeHigh = 20000.0f;
        else edgeHigh = sqrtf(binFreq_[i] * binFreq_[i+1]);
        int lo = (int)ceilf(edgeLow / hzPerBin);
        int hi = (int)ceilf(edgeHigh / hzPerBin) - 1;
        if (lo < 1) lo = 1;
        if (hi >= kFftSize/2) hi = kFftSize/2 - 1;
        // Do not widen empty intervals; keep lo>hi for interpolation case
        binLo_[i] = lo;
        binHi_[i] = hi;
    }
    windowReady_ = false;
    amplitudeScale_ = 0.0f;
}

void SpectrumAnalyzer::clearCapture() {
    ringPos_ = 0;
    memset(ring_, 0, sizeof(ring_));
    memset(bins_, 0, sizeof(bins_));
    lastSeenGeneration_ = generation_;
    peakHz_ = 0.0f;
    peakMag2_ = 0.0f;
    peakHzHist_ = 0.0f;
    peakMag2Hist_ = 0.0f;
}

void SpectrumAnalyzer::SetArmed(bool armed) {
    if (armed_ == armed) return;
    armed_ = armed;
    clearCapture();
}

void SpectrumAnalyzer::SetInstrumentTarget(const I_Instrument *instr) {
    if (targetInstr_ == instr) return;
    targetInstr_ = instr;
    clearCapture();
}

float SpectrumAnalyzer::BinFrequency(int index) const {
    if (index < 0) index = 0;
    if (index >= kLogBins) index = kLogBins - 1;
    return binFreq_[index];
}

void SpectrumAnalyzer::FeedMix(const fixed *stereo, int frames) {
    if (!armed_) return;
    if (targetInstr_) return;
    if (!stereo || frames <= 0) return;
    for (int i = 0; i < frames; i++) {
        fixed l = stereo[i * 2];
        fixed r = stereo[i * 2 + 1];
        ring_[ringPos_] = (l >> 16) + (r >> 16);
        if (++ringPos_ >= kRingFrames) ringPos_ = 0;
    }
    generation_++;
}

void SpectrumAnalyzer::FeedInstrument(const fixed *stereo, int frames) {
    if (!armed_) return;
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
    // Lazy window precompute
    if (!windowReady_) {
        double sum = 0.0;
        for (int i = 0; i < kFftSize; i++) {
            float phase = 2.0f * 3.141592653589793f * i / (float)(kFftSize - 1);
            float w = 0.42f - 0.5f * cosf(phase) + 0.08f * cosf(2.0f * phase);
            window_[i] = w;
            sum += w;
        }
        amplitudeScale_ = (float)(2.0 / sum);
        windowReady_ = true;
    }
    // Copy ring once while computing mean
    double mean = 0.0;
    for (int i = 0; i < kFftSize; i++) {
        int idx = ringPos_ - kFftSize + i;
        if (idx < 0) idx += kRingFrames;
        float s = fp2fl(ring_[idx]);
        wre_[i] = s;
        mean += s;
    }
    mean /= (double)kFftSize;
    // Apply window and DC block
    for (int i = 0; i < kFftSize; i++) {
        wre_[i] = (wre_[i] - (float)mean) * window_[i];
        wim_[i] = 0.0f;
    }
    // Radix-2 FFT
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
        float ang = -2.0f * 3.141592653589793f / (float)len;
        float wR = cosf(ang);
        float wI = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cR = 1.0f, cI = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k;
                int b = a + len / 2;
                float tR = cR * wre_[b] - cI * wim_[b];
                float tI = cR * wim_[b] + cI * wre_[b];
                wre_[b] = wre_[a] - tR;
                wim_[b] = wim_[a] - tI;
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
    const float hzPerBin = (float)kRate / (float)kFftSize;
    // Peak search over raw FFT bins 20..20000
    int peakLo = (int)ceilf(20.0f / hzPerBin);
    int peakHi = (int)floorf(20000.0f / hzPerBin);
    if (peakLo < 1) peakLo = 1;
    if (peakHi >= kFftSize/2) peakHi = kFftSize/2 - 1;
    float best2 = -1.0f;
    int best = peakLo;
    for (int b = peakLo; b <= peakHi; b++) {
        float m2 = wre_[b]*wre_[b] + wim_[b]*wim_[b];
        if (m2 > best2) { best2 = m2; best = b; }
    }
    if (best2 > 0.0f) {
        float a = (best > peakLo) ? wre_[best-1]*wre_[best-1] + wim_[best-1]*wim_[best-1] : 0.0f;
        float c = (best < peakHi) ? wre_[best+1]*wre_[best+1] + wim_[best+1]*wim_[best+1] : 0.0f;
        float den = a - 2.0f*best2 + c;
        float delta = 0.0f;
        if (den != 0.0f) {
            delta = 0.5f * (a - c) / den;
            if (delta < -1.0f) delta = -1.0f;
            if (delta > 1.0f) delta = 1.0f;
        }
        float hz = (best + delta) * hzPerBin;
        if (hz < 20.0f) hz = 20.0f;
        if (hz > 20000.0f) hz = 20000.0f;
        peakHz_ = hz;
        peakMag2_ = best2;
        if (best2 > peakMag2Hist_) {
            peakMag2Hist_ = best2;
            peakHzHist_ = hz;
        }
    } else {
        peakHz_ = 0.0f;
        peakMag2_ = 0.0f;
    }
    // Build log bins
    for (int i = 0; i < kLogBins; i++) {
        int lo = binLo_[i];
        int hi = binHi_[i];
        float mag2;
        if (lo <= hi) {
            float peak2 = 0.0f;
            for (int b = lo; b <= hi; b++) {
                float m2 = wre_[b]*wre_[b] + wim_[b]*wim_[b];
                if (m2 > peak2) peak2 = m2;
            }
            mag2 = peak2;
        } else {
            // Interpolate at exact center
            float fc = binFreq_[i];
            float b = fc / hzPerBin;
            int b0 = (int)floorf(b);
            int b1 = (int)ceilf(b);
            if (b0 < 1) b0 = 1;
            if (b1 >= kFftSize/2) b1 = kFftSize/2 - 1;
            if (b0 == b1) {
                mag2 = wre_[b0]*wre_[b0] + wim_[b0]*wim_[b0];
            } else {
                float frac = b - (float)b0;
                float m0 = wre_[b0]*wre_[b0] + wim_[b0]*wim_[b0];
                float m1 = wre_[b1]*wre_[b1] + wim_[b1]*wim_[b1];
                mag2 = m0 * (1.0f - frac) + m1 * frac;
            }
        }
        float peak = sqrtf(mag2) * amplitudeScale_;
        if (peak > 1.0f) peak = 1.0f;
        bins_[i] = fl2fp(peak);
    }
    return true;
}

void SpectrumAnalyzer::PeakTrackReset() {
    peakHzHist_ = 0.0f;
    peakMag2Hist_ = 0.0f;
}
