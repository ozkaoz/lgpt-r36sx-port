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
    static const int kRingFrames = 4096;
    static const int kFftSize = 4096;
    // BACON_1.5_ANALYZER_96BARS (U2.59, feedback #12): the log bars go from
    // 24 to 96 (4x, like a VST Spectrum Analyzer) over the SAME 20 Hz..20
    // kHz span.  The FFT grows to 4096 points (11.7 Hz/bin at 48 kHz) so
    // every bar still resolves several real FFT bins (a 1 kHz tone lands on
    // bin 85, ~85 bins of resolution across the 20 Hz..20 kHz log span) --
    // the bar count is no longer limited by the FFT resolution, and the
    // 20 Hz..70 Hz floor (the first ~3 bars) is preserved.
    static const int kLogBins = 96;
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
};

#endif