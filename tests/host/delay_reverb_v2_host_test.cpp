// FXP_DELAY_REVERB_V2 (bacon-1.5, item 3): host DSP test of DelayLine V2
// (musical sync, per-sample glide, loop LOW/HIGH CUT, ping-pong) and Reverb
// V2 (fractional delays, control-rate RT60, per-sample gain/length glides,
// LFO modulation, wet-only RC2).  Compiles the two .cpp with ASAN/UBSAN.
#include "Application/Audio/FxEngine/DelayLine.h"
#include "Application/Audio/FxEngine/Reverb.h"
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

static bool feq(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

// ---- DelayLine tests ----
static void testSyncMath() {
    // Integer SyncDivisionToMs vs the float formula, across all divisions.
    for (int div = 0; div < SDIV_COUNT; div++) {
        int num = kSyncDivisions[div].num;
        int den = kSyncDivisions[div].den;
        int bpms[5] = {60, 90, 120, 140, 180};
        for (int i = 0; i < 5; i++) {
            float ref = (240000.0f * (float)num) / ((float)bpms[i] * (float)den);
            if (ref > (float)DelayLine::kMaxMs) ref = (float)DelayLine::kMaxMs;
            float got = fp2fl(DelayLine::SyncDivisionToMs(div, bpms[i]));
            check(feq(got, ref, 1.0f), "sync int math matches float");
        }
    }
    // 1/1 @ 40 bpm = 6000 ms -> clamped to kMaxMs.
    check(fp2fl(DelayLine::SyncDivisionToMs(SDIV_1_1, 40)) == (float)DelayLine::kMaxMs,
          "sync clamps at kMaxMs");
    // Out-of-range division falls back to 1/16.
    check(DelayLine::SyncDivisionToMs(999, 120)
          == DelayLine::SyncDivisionToMs(SDIV_1_16, 120), "sync div fallback");
    // Out-of-range bpm uses 120.
    check(DelayLine::SyncDivisionToMs(SDIV_1_4, 30)
          == DelayLine::SyncDivisionToMs(SDIV_1_4, 120), "sync bpm fallback");
    // Musical golden: 1/4 @ 120 = 500 ms.
    check(fp2fl(DelayLine::SyncDivisionToMs(SDIV_1_4, 120)) == 500.0f, "1/4 @120 = 500ms");
    // Triplet is 2/3 of straight, dotted is 3/2 (within 1 ms).
    check(feq(fp2fl(DelayLine::SyncDivisionToMs(SDIV_1_8T, 120)) * 1.5f,
              fp2fl(DelayLine::SyncDivisionToMs(SDIV_1_8, 120)), 1.0f), "triplet = 2/3");
    check(feq(fp2fl(DelayLine::SyncDivisionToMs(SDIV_1_8D, 120)) * 2.0f / 3.0f,
              fp2fl(DelayLine::SyncDivisionToMs(SDIV_1_8, 120)), 1.0f), "dotted = 3/2");
    check(SDIV_1_16 == 3 && SDIV_COUNT == 16,
          "1/16 index 3, 16 divisions");
}

static void testDelayTapTiming() {
    DelayLine d;
    d.SetSampleRate(48000);
    d.SetDelayMs(fl2fp(500.0f));
    d.SetFeedback(fl2fp(0.5f));
    d.SetMix(i2fp(1));
    d.SetPingPong(false);
    d.SetSaturation(false);
    d.SetBypass(false);

    // Pre-warm: let the per-sample glide reach 24000 samples (0.5/sample ->
    // 48000 frames) with silence.
    static fixed zero[DelayLine::kBufferSize];
    for (int i = 0; i < DelayLine::kBufferSize; i++) zero[i] = 0;
    d.Process(zero, zero, DelayLine::kMaxSamplesPerChannel);

    // Impulse on L, silence afterwards.
    static fixed in[DelayLine::kBufferSize];
    static fixed out[DelayLine::kBufferSize];
    memset(in, 0, sizeof(in));
    memset(out, 0, sizeof(out));
    in[0] = i2fp(1);
    d.Process(in, out, DelayLine::kMaxSamplesPerChannel);

    int peakIdx = -1;
    for (int i = 21000; i < 27000 && peakIdx < 0; i++) {
        if (out[2 * i] > i2fp(1) / 2) peakIdx = i;
    }
    check(peakIdx > 0 && peakIdx >= 23990 && peakIdx <= 24010,
          "500ms tap lands at ~24000 samples");
    // Echo on L only (no ping-pong), and no wet when input silent beyond tap.
    check(out[2 * 24000 + 1] < i2fp(1) / 2, "no echo on R without ping-pong");
    // Follow-up echo at 2*tap (feedback 0.5), smaller.
    int peak2 = -1;
    for (int i = 47000; i < 49000 && peak2 < 0; i++) {
        if (out[2 * i] > i2fp(1) / 4) peak2 = i;
    }
    check(peak2 >= 47990 && peak2 <= 48010, "second echo at ~2*tap");
    check(d.GetRtViolations() == 0, "delay no rt violations");
}

static void testDelayPingPong() {
    DelayLine d;
    d.SetSampleRate(48000);
    d.SetDelayMs(fl2fp(100.0f));
    d.SetFeedback(fl2fp(0.5f));
    d.SetMix(i2fp(1));
    d.SetPingPong(true);

    static fixed zero[DelayLine::kBufferSize];
    static fixed in[DelayLine::kBufferSize];
    static fixed out[DelayLine::kBufferSize];
    memset(in, 0, sizeof(in));
    for (int i = 0; i < DelayLine::kBufferSize; i++) zero[i] = 0;
    d.Process(zero, zero, DelayLine::kMaxSamplesPerChannel);
    in[0] = i2fp(1);
    d.Process(in, out, DelayLine::kMaxSamplesPerChannel);
    // First echo on L, second (ping-pong crossed) on R.
    check(out[2 * 4800] > i2fp(1) / 2, "ping-pong first echo on L");
    check(out[2 * 9600 + 1] > i2fp(1) / 4, "ping-pong second echo on R");
}

static void testDelayLoopCuts() {
    // Energy of the tail with LOW CUT (LP 200 Hz) or HIGH CUT (HP 4 kHz)
    // must be below the open-filter tail.
    DelayLine open, lp, hp;
    open.SetSampleRate(48000);
    lp.SetSampleRate(48000);
    hp.SetSampleRate(48000);
    for (int t = 0; t < 3; t++) {
        DelayLine *d = (t == 0) ? &open : (t == 1) ? &lp : &hp;
        d->SetDelayMs(fl2fp(30.0f));
        d->SetFeedback(fl2fp(0.9f));
        d->SetMix(i2fp(1));
        d->SetPingPong(false);
        d->SetBypass(false);
    }
    lp.SetLoopLPHz(fl2fp(200.0f));
    hp.SetLoopHPHz(fl2fp(4000.0f));

    static fixed zero[DelayLine::kBufferSize];
    static fixed in[DelayLine::kBufferSize];
    static fixed o1[DelayLine::kBufferSize];
    static fixed o2[DelayLine::kBufferSize];
    static fixed o3[DelayLine::kBufferSize];
    memset(in, 0, sizeof(in));
    for (int i = 0; i < DelayLine::kBufferSize; i++) zero[i] = 0;
    for (int t = 0; t < 3; t++) {
        DelayLine *d = (t == 0) ? &open : (t == 1) ? &lp : &hp;
        d->Process(zero, zero, DelayLine::kMaxSamplesPerChannel);
    }
    in[0] = i2fp(1);
    open.Process(in, o1, DelayLine::kMaxSamplesPerChannel);
    lp.Process(in, o2, DelayLine::kMaxSamplesPerChannel);
    hp.Process(in, o3, DelayLine::kMaxSamplesPerChannel);
    long long e1 = 0, e2 = 0, e3 = 0;
    for (int i = 0; i < DelayLine::kBufferSize; i++) {
        e1 += (long long)o1[i] * o1[i];
        e2 += (long long)o2[i] * o2[i];
        e3 += (long long)o3[i] * o3[i];
    }
    check(e2 * 100 < e1 * 95, "LOW CUT reduces tail energy");
    check(e3 * 100 < e1 * 95, "HIGH CUT reduces tail energy");
}

static void testDelayMixBypass() {
    // mix=0: bit-exact passthrough.
    DelayLine d;
    d.SetSampleRate(48000);
    d.SetDelayMs(fl2fp(50.0f));
    d.SetFeedback(fl2fp(0.5f));
    d.SetMix(0);
    static fixed in[DelayLine::kBufferSize];
    static fixed out[DelayLine::kBufferSize];
    int ok = 1;
    for (int i = 0; i < DelayLine::kBufferSize; i++) {
        int v = (i % 997) * 31 - 16000;
        in[i] = (fixed)v;
    }
    d.Process(in, out, DelayLine::kMaxSamplesPerChannel);
    for (int i = 0; i < DelayLine::kBufferSize; i++) {
        if (out[i] != in[i]) ok = 0;
    }
    check(ok, "mix=0 is bit-exact passthrough");

    // Bypass toggle: wet glides out/in, no click (bounded per-sample delta).
    DelayLine b;
    b.SetSampleRate(48000);
    b.SetDelayMs(fl2fp(50.0f));
    b.SetFeedback(fl2fp(0.5f));
    b.SetMix(i2fp(1));
    b.SetBypass(false);
    for (int i = 0; i < DelayLine::kBufferSize; i++) {
        in[i] = fl2fp(0.25f * sinf(2.0f * 3.14159265f * 220.0f * (i / 2) / 48000.0f));
    }
    int maxDelta = 0;
    for (int n = 0; n < 48000; n += 512) {
        // Alternate every chunk to stress the crossfade, then stay bypassed
        // for the last ~0.5 s so the wet gain settles to dry.  n is in
        // frames, the interleaved buffers step 2 samples per frame.
        b.SetBypass(((n / 512) % 2 == 1) || n >= 24000);
        b.Process(in + 2 * n, out + 2 * n, 512);
        if (n > 0) {
            for (int i = 0; i < 512 * 2; i++) {
                int d0 = out[2 * n + i] - out[2 * n + i - 1];
                if (d0 < 0) d0 = -d0;
                if (d0 > maxDelta) maxDelta = d0;
            }
        }
    }
    check(maxDelta < fl2fp(0.15f), "bypass crossfade has no click");
    // After bypass settles (~24000 samples of bypass), output is dry.
    check(abs(out[48000 - 2] - in[48000 - 2]) < fl2fp(0.01f),
          "bypass settled to dry");
    check(b.GetRtViolations() == 0, "bypass no rt violations");
}

static void testDelayWidth() {
    DelayLine d;
    d.SetSampleRate(48000);
    d.SetDelayMs(fl2fp(50.0f));
    d.SetFeedback(fl2fp(0.5f));
    d.SetMix(i2fp(1));
    d.SetWidth(0);
    static fixed zero[DelayLine::kBufferSize];
    static fixed in[DelayLine::kBufferSize];
    static fixed out[DelayLine::kBufferSize];
    memset(in, 0, sizeof(in));
    for (int i = 0; i < DelayLine::kBufferSize; i++) zero[i] = 0;
    d.Process(zero, zero, DelayLine::kMaxSamplesPerChannel);
    in[0] = i2fp(1);          // L impulse only
    d.Process(in, out, DelayLine::kMaxSamplesPerChannel);
    check(out[2 * 2400] == out[2 * 2400 + 1], "width=0 collapses to mono");
}

// ---- Reverb tests ----
static void testReverbStability(bool eco) {
    Reverb r;
    r.SetSampleRate(48000);
    r.SetMode(eco ? Reverb::ECO : Reverb::NORMAL);
    r.SetDecay(fl2fp(1.5f));
    r.SetSize(i2fp(1));
    r.SetDamping(fl2fp(0.5f));
    r.SetWidth(i2fp(1));
    r.SetPredelayMs(0);
    r.SetInputHP(fl2fp(20.0f));
    r.SetInputLP(fl2fp(20000.0f));
    r.SetBypass(false);

    // 4 s of stereo (interleaved): 2 * 48000 * 4 samples.
    static fixed in[48000 * 4 * 2];   // 4 s
    static fixed out[48000 * 4 * 2];
    memset(in, 0, sizeof(in));
    memset(out, 0, sizeof(out));
    in[0] = i2fp(1);
    r.Process(in, out, 48000 * 4);
    fixed mx = 0;
    for (int i = 0; i < 48000 * 4 * 2; i++) {
        if (out[i] > mx) mx = out[i];
        if (-out[i] > mx) mx = -out[i];
    }
    check(mx < i2fp(2), eco ? "ECO stable, bounded" : "NORMAL stable, bounded");
    check(r.GetRtViolations() == 0, eco ? "ECO no rt violations" : "NORMAL no rt violations");
}

static void testReverbRT60() {
    Reverb r;
    r.SetSampleRate(48000);
    r.SetMode(Reverb::NORMAL);
    r.SetDecay(fl2fp(1.0f));
    r.SetSize(i2fp(1));
    r.SetDamping(0);
    r.SetWidth(i2fp(1));
    r.SetPredelayMs(0);
    r.SetInputHP(fl2fp(20.0f));
    r.SetInputLP(fl2fp(20000.0f));
    r.SetBypass(false);

    // 6 s of stereo (interleaved): 2 * 48000 * 6 samples.
    static fixed in[48000 * 6 * 2];
    static fixed out[48000 * 6 * 2];
    memset(in, 0, sizeof(in));
    memset(out, 0, sizeof(out));
    in[0] = i2fp(1);
    r.Process(in, out, 48000 * 6);

    // Windowed RMS every 100 ms; find when the envelope first falls 60 dB
    // below the initial post-attack envelope (frames after 100 ms).
    long long firstE = 0;
    for (int i = 4800; i < 9600; i++) firstE += (long long)out[i] * out[i];
    long long target = firstE / 1000000;  // -60 dB
    if (target < 1) target = 1;
    float rt60 = -1.0f;
    for (int w = 1; w < 58; w++) {
        long long e = 0;
        for (int i = w * 4800; i < (w + 1) * 4800; i++) e += (long long)out[i] * out[i];
        if (e < target) {
            rt60 = (float)(w - 1) * 0.1f;
            break;
        }
    }
    check(rt60 > 0.5f && rt60 < 1.8f, "RT60 ~1s (tolerance 0.5..1.8)");
    check(r.GetRtViolations() == 0, "RT60 no rt violations");
}

static void testReverbSizeMorph() {
    Reverb r;
    r.SetSampleRate(48000);
    r.SetMode(Reverb::NORMAL);
    r.SetDecay(fl2fp(1.0f));
    r.SetSize(fl2fp(0.5f));
    r.SetDamping(fl2fp(0.5f));
    r.SetWidth(i2fp(1));
    r.SetPredelayMs(0);
    r.SetBypass(false);

    // 2 s of stereo (interleaved) + slack for the final 512-frame chunk.
    static fixed in[48000 * 2 * 2 + 2048];
    static fixed out[48000 * 2 * 2 + 2048];
    memset(in, 0, sizeof(in));
    in[0] = i2fp(1);
    int maxDelta = 0;
    for (int n = 0; n < 48000 * 2; n += 512) {
        r.SetSize(n < 24000 ? fl2fp(0.5f) : fl2fp(1.5f));
        r.Process(in + 2 * n, out + 2 * n, 512);
        if (n > 0) {
            for (int i = 0; i < 512 * 2; i++) {
                int d0 = out[2 * n + i] - out[2 * n + i - 1];
                if (d0 < 0) d0 = -d0;
                if (d0 > maxDelta) maxDelta = d0;
            }
        }
    }
    check(maxDelta < fl2fp(0.3f), "size morph has no click");
}

static void testReverbPredelay() {
    Reverb r;
    r.SetSampleRate(48000);
    r.SetMode(Reverb::NORMAL);
    r.SetDecay(fl2fp(0.2f));
    r.SetSize(i2fp(1));
    r.SetDamping(fl2fp(0.5f));
    r.SetWidth(i2fp(1));
    r.SetPredelayMs(fl2fp(50.0f));
    r.SetBypass(false);

    // 2 s of stereo (interleaved): 2 * 48000 * 2 samples.
    static fixed in[48000 * 2 * 2];
    static fixed out[48000 * 2 * 2];
    static fixed zero[48000 * 2 * 2];
    memset(in, 0, sizeof(in));
    memset(out, 0, sizeof(out));
    memset(zero, 0, sizeof(zero));
    // Pre-warm so the predelay length glide (0 -> 2400 at 1 sample/sample)
    // has settled before the impulse; the impulse then waits ~50 ms + the
    // comb latency.
    r.Process(zero, zero, 48000 * 2);
    in[0] = i2fp(1);
    r.Process(in, out, 48000 * 2);
    // First energy must arrive after the ~50 ms predelay (glide starts at 0
    // and advances 1 sample/sample: ~2400 frames ramp) plus the shortest comb
    // latency (1214 frames at 48 kHz) +- the LFO modulation (+-2 frames):
    // sample ~2*(2400+1214) = 7228, i.e. frames [3608, 3640] both channels.
    // The first echo is inverted by the allpass chain, so scan |out|.
    int firstActive = -1;
    for (int i = 0; i < 48000 * 2 * 2 && firstActive < 0; i++) {
        if (out[i] > i2fp(1) / 1000 || -out[i] > i2fp(1) / 1000) firstActive = i;
    }
    check(firstActive >= 6500 && firstActive <= 7800, "predelay ~50ms first energy");
}

static void testReverbWetOnlyMixInert() {
    // RC2: RVB MIX is persisted but inert -- DSP is fixed 100% wet.
    Reverb a, b;
    for (int t = 0; t < 2; t++) {
        Reverb *r = (t == 0) ? &a : &b;
        r->SetSampleRate(48000);
        r->SetMode(Reverb::NORMAL);
        r->SetDecay(fl2fp(1.0f));
        r->SetSize(i2fp(1));
        r->SetDamping(fl2fp(0.3f));
        r->SetWidth(i2fp(1));
        r->SetPredelayMs(fl2fp(10.0f));
        r->SetBypass(false);
    }
    a.SetMix(i2fp(1));
    b.SetMix(fl2fp(0.3f));
    check(a.GetMix() == i2fp(1) && b.GetMix() == fl2fp(0.3f), "mix stored");
    // 2 s of stereo (interleaved): 2 * 48000 * 2 samples.
    static fixed in[48000 * 2 * 2];
    static fixed o1[48000 * 2 * 2];
    static fixed o2[48000 * 2 * 2];
    memset(in, 0, sizeof(in));
    for (int i = 0; i < 48000; i++) {
        in[2 * i] = fl2fp(0.8f * sinf(2.0f * 3.14159265f * 300.0f * i / 48000.0f));
        in[2 * i + 1] = in[2 * i];
    }
    a.Process(in, o1, 48000 * 2);
    b.Process(in, o2, 48000 * 2);
    int same = 1;
    for (int i = 0; i < 48000 * 2 * 2; i++) {
        if (o1[i] != o2[i]) same = 0;
    }
    check(same, "RVB MIX inert: DSP bit-identical at any stored mix");
}

static void testReverbInputFilters() {
    // Input LP 500 Hz vs open on white noise: much less output energy.
    Reverb open, lp, hp;
    for (int t = 0; t < 3; t++) {
        Reverb *r = (t == 0) ? &open : (t == 1) ? &lp : &hp;
        r->SetSampleRate(48000);
        r->SetMode(Reverb::NORMAL);
        r->SetDecay(fl2fp(0.15f));
        r->SetSize(i2fp(1));
        r->SetDamping(0);
        r->SetWidth(i2fp(1));
        r->SetPredelayMs(0);
        r->SetBypass(false);
    }
    open.SetInputHP(fl2fp(20.0f));
    open.SetInputLP(fl2fp(20000.0f));
    lp.SetInputLP(fl2fp(500.0f));
    lp.SetInputHP(fl2fp(20.0f));
    hp.SetInputHP(fl2fp(2000.0f));
    hp.SetInputLP(fl2fp(20000.0f));

    // 2 s of stereo (interleaved): 2 * 48000 * 2 samples.
    static fixed in[48000 * 2 * 2];
    static fixed o1[48000 * 2 * 2];
    static fixed o2[48000 * 2 * 2];
    static fixed o3[48000 * 2 * 2];
    memset(in, 0, sizeof(in));
    // LP test: white noise (broadband) through a 500 Hz input LP.
    unsigned int seed = 12345;
    for (int i = 0; i < 48000 * 2 * 2; i++) {
        seed = seed * 1103515245u + 12345u;
        in[i] = (fixed)(((seed >> 16) & 0x7FFF) - 16384);
    }
    open.Process(in, o1, 48000 * 2);
    lp.Process(in, o2, 48000 * 2);
    long long e1 = 0, e2 = 0;
    for (int i = 0; i < 48000 * 2 * 2; i++) {
        e1 += (long long)o1[i] * o1[i];
        e2 += (long long)o2[i] * o2[i];
    }
    check(e2 * 100 < e1 * 90, "reverb input LP attenuates");
    // HP test: a 100 Hz sine (mostly below 2 kHz) through a 2 kHz input HP.
    static fixed o4[48000 * 2 * 2];
    for (int i = 0; i < 48000 * 2 * 2; i++) {
        int f = i / 2;
        in[i] = fl2fp(0.8f * sinf(2.0f * 3.14159265f * 100.0f * f / 48000.0f));
    }
    open.Process(in, o3, 48000 * 2);
    hp.Process(in, o4, 48000 * 2);
    long long e1b = 0, e3 = 0;
    for (int i = 0; i < 48000 * 2 * 2; i++) {
        e1b += (long long)o3[i] * o3[i];
        e3 += (long long)o4[i] * o4[i];
    }
    check(e3 * 100 < e1b * 30, "reverb input HP attenuates");
    // The stored Hz survive a sample-rate change (legacy bug fixed).
    lp.SetSampleRate(48000);
    check(lp.GetInputLPHz() == fl2fp(500.0f), "input LP Hz kept after SetSampleRate");
    check(hp.GetInputHPHz() == fl2fp(2000.0f), "input HP Hz kept after SetSampleRate");
    check(lp.GetRtViolations() == 0 && hp.GetRtViolations() == 0, "filters no violations");
}

static void testReverbBypassGlide() {
    Reverb r;
    r.SetSampleRate(48000);
    r.SetMode(Reverb::NORMAL);
    r.SetDecay(fl2fp(0.1f));
    r.SetSize(i2fp(1));
    r.SetDamping(fl2fp(0.5f));
    r.SetWidth(i2fp(1));
    r.SetPredelayMs(0);
    r.SetBypass(false);

    // 2 s of stereo (interleaved): 2 * 48000 * 2 samples.
    static fixed in[48000 * 2 * 2];
    static fixed out[48000 * 2 * 2];
    memset(in, 0, sizeof(in));
    for (int i = 0; i < 48000; i++) {
        in[2 * i] = fl2fp(0.8f * sinf(2.0f * 3.14159265f * 220.0f * i / 48000.0f));
        in[2 * i + 1] = in[2 * i];
    }
    int maxDelta = 0;
    for (int n = 0; n < 48000; n += 512) {
        r.SetBypass(n >= 24000);
        r.Process(in + 2 * n, out + 2 * n, 512);
        if (n > 0) {
            // Per-channel deltas only: the L/R banks are decorrelated
            // (different comb lengths), so interleaved L->R deltas are large
            // by design and say nothing about clicks.
            for (int i = 2; i < 512 * 2; i++) {
                int d0 = out[2 * n + i] - out[2 * n + i - 2];
                if (d0 < 0) d0 = -d0;
                if (d0 > maxDelta) maxDelta = d0;
            }
        }
    }
    check(maxDelta < fl2fp(0.15f), "bypass glide no click");
    // Fully bypassed for ~1 s: wet gain ~0 -> output near 0.
    fixed tailMax = 0;
    for (int i = 48000 * 2 - 2000; i < 48000 * 2; i++) {
        if (out[i] > tailMax) tailMax = out[i];
        if (-out[i] > tailMax) tailMax = -out[i];
    }
    check(tailMax < fl2fp(0.02f), "bypass settled to silence (wet-only)");
    check(r.GetRtViolations() == 0, "bypass no violations");
}

static void testReverbGettersClamps() {
    Reverb r;
    r.SetSampleRate(48000);
    r.SetDecay(fl2fp(2.0f));
    check(r.GetDecayTarget() == fl2fp(2.0f), "decay target readback");
    r.SetPredelayMs(fl2fp(200.0f));   // clamped to 100
    check(r.GetPredelayMs() == fl2fp(100.0f), "predelay clamps at 100ms");
    r.SetSize(fl2fp(5.0f));           // clamped to 1.5
    check(r.GetSize() == fl2fp(1.5f), "size clamps at 1.5");
    r.SetDecay(fl2fp(100.0f));        // clamped to 8
    check(r.GetDecayTarget() == fl2fp(8.0f), "decay clamps at 8s");
    r.SetInputHP(fl2fp(25000.0f));
    check(r.GetInputHPHz() == fl2fp(20000.0f), "input HP clamps at 20kHz");
    r.SetMode(Reverb::ECO);
    check(r.GetMode() == Reverb::ECO, "mode readback");
    check(Reverb::StaticMemoryBytes() > 0, "static memory reported");
    r.Reset();
    check(r.GetRtViolations() == 0, "reset keeps rt clean");
}

int main() {
    testSyncMath();
    testDelayTapTiming();
    testDelayPingPong();
    testDelayLoopCuts();
    testDelayMixBypass();
    testDelayWidth();

    testReverbStability(false);
    testReverbStability(true);
    testReverbRT60();
    testReverbSizeMorph();
    testReverbPredelay();
    testReverbWetOnlyMixInert();
    testReverbInputFilters();
    testReverbBypassGlide();
    testReverbGettersClamps();

    if (failures == 0) {
        printf("ALL OK (%d checks)\n", checks);
        return 0;
    }
    printf("%d/%d checks FAILED\n", failures, checks);
    return 1;
}
