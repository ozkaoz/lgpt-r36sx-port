#include "AudioMixer.h"
#include "System/System/System.h"
#include <math.h>

#define MAX_POSITIVE_FIXED i2fp(32767)
#define MAX_NEGATIVE_FIXED i2fp(-32768)

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

    fixed *mixBuffer = 0;
    bool gotData = false;
    IteratorPtr<AudioModule> it(GetIterator());
    for (it->Begin(); !it->IsDone(); it->Next()) {
        AudioModule &current = it->CurrentItem();
        if (!gotData) {
            gotData=current.Render(buffer,samplecount) ;           
         } else {
            if (!mixBuffer) {
               mixBuffer=(fixed *)malloc(samplecount*2*sizeof(fixed)) ;
            } 
            if (current.Render(mixBuffer,samplecount)) {
               fixed *dst=buffer ;
               fixed *src=mixBuffer ;
               int count=samplecount*2 ;
               while (count--) {
                 *dst+=*src ;
                 dst++ ;
                 src++ ;
               }
            }
         }
     }

     //  Apply volume

     if (gotData) {
         fixed *c = buffer;
         // H38.7 OPT_PERF: pow() was recomputed on every buffer. Cache it and
         // only recompute when the master volume actually changes.
         if (masterVolume_ != masterVolumeCached_) {
             dampCached_ = pow((float)masterVolume_ / 100, 4.0f);
             masterVolumeCached_ = masterVolume_;
         }
         float damp = dampCached_;

         if (volume_ != i2fp(1)) {
             for (int i = 0; i < samplecount * 2; i++) {
                 fixed v = fp_mul(*c, volume_);
                 *c++ = v;
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
         if (damp != 1.0f) {
             for (int i = 0; i < samplecount * 2; i++) {
                 fixed v = fl2fp(damp * fp2fl(*c));
                 *c++ = v;
             }
         }

         // TREEFROG_VU_METERS_V1 + TREEFROG_MIXER_STEREO_METERS_V1:
         // Track the smoothed peak of the output (audible level).  Fast
         // attack, slow decay, so the mixer bars bounce with the music.
         // The scan runs in short sub-blocks with a decay between them, so
         // the meters dip between closely-spaced notes (hi-hats) instead of
         // staying pinned at the held peak of the whole buffer.  Measured on
         // the post-damp, PRE-CLIP buffer and split per side (even samples =
         // L, odd = R in the interleaved buffer), so each bar reflects the
         // true level of its side -- including mix sums above 0 dB.
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
                      // off is always even (block is even), so c[0] is always L
                      // and c[1] always R: take the pair per 8 samples (same
                      // density as the old stride 4) and measure both sides.
                      fixed *c = buffer + off ;
                      for (int i = 0; i < n; i += 8) {
                          float vL = fp2fl(c[0]) * (1.0f / 32767.0f) ;
                          if (vL < 0.0f) vL = -vL ;
                          if (vL > peakL) peakL = vL ;
                          float vR = fp2fl(c[1]) * (1.0f / 32767.0f) ;
                          if (vR < 0.0f) vR = -vR ;
                          if (vR > peakR) peakR = vR ;
                          c += 8 ;
                      }
                  }
                 if (peakL > peakValueL_) {
                     peakValueL_ = peakL;
                 } else {
                     // Fast release so the master bar empties in a few ms of
                     // quiet, giving a live-meter feel even for dense patterns.
                     peakValueL_ *= 0.5f;
                     if (peakValueL_ < 0.002f) peakValueL_ = 0.0f;
                 }
                 if (peakR > peakValueR_) {
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
         if (!clipBypass_) {
            c = buffer;
            if (softclip_ == -1) {
                for (int i = 0; i < samplecount * 2; i++) {
                    fixed sample = *c;
                    *c++ = hardClip(sample);
                }
            } else {
                for (int i = 0; i < samplecount * 2; i++) {
                    fixed sample = *c;
                    *c++ = hardClip(softClip(sample));
                }
            }
         }
     }
    if (enableRendering_&&writer_) {
		if (!gotData) {
			memset(buffer,0,samplecount*2*sizeof(fixed)) ;
		} ;
		writer_->AddBuffer(buffer,samplecount) ;
	}

     SAFE_FREE(mixBuffer) ;
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
        sampleFloat = maxFloat * (data->alpha * (x - (pow(x, 3.0f) / 3.0f)));
    } else {
        sampleFloat = maxFloat * data->alpha23;
    }

    if (softclipGain_) {
        sampleFloat = sampleFloat * data->gainCmp;
    }

    return fl2fp(sampleFloat);
}
