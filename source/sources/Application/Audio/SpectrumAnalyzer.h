#ifndef _SPECTRUM_ANALYZER_H_
#define _SPECTRUM_ANALYZER_H_

#include "Application/Utils/fixed.h"

/*
 * SpectrumAnalyzer -- real-time spectral display backend for the instrument
 * 8-band EQ modal.
 *
 * Threading / perf contract:
 *   - The audio (RT) thread ONLY calls Feed(): a mono downsample of the
 *     instrument's dry output into a small ring.  When nobody is listening
 *     (armed == false) it returns on a single branch -- zero cost.
 *   - Compute() runs on the UI thread only while the EQ modal is open,
 *     throttled to ~12 fps.  It runs a 256-point radix-2 FFT (float math is
 *     fine here: never the audio thread) and derives log-scaled bins.
 *   - No allocation in either path; all storage is a singleton.
 */

class SpectrumAnalyzer {
public:
    static const int kRingFrames = 512;
    static const int kFftSize = 256;
    static const int kLogBins = 24;
    static const int kRate = 48000;

    static SpectrumAnalyzer &Get();

    // RT thread: push interleaved stereo samples; the analyzer keeps the mono
    // average.  No-op (single branch) when not armed.
    void Feed(const fixed *stereo, int frames);

    void SetArmed(bool armed);
    bool IsArmed() const { return armed_; }

    // UI thread, throttled by the caller.  Recomputes bins from the newest
    // window; returns true when new audio has arrived since the last call.
    bool Compute();

    const fixed *Bins() const { return bins_; }
    int BinCount() const { return kLogBins; }

private:
    SpectrumAnalyzer();
    void runFft();

    fixed ring_[kRingFrames];
    int ringPos_;
    volatile bool armed_;
    volatile unsigned int generation_;
    unsigned int lastSeenGeneration_;

    float wre_[kFftSize];
    float wim_[kFftSize];
    fixed bins_[kLogBins];
    int binLo_[kLogBins];
    int binHi_[kLogBins];
};

#endif