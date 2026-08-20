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
    // BACON_1.5_ANALYZER_96BARS (U2.59, feedback #12): 4096 points
    // (11.7 Hz/bin) so the 96 log bars each still cover 2+ real bins at
    // every frequency (the top 20 kHz bar covers bins ~1549..2171).
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
    // BACON_1.5_ANALYZER_INSTRUMENT (U2.59): while the EQ8 view targets an
    // instrument, the master tap is ignored -- the ring belongs to the
    // instrument being edited (see SetInstrumentTarget).
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

// BACON_1.5_ANALYZER_INSTRUMENT (U2.59, feedback #12): same ring contract
// as FeedMix, fed from inside the instrument Render right after its EQ8
// (post-EQ dry output, pre track gain/FX sends -- the exact signal the
// drawn EQ curve describes).  Same count<<15 scale and (l>>16)+(r>>16)
// mono average, so the bins read exactly like the master tap.
void SpectrumAnalyzer::FeedInstrument(const fixed *stereo, int frames) {
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
    // BACON_1.5_ANALYZER_DCBLOCK (U2.57, feedback #10): the window mean is
    // subtracted BEFORE the FFT.  Percussive attacks carry a DC transient:
    // the kit's "HI HAT 01.wav" swings -4000..+9000 counts (up to ~13% of
    // full scale) during the first 20 ms of the hit (measured window means,
    // dc_attack.py), because the struck membrane starts displaced to one
    // side.  The Hann FFT maps that step onto bins 1-3, so bars 20..120 Hz
    // pinned at full on EVERY percussive hit even though the sustained body
    // of a hi-hat is 500 Hz..10 kHz.  The ear hears the AC content (the
    // click), not the DC step; subtracting the window mean removes exactly
    // that step (it is ~flat inside a 21.3 ms window).
    // U2.57b: a LINEAR detrend (mean + slope) was tried and REJECTED: it
    // cancels part of a 1-cycle tone (a 46.875 Hz sine at bin 1 read 0.136
    // instead of 0.25, i.e. -5.3 dB of a real kick body).  The residual
    // sub-bin ramp of the attack keeps ~3.5% of a bar on the low bars --
    // accepted, it is below perception.
    double mean = 0.0;
    for (int i = 0; i < kFftSize; i++) {
        int idx = ringPos_ - kFftSize + i;
        if (idx < 0) idx += kRingFrames;
        mean += fp2fl(ring_[idx]);
    }
    mean /= (double)kFftSize;
    for (int i = 0; i < kFftSize; i++) {
        int idx = ringPos_ - kFftSize + i;
        if (idx < 0) idx += kRingFrames;
        float s = fp2fl(ring_[idx]) - (float)mean;
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
        // BACON_1.5_ANALYZER_INSTANT_PEAK (U2.57b, feedback #10): the bins
        // are the INSTANTANEOUS per-window peak, no temporal smoothing.
        // The U2.57 exponential smoothing (0.86/0.14 per frame, ~150 ms)
        // was too slow for the high end: a hi-hat's wash decays in
        // 200-500 ms, so its bars never reached a visible level while the
        // sustained kick body (50-100 Hz) filled its bars -- "las barras
        // no se dibujan en sonidos altos; en los graves (kick) si se
        // dibujan".  The detrend above already kills the fake DC transient
        // in the low bars; the highs now react frame-exact like the mixer
        // VU peak.  The view scales peak 0.25 (0 dBFS sine, Hann) x4 to a
        // full bar, so a clean transient lights its real spectral region.
        bins_[i] = fl2fp(peak);
    }
    return true;
}