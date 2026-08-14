/*
 * F3-3a (docs/F3_ARCHITECTURE_ES.md): PreviewService del Chopper.
 * Equivalencia golden bajo ASAN/UBSAN: las funciones puras aquí replican
 * EXACTAMENTE los algoritmos de bacon 1.2.1 del SampleChopperModal y se
 * comparan contra la capa sobre la misma tabla de escenarios.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Application/UI/Views/ModalDialogs/PreviewService.h"

static int checks = 0;

static int g_clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* --- oracle golden (bacon 1.2.1) --- */

struct R {
    int start;
    int end;
};

static R g_setRange(int start, int end, int size) {
    R r;
    /* golden setPreviewPlaybackRange */
    start = g_clamp(start, 0, size > 0 ? size - 1 : 0);
    end = g_clamp(end, start, size > 0 ? size - 1 : start);
    r.start = start;
    r.end = end;
    return r;
}

static R g_trimStart(int chopStart, int chopEnd, int rate) {
    R r;
    /* golden previewTrimStart */
    int previewEnd = chopStart + (rate > 0 ? rate * 5 : 220500);
    if (previewEnd > chopEnd) previewEnd = chopEnd;
    r.start = chopStart;
    r.end = previewEnd;
    return r;
}

static R g_trimEnd(int chopStart, int chopEnd, int rate) {
    R r;
    /* golden previewTrimEnd */
    int previewStart = chopEnd - (rate > 0 ? rate * 1 : 44100);
    if (previewStart < chopStart) previewStart = chopStart;
    r.start = previewStart;
    r.end = chopEnd;
    return r;
}

static int g_playFrame(int frame, int size) {
    /* golden playFromFrame */
    if (size > 0) return g_clamp(frame, 0, size - 1);
    return 0;
}

static R g_playRange(int start, int end, int size) {
    R r;
    /* golden playFrameRange */
    if (size > 0) {
        start = g_clamp(start, 0, size - 1);
        end = g_clamp(end, 0, size - 1);
    } else {
        start = 0;
        end = 0;
    }
    if (end < start) end = start;
    r.start = start;
    r.end = end;
    return r;
}

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL line %d: %s\n", __LINE__, #cond);              \
            exit(1);                                                    \
        }                                                               \
        checks++;                                                       \
    } while (0)

static void checkRange(const char *tag, int gotStart, int gotEnd,
                       int expStart, int expEnd) {
    if (gotStart != expStart || gotEnd != expEnd) {
        printf("FAIL %s: got %d-%d, expected %d-%d\n", tag, gotStart,
               gotEnd, expStart, expEnd);
        exit(1);
    }
    checks++;
}

int main() {
    /* --- SetRange vs golden (con orden de clamps) --- */
    {
        int sizes[] = {0, 1, 100, 44100, 48000};
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            int size = sizes[s];
            int starts[] = {-10, -1, 0, 1, size / 2, size, size + 7,
                            size * 3};
            for (size_t si = 0;
                 si < sizeof(starts) / sizeof(starts[0]); si++) {
                int ends[] = {-5, 0, size / 3, size - 1, size, size + 9};
                for (size_t ei = 0;
                     ei < sizeof(ends) / sizeof(ends[0]); ei++) {
                    R exp = g_setRange(starts[si], ends[ei], size);
                    PreviewService ps;
                    ps.SetRange(starts[si], ends[ei], size);
                    CHECK(ps.Active());
                    checkRange("SetRange", ps.StartFrame(), ps.EndFrame(),
                               exp.start, exp.end);
                }
            }
        }
    }

    /* --- orden: end se clampa contra el start YA clampeado --- */
    {
        PreviewService ps;
        ps.SetRange(500, 200, 100);
        checkRange("clamp order", ps.StartFrame(), ps.EndFrame(), 99, 99);
        ps.SetRange(-3, -7, 100);
        checkRange("clamp neg", ps.StartFrame(), ps.EndFrame(), 0, 0);
        ps.SetRange(0, 50, 1);
        checkRange("size 1", ps.StartFrame(), ps.EndFrame(), 0, 0);
    }

    /* --- ClearRange / Deactivate --- */
    {
        PreviewService ps;
        ps.SetRange(10, 20, 100);
        ps.ClearRange();
        CHECK(!ps.Active());
        CHECK(ps.StartFrame() == 0);
        CHECK(ps.EndFrame() == 0);
        ps.ClearRange();
        CHECK(!ps.Active());
        ps.SetRange(30, 40, 100);
        ps.Deactivate();
        CHECK(!ps.Active());
        CHECK(ps.StartFrame() == 30);
        CHECK(ps.EndFrame() == 40);
        checks += 6;
    }

    /* --- TrimStart vs golden --- */
    {
        struct {
            int start, end, rate;
        } cases[] = {
            {100, 5000, 44100},
            {100, 5000, 48000},
            {100, 5000, 0},
            {100, 200, 44100},
            {100, 1000000, 44100},
            {0, 0, 44100},
            {0, 0, 0},
            {1234, 1234, 8000},
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            R exp = g_trimStart(cases[i].start, cases[i].end, cases[i].rate);
            PreviewService::Range got = PreviewService::TrimStart(
                cases[i].start, cases[i].end, cases[i].rate);
            checkRange("TrimStart", got.start, got.end, exp.start, exp.end);
        }
    }

    /* --- TrimEnd vs golden --- */
    {
        struct {
            int start, end, rate;
        } cases[] = {
            {100, 5000, 44100},
            {100, 5000, 48000},
            {100, 5000, 0},
            {100000, 200000, 44100},
            {100000, 200000, 48000},
            {0, 0, 44100},
            {0, 0, 0},
            {500, 1200, 8000},
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            R exp = g_trimEnd(cases[i].start, cases[i].end, cases[i].rate);
            PreviewService::Range got = PreviewService::TrimEnd(
                cases[i].start, cases[i].end, cases[i].rate);
            checkRange("TrimEnd", got.start, got.end, exp.start, exp.end);
        }
    }

    /* --- ClampPlayFrame vs golden --- */
    {
        int sizes[] = {0, 1, 100, 44100};
        int frames[] = {-50, -1, 0, 1, 99, 100, 101, 44100, 100000};
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
            for (size_t f = 0; f < sizeof(frames) / sizeof(frames[0]); f++) {
                int exp = g_playFrame(frames[f], sizes[s]);
                int got = PreviewService::ClampPlayFrame(frames[f],
                                                         sizes[s]);
                if (got != exp) {
                    printf("FAIL ClampPlayFrame size=%d frame=%d: got %d "
                           "expected %d\n",
                           sizes[s], frames[f], got, exp);
                    exit(1);
                }
                checks++;
            }
    }

    /* --- ClampPlayRange vs golden --- */
    {
        int sizes[] = {0, 1, 100, 44100};
        int starts[] = {-5, 0, 50, 99, 100, 5000};
        int ends[] = {-5, 0, 50, 99, 100, 5000, -100, 1000};
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
            for (size_t si = 0; si < sizeof(starts) / sizeof(starts[0]);
                 si++)
                for (size_t ei = 0; ei < sizeof(ends) / sizeof(ends[0]);
                     ei++) {
                    R exp = g_playRange(starts[si], ends[ei], sizes[s]);
                    PreviewService::Range got = PreviewService::ClampPlayRange(
                        starts[si], ends[ei], sizes[s]);
                    checkRange("ClampPlayRange", got.start, got.end,
                               exp.start, exp.end);
                }
    }

    printf("preview_service_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}