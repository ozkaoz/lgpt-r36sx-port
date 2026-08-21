#ifndef _SPECTRUM_ANALYZER_H_
#define _SPECTRUM_ANALYZER_H_

#include "Application/Utils/fixed.h"

class I_Instrument;

/*
 * SpectrumAnalyzer -- real-time spectral display backend for the instrument
 * 8-band EQ view.
 *
 * Threading / perf contract:
 *   - The audio (RT) thread ONLY calls FeedMix()/FeedInstrument(): a mono
 *     downsample into a small ring.  When nobody is listening
 *     (armed == false, or no target matches) it returns on a single branch
 *     -- zero cost.  SetInstrumentTarget()/SetArmed() run on the UI thread;
 *     the RT thread reads the target pointer, which is written only while
 *     the RT thread is not running (focus changes), so a volatile pointer
 *     is enough.  In TreeFrog/libretro, GUIWindow::Update() and
 *     TreeFrogAudioDriver::Render() are sequential inside retro_run(),
 *     so no mutex is needed.  Other adapters with concurrent audio/UI threads
 *     would need explicit synchronization.
 *   - Compute() runs on the UI thread only while the EQ view is open,
 *     throttled to ~12 fps.  It runs a 16384-point radix-2 FFT (float math
 *     is fine here: never the audio thread) and derives log-scaled bins.
 *   - No allocation in either path; all storage is a singleton.
 *   - Window Blackman is precomputed lazily on first Compute() to avoid
 *     thousands of cosf() at Get() time (which is called from instrument
 *     Render even when disarmed).
 */

class SpectrumAnalyzer {
public:
    static const int kRingFrames = 16384;
    static const int kFftSize = 16384;
    static const int kLogBins = 308;
    static const int kRate = 48000;

    static SpectrumAnalyzer &Get();

    void FeedMix(const fixed *stereo, int frames);
    void FeedInstrument(const fixed *stereo, int frames);
    bool WantsInstrument(const I_Instrument *instr) const {
        return armed_ && targetInstr_ != 0 && targetInstr_ == instr;
    }
    void SetInstrumentTarget(const I_Instrument *instr);
    void SetArmed(bool armed);
    bool IsArmed() const { return armed_; }

    bool Compute();

    const fixed *Bins() const { return bins_; }
    int BinCount() const { return kLogBins; }
    float BinFrequency(int index) const;

    float PeakFrequency() const { return peakHz_; }

    void PeakTrackReset();
    float PeakFrequencyHistory() const { return peakHzHist_; }
    bool PeakHasHistory() const { return peakMag2Hist_ > 0.0f; }

private:
    SpectrumAnalyzer();
    void runFft();
    void clearCapture();

    fixed ring_[kRingFrames];
    int ringPos_;
    volatile bool armed_;
    volatile const I_Instrument *targetInstr_;
    volatile unsigned int generation_;
    unsigned int lastSeenGeneration_;

    float wre_[kFftSize];
    float wim_[kFftSize];
    float window_[kFftSize];
    bool windowReady_;
    float amplitudeScale_;

    fixed bins_[kLogBins];
    int binLo_[kLogBins];
    int binHi_[kLogBins];
    float binFreq_[kLogBins];

    float peakHz_;
    float peakMag2_;
    float peakHzHist_;
    float peakMag2Hist_;
};

#endif
