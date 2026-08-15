// FXP_FILTER_V2 (bacon-1.5, item 2): host test for the TPT state-variable
// filter.  Verifies spectral behaviour of each topology (LP/HP/BP/NOTCH),
// passthrough at mix=0, and long-run stability (no NaN/Inf, bounded states)
// under a hot signal.
#include "Application/Instruments/FilterV2.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int failures = 0 ;
static int checks = 0 ;

static void check(bool cond, const char *what) {
    checks++ ;
    if (!cond) {
        failures++ ;
        printf("FAIL: %s\n", what) ;
    }
}

// p1 for a target cutoff fc under the legacy mapping fc = p1^2 * 22050.
static float p1ForFc(float fc) {
    return sqrtf(fc / 22050.0f) ;
}

// p2 for a target Q under the legacy curve res = 1-(1-p2)^3, Q = 1+3*res.
static float p2ForQ(float q) {
    float res = (q - 1.0f) / 3.0f ;
    if (res < 0.0f) res = 0.0f ;
    if (res > 1.0f) res = 1.0f ;
    return 1.0f - powf(1.0f - res, 1.0f / 3.0f) ;
}

// Steady-state RMS of the filter output for a sine at f Hz, amplitude a.
static double sineRms(FilterV2Type type, float fc, float q, float f, float a,
                      int mix) {
    set_filter_v2(0, type, fl2fp(p1ForFc(fc)), fl2fp(p2ForQ(q)), mix,
                  false, false, 48000) ;
    const int warmup = 48000 ;
    const int measure = 8000 ;
    double acc = 0.0 ;
    long long n = 0 ;
    for (int i = 0; i < warmup + measure; i++) {
        float x = a * sinf(2.0f * 3.14159265f * f * i / 48000.0f) ;
        fixed y = filterv2_process(get_filter_v2(0), 0, fl2fp(x)) ;
        if (i >= warmup) {
            double v = fp2fl(y) ;
            acc += v * v ;
            n++ ;
        }
    }
    return (n > 0) ? sqrt(acc / (double)n) : 0.0 ;
}

int main() {
    init_filters_v2() ;

    // ---- Spectral behaviour at fc = 2000 Hz, Q = 1.6 ----
    // LP passes 200 Hz, cuts 8 kHz.
    double lpLow  = sineRms(FV2_LOWPASS, 2000.0f, 1.6f, 200.0f, 0.5f, 255) ;
    double lpHigh = sineRms(FV2_LOWPASS, 2000.0f, 1.6f, 8000.0f, 0.5f, 255) ;
    check(lpLow > 0.3, "LP passes below cutoff") ;
    check(lpHigh < lpLow * 0.15, "LP cuts far above cutoff") ;

    // HP cuts 200 Hz, passes 8 kHz.
    double hpLow  = sineRms(FV2_HIGHPASS, 2000.0f, 1.6f, 200.0f, 0.5f, 255) ;
    double hpHigh = sineRms(FV2_HIGHPASS, 2000.0f, 1.6f, 8000.0f, 0.5f, 255) ;
    check(hpLow < 0.15, "HP cuts below cutoff") ;
    check(hpHigh > 0.3, "HP passes above cutoff") ;

    // BP passes at the center, cuts well below it.
    double bpC  = sineRms(FV2_BANDPASS, 2000.0f, 1.6f, 2000.0f, 0.5f, 255) ;
    double bpLo = sineRms(FV2_BANDPASS, 2000.0f, 1.6f, 200.0f, 0.5f, 255) ;
    check(bpC > 0.3, "BP passes at center") ;
    check(bpLo < bpC * 0.2, "BP cuts far below center") ;

    // NOTCH cuts at the center, passes below it.
    double ntC  = sineRms(FV2_NOTCH, 2000.0f, 1.6f, 2000.0f, 0.5f, 255) ;
    double ntLo = sineRms(FV2_NOTCH, 2000.0f, 1.6f, 200.0f, 0.5f, 255) ;
    check(ntC < 0.2, "NOTCH cuts at center") ;
    check(ntLo > 0.3, "NOTCH passes below center") ;

    // ---- mix = 0 passthrough (bit-exact) ----
    set_filter_v2(0, FV2_LOWPASS, fl2fp(p1ForFc(2000.0f)), fl2fp(p2ForQ(1.6f)),
                  0, false, false, 48000) ;
    {
        bool exact = true ;
        for (int i = 0; i < 1000; i++) {
            fixed x = fl2fp(0.25f * sinf(2.0f * 3.14159265f * 400.0f * i / 48000.0f)) ;
            if (filterv2_process(get_filter_v2(0), 0, x) != x) exact = false ;
        }
        check(exact, "mix=0 passes input bit-exact") ;
    }

    // ---- Stability: hot square wave for 4 s through every type at high
    // resonance and full scream dirt.  Outputs must stay finite and bounded.
    {
        bool stable = true ;
        FilterV2Type types[4] = { FV2_LOWPASS, FV2_HIGHPASS, FV2_BANDPASS,
                                  FV2_NOTCH } ;
        for (int t = 0; t < 4; t++) {
            set_filter_v2(0, types[t], fl2fp(p1ForFc(500.0f)), fl2fp(p2ForQ(4.0f)),
                          255, true, true, 48000) ;
            for (int i = 0; i < 192000; i++) {
                float x = (i % 2400 < 1200) ? 0.99f : -0.99f ;
                fixed y = filterv2_process(get_filter_v2(0), 0, fl2fp(x)) ;
                float v = fp2fl(y) ;
                if (!(v == v) || isinf(v)) stable = false ;  // NaN/Inf
                // Q15 state is inherently bounded; a high-Q SVF under a
                // full-scale driven square overshoots transiently (resonance),
                // so the bound is a generous hard cap proving no blow-up.
                if (fabsf(v) > 16.0f) stable = false ;
            }
        }
        check(stable, "all topologies stay finite and bounded under stress") ;
    }

    if (failures == 0) {
        printf("ALL OK (%d checks)\n", checks) ;
        return 0 ;
    }
    printf("%d/%d checks FAILED\n", failures, checks) ;
    return 1 ;
}