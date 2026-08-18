#ifndef _SPECTRUM_ANALYZER_H_
#define _SPECTRUM_ANALYZER_H_

#include "Application/Utils/fixed.h"

class I_Instrument;

/*
 * SpectrumAnalyzer -- real-time spectral display backend for the instrument
 * 8-band EQ view.
 *
 * BACON_1.5_ANALYZER_MIX (bacon-1.5, item 7, feedback): the analyzer is fed
 * from the FINAL MASTER MIX instead of individual instruments:
 *   - AudioOutDriver / DummyAudioOut feed the post-master, post-FxEngine
 *     output buffer (the exact signal that reaches the speakers), so the
 *     spectrum shows the whole mix -- all instruments, EQ8 included -- no
 *     matter which instrument is being edited.
 *   - The per-instrument taps in PlayerChannel / AuditionChannel and the
 *     targetInstrument_ filtering are gone.
 *
 * Threading / perf contract:
 *   - The audio (RT) thread ONLY calls FeedMix(): a mono downsample of the
 *     master buffer into a small ring.  When nobody is listening
 *     (armed == false) it returns on a single branch -- zero cost.
 *   - Compute() runs on the UI thread only while the EQ view is open,
 *     throttled to ~12 fps.  It runs a 1024-point radix-2 FFT (float math is
 *     fine here: never the audio thread) and derives log-scaled bins.
 *   - No allocation in either path; all storage is a singleton.
 */

class SpectrumAnalyzer {
public:
    static const int kRingFrames = 2048;
    static const int kFftSize = 1024;
    static const int kLogBins = 24;
    static const int kRate = 48000;

    static SpectrumAnalyzer &Get();

    // RT thread: push the interleaved stereo samples of the master mix
    // (post everything).  The tap records only when armed; otherwise it
    // returns on a single branch.
    void FeedMix(const fixed *stereo, int frames);

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