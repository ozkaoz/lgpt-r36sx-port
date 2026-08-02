#!/usr/bin/env bash
# FxEngine bench: host measurement of the send/return DSP cost.
# - Process() call overhead (ns/call) in legacy bypass mode
# - Process() call overhead with delay + reverb active (Fase 2)
# - Static memory footprint of the FxEngine buses + DelayLine + Reverb
# - rtViolations_ / counters after many callbacks
# Run from repo root:  bash scripts/bench_fxengine.sh
# Real-device numbers must be captured on the R36SX v2.6 (plan section D).
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="${TMPDIR:-/tmp}/lgpt_bench_fxengine"
mkdir -p "$TMP"

cat > "$TMP/bench_fxengine.cpp" <<CPP
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "Application/Utils/fixed.h"
#include "Application/Audio/FxEngine/FxEngine.h"

#define ITER 200000
#define FRAMES 918   // ~44.1 kHz, 120 BPM slice (SyncMaster default)

static double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

volatile fixed sink;

static double bench(FxEngine::FxEngine &fx, fixed *master) {
    double t0 = now_ms();
    for (int i = 0; i < ITER; i++) fx.Process(master, FRAMES);
    double ms = now_ms() - t0;
    sink = master[0];
    return ms * 1e6 / ITER;
}

int main() {
    static fixed master[FRAMES * 2];
    for (int i = 0; i < FRAMES * 2; i++) master[i] = i2fp((i % 20000) - 10000);

    FxEngine::FxEngine &fx = FxEngine::FxEngine::GetInstance();
    fx.Reset();
    fx.SetSampleRate(44100);

    // warm up (legacy)
    for (int i = 0; i < 1000; i++) fx.Process(master, FRAMES);

    double bypass_ns = bench(fx, master);

    // Fase 2: turn off legacy, wire delay + reverb sends/returns.
    fx.SetLegacyMode(false);
    fx.SetDelaySend(fl2fp(0.25f));
    fx.SetDelayReturn(fl2fp(0.5f));
    fx.SetDelayTimeMs(fl2fp(120.0f));   // ~120 BPM quarter
    fx.SetDelayFeedback(fl2fp(0.35f));
    fx.SetDelayWidth(fl2fp(0.8f));
    fx.SetReverbSend(fl2fp(0.15f));
    fx.SetReverbReturn(fl2fp(0.5f));
    fx.SetReverbDecay(fl2fp(2.0f));
    fx.SetReverbSize(fl2fp(1.0f));
    fx.SetReverbDamping(fl2fp(0.5f));
    fx.SetReverbPredelayMs(fl2fp(20.0f));

    for (int i = 0; i < 1000; i++) fx.Process(master, FRAMES); // warm up + settle
    double dsp_ns = bench(fx, master);

    // Fase 3: enable master EQ + compressor/limiter.
    fx.SetEqBypass(false);
    fx.SetEqBandEnabled(0, true);
    fx.SetEqBandFreq(0, fl2fp(80.0f));
    fx.SetEqBandGainDb(0, fl2fp(3.0f));
    fx.SetEqBandEnabled(1, true);
    fx.SetEqBandFreq(1, fl2fp(1200.0f));
    fx.SetEqBandGainDb(1, fl2fp(-2.0f));
    fx.SetEqBandQ(1, fl2fp(1.5f));
    fx.SetEqBandEnabled(2, true);
    fx.SetEqBandFreq(2, fl2fp(9000.0f));
    fx.SetEqBandGainDb(2, fl2fp(2.0f));
    fx.SetCompBypass(false);
    fx.SetCompThresholdDb(fl2fp(-18.0f));
    fx.SetCompRatio(fl2fp(3.0f));
    fx.SetCompKneeDb(fl2fp(6.0f));
    fx.SetCompAttackMs(fl2fp(10.0f));
    fx.SetCompReleaseMs(fl2fp(150.0f));
    fx.SetCompSoftClip(true);

    for (int i = 0; i < 1000; i++) fx.Process(master, FRAMES); // settle
    double full_ns = bench(fx, master);

    printf("ITER                  : %d\n", ITER);
    printf("Process bypass/call   : %.3f ns\n", bypass_ns);
    printf("Process delay+rev/call: %.3f ns\n", dsp_ns);
    printf("Process full/call     : %.3f ns\n", full_ns);
    printf("StaticMemoryBytes     : %lu\n",
           (unsigned long)FxEngine::FxEngine::StaticMemoryBytes());
    printf("CallCount             : %lu\n", fx.GetCallCount());
    printf("FramesProcessed       : %lu\n", fx.GetFramesProcessed());
    printf("MaxFrames             : %lu\n", fx.GetMaxFrames());
    printf("RtViolations          : %lu\n", fx.GetRtViolations());
    printf("CompGRdB              : %.3f\n", fp2fl(fx.GetCompGainReductionDb()));
    if (fx.GetRtViolations() != 0) return 1;
    return 0;
}
CPP

g++ -O2 -std=gnu++03 -I"$ROOT/source/sources" \
  "$TMP/bench_fxengine.cpp" \
  "$ROOT/source/sources/Application/Audio/FxEngine/FxEngine.cpp" \
  "$ROOT/source/sources/Application/Audio/FxEngine/DelayLine.cpp" \
  "$ROOT/source/sources/Application/Audio/FxEngine/Reverb.cpp" \
  "$ROOT/source/sources/Application/Audio/FxEngine/ParametricEQ.cpp" \
  "$ROOT/source/sources/Application/Audio/FxEngine/Compressor.cpp" \
  -lm \
  -o "$TMP/bench_fxengine"
"$TMP/bench_fxengine"
echo BENCH_FXENGINE_OK
