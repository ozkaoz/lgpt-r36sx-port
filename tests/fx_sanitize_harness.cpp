#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "Application/Utils/fixed.h"
#include "Application/Audio/FxEngine/DelayLine.h"
#include "Application/Audio/FxEngine/Reverb.h"
#include "Application/Audio/FxEngine/ParametricEQ.h"
#include "Application/Audio/FxEngine/Compressor.h"

using namespace FxEngine;

#define FRAMES 64
#define ITER 2000

static void run_delay() {
    DelayLine d;
    d.SetSampleRate(44100);
    d.Reset();
    fixed in[FRAMES * 2], out[FRAMES * 2];
    for (int it = 0; it < ITER; it++) {
        for (int i = 0; i < FRAMES * 2; i++)
            in[i] = fl2fp(((float)(rand() % 2000 - 1000)) / 1000.0f);
        d.SetDelayMs(fl2fp((float)(rand() % 2000)));
        d.SetFeedback(fl2fp((float)(rand() % 100) / 100.0f));
        d.SetWidth(fl2fp((float)(rand() % 100) / 100.0f));
        d.SetPingPong((rand() & 1) != 0);
        d.SetSaturation((rand() & 1) != 0);
        d.SetBypass((rand() & 1) != 0);
        d.SetMix(fl2fp((float)(rand() % 100) / 100.0f));
        d.SetLoopLPHz(fl2fp((float)(rand() % 20000)));
        d.SetLoopHPHz(fl2fp((float)(rand() % 20000)));
        d.Process(in, out, FRAMES);
    }
    printf("delay rtViolations=%lu\n", d.GetRtViolations());
}

static void run_reverb() {
    Reverb r;
    r.SetSampleRate(44100);
    r.Reset();
    fixed in[FRAMES * 2], out[FRAMES * 2];
    for (int it = 0; it < ITER; it++) {
        for (int i = 0; i < FRAMES * 2; i++)
            in[i] = fl2fp(((float)(rand() % 2000 - 1000)) / 1000.0f);
        r.SetPredelayMs(fl2fp((float)(rand() % 300)));
        r.SetDecay(fl2fp((float)(rand() % 800) / 100.0f));
        r.SetSize(fl2fp((float)(rand() % 100) / 100.0f));
        r.SetDamping(fl2fp((float)(rand() % 100) / 100.0f));
        r.SetWidth(fl2fp((float)(rand() % 100) / 100.0f));
        r.SetMode((Reverb::Mode)(rand() % 2));
        r.SetBypass((rand() & 1) != 0);
        r.SetMix(fl2fp((float)(rand() % 100) / 100.0f));
        r.Process(in, out, FRAMES);
    }
    printf("reverb rtViolations=%lu\n", r.GetRtViolations());
}

static void run_eq() {
    ParametricEQ eq;
    eq.SetSampleRate(44100);
    eq.Reset();
    fixed in[FRAMES * 2], out[FRAMES * 2];
    for (int it = 0; it < ITER; it++) {
        for (int i = 0; i < FRAMES * 2; i++)
            in[i] = fl2fp(((float)(rand() % 2000 - 1000)) / 1000.0f);
        for (int b = 0; b < ParametricEQ::kNumBands; b++) {
            eq.SetBandFreq((ParametricEQ::Band)b, fl2fp((float)(rand() % 20000 + 20)));
            eq.SetBandGainDb((ParametricEQ::Band)b, fl2fp((float)(rand() % 2400 - 1200) / 100.0f));
            eq.SetBandQ((ParametricEQ::Band)b, fl2fp((float)(rand() % 990 + 10) / 100.0f));
            eq.SetBandEnabled((ParametricEQ::Band)b, (rand() & 1) != 0);
        }
        eq.SetBypass((rand() & 1) != 0);
        eq.Process(in, out, FRAMES);
    }
    printf("eq rtViolations=%lu\n", eq.GetRtViolations());
}

static void run_comp() {
    Compressor c;
    c.SetSampleRate(44100);
    c.Reset();
    fixed in[FRAMES * 2], out[FRAMES * 2];
    for (int it = 0; it < ITER; it++) {
        for (int i = 0; i < FRAMES * 2; i++)
            in[i] = fl2fp(((float)(rand() % 2000 - 1000)) / 1000.0f);
        c.SetThresholdDb(fl2fp((float)(rand() % 6000 - 6000) / 100.0f));
        c.SetRatio(fl2fp((float)(rand() % 2000 + 100) / 100.0f));
        c.SetKneeDb(fl2fp((float)(rand() % 2000) / 100.0f));
        c.SetMakeupDb(fl2fp((float)(rand() % 2400) / 100.0f));
        c.SetAttackMs(fl2fp((float)(rand() % 3000) / 100.0f));
        c.SetReleaseMs(fl2fp((float)(rand() % 80000) / 100.0f));
        c.SetStereoLink((rand() & 1) != 0);
        c.SetBypass((rand() & 1) != 0);
        c.SetSoftClip((rand() & 1) != 0);
        c.Process(in, out, FRAMES);
    }
    printf("comp rtViolations=%lu\n", c.GetRtViolations());
}

int main() {
    srand(12345);
    run_delay();
    run_reverb();
    run_eq();
    run_comp();
    printf("HARDEN_SANITIZERS_OK\n");
    return 0;
}
