// MIXER_64BIT_SUM (U2.53, feedback #7, BACON_1.5_64BIT_MASTER_SUM): the
// master sum accumulates in 64 bits, so a hot mix (2+ full-scale channels,
// or EQ-boosted channels up to 2x) no longer WRAPS the int32 output buffer
// at +/-2.0 linear.  The pre-clip meter reads the TRUE unclipped level, the
// master bus still hard-clips at +/-1.0 for the int16 DAC path, and
// clipBypass_ buses clamp at +/-INT32_MAX (never wrap).
//
// This harness compiles the REAL AudioMixer.cpp with minimal stubs (System
// clock, WavFileWriter) -- the other host tests stub the AudioMixer out, so
// the 64-bit sum itself had no host coverage before.
//
// Scenarios:
//   1. 3 channels at 0.8 on the master bus: sum 2.4 -> the OLD int32 sum
//      wrapped (output garbage, meter ~0.8); the 64-bit sum hard-clips at
//      +/-32767 counts and the meter shows the true 2.4.
//   2. same sum on a clipBypass_ bus: clamped at INT32_MAX, clip flag NOT
//      set (the master bus decides the clip).
//   3. master volume damp applied in the 64-bit domain: 2.4 x damp(80) ->
//      ~0.983, no clip, meter reads the damped pre-clip level.
//   4. idle path is bit-exact: one module below unity, volume 1, damp 1 ->
//      the output equals the input, sample for sample.
//   5. empty mixer: Render returns false.
//   6. NESTED graph (the device shape: master_ -> bus_[i] -> PlayerChannel):
//      every bus contributes to the master.  The OLD single flat scratch was
//      clobbered by the nested child renders (each bus.Render() re-used the
//      master accumulator), so ONLY the last bus survived -> ONE audible
//      track.  BACON_1.5_64BIT_SUM_DEPTH (U2.54, feedback #8) indexes the
//      accumulator by render depth; here 3 buses at 0.4/0.3/0.2 MUST sum to
//      0.9 (not 0.4) and 8 buses at 0.1 MUST sum to 0.8 (not 0.2).
#include "Services/Audio/AudioMixer.h"
#include "System/System/System.h"
#include "Application/Utils/fixed.h"
#include "System/Console/n_assert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// --- System stub (GetClock only; the meter getters decay by elapsed
// wall clock, so g_clock stays 0 and the getters are pure reads) ---
static unsigned long g_clock = 0;
class StubSystem : public System {
  public:
    virtual unsigned long GetClock() { return g_clock; }
    virtual int GetBatteryLevel() { return 100; }
    virtual void *Malloc(unsigned size) { return malloc(size); }
    virtual void Free(void *p) { free(p); }
    virtual void Memset(void *addr, char value, int size) { memset(addr, value, size); }
    virtual void *Memcpy(void *s1, const void *s2, int n) { return memcpy(s1, s2, n); }
    virtual void PostQuitMessage() {}
    virtual unsigned int GetMemoryUsage() { return 0; }
};

// --- WavFileWriter stub (EnableRendering is never used here) ---
WavFileWriter::WavFileWriter(const char *path) {}
WavFileWriter::~WavFileWriter() {}
void WavFileWriter::AddBuffer(fixed *, int) {}
void WavFileWriter::Close() {}

static int checks = 0;
static int failures = 0;

static void check(bool cond, const char *what) {
    checks++;
    if (!cond) {
        failures++;
        printf("FAIL: %s\n", what);
    }
}

class TestModule : public AudioModule {
  public:
    explicit TestModule(fixed level) : level_(level) {}
    virtual bool Render(fixed *buffer, int samplecount) {
        for (int i = 0; i < samplecount * 2; i++) buffer[i] = level_;
        return true;
    }
  private:
    fixed level_;
};

int main() {
    System::Install(new StubSystem());
    const int kFrames = 512;
    fixed buf[2 * kFrames];

    // 1. 3 channels at 0.8 on the MASTER bus: sum 2.4 (over the int32
    //    wrap point).  The 64-bit sum hard-clips at +/-32767 counts and
    //    the pre-clip meter reads the TRUE 2.4 (the old int32 buffer
    //    wrapped and the meter read the wrapped ~0.8).
    {
        AudioMixer mix("Master");
        TestModule a(i2fp(26214)), b(i2fp(26214)), c(i2fp(26214));  // 0.8
        mix.Insert(a);
        mix.Insert(b);
        mix.Insert(c);
        memset(buf, 0, sizeof(buf));
        check(mix.Render(buf, kFrames), "hot master renders");
        check(buf[0] == i2fp(32767), "hot master clamps to +full scale (L)");
        check(buf[1] == i2fp(32767), "hot master clamps to +full scale (R)");
        check(mix.Clipped(), "hot master reports clipping");
        float pkL = mix.GetPeakValueL();
        float pkR = mix.GetPeakValueR();
        printf("hot master: peakL=%.3f peakR=%.3f\n", pkL, pkR);
        check(pkL > 2.35f && pkL < 2.45f, "meter reads the true 2.4 sum (L)");
        check(pkR > 2.35f && pkR < 2.45f, "meter reads the true 2.4 sum (R)");
    }

    // 2. same sum on a clipBypass_ (channel bus): the 64-bit -> int32
    //    narrow CLAMPS at INT32_MAX -- never wraps, no clip flag (the
    //    master bus owns the clip decision).
    {
        AudioMixer bus("Ch1");
        bus.SetClipBypass(true);
        TestModule a(i2fp(26214)), b(i2fp(26214)), c(i2fp(26214));
        bus.Insert(a);
        bus.Insert(b);
        bus.Insert(c);
        memset(buf, 0, sizeof(buf));
        bus.Render(buf, kFrames);
        check(buf[0] == 2147483647, "bypassed bus clamps at INT32_MAX (L)");
        check(buf[1] == 2147483647, "bypassed bus clamps at INT32_MAX (R)");
        check(!bus.Clipped(), "bypassed bus never sets the clip flag");
    }

    // 3. master volume damp in the 64-bit domain: 2.4 x damp(80)^4 =
    //    0.98304 -> ~32211 counts, NO clip, and the meter reads the
    //    damped pre-clip level (the old order damped the WRAPPED int32).
    {
        AudioMixer mix("Master");
        TestModule a(i2fp(26214)), b(i2fp(26214)), c(i2fp(26214));
        mix.Insert(a);
        mix.Insert(b);
        mix.Insert(c);
        mix.SetMasterVolume(80);          // damp = (80/100)^4 = 0.4096
        memset(buf, 0, sizeof(buf));
        mix.Render(buf, kFrames);
        // 78642 counts x 0.4096 = ~0.983 linear: the OLD int32 sum wrapped
        // to -1.6 and the damped output was NEGATIVE; the 64-bit sum
        // produces the true positive ~0.983 level (float pow() precision
        // makes the last ~50 of 1e9 vary, so assert the level, not bits).
        check(buf[0] > 1050000000 && buf[0] < 1060000000,
              "damp applied to the 64-bit sum (no wrap, no clip)");
        check(!mix.Clipped(), "damped sum does not clip");
        float pk = mix.GetPeakValueL();
        printf("damped master: peakL=%.3f out=%d\n", pk, buf[0]);
        check(pk > 0.97f && pk < 1.0f, "meter reads the damped pre-clip level");
    }

    // 4. idle path exact: one module below unity, volume 1, damp 1 -> the
    //    module's output passes through sample-for-sample (no clip, no
    //    rounding anywhere in the 64-bit path).
    {
        AudioMixer mix("Master");
        const fixed level = i2fp(9830);   // 0.3
        TestModule a(level);
        mix.Insert(a);
        memset(buf, 0, sizeof(buf));
        mix.Render(buf, kFrames);
        bool exact = true;
        for (int i = 0; i < 2 * kFrames; i++) {
            if (buf[i] != level) { exact = false; break; }
        }
        float pk = mix.GetPeakValueL();
        printf("idle master: peakL=%.3f\n", pk);
        check(exact, "idle path is exact (no rounding, no clip)");
        check(!mix.Clipped(), "idle path never clips");
        check(pk > 0.29f && pk < 0.31f, "meter reads the single-module level");
    }

    // 5. empty mixer: nothing to render.
    {
        AudioMixer mix("Master");
        memset(buf, 0, sizeof(buf));
        check(!mix.Render(buf, kFrames), "empty mixer renders nothing");
    }

    // 6. NESTED graph (device shape): master -> buses -> channel.  This is
    //    the POLYPHONY regression: the flat scratch made every child bus
    //    clobber the master accumulator, so only the LAST rendered bus was
    //    audible (the last non-muted track).  Each bus here carries ONE
    //    source (one track), exactly like the device.
    {
        AudioMixer master("Master");
        AudioMixer bus0("Ch0"), bus1("Ch1"), bus2("Ch2");
        bus0.SetClipBypass(true);
        bus1.SetClipBypass(true);
        bus2.SetClipBypass(true);
        TestModule a0(i2fp(13107)), a1(i2fp(9830)), a2(i2fp(6554)); // 0.4/0.3/0.2
        bus0.Insert(a0);
        bus1.Insert(a1);
        bus2.Insert(a2);
        master.Insert(bus0);
        master.Insert(bus1);
        master.Insert(bus2);
        memset(buf, 0, sizeof(buf));
        check(master.Render(buf, kFrames), "nested master renders");
        // All three buses MUST be in the sum: 0.4+0.3+0.2 = 0.9.  The old
        // flat-scratch bug left only the last bus (0.2+0.2 = 0.4).
        check(buf[0] == i2fp(29491) || (buf[0] >= 29300 && buf[0] <= 29600),
              "nested sum keeps ALL buses (0.9, not 0.4)");
        float pk = master.GetPeakValueL();
        printf("nested 3ch: peakL=%.3f out=%d\n", pk, buf[0]);
        check(pk > 0.88f && pk < 0.92f, "nested meter reads the full 0.9 sum");
    }

    // 7. Full 8-track device graph: 8 buses x 0.1 -> 0.8.  The bug left
    //    only the LAST track (0.2); the fix sums every track.
    {
        AudioMixer master("Master");
        AudioMixer buses[8] = { AudioMixer("B0"), AudioMixer("B1"), AudioMixer("B2"),
                                AudioMixer("B3"), AudioMixer("B4"), AudioMixer("B5"),
                                AudioMixer("B6"), AudioMixer("B7") };
        TestModule *sources[8];
        for (int i = 0; i < 8; i++) {
            buses[i].SetClipBypass(true);
            sources[i] = new TestModule(i2fp(3277));   // 0.1
            buses[i].Insert(*sources[i]);
            master.Insert(buses[i]);
        }
        memset(buf, 0, sizeof(buf));
        check(master.Render(buf, kFrames), "8-bus master renders");
        // 8 x 0.1 = 0.8 (26216 counts << 15); the buggy flat scratch gave
        // 0.2 (last bus doubled).
        check(buf[0] == i2fp(26216) || (buf[0] >= 859000000 && buf[0] <= 859100000),
              "8-track polyphony: ALL tracks in the sum (0.8)");
        float pk = master.GetPeakValueL();
        printf("nested 8ch: peakL=%.3f out=%d\n", pk, buf[0]);
        check(pk > 0.78f && pk < 0.82f, "8-track meter reads the full 0.8 sum");
        for (int i = 0; i < 8; i++) delete sources[i];
    }

    if (failures == 0) {
        printf("MIXER_64BIT_SUM: %d checks, 0 failures\n", checks);
        return 0;
    }
    printf("MIXER_64BIT_SUM: %d checks, %d failures\n", checks, failures);
    return 1;
}
