/*
 * ANALYZER_TARGET (bacon-1.5, item 7): host test of the SpectrumAnalyzer
 * targeted-tap contract under ASAN/UBSAN.
 *
 * The RT thread feeds the analyzer only when:
 *   - armed (a view is open and listening), AND
 *   - the instrument pointer matches the current target.
 * Instruments that are not being edited must never leak into the ring.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Application/Audio/SpectrumAnalyzer.h"
#include "Application/Instruments/I_Instrument.h"

static int checks = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL line %d: %s\n", __LINE__, #cond);              \
            exit(1);                                                    \
        }                                                               \
        checks++;                                                       \
    } while (0)

/* Minimal I_Instrument so FeedChannel has real pointers to compare. */
class StubInstrument : public I_Instrument {
public:
    virtual bool Init() { return true; }
    virtual bool Start(int, unsigned char, bool) { return true; }
    virtual void Stop(int) {}
    virtual void OnStart() {}
    virtual bool Render(int, fixed *, int, bool) { return false; }
    virtual bool IsInitialized() { return true; }
    virtual bool IsEmpty() { return true; }
    virtual InstrumentType GetType() { return IT_SAMPLE; }
    virtual const char *GetName() { return "stub"; }
    virtual void ProcessCommand(int, FourCC, ushort) {}
    virtual void Purge() {}
    virtual int GetTable() { return 0; }
    virtual bool GetTableAutomation() { return false; }
    virtual void GetTableState(TableSaveState &) {}
    virtual void SetTableState(TableSaveState &) {}
};

static void feedSine(SpectrumAnalyzer &sp, I_Instrument *target, double hz,
                     int frames) {
    fixed buf[2 * 512];
    for (int i = 0; i < frames && i < 512; i++) {
        fixed s = fl2fp((float)(0.5 * sin(2.0 * 3.14159265 * hz *
                                          (double)i / 48000.0)));
        buf[i * 2] = s;
        buf[i * 2 + 1] = s;
    }
    sp.FeedChannel(3, target, buf, frames);
}

int main() {
    StubInstrument instA, instB;
    SpectrumAnalyzer &sp = SpectrumAnalyzer::Get();

    /* --- 1. not armed: feeding is a no-op (Compute says "no new data") --- */
    {
        feedSine(sp, &instA, 1000.0, 512);
        CHECK(sp.Compute() == false);
        for (int i = 0; i < sp.BinCount(); i++) CHECK(sp.Bins()[i] == 0);
    }

    /* --- 2. armed + target matches: the 1 kHz sine reaches the bins --- */
    {
        sp.SetTargetInstrument(&instA);
        sp.SetArmed(true);
        feedSine(sp, &instA, 1000.0, 512);
        CHECK(sp.Compute() == true);

        // expected log-bin index for 1 kHz over 30..20000 Hz, 24 bins
        double idx = log(1000.0 / 30.0) / log(20000.0 / 30.0) * 23.0;
        int expect = (int)(idx + 0.5);
        int peak = 0;
        for (int i = 0; i < sp.BinCount(); i++) {
            if (fp2fl(sp.Bins()[i]) > fp2fl(sp.Bins()[peak])) peak = i;
        }
        CHECK(fp2fl(sp.Bins()[peak]) > 0.05f);
        CHECK(peak >= expect - 2 && peak <= expect + 2);
        sp.SetArmed(false);
        sp.SetTargetInstrument(0);
    }

    /* --- 3. wrong target: the OTHER instrument's audio is dropped --- */
    {
        sp.SetTargetInstrument(&instA);
        sp.SetArmed(true);
        feedSine(sp, &instB, 1000.0, 512);   // instB is not the target
        CHECK(sp.Compute() == false);         // no new generation
        sp.SetTargetInstrument(0);
        sp.SetArmed(false);
    }

    /* --- 4. silence from the target still produces fresh bins (all ~0) --- */
    {
        sp.SetTargetInstrument(&instA);
        sp.SetArmed(true);
        fixed buf[2 * 512];
        memset(buf, 0, sizeof(buf));
        sp.FeedChannel(0, &instA, buf, 512);
        CHECK(sp.Compute() == true);
        for (int i = 0; i < sp.BinCount(); i++) {
            CHECK(fp2fl(sp.Bins()[i]) < 0.001f);
        }
        sp.SetArmed(false);
        sp.SetTargetInstrument(0);
    }

    printf("analyzer_target_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}