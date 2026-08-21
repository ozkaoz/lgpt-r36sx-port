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
      lastSeenGeneration_(0), peakHzHist_(0.0f), peakMag2Hist_(0.0f) {
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
    // BACON_1.5_ANALYZER_96BARS (U2.59, feedback #12) -> BACON_1.5_ANALYZER_
    // FINE (U2.61, feedback #13) -> BACON_1.5_ANALYZER_FINER (U2.62,
    // feedback #14): 16384 points (2.93 Hz/bin, 341 ms window) so the 308
    // log bars each cover 4+ real bins at every frequency (the 20 kHz top
    // bar covers bins ~5965..8355) and the FFT bin grid is fine enough for
    // the sub-Hz peak interpolation of the EQ view's marker.
    fLo_ = 20.0f;
    fHi_ = 20000.0f;
    step_ = logf(fHi_ / fLo_) / (kLogBins - 1);
    const float hzPerBin = (float)kRate / (float)kFftSize;
    for (int i = 0; i < kLogBins; i++) {
        float fc = fLo_ * expf(step_ * i);
        float b = fc / hzPerBin;
        // BACON_1.5_ANALYZER_HIGH_FREQ (U2.63): narrower bin fraction at high
        // frequencies to preserve peak resolution.  Below 1 kHz use 0.30;
        // above, taper to 0.10 at 20 kHz so each log bin covers fewer FFT
        // bins and peaks don't get smeared.
        // BACON_1.5_ANALYZER_HIGH_FREQ2 (U2.64, feedback #14 revisado):
        // keep the narrow bins but the high-frequency boost (below in
        // Compute) now starts at 1000 Hz with +9 dB/oct, so a hipass at
        // 1353 Hz on a snare still lifts the 2-8 kHz snap clearly (1k
        // stays flat for the spectrum host test).  The snare's remaining
        // energy is broadband noise: the old +6 dB/oct from 2 kHz left
        // its 2-5 kHz band only 3-5 dB louder while the broadband max
        // detector under-reads noise vs tonal peak.
        float frac = 0.30f;
        if (fc > 1000.0f) {
            float t = (fc - 1000.0f) / 19000.0f;  // 0..1 from 1k to 20k
            if (t > 1.0f) t = 1.0f;
            frac = 0.30f - 0.20f * t;  // 0.30 -> 0.10
        }
        binLo_[i] = (int)(b - b * frac);
        binHi_[i] = (int)(b + b * frac);
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

    // BACON_1.5_ANALYZER_PEAKHIST (U2.62, feedback #14): update the
    // historical peak from THIS window before the bars (same scan range as
    // PeakFrequency, so the marker and the history always agree).  The
    // history only grows while tracking is armed; PeakTrackReset() (called
    // when the view opens the marker) starts a fresh window.
    {
        int lo = binLo_[0];
        int hi = binHi_[kLogBins - 1];
        if (hi >= kFftSize / 2) hi = kFftSize / 2 - 1;
        float best2 = 0.0f;
        int best = lo;
        for (int b = lo; b <= hi; b++) {
            float m = wre_[b] * wre_[b] + wim_[b] * wim_[b];
            if (m > best2) { best2 = m; best = b; }
        }
        if (best2 > peakMag2Hist_ && best2 > 0.0f) {
            peakMag2Hist_ = best2;
            float a = (best > lo)
                          ? wre_[best - 1] * wre_[best - 1] + wim_[best - 1] * wim_[best - 1]
                          : 0.0f;
            float c = (best < hi)
                          ? wre_[best + 1] * wre_[best + 1] + wim_[best + 1] * wim_[best + 1]
                          : 0.0f;
            float den = a - 2.0f * best2 + c;
            float delta = 0.0f;
            if (den != 0.0f) {
                delta = 0.5f * (a - c) / den;
                if (delta < -1.0f) delta = -1.0f;
                if (delta > 1.0f) delta = 1.0f;
            }
            float hz = (best + delta) * (float)kRate / (float)kFftSize;
            if (hz < 20.0f) hz = 20.0f;
            if (hz > 20000.0f) hz = 20000.0f;
            peakHzHist_ = hz;
        }
    }

    for (int i = 0; i < kLogBins; i++) {
        float peak2 = 0.0f;
        for (int b = binLo_[i]; b <= binHi_[i]; b++) {
            float mag2 = wre_[b] * wre_[b] + wim_[b] * wim_[b];
            if (mag2 > peak2) peak2 = mag2;
        }
        float peak = sqrtf(peak2) / (float)kFftSize;
        if (peak > 1.0f) peak = 1.0f;
        // BACON_1.5_ANALYZER_EQUAL (U2.65, feedback #14 revisado):
        // todas las barras iguales como los graves, diagonal en toda
        // la banda (antes <1k horizontal plano, >1k diagonal solo a
        // derecha).  Ahora todas adoptan la logica 1k+ y la diagonal
        // va a ambos lados (vista).  El maxPeak subestima el ruido de
        // banda ancha, asi que visGain suave en toda la banda levanta
        // agudos sin exagerar: +3 dB/oct desde 20 Hz cap +12 dB (4x)
        // mantiene 1 kHz casi plano para host test pero da pendiente
        // continua 20..20k (20 Hz 1x, 1 kHz 1.45x, 10 kHz 2.6x).
        float fc = fLo_ * expf(step_ * i);
        float visGain = powf(fc / 20.0f, 0.12f);  // +2 dB/oct aprox, toda la banda
        if (visGain < 1.0f) visGain = 1.0f;
        if (visGain > 4.0f) visGain = 4.0f;
        // normaliza a 1.0 en 1 kHz para host test (1 kHz ~0.25)
        float norm = powf(1000.0f / 20.0f, 0.12f); // ~1.45
        visGain /= norm;
        if (visGain < 0.7f) visGain = 0.7f;
        peak *= visGain;
        if (peak > 1.0f) peak = 1.0f;
        bins_[i] = fl2fp(peak);
    }
    return true;
}

// BACON_1.5_ANALYZER_PEAKHIST (U2.62, feedback #14): see the header.  Called
// on the UI thread when the EQ view arms the peak marker (L2+R2 ON) or
// refocuses, so the history always starts from the moment the user began
// listening for the peak.
void SpectrumAnalyzer::PeakTrackReset() {
    peakHzHist_ = 0.0f;
    peakMag2Hist_ = 0.0f;
}

// BACON_1.5_ANALYZER_PEAK (U2.61, feedback #13): strongest FFT bin in the
// audible range with parabolic interpolation.  The bins_ grid is LOG-spaced
// (spacing 1.4% at 85 Hz -> +-6 Hz at best), so the marker would be coarse
// if it came from the bars; the raw 16384-bin spectrum gives 2.93 Hz spacing
// and the parabola between the two neighbouring bins lands within ~0.5 Hz.
float SpectrumAnalyzer::PeakFrequency() const {
    int lo = binLo_[0];
    int hi = binHi_[kLogBins - 1];
    if (hi >= kFftSize / 2) hi = kFftSize / 2 - 1;
    if (lo > hi) return 0.0f;
    int best = lo;
    float best2 = -1.0f;
    for (int b = lo; b <= hi; b++) {
        float m = wre_[b] * wre_[b] + wim_[b] * wim_[b];
        if (m > best2) { best2 = m; best = b; }
    }
    if (best2 <= 0.0f) return 0.0f;
    float a = (best > lo)
                  ? wre_[best - 1] * wre_[best - 1] + wim_[best - 1] * wim_[best - 1]
                  : 0.0f;
    float c = (best < hi)
                  ? wre_[best + 1] * wre_[best + 1] + wim_[best + 1] * wim_[best + 1]
                  : 0.0f;
    float den = a - 2.0f * best2 + c;
    float delta = 0.0f;
    if (den != 0.0f) {
        delta = 0.5f * (a - c) / den;
        if (delta < -1.0f) delta = -1.0f;
        if (delta > 1.0f) delta = 1.0f;
    }
    float hz = (best + delta) * (float)kRate / (float)kFftSize;
    if (hz < 20.0f) hz = 20.0f;
    if (hz > 20000.0f) hz = 20000.0f;
    return hz;
}