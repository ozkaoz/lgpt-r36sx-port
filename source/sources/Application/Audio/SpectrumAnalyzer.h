#ifndef _SPECTRUM_ANALYZER_H_
#define _SPECTRUM_ANALYZER_H_

#include "Application/Utils/fixed.h"

class I_Instrument;

/*
 * SpectrumAnalyzer -- real-time spectral display backend for the instrument
 * 8-band EQ view.
 *
 * BACON_1.5_ANALYZER_TAP (bacon-1.5, item 7): the analyzer is fed from a
 * common, TARGETED tap instead of individual instruments:
 *   - PlayerChannel::Render feeds AFTER the instrument (filter + EQ8) and
 *     BEFORE the track gain/pan (the exact post-EQ/pre-gain point).
 *   - The audition channel feeds the same way.
 *   - A tap only records when its instrument pointer matches the explicit
 *     target (targetInstrument_, set by the EQ8 view).  The mix of other
 *     instruments never leaks in.
 *   - The per-instrument Feed() call inside SampleInstrument is gone.
 *
 * Threading / perf contract:
 *   - The audio (RT) thread ONLY calls FeedChannel(): a mono downsample of
 *     the instrument's dry output into a small ring.  When nobody is
 *     listening (armed == false) it returns on a single branch -- zero cost.
 *   - Compute() runs on the UI thread only while the EQ view is open,
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

    // RT thread: push the interleaved stereo samples of ONE instrument
    // (already post-filter/post-EQ8, pre track gain).  The tap records only
    // when armed AND the instrument is the current target; otherwise it
    // returns on a single branch.
    void FeedChannel(int channel, I_Instrument *instr, const fixed *stereo,
                     int frames);

    void SetArmed(bool armed);
    bool IsArmed() const { return armed_; }

    // Control-rate (UI) target: the instrument whose spectrum is displayed.
    void SetTargetInstrument(I_Instrument *instr) { targetInstrument_ = instr; }

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
    volatile I_Instrument *targetInstrument_;
    volatile unsigned int generation_;
    unsigned int lastSeenGeneration_;

    float wre_[kFftSize];
    float wim_[kFftSize];
    fixed bins_[kLogBins];
    int binLo_[kLogBins];
    int binHi_[kLogBins];
};

#endif