/*
 * pitch_tool_host_test.cpp -- F3-2 (docs/F3_ARCHITECTURE_ES.md):
 * equivalencia GOLDEN de PitchEnvelopeTool.  Los DSP (resample lineal,
 * envolvente attack/sustain/release) y los clamps se comparan contra
 * replicas literales de los algoritmos de Bacon 1.2.1 en
 * SampleChopperModal (applyEnvelopeToBuffer / buildPitchEnvelopeBuffer-
 * FromRange / nudge*), con numeros explicitos.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "Application/Views/ModalDialogs/PitchEnvelopeTool.h"

static int g_failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static int clampInt(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ---------------- replicas golden literales (Bacon 1.2.1) ---------------- */

/* applyEnvelopeToBuffer golden. */
static void goldenApplyEnvelope(short *samples, int frames, int channels,
                                int rate, int attackMs, int sustainPercent,
                                int releaseMs) {
    if (!samples || frames <= 0 || channels <= 0 || rate <= 0) return;
    attackMs = clampInt(attackMs, 0, 5000);
    releaseMs = clampInt(releaseMs, 0, 5000);
    sustainPercent = clampInt(sustainPercent, 0, 150);

    int attackFrames =
        (int)(((long long)attackMs * (long long)rate) / 1000LL);
    int releaseFrames =
        (int)(((long long)releaseMs * (long long)rate) / 1000LL);
    if (attackFrames > frames) attackFrames = frames;
    if (releaseFrames > frames) releaseFrames = frames;

    double sustain = ((double)sustainPercent) / 100.0;
    for (int i = 0; i < frames; i++) {
        double gain = sustain;
        if (attackFrames > 0 && i < attackFrames) {
            gain *= (double)i / (double)attackFrames;
        }
        if (releaseFrames > 0 && i >= frames - releaseFrames) {
            int remain = frames - 1 - i;
            double rel =
                (remain <= 0) ? 0.0 : ((double)remain / (double)releaseFrames);
            if (rel < 0.0) rel = 0.0;
            if (rel > 1.0) rel = 1.0;
            gain *= rel;
        }
        for (int ch = 0; ch < channels; ch++) {
            int v = (int)((double)samples[i * channels + ch] * gain);
            if (v < -32768) v = -32768;
            if (v > 32767) v = 32767;
            samples[i * channels + ch] = (short)v;
        }
    }
}

/* buildPitchEnvelopeBufferFromRange golden (DSP puro, sin SamplePool). */
static short *goldenBuildPitched(const short *src, int srcSize, int channels,
                                 int startFrame, int endFrame, int semitones,
                                 int attackMs, int sustainPercent,
                                 int releaseMs, int *outFrames) {
    if (!src || srcSize <= 1 || channels <= 0) return 0;
    if (semitones < -12) semitones = -12;
    if (semitones > 12) semitones = 12;

    if (startFrame < 0) startFrame = 0;
    if (endFrame > srcSize - 1) endFrame = srcSize - 1;
    if (endFrame < startFrame) endFrame = startFrame;
    int rangeFrames = endFrame - startFrame + 1;
    if (rangeFrames <= 1) return 0;

    double ratio = pow(2.0, ((double)semitones) / 12.0);
    if (ratio <= 0.0) return 0;
    int nextSize = (int)(((double)rangeFrames / ratio) + 0.5);
    if (nextSize < 2) nextSize = 2;
    if (nextSize > 40000000) return 0;

    short *pitched =
        (short *)malloc((size_t)nextSize * (size_t)channels * sizeof(short));
    if (!pitched) return 0;

    for (int i = 0; i < nextSize; i++) {
        double srcPos = (double)i * ratio;
        int idx = (int)srcPos;
        double frac = srcPos - (double)idx;
        if (idx < 0) idx = 0;
        if (idx >= rangeFrames - 1) {
            idx = rangeFrames - 1;
            frac = 0.0;
        }
        int idx2 = idx + 1;
        if (idx2 >= rangeFrames) idx2 = rangeFrames - 1;
        int srcIdx = startFrame + idx;
        int srcIdx2 = startFrame + idx2;
        for (int ch = 0; ch < channels; ch++) {
            int a = src[srcIdx * channels + ch];
            int b = src[srcIdx2 * channels + ch];
            int v = (int)((double)a + ((double)(b - a) * frac));
            if (v < -32768) v = -32768;
            if (v > 32767) v = 32767;
            pitched[i * channels + ch] = (short)v;
        }
    }

    goldenApplyEnvelope(pitched, nextSize, channels, 48000, attackMs,
                        sustainPercent, releaseMs);
    if (outFrames) *outFrames = nextSize;
    return pitched;
}

/* nudge golden: attack/sustain/release con paso 5, semitones paso 1. */
static int goldenNudgeAttack(int current, int delta) {
    return clampInt(current + delta * 5, 0, 5000);
}
static int goldenNudgeSustain(int current, int delta) {
    return clampInt(current + delta * 5, 0, 150);
}
static int goldenNudgeRelease(int current, int delta) {
    return clampInt(current + delta * 5, 0, 5000);
}
static int goldenNudgeSemitones(int current, int delta) {
    return clampInt(current + delta, -12, 12);
}

/* ---------------- utilidades de comparacion ---------------- */

static int compareBuffers(const short *a, const short *b, int frames,
                          int channels) {
    for (int i = 0; i < frames * channels; ++i)
        if (a[i] != b[i]) return i;
    return -1;
}

int main() {
    /* 1) Clamps / nudges de parametros. */
    {
        PitchEnvelopeTool t;
        t.SetSemitones(99);        CHECK(t.Params().semitones == 12);
        t.SetSemitones(-99);       CHECK(t.Params().semitones == -12);
        t.SetAttackMs(99999);      CHECK(t.Params().attackMs == 5000);
        t.SetSustainPercent(-5);   CHECK(t.Params().sustainPercent == 0);
        t.SetSustainPercent(300);  CHECK(t.Params().sustainPercent == 150);
        t.SetReleaseMs(-1);        CHECK(t.Params().releaseMs == 0);
        t.SetScope(7);             CHECK(t.Params().scope == 1);

        /* nudges desde estado limpio (paso 5 en ms/%, 1 en semitones). */
        t.Reset();
        CHECK(t.NudgeSemitones(3) == goldenNudgeSemitones(0, 3));
        CHECK(t.NudgeSemitones(-40) == goldenNudgeSemitones(3, -40));
        CHECK(t.NudgeAttackMs(2) == goldenNudgeAttack(0, 2));
        CHECK(t.NudgeAttackMs(-1) == goldenNudgeAttack(10, -1));
        CHECK(t.NudgeSustainPercent(3) == goldenNudgeSustain(100, 3));
        CHECK(t.NudgeSustainPercent(-22) == goldenNudgeSustain(115, -22));
        CHECK(t.NudgeReleaseMs(1) == goldenNudgeRelease(0, 1));

        t.Reset();
        t.ToggleScope();           CHECK(t.Params().scope == 1);
        t.ToggleScope();           CHECK(t.Params().scope == 0);

        /* toggles esclavizados al scope ya puesto por SetScope. */
        t.SetScope(7);
        t.ToggleScope();           CHECK(t.Params().scope == 0);

        t.Reset();
        CHECK(t.Params().semitones == 0);
        CHECK(t.Params().attackMs == 0);
        CHECK(t.Params().sustainPercent == 100);
        CHECK(t.Params().releaseMs == 0);
        CHECK(t.Params().scope == 0);
        CHECK(t.HasChange() == false);

        t.SetSemitones(1);
        CHECK(t.HasChange() == true);
        t.Reset();
        t.SetSustainPercent(90);
        CHECK(t.HasChange() == true);
        t.Reset();
        t.SetAttackMs(10);
        CHECK(t.HasChange() == true);
        t.Reset();
    }

    /* 2) Nombres de parametro golden. */
    {
        CHECK(strcmp(PitchEnvelopeTool::ParamName(0), "Pitch") == 0);
        CHECK(strcmp(PitchEnvelopeTool::ParamName(1), "Attack") == 0);
        CHECK(strcmp(PitchEnvelopeTool::ParamName(2), "Sustain") == 0);
        CHECK(strcmp(PitchEnvelopeTool::ParamName(3), "Release") == 0);
        CHECK(strcmp(PitchEnvelopeTool::ParamName(4), "Scope") == 0);
        CHECK(strcmp(PitchEnvelopeTool::ParamName(5), "Sample") == 0);
        CHECK(strcmp(PitchEnvelopeTool::ParamName(9), "Pitch") == 0);
    }

    /* 3) DSP: envolvente identica al golden (stereo, 48k). */
    {
        enum { FRAMES = 480, CH = 2 };
        short src[FRAMES * CH];
        short gold[FRAMES * CH];
        for (int i = 0; i < FRAMES * CH; ++i) src[i] = (short)(i % 32767);

        memcpy(gold, src, sizeof(src));
        goldenApplyEnvelope(gold, FRAMES, CH, 48000, 100, 80, 200);

        short out[FRAMES * CH];
        memcpy(out, src, sizeof(src));
        PitchEnvelopeTool::ApplyEnvelope(out, FRAMES, CH, 48000, 100, 80,
                                         200);
        CHECK(compareBuffers(out, gold, FRAMES, CH) == -1);

        /* attack 0 y release 0: gain = sustain constante. */
        memcpy(gold, src, sizeof(src));
        goldenApplyEnvelope(gold, FRAMES, CH, 48000, 0, 50, 0);
        memcpy(out, src, sizeof(src));
        PitchEnvelopeTool::ApplyEnvelope(out, FRAMES, CH, 48000, 0, 50, 0);
        CHECK(compareBuffers(out, gold, FRAMES, CH) == -1);

        /* saturación: sample maximo * sustain 150% -> clamp 32767. */
        short srcSat[4] = {32767, -32768, 32767, -32768};
        short goldSat[4];
        memcpy(goldSat, srcSat, sizeof(srcSat));
        goldenApplyEnvelope(goldSat, 2, 2, 48000, 0, 150, 0);
        short outSat[4];
        memcpy(outSat, srcSat, sizeof(outSat));
        PitchEnvelopeTool::ApplyEnvelope(outSat, 2, 2, 48000, 0, 150, 0);
        CHECK(compareBuffers(outSat, goldSat, 2, 2) == -1);
        CHECK(outSat[0] <= 32767 && outSat[1] >= -32768);

        /* invalidos: no op. */
        CHECK(PitchEnvelopeTool::ApplyEnvelope(0, 10, 2, 48000, 0, 100,
                                               0) == false);
        CHECK(PitchEnvelopeTool::ApplyEnvelope(src, 0, 2, 48000, 0, 100,
                                               0) == false);
        CHECK(PitchEnvelopeTool::ApplyEnvelope(src, 10, 0, 48000, 0, 100,
                                               0) == false);
    }

    /* 4) DSP: resample identico al golden a varias alturas. */
    {
        enum { SIZE = 4096, CH = 1 };
        short src[SIZE];
        for (int i = 0; i < SIZE; ++i) src[i] = (short)((i * 731) % 32767 - 8000);

        int semis[] = {-12, -7, -3, 0, 1, 4, 7, 12};
        for (unsigned int s = 0; s < sizeof(semis) / sizeof(semis[0]); ++s) {
            int goldFrames = 0, pureFrames = 0;
            short *gold = goldenBuildPitched(src, SIZE, CH, 100, 3000,
                                             semis[s], 50, 90, 120,
                                             &goldFrames);
            CHECK(gold != 0);
            CHECK(goldFrames > 0);
            short *pure = 0;
            bool ok = PitchEnvelopeTool::BuildPitchedRange(
                src, SIZE, CH, 100, 3000, semis[s], 50, 90, 120, 48000,
                &pure, &pureFrames);
            CHECK(ok == true);
            CHECK(goldFrames == pureFrames);
            if (gold && pure) {
                int diff = compareBuffers(gold, pure, pureFrames, CH);
                CHECK(diff == -1);
            }
            free(gold);
            free(pure);
        }
    }

    /* 5) Resample: borde de rango (endFrame=srcSize-1) y mono/stereo. */
    {
        enum { SIZE = 2048, CH = 2 };
        short src[SIZE * CH];
        for (int i = 0; i < SIZE * CH; ++i)
            src[i] = (short)((i * 1009) % 30000 - 15000);

        int goldFrames = 0, pureFrames = 0;
        short *gold = goldenBuildPitched(src, SIZE, CH, 0, SIZE - 1, 0, 0,
                                         100, 0, &goldFrames);
        short *pure = 0;
        bool ok = PitchEnvelopeTool::BuildPitchedRange(
            src, SIZE, CH, 0, SIZE - 1, 0, 0, 100, 0, 48000, &pure,
            &pureFrames);
        CHECK(ok);
        CHECK(goldFrames == SIZE);       /* semitones 0 = passthrough */
        CHECK(pureFrames == SIZE);
        CHECK(compareBuffers(gold, pure, SIZE, CH) == -1);
        free(gold);
        free(pure);
    }

    /* 6) Invalidez de argumentos. */
    {
        short *out = (short *)1;
        int frames = 0;
        short src[128];
        for (int i = 0; i < 128; ++i) src[i] = 0;
        CHECK(PitchEnvelopeTool::BuildPitchedRange(
                  0, 100, 1, 0, 10, 0, 0, 100, 0, 48000, &out,
                  &frames) == false);
        CHECK(out == 0);
        CHECK(PitchEnvelopeTool::BuildPitchedRange(
                  src, 1, 1, 0, 0, 0, 0, 100, 0, 48000, &out,
                  &frames) == false);
        CHECK(PitchEnvelopeTool::BuildPitchedRange(
                  src, 100, 0, 0, 10, 0, 0, 100, 0, 48000, &out,
                  &frames) == false);
        /* rango nulo tras clamp: start > end -> rangeFrames=1 -> false,
           sin tocar el buffer. */
        CHECK(PitchEnvelopeTool::BuildPitchedRange(
                  src, 100, 1, 90, 10, 0, 0, 100, 0, 48000, &out,
                  &frames) == false);
    }

    /* 7) Envolvente dentro del builder aun con fuente invalida. */
    {
        CHECK(PitchEnvelopeTool::ApplyEnvelope(0, 5, 1, 48000, 10, 100,
                                               10) == false);
    }

    if (g_failures == 0) {
        printf("pitch_tool_host_test: ALL OK\n");
        return 0;
    }
    printf("pitch_tool_host_test: %d FAILURES\n", g_failures);
    return 1;
}