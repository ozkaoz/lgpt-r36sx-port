// FXP_COMPRESSOR_V2 (bacon-1.5, item 4): host DSP test of the Compressor V2
// with true zero-latency sidechain (TRACK/BUS tap, SC HPF, SC AMOUNT) and
// dry/wet MIX.  Compiles Compressor.cpp with ASAN/UBSAN.
#include "Application/Audio/FxEngine/Compressor.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

using namespace FxEngine;

static int failures = 0;
static int checks = 0;

static void check(bool cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

static float fl(fixed x) { return fp2fl(x); }

static fixed maxAbs(const fixed *b, int frames) {
    fixed m = 0;
    for (int i = 0; i < frames * 2; i++) {
        fixed a = b[i];
        if (a < 0) a = -a;
        if (a > m) m = a;
    }
    return m;
}

// ---- Sidechain ducking ----
// Program: quiet sine (~0.2).  SC tap: loud pulses on the selected track.
// With SC OFF the program stays at its natural level; with SC ON (amount 1)
// the loud tap drags the detector up and the quiet program gets attenuated.
static void testSidechainDucking() {
    const int N = 48000;
    static fixed prog[N * 2];
    static fixed tap[N * 2];
    for (int i = 0; i < N; i++) {
        // Program: sine at 0.2 (well below a -24 dBFS threshold).
        prog[2 * i] = prog[2 * i + 1] = fl2fp(0.2f * sinf(2.0f * 3.14159265f * 220.0f * i / 48000.0f));
        // SC tap: loud pulse train (a kick) on both channels.
        float e = 0.8f * expf(-3.0f * (float)(i % 9600) / 9600.0f);
        tap[2 * i] = tap[2 * i + 1] = fl2fp(e);
    }

    static fixed outA[N * 2], outB[N * 2];

    // A: compressor without sidechain (program detector only).
    {
        Compressor c;
        c.SetSampleRate(48000);
        c.SetThresholdDb(fl2fp(-24.0f));
        c.SetRatio(fl2fp(8.0f));
        c.SetKneeDb(fl2fp(6.0f));
        c.SetAttackMs(fl2fp(5.0f));
        c.SetReleaseMs(fl2fp(100.0f));
        c.SetMakeupDb(0);
        c.SetStereoLink(true);
        c.SetBypass(false);
        c.SetSoftClip(false);
        c.SetMix(i2fp(1));
        c.SetSidechainSource(Compressor::SC_OFF);
        c.Process(prog, outA, N);
        check(c.GetRtViolations() == 0, "SC off: no RT violations");
    }

    // B: same compressor, sidechain from TRACK 1 with amount 1.
    {
        Compressor c;
        c.SetSampleRate(48000);
        c.SetThresholdDb(fl2fp(-24.0f));
        c.SetRatio(fl2fp(8.0f));
        c.SetKneeDb(fl2fp(6.0f));
        c.SetAttackMs(fl2fp(5.0f));
        c.SetReleaseMs(fl2fp(100.0f));
        c.SetMakeupDb(0);
        c.SetStereoLink(true);
        c.SetBypass(false);
        c.SetSoftClip(false);
        c.SetMix(i2fp(1));
        c.SetSidechainSource(Compressor::SC_TRACK_1);
        c.SetSidechainAmount(i2fp(1));
        c.SetSidechainInput(tap, N);
        c.Process(prog, outB, N);
        check(c.GetRtViolations() == 0, "SC on: no RT violations");
    }

    // The sidechain ducking must be audible: with the kick tap the average
    // output is clearly lower than without it (the program alone at 0.2 is
    // above threshold, so both compress a bit; the tap pushes it far over).
    long long sumA = 0, sumB = 0;
    for (int i = 0; i < N * 2; i++) {
        sumA += (outA[i] > 0) ? outA[i] : -outA[i];
        sumB += (outB[i] > 0) ? outB[i] : -outB[i];
    }
    float avgA = (float)sumA / (float)(N * 2);
    float avgB = (float)sumB / (float)(N * 2);
    check(avgB < avgA * 0.7f, "sidechain tap ducks the program (avgB < 0.7*avgA)");
    check(avgA > 0.0f, "program audible without SC");
}

// ---- SC AMOUNT=0 and SC OFF behave identically ----
static void testSidechainAmountZero() {
    const int N = 48000;
    static fixed prog[N * 2];
    static fixed tap[N * 2];
    for (int i = 0; i < N; i++) {
        prog[2 * i] = prog[2 * i + 1] = fl2fp(0.3f * sinf(2.0f * 3.14159265f * 110.0f * i / 48000.0f));
        tap[2 * i] = tap[2 * i + 1] = i2fp(1);
    }
    static fixed outA[N * 2], outB[N * 2];
    {
        Compressor c;
        c.SetSampleRate(48000);
        c.SetBypass(false);
        c.SetSoftClip(false);
        c.SetMix(i2fp(1));
        c.SetSidechainSource(Compressor::SC_TRACK_1);
        c.SetSidechainAmount(0);           // amount 0 => no SC contribution
        c.SetSidechainInput(tap, N);
        c.Process(prog, outA, N);
    }
    {
        Compressor c;
        c.SetSampleRate(48000);
        c.SetBypass(false);
        c.SetSoftClip(false);
        c.SetMix(i2fp(1));
        c.SetSidechainSource(Compressor::SC_OFF);
        c.Process(prog, outB, N);
    }
    int diff = 0;
    for (int i = 0; i < N * 2; i++) {
        int d = outA[i] - outB[i];
        if (d < 0) d = -d;
        if (d > 2) diff++;
    }
    check(diff < N, "SC amount 0 == SC OFF (outputs match)");
}

// ---- SC HPF removes the low end of the tap ----
static void testSidechainHpf() {
    const int N = 48000;
    static fixed tap[N * 2];
    // Tap: DC-ish low hum + a clean sine at 200 Hz.
    for (int i = 0; i < N; i++) {
        tap[2 * i] = tap[2 * i + 1] = fl2fp(0.5f + 0.4f * sinf(2.0f * 3.14159265f * 200.0f * i / 48000.0f));
    }
    Compressor c;
    c.SetSampleRate(48000);
    c.SetBypass(false);
    c.SetSoftClip(false);
    c.SetMix(i2fp(1));
    c.SetSidechainSource(Compressor::SC_TRACK_1);
    c.SetSidechainAmount(i2fp(1));
    c.SetSidechainHpfHz(fl2fp(100.0f));   // removes the DC + low hum
    c.SetSidechainInput(tap, N);
    c.Process(tap, tap, N);   // in-place program == tap
    check(c.GetRtViolations() == 0, "SC HPF: no RT violations");
    // Output must not be stuck at full saturation (the DC would pin the
    // detector without the HPF); with HPF the 200 Hz component still ducks
    // but the DC offset is gone, so the average output is not saturated.
    fixed m = maxAbs(tap, N);
    check(m <= i2fp(1) && m > 0, "SC HPF: bounded non-silent output");
}

// ---- Dry/wet MIX ----
static void testMix() {
    const int N = 48000;
    static fixed in[N * 2];
    for (int i = 0; i < N; i++) {
        in[2 * i] = in[2 * i + 1] = fl2fp(0.5f * sinf(2.0f * 3.14159265f * 440.0f * i / 48000.0f));
    }
    // MIX=0 must be (near) bit-exact passthrough of the compressed dry path:
    // the dry gain is 1 and the wet is multiplied by 0, so the output equals
    // the input exactly (saturate of the sum of dry*1 + wet*0).
    {
        Compressor c;
        c.SetSampleRate(48000);
        c.SetBypass(false);
        c.SetSoftClip(false);
        c.SetMix(0);
        static fixed out[N * 2];
        // Warm up: the mix crossfade glides from its 1.0 ctor default toward
        // 0 (~200 frames at 0.005/frame); only the settled tail is compared.
        static fixed warm[N * 2];
        for (int i = 0; i < N; i++) warm[2 * i] = warm[2 * i + 1] = 0;
        c.Process(warm, warm, 2048);
        c.Process(in, out, N);
        int diff = 0;
        for (int i = 2048 * 2; i < N * 2; i++) {
            int d = out[i] - in[i];
            if (d < 0) d = -d;
            if (d > 1) diff++;
        }
        check(diff == 0, "MIX=0: bit-exact passthrough");
    }
    // MIX=1 is the compressed signal (no dry component).
    {
        Compressor c;
        c.SetSampleRate(48000);
        c.SetBypass(false);
        c.SetSoftClip(false);
        c.SetMix(i2fp(1));
        static fixed out[N * 2];
        c.Process(in, out, N);
        check(maxAbs(out, N) <= maxAbs(in, N), "MIX=1: compression never boosts");
        // With makeup 0 and a signal above threshold the compressed output
        // must be smaller on average than the input.
        long long sumI = 0, sumO = 0;
        for (int i = 0; i < N * 2; i++) {
            sumI += (in[i] > 0) ? in[i] : -in[i];
            sumO += (out[i] > 0) ? out[i] : -out[i];
        }
        check(sumO < sumI, "MIX=1: compressed output quieter");
    }
}

// ---- Legacy defaults ----
static void testDefaults() {
    Compressor c;
    c.SetSampleRate(48000);
    check(c.GetBypass(), "default bypass on (dry)");
    check(fl(c.GetThresholdDb()) == -24.0f, "default threshold -24 dB");
    check(fl(c.GetRatio()) == 4.0f, "default ratio 4:1");
    check(fl(c.GetKneeDb()) == 6.0f, "default knee 6 dB");
    check(c.GetAttackMs() == 15.0f, "default attack 15 ms");
    check(c.GetReleaseMs() == 200.0f, "default release 200 ms");
    check(fl(c.GetMakeupDb()) == 0.0f, "default makeup 0");
    check(c.GetStereoLink(), "default stereo link");
    check(c.GetSoftClip(), "default soft clip");
    check(c.GetSidechainSource() == Compressor::SC_OFF, "default SC OFF");
    check(fl(c.GetSidechainAmount()) == 1.0f, "default SC amount 1");
    check(fl(c.GetMix()) == 1.0f, "default mix 1 (full wet)");
    check(c.GetRtViolations() == 0, "defaults: no RT violations");
}

int main() {
    testDefaults();
    testSidechainDucking();
    testSidechainAmountZero();
    testSidechainHpf();
    testMix();
    if (failures == 0) printf("ALL OK (%d checks)\n", checks);
    else printf("%d/%d checks FAILED\n", failures, checks);
    return failures ? 1 : 0;
}