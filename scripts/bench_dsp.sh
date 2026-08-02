#!/usr/bin/env bash
# Phase 0 DSP benchmark: compiles the project's fixed-point primitives as a
# standalone host benchmark and measures per-operation cost. Run from repo
# root:  bash scripts/bench_dsp.sh
# Real-device numbers must be captured on the R36SX v2.6 (see
# PLAN_FX_REDESIGN_ES.md section D). This host build only establishes a
# relative baseline for the Q15 kernel before the FxEngine lands.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="${TMPDIR:-/tmp}/lgpt_bench_dsp"
mkdir -p "$TMP"

cat > "$TMP/bench_dsp.cpp" <<'CPP'
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "Application/Utils/fixed.h"

#define FIXED_SHIFT 15
#define ITER 4000000

static double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

volatile fixed sink;

int main() {
    fixed a = i2fp(3000);
    fixed b = fl2fp(0.35f);
    fixed acc = 0;

    // fp_mul (Q15 multiply)
    double t0 = now_ms();
    for (int i = 0; i < ITER; i++) acc = fp_add(acc, fp_mul(a, (fixed)(b + i)));
    double mul_ms = now_ms() - t0;

    // fp_add / fp_sub
    t0 = now_ms();
    for (int i = 0; i < ITER; i++) { acc = fp_add(acc, b); acc = fp_sub(acc, (fixed)i); }
    double add_ms = now_ms() - t0;

    // fp_div (fixed.h approximation: (((x<<2)/((y>>8)))<<10))
    t0 = now_ms();
    for (int i = 0; i < ITER; i++) acc = fp_add(acc, fp_div(a, i2fp((i & 3) + 1)));
    double div_ms = now_ms() - t0;

    // i2fp/fp2i round-trip
    t0 = now_ms();
    for (int i = 0; i < ITER; i++) acc = fp_add(acc, i2fp(fp2i(a + i)));
    double cvt_ms = now_ms() - t0;
    sink = acc;
    printf("ITER            : %d\n", ITER);
    printf("fp_mul/op       : %.3f ns\n", mul_ms * 1e6 / ITER);
    printf("fp_add+sub pair : %.3f ns\n", add_ms * 1e6 / ITER);
    printf("fp_div/op       : %.3f ns\n", div_ms * 1e6 / ITER);
    printf("i2fp+fp2i pair  : %.3f ns\n", cvt_ms * 1e6 / ITER);
    return 0;
}
CPP

g++ -O2 -std=gnu++03 -I"$ROOT/source/sources" "$TMP/bench_dsp.cpp" -o "$TMP/bench_dsp" -lrt
"$TMP/bench_dsp"
echo BENCH_DSP_HOST_OK
