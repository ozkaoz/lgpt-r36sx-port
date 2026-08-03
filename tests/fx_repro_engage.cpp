#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "Application/Utils/fixed.h"
#include "Application/Audio/FxEngine/FxEngine.h"
#include "Application/Audio/FxEngine/DelayLine.h"

using namespace FxEngine;

#define FRAMES 1000
#define ITER 40

// Simulates the user scenario:
// 1) legacy bypass -> dry passes (baseline)
// 2) user raises a channel delay send (DLYS) on channel 0
// 3) user sets delay time (DLYT) and feedback (DLYF)
// 4) player channel accumulates its audio into the send bus
// 5) FxEngine::Process runs on the mixed master
// Checks output RMS stays comparable to input (no total silence).
//
// Buffers are generated at the REAL master scale (int16 << 15, as AudioMixer
// produces) so this reproduces the scale-mismatch bug that destroyed the
// audio: the DSP must normalize >>FIXED_SHIFT to Q15, process, then expand
// <<FIXED_SHIFT back with a hard clip.  A Q15-only harness would miss it.

static float rms(const fixed *b, int n) {
    double acc = 0;
    for (int i = 0; i < n; i++) {
        float v = fp2fl(b[i]);
        acc += (double)v * v;
    }
    return (float)sqrt(acc / n);
}

int main() {
    srand(42);
    FxEngine::FxEngine &fx = FxEngine::FxEngine::GetInstance();
    fx.Reset();
    fx.SetSampleRate(48000);

    static fixed in[FRAMES * 2], master[FRAMES * 2];

    // Baseline: legacy bypass (int16<<15 scale, 0.5 amplitude).
    for (int i = 0; i < FRAMES * 2; i++)
        in[i] = i2fp((int)(0.5f * 32767.0f * sinf(i * 0.01f)));
    float inRms = rms(in, FRAMES * 2);

    fx.Process(in, FRAMES);
    printf("legacy  in_rms=%.4f out_rms=%.4f legacy=%d\n", inRms,
           rms(in, FRAMES * 2), fx.IsLegacyMode());

    // Engage: DLYS on channel 0 (send 40%), DLYT (time ~200ms), DLYF (0.4).
    fx.SetDelaySend(fl2fp(0.40f));
    fx.SetDelayTimeMs(fl2fp(200.0f));
    fx.SetDelayFeedback(fl2fp(0.4f));
    fx.SetDelayReturn(fl2fp(0.5f));
    printf("after edit: legacy=%d\n", fx.IsLegacyMode());

    // Play 40 frames: accumulate the channel send (as PlayerChannel does)
    // then Process the master.
    for (int it = 0; it < ITER; it++) {
        for (int i = 0; i < FRAMES * 2; i++)
            master[i] = i2fp((int)(0.5f * 32767.0f * sinf((i + it * FRAMES) * 0.01f)));
        // PlayerChannel: delayGain = 40/100
        fx.AccumulateChannelSend(0, master, FRAMES, fl2fp(0.4f), fl2fp(0.0f));
        fx.Process(master, FRAMES);
    }
    float outRms = rms(master, FRAMES * 2);
    printf("engage  in_rms=%.4f out_rms=%.4f legacy=%d\n", inRms, outRms,
           fx.IsLegacyMode());
    if (outRms < inRms * 0.01f) {
        printf("FAIL: output near silence\n");
        return 1;
    }
    printf("REPRO_ENGAGE_OK\n");
    return 0;
}
