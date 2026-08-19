#include "AudioMixer.h"
#include "System/System/System.h"
#include <math.h>

#define MAX_POSITIVE_FIXED i2fp(32767)
#define MAX_NEGATIVE_FIXED i2fp(-32768)

// H38.8 OPT_PERF (bacon-1.5 FX/Synth optimization): the module-sum scratch
// used to be heap-allocated on every Render() call (malloc in the audio
// thread).  The primary mix buffer can never hold more than 10000 stereo
// frames (AudioOutDriver::primarySoundBuffer_ is MIX_BUFFER_SIZE/2 fixed
// samples), so a static scratch of that size covers any samplecount the
// pipeline can request.  No dynamic allocation, no freeing, no cache
// misses from a fresh heap block each buffer.
#define AUDIO_MIXER_MAX_RENDER_FRAMES 10000
static fixed s_moduleMixScratch[AUDIO_MIXER_MAX_RENDER_FRAMES * 2];

// BACON_1.5_64BIT_MASTER_SUM (U2.53, feedback #7): the master sum used to
// accumulate INTO the int32 output buffer, so it WRAPPED at +/-2.0 linear
// (2 channels at full scale = 2.147e9 = INT32_MAX).  A hot mix (full-scale
// channels + EQ boosts up to 2x, see InstrumentEq kBlockLimit) wrapped into
// garbage -- and the pre-clip meters could never show a true level above
// 0 dB.  The sum now accumulates in 64 bits; the int32 buffer is written at
// the very end (clamped, never wrapped).
static long long s_moduleSumScratch[AUDIO_MIXER_MAX_RENDER_FRAMES * 2];

// BACON_1.5_64BIT_MASTER_SUM (U2.53, feedback #7): 64-bit -> int32 with a
// CLAMP instead of the old wraparound.  Values inside the int32 range pass
// through bit-exact (the normal path is untouched); only values beyond
// +/-2.0 linear are clamped.
static fixed clamp32(long long v) {
    if (v > 2147483647LL) return 2147483647;
    if (v < -2147483648LL) return -2147483648LL;
    return (fixed)v;
}

AudioMixer::AudioMixer(const char *name):
	T_SimpleList<AudioModule>(false),
	enableRendering_(0),
	writer_(0),
	name_(name)
{
	volume_=(i2fp(1)) ;
    softclip_ = -1;
    softclipGain_ = 0 ;
	masterVolume_ = 100 ;
	masterVolumeCached_ = -1 ;
	dampCached_ = 1.0f ;
	clipped_ = false ;
    clipBypass_ = false ;
    peakValue_ = 0.0f ;
	lastPeakClock_ = 0 ;
	
	// Precalculate constant values for softclipping algorithm
	softClipData_[0].alpha = 1.45f; // -1.5db (approx.)
	softClipData_[1].alpha = 1.07f; // -3db (approx.)
	softClipData_[2].alpha = 0.75f; // -6db (approx.)
	softClipData_[3].alpha = 0.53f; // -9db (approx.)

	for (int i = 0; i < 4; i++) {
		softClipData_[i].alpha23 = softClipData_[i].alpha * (2.0f / 3.0f);
		softClipData_[i].alphaInv = 1.0f / softClipData_[i].alpha;

		if (softClipData_[i].alpha > 1.0f) {
			/* calculates gain compensation differently for
			 * modes with alpha > 1, so there's no drop in loudness
			 * and we can still drive the hard clipper when the input
			 * goes over 1.0
			 */
			softClipData_[i].gainCmp = 1.0f / (1.0f - (pow(softClipData_[i].alphaInv, 2.0f) / 3.0f));
		} else {
			softClipData_[i].gainCmp = 1.0f / softClipData_[i].alpha23;
		}
	}
} ;

AudioMixer::~AudioMixer() {
}

void AudioMixer::SetFileRenderer(const char *path) {
	renderPath_=path ;
} ;

void AudioMixer::EnableRendering(bool enable) {

	if (enable==enableRendering_) {
		return ;
	}

	if (enable) {
		writer_=new WavFileWriter(renderPath_.c_str()) ;
	} 

	enableRendering_=enable ;
	if (!enable) {
		writer_->Close() ;
		SAFE_DELETE(writer_) ;
	}
} ;

bool AudioMixer::Render(fixed *buffer,int samplecount) {
    clipped_ = false;

    // BACON_1.5_64BIT_MASTER_SUM (U2.53, feedback #7): the module sum lands
    // in s_moduleSumScratch (64-bit) instead of wrapping inside `buffer`.
    // The first module's output is copied in, subsequent modules are added.
    // samplecount is bounded by the primary mix buffer (10000 stereo
    // frames), so the scratch never overflows.
    long long *sum64 = s_moduleSumScratch;
    fixed *mixBuffer = 0;
    bool gotData = false;
    IteratorPtr<AudioModule> it(GetIterator());
    for (it->Begin(); !it->IsDone(); it->Next()) {
        AudioModule &current = it->CurrentItem();
        if (!gotData) {
            gotData=current.Render(buffer,samplecount) ;
            if (gotData) {
                for (int i = 0; i < samplecount * 2; i++) sum64[i] = buffer[i];
            }
         } else {
            if (!mixBuffer) {
                // H38.8 OPT_PERF: static scratch (see s_moduleMixScratch).
                mixBuffer = s_moduleMixScratch;
            }
            if (current.Render(mixBuffer,samplecount)) {
                for (int i = 0; i < samplecount * 2; i++) sum64[i] += mixBuffer[i];
            }
         }
     }

     //  Apply volume

     if (gotData) {
         // H38.7 OPT_PERF: pow() was recomputed on every buffer. Cache it and
         // only recompute when the master volume actually changes.
         if (masterVolume_ != masterVolumeCached_) {
             dampCached_ = pow((float)masterVolume_ / 100, 4.0f);
             masterVolumeCached_ = masterVolume_;
         }
         float damp = dampCached_;

         // BACON_1.5_64BIT_MASTER_SUM (U2.53, feedback #7): volume and damp
         // are applied in the 64-bit domain (same math as the old int32
         // path: fp_mul's truncating >>15, and damp*c as a float product),
         // so the pre-clip meter below reads the TRUE headroom instead of
         // the wrapped int32 value.
         if (volume_ != i2fp(1) || damp != 1.0f) {
             for (int i = 0; i < samplecount * 2; i++) {
                 long long v = sum64[i];
                 if (volume_ != i2fp(1)) v = (v * (long long)volume_) >> 15;
                 if (damp != 1.0f) v = (long long)((double)damp * (double)v);
                 sum64[i] = v;
             }
         }

         // TREEFROG_UNCLIPPED_METER_V1 (Bacon 1.1.1):
         // The master-volume damp is applied here, BEFORE metering and
         // clipping.  The old order (damp inside the clip loop) made the
         // meters read the post-clip level, so the master bar could never
         // exceed 1.0 (0 dB) no matter how hot the mix sum was.  Damp first,
         // then measure the TRUE pre-clip level (which CAN exceed 0 dB), then
         // clamp for the int16 output (the driver conversion wraps above 1.0,
         // so the clip itself must stay).
         // BACON_1.5_64BIT_MASTER_SUM (U2.53, feedback #7): the scan reads
         // the 64-bit sum, so sums above +/-2.0 linear (which the old int32
         // buffer could not even hold) still show on the meter.

         // TREEFROG_VU_METERS_V1 + TREEFROG_MIXER_STEREO_METERS_V1:
         // Track the smoothed peak of the output (audible level).  Fast
         // attack, slow decay, so the mixer bars bounce with the music.
         // The scan runs in short sub-blocks with a decay between them, so
         // the meters dip between closely-spaced notes (hi-hats) instead of
         // staying pinned at the held peak of the whole buffer.  Measured on
         // the post-damp, PRE-CLIP sum and split per side (even samples = L,
         // odd = R in the interleaved buffer), so each bar reflects the true
         // level of its side -- including mix sums above 0 dB.
         {
             const int block = 128 ; // stereo samples
             for (int off = 0; off < samplecount * 2; off += block) {
                 int n = samplecount * 2 - off ;
                 if (n > block) n = block ;
                 float peakL = 0.0f ;
                 float peakR = 0.0f ;
                 if (gotData) {
                     // H38.7 OPT_PERF: sample every 4th sample for the peak (see
                     // PlayerChannel::Render for rationale).
                     // TREEFROG_MIXER_STEREO_METERS_V3 (Bacon 1.1.1): a stride-4
                     // scan can only ever visit EVEN indices (0,4,8,...), so
                     // any per-index parity test classifies everything as L and
                     // the R side stayed dead.  The buffer is interleaved and
                     // off is always even (block is even), so sum64[0] is always
                     // L and sum64[1] always R: take the pair per 8 samples
                     // (same density as the old stride 4) and measure both
                     // sides.
                     long long *c = sum64 + off ;
                     for (int i = 0; i < n; i += 8) {
                         // BACON_1.5_VU_SCALE_FIX (U2.52.7) +
                         // BACON_1.5_VOL_SYNTHS_FIX (U2.52.8): the master
                         // bus is int16<<15 (count<<15) scale, so /2^30
                         // gives the linear 0..1 audio level the bars draw
                         // (see PlayerChannel::Render for the rationale).
                         float vL = (float)((double)c[0] / 1073741824.0) ;
                         if (vL < 0.0f) vL = -vL ;
                         if (vL > peakL) peakL = vL ;
                         float vR = (float)((double)c[1] / 1073741824.0) ;
                        if (vR < 0.0f) vR = -vR ;
                        if (vR > peakR) peakR = vR ;
                        c += 8 ;
                    }
                 }
                if (peakL >= peakValueL_) {
                    // BACON_1.5_64BIT_MASTER_SUM (U2.53, feedback #7): >=
                    // (was >): with a constant signal every sub-block has
                    // the EXACT same peak, and strict > decayed the meter
                    // to half on every equal sub-block (the master bar
                    // flickered between full and half on a sustained tone).
                    peakValueL_ = peakL;
                } else {
                    // Fast release so the master bar empties in a few ms of
                    // quiet, giving a live-meter feel even for dense patterns.
                    peakValueL_ *= 0.5f;
                    if (peakValueL_ < 0.002f) peakValueL_ = 0.0f;
                }
                if (peakR >= peakValueR_) {
                    peakValueR_ = peakR;
                } else {
                    peakValueR_ *= 0.5f;
                    if (peakValueR_ < 0.002f) peakValueR_ = 0.0f;
                }
            }
            lastPeakClock_ = System::GetInstance()->GetClock() ;
        }

        // Apply soft/hard clipping before recording.
        // H38.7 OPT_PERF: when the softclipper is bypassed the per-sample
        // path is a pure fixed-point hard clip; when active, soft then hard.
        // The damp has already been applied above, so the clip loop is
        // float-free.
        // TREEFROG_MIXER_STEREO_METERS_V2 (Bacon 1.1.1): with clipBypass_
        // (channel/stream buses) the sum is left UNCLIPPED so the master bus
        // -- and its pre-clip meter -- reads the real level of every channel;
        // the master bus and the audio out keep the clip below.
        // BACON_1.5_64BIT_MASTER_SUM (U2.53, feedback #7): the 64-bit sum is
        // narrowed to int32 HERE.  clipBypass_ buses clamp at +/-INT32_MAX
        // (never wrap); the master bus hard-clips at +/-1.0 (32767 counts)
        // for the int16 DAC path as before.
        if (!clipBypass_) {
            fixed *c = buffer;
            if (softclip_ == -1) {
                for (int i = 0; i < samplecount * 2; i++) {
                    fixed sample = clamp32(sum64[i]);
                    *c++ = hardClip(sample);
                }
            } else {
                for (int i = 0; i < samplecount * 2; i++) {
                    fixed sample = clamp32(sum64[i]);
                    *c++ = hardClip(softClip(sample));
                }
            }
        } else {
            fixed *c = buffer;
            for (int i = 0; i < samplecount * 2; i++) {
                *c++ = clamp32(sum64[i]);
            }
        }
     }
    if (enableRendering_&&writer_) {
		if (!gotData) {
			memset(buffer,0,samplecount*2*sizeof(fixed)) ;
		} ;
		writer_->AddBuffer(buffer,samplecount) ;
	}

     // H38.8 OPT_PERF: no free -- s_moduleMixScratch is a static buffer.
     return gotData ;
} ;

float AudioMixer::GetPeakValue() {
    // TREEFROG_VU_METERS_V4 (H38.7-r3): when the player is stopped the audio
    // driver stops pulling buffers, so Render() (and its per-buffer decay)
    // never runs again and peakValue_ would freeze at its last value. Decay
    // the level here based on wall-clock time instead, so the bars still fall
    // to 0 shortly after the music stops and bounce again on the next start.
    //
    // While the player is running, Render() refreshes peakValue_ every audio
    // buffer (~16.6 ms) and owns the decay; the getter must stay a pure read
    // then (elapsed < idle threshold), otherwise it would double-decay the
    // bars and they would never reach full.
    unsigned long now = System::GetInstance()->GetClock() ;
    unsigned long elapsed = (lastPeakClock_ == 0) ? 0 : (now - lastPeakClock_) ;
    if (elapsed > 100) {
        // Render() has been idle for >100ms: player is stopped. Apply the
        // same 0.5-per-block rate (per 16.6ms) so the fall matches the live
        // fall. 0.5^N empties a full bar in ~10 blocks (~170ms).
        peakValue_ *= powf(0.5f, (float)elapsed / 16.6f) ;
        if (peakValue_ < 0.002f) peakValue_ = 0.0f ;
        lastPeakClock_ = now ;
    }
    return peakValue_ ;
}

// TREEFROG_MIXER_STEREO_METERS_V1 (Bacon 1.1.1): per-side variants of
// GetPeakValue() with the same idle decay (see GetPeakValue for the
// rationale).
float AudioMixer::GetPeakValueL() {
    unsigned long now = System::GetInstance()->GetClock() ;
    unsigned long elapsed = (lastPeakClock_ == 0) ? 0 : (now - lastPeakClock_) ;
    if (elapsed > 100) {
        peakValueL_ *= powf(0.5f, (float)elapsed / 16.6f) ;
        if (peakValueL_ < 0.002f) peakValueL_ = 0.0f ;
        lastPeakClock_ = now ;
    }
    return peakValueL_ ;
}

float AudioMixer::GetPeakValueR() {
    unsigned long now = System::GetInstance()->GetClock() ;
    unsigned long elapsed = (lastPeakClock_ == 0) ? 0 : (now - lastPeakClock_) ;
    if (elapsed > 100) {
        peakValueR_ *= powf(0.5f, (float)elapsed / 16.6f) ;
        if (peakValueR_ < 0.002f) peakValueR_ = 0.0f ;
        lastPeakClock_ = now ;
    }
    return peakValueR_ ;
}

void AudioMixer::SetVolume(fixed volume) { volume_ = volume; }

void AudioMixer::SetClipBypass(bool bypass) { clipBypass_ = bypass; }

void AudioMixer::SetSoftclip(int clip, int gain) {
    softclip_ = clip - 1;
	softclipGain_ = gain;
}

void AudioMixer::SetMasterVolume(int volume) {
	masterVolume_ = volume;
}

bool AudioMixer::Clipped() { return clipped_; }

fixed AudioMixer::hardClip(fixed sample) {
    if (sample > MAX_POSITIVE_FIXED || sample < MAX_NEGATIVE_FIXED) {
        clipped_ = true;
		return sample > 0 ? MAX_POSITIVE_FIXED : MAX_NEGATIVE_FIXED;
    }
    return sample;
}

/* Implements standard cubic algorithm
 * https://wiki.analog.com/resources/tools-software/sigmastudio/toolbox/nonlinearprocessors/standardcubic
 */
fixed AudioMixer::softClip(fixed sample) {
    if (softclip_ == -1 || sample == 0)
        return sample;

    float x;
    float sampleFloat = fp2fl(sample);
	float maxFloat = fp2fl(sampleFloat > 0 ? MAX_POSITIVE_FIXED : MAX_NEGATIVE_FIXED);
	SoftClipData* data = &softClipData_[softclip_];

    x = data->alphaInv * (sampleFloat / maxFloat);
    if (x > -1.0f && x < 1.0f) {
        // H38.8 OPT_PERF: x*x*x replaces pow(x,3.0f) -- same cubic, no
        // transcendental call on the audio thread (per sample).
        sampleFloat = maxFloat * (data->alpha * (x - (x * x * x / 3.0f)));
    } else {
        sampleFloat = maxFloat * data->alpha23;
    }

    if (softclipGain_) {
        sampleFloat = sampleFloat * data->gainCmp;
    }

    return fl2fp(sampleFloat);
}
