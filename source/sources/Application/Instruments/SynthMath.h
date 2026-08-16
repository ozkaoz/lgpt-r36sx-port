#ifndef SYNTHMATH_H
#define SYNTHMATH_H

// TREEFROG_FAST_MATH_V1 (bacon-1.5 fix): the R36S audio thread must not call
// per-sample transcendental libm functions (sinf/cosf/powf/expf).  On the
// weak MIPS core they are slow (dropouts, unstable audio) and the device's
// libm has been observed faulting (SIGSEGV) inside these hot loops
// (crash.txt sig=11 while rendering synth voices).  PianoSynth already
// rendered its partials from a 256-entry sine table; BassSynth now uses the
// same pattern through this shared header.  A 1024-entry sine table with
// linear interpolation keeps the worst-case error below ~1e-6 of full scale
// (inaudible); a 256-entry 2^x table covers the pitch-LFO range.
//
// C++03 header-only: static storage (one copy per TU, lazily built) is
// deliberate to keep both synths independent of any runtime init order.

#include <math.h>

static float gSynthSinTable[1024];
static float gSynthPow2Table[256];
static bool gSynthMathReady = false;

static void ensureSynthMath() {
    if (gSynthMathReady) return;
    for (int i = 0; i < 1024; i++) {
        gSynthSinTable[i] = sinf(2.0f * 3.14159265f * (float)i / 1024.0f);
    }
    for (int i = 0; i < 256; i++) {
        gSynthPow2Table[i] = powf(2.0f, -1.0f + (float)i / 128.0f);
    }
    gSynthMathReady = true;
}

// sin(2*pi*p) for p in [0,1), linearly interpolated.
static inline float synthSinP1(float p) {
    float x = p * 1024.0f;
    int i0 = (int)x;
    float frac = x - (float)i0;
    int i1 = (i0 + 1) & 1023;
    i0 &= 1023;
    return gSynthSinTable[i0] +
           (gSynthSinTable[i1] - gSynthSinTable[i0]) * frac;
}

// sin(phase) for phase in radians (any magnitude, wrapped to [0,2*pi)).
static inline float synthSinRad(float phase) {
    while (phase >= 6.28318531f) phase -= 6.28318531f;
    while (phase < 0.0f) phase += 6.28318531f;
    return synthSinP1(phase * 0.15915494f); // / (2*pi)
}

// 2^x for x in [-1,1] (clamped outside), linearly interpolated.
static inline float synthPow2Bounded(float x) {
    float t = (x + 1.0f) * 128.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 255.0f) t = 255.0f;
    int i0 = (int)t;
    float frac = t - (float)i0;
    int i1 = (i0 + 1) & 255;
    return gSynthPow2Table[i0] +
           (gSynthPow2Table[i1] - gSynthPow2Table[i0]) * frac;
}

#endif // SYNTHMATH_H
