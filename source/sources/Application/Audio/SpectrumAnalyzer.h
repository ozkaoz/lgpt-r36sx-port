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
 * BACON_1.5_ANALYZER_INSTRUMENT (U2.59, feedback #12): that went too far
 * for the EQ8 view -- with the analyzer stuck on the whole mix, the user
 * could NOT see what the EQ was doing to the instrument being edited:
 *   - "el EQ muestra que el kick en 2500 hz no tiene nada, pero si le subo
 *     a la EQ en bell ahi si modifica el sonido" (the kick's 2500 Hz click
 *     was drowned by the mix, so the bars showed nothing while the boost
 *     was audible), and
 *   - "bajar db en las frecuencias mas bajas sigue bosteando esas
 *     frecuencias" (cutting the kick's lows did not drop the low bars:
 *     the bass and the rest of the mix still filled them).
 * The EQ8 view now targets the instrument it edits: SetInstrumentTarget()
 * redirects the tap to that instrument's POST-EQ dry output (tapped inside
 * the instrument Render, pre track gain/FX sends, same count<<15 scale and
 * mono average as the master tap).  The spectrum and the drawn EQ curve
 * then show the SAME signal: boosts visibly raise the bars, cuts visibly
 * lower them.  The master FeedMix is ignored while a target is set; when
 * the view closes the target clears and the analyzer goes back to the mix.
 *
 * Threading / perf contract:
 *   - The audio (RT) thread ONLY calls FeedMix()/FeedInstrument(): a mono
 *     downsample into a small ring.  When nobody is listening
 *     (armed == false, or no target matches) it returns on a single branch
 *     -- zero cost.  SetInstrumentTarget()/SetArmed() run on the UI thread;
 *     the RT thread reads the target pointer, which is written only while
 *     the RT thread is not running (focus changes), so a volatile pointer
 *     is enough.
 *   - Compute() runs on the UI thread only while the EQ view is open,
 *     throttled to ~12 fps.  It runs a 4096-point radix-2 FFT (float math
 *     is fine here: never the audio thread) and derives log-scaled bins.
 *   - No allocation in either path; all storage is a singleton.
 */

class SpectrumAnalyzer {
public:
    // BACON_1.5_ANALYZER_FINE (U2.61, feedback #13) -> BACON_1.5_ANALYZER_
    // FINER (U2.62, feedback #14): 16384 points (2.93 Hz/bin at 48 kHz,
    // 341 ms window) so the log bars each resolve 4+ real bins even at
    // 20 Hz (bar 0 covers bins 4..10) and the PEAK marker interpolates to
    // ~0.5 Hz precision ("analizador mas preciso" on top of U2.61's fine
    // grid).  308 log bars at 1 px each draw the full 308 px EQ canvas as a
    // contiguous 1 px spectrum line.  The 16384-point FFT costs ~2x the
    // 8192 one (still UI-thread, throttled to ~12 fps).
    static const int kRingFrames = 16384;
    static const int kFftSize = 16384;
    static const int kLogBins = 308;
    static const int kRate = 48000;

    static SpectrumAnalyzer &Get();

    // RT thread: push the interleaved stereo samples of the master mix
    // (post everything).  The tap records only when armed; otherwise it
    // returns on a single branch.  While an instrument target is set the
    // master tap is ignored (the ring belongs to the instrument).
    void FeedMix(const fixed *stereo, int frames);

    // BACON_1.5_ANALYZER_INSTRUMENT (U2.59): RT thread: push ONE
    // instrument's post-EQ dry output (same scale as the master tap).
    // Called from inside the instrument Render only when WantsInstrument()
    // matches, so the branch cost is a pointer compare.
    void FeedInstrument(const fixed *stereo, int frames);
    bool WantsInstrument(const I_Instrument *instr) const {
        return armed_ && targetInstr_ != 0 && targetInstr_ == instr;
    }
    // UI thread (EQ8 view focus): target the instrument being edited; 0
    // clears the target and the analyzer goes back to the master mix.
    void SetInstrumentTarget(const I_Instrument *instr) { targetInstr_ = instr; }

    void SetArmed(bool armed);
    bool IsArmed() const { return armed_; }

    // UI thread, throttled by the caller.  Recomputes bins from the newest
    // window; returns true when new audio has arrived since the last call.
    bool Compute();

    const fixed *Bins() const { return bins_; }
    int BinCount() const { return kLogBins; }

    // BACON_1.5_ANALYZER_PEAK (U2.61, feedback #13): frequency in Hz of the
    // strongest FFT bin within 20 Hz..20 kHz from the LAST Compute(), with
    // parabolic interpolation between the two neighbours -- sub-Hz precision
    // at the 2.93 Hz/bin grid.  Returns 0 when no window has run yet.  The
    // EQ8 view uses it to place the L2+R2 peak marker.
    float PeakFrequency() const;

    // BACON_1.5_ANALYZER_PEAKHIST (U2.62, feedback #14): the marker is now
    // the HISTORICAL peak: Compute() keeps the loudest (frequency, magnitude)
    // pair seen since the last PeakTrackReset(), so L2+R2 marks where the
    // sound's energy is CENTERED over the whole listening window, not where
    // it happened to be the moment the buttons were pressed.  The values are
    // read on the UI thread after Compute() (the only writer), so no locking.
    void PeakTrackReset();
    float PeakFrequencyHistory() const { return peakHzHist_; }
    bool PeakHasHistory() const { return peakMag2Hist_ > 0.0f; }

private:
    SpectrumAnalyzer();
    void runFft();

    fixed ring_[kRingFrames];
    int ringPos_;
    volatile bool armed_;
    volatile const I_Instrument *targetInstr_;
    volatile unsigned int generation_;
    unsigned int lastSeenGeneration_;

    float wre_[kFftSize];
    float wim_[kFftSize];
    fixed bins_[kLogBins];
    int binLo_[kLogBins];
    int binHi_[kLogBins];

    // BACON_1.5_ANALYZER_PEAKHIST (U2.62): historical peak tracking state
    // (see the accessors above).  Written only by Compute() (UI thread).
    float peakHzHist_;
    float peakMag2Hist_;

    // BACON_1.5_ANALYZER_HIGH_FREQ (U2.63): log bin parameters for
    // Compute() high-frequency boost and variable bin fraction.
    float fLo_;
    float fHi_;
    float step_;
};

#endif