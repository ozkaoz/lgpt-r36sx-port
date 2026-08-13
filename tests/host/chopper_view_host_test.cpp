/*
 * F3-3a (docs/F3_ARCHITECTURE_ES.md): ChopperView (geometria + waveform)
 * del Chopper.  Equivalencia golden bajo ASAN/UBSAN: oracles replicando
 * bacon 1.2.1 del SampleChopperModal comparados contra la capa.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Application/Views/ModalDialogs/ChopperView.h"

static int checks = 0;
static const int G_ZOOMS[3] = {5, 50, 100};

static int g_clamp(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* --- oracles golden (bacon 1.2.1) --- */
static const int G_WAVE_W = 288;

static int g_zoomFactor(int zoom) {
    int z = zoom / 5;
    if (z < 1) z = 1;
    return z;
}

static int g_viewFrames(int size, int zoom) {
    if (size <= 0) return 0;
    int frames = size / g_zoomFactor(zoom);
    if (frames < G_WAVE_W) frames = G_WAVE_W;
    if (frames > size) frames = size;
    return frames;
}

static int g_clampView(int viewStart, int size, int zoom) {
    if (size <= 0) return 0;
    int viewFrames = g_viewFrames(size, zoom);
    int maxStart = size - viewFrames;
    if (maxStart < 0) maxStart = 0;
    return g_clamp(viewStart, 0, maxStart);
}

static int g_center(int cursor, int size, int zoom) {
    if (size <= 0) return 0;
    int viewFrames = g_viewFrames(size, zoom);
    return g_clampView(cursor - viewFrames / 2, size, zoom);
}

static int g_ensureVisible(int viewStart, int cursor, int size, int zoom) {
    if (size <= 0) return viewStart;
    int viewFrames = g_viewFrames(size, zoom);
    if (cursor < viewStart) viewStart = cursor;
    if (cursor >= viewStart + viewFrames)
        viewStart = cursor - viewFrames + 1;
    return g_clampView(viewStart, size, zoom);
}

static int g_frameToPixel(int frame, int viewStart, int size, int zoom) {
    int viewFrames = g_viewFrames(size, zoom);
    if (viewFrames <= 1) return -1;
    if (frame < viewStart) return -1;
    if (frame > viewStart + viewFrames - 1) return -1;
    long long rel = (long long)(frame - viewStart) * (long long)(G_WAVE_W - 1);
    rel /= (long long)(viewFrames - 1);
    return g_clamp((int)rel, 0, G_WAVE_W - 1);
}

static int g_pixelToFrame(int px, int viewStart, int size, int zoom) {
    if (size <= 0) return 0;
    int viewFrames = g_viewFrames(size, zoom);
    if (viewFrames <= 1) return viewStart;
    px = g_clamp(px, 0, G_WAVE_W - 1);
    long long frame =
        (long long)viewStart +
        ((long long)px * (long long)(viewFrames - 1)) /
            (long long)(G_WAVE_W - 1);
    if (frame < 0) frame = 0;
    if (frame >= size) frame = size - 1;
    return (int)frame;
}

static int g_nudgeCursor(int cursor, int deltaPx, int size, int zoom) {
    int viewFrames = g_viewFrames(size, zoom);
    int step = viewFrames / G_WAVE_W;
    if (step < 1) step = 1;
    long long deltaFrames = (long long)step * (long long)deltaPx;
    long long next = (long long)cursor + deltaFrames;
    if (next < 0) next = 0;
    if (next >= size) next = size - 1;
    return (int)next;
}

static void g_buildWaveform(const short *samples, int size, int channels,
                            int viewStart, int viewFrames,
                            int *minColumn, int *maxColumn) {
    for (int col = 0; col < G_WAVE_W; col++) {
        int start = viewStart + (col * viewFrames) / G_WAVE_W;
        int end = viewStart + ((col + 1) * viewFrames) / G_WAVE_W;
        if (end <= start) end = start + 1;
        if (start < 0) start = 0;
        if (end > size) end = size;
        int minValue = 32767;
        int maxValue = -32768;
        for (int i = start; i < end; i++) {
            int value = samples[i * channels];
            if (value < minValue) minValue = value;
            if (value > maxValue) maxValue = value;
        }
        if (minValue == 32767 && maxValue == -32768) {
            minValue = 0;
            maxValue = 0;
        }
        minColumn[col] = minValue;
        maxColumn[col] = maxValue;
    }
}

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL line %d: %s\n", __LINE__, #cond);              \
            exit(1);                                                    \
        }                                                               \
        checks++;                                                       \
    } while (0)

static void checkGeom(const char *tag, int got, int exp) {
    if (got != exp) {
        printf("FAIL %s: got %d, expected %d\n", tag, got, exp);
        exit(1);
    }
    checks++;
}

int main() {
    /* --- zoom / view frames --- */
    {
        int zooms[] = {0, 1, 4, 5, 10, 25, 95, 100, 101, 200};
        int sizes[] = {0, 1, 100, 288, 500, 1000, 44100, 100000, 1000000};
        for (size_t z = 0; z < sizeof(zooms) / sizeof(zooms[0]); z++) {
            checkGeom("GetZoomFactor", ChopperView::GetZoomFactor(zooms[z]),
                      g_zoomFactor(zooms[z]));
        }
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
            for (size_t z = 0; z < sizeof(zooms) / sizeof(zooms[0]); z++) {
                checkGeom("GetViewFrameCount",
                          ChopperView::GetViewFrameCount(sizes[s], zooms[z]),
                          g_viewFrames(sizes[s], zooms[z]));
            }
    }

    /* --- clamp / center / ensure visible --- */
    {
        int sizes[] = {0, 1, 100, 288, 1000, 44100, 1000000};
        int views[] = {-100, -1, 0, 100, 500, 9999, 100000, 5000000};
        int cursors[] = {-5, 0, 10, 100, 43700, 999999, 3000000};
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
            for (size_t z = 0; z < 3; z++) {
                int zoom = G_ZOOMS[z % 3];
                for (size_t v = 0; v < sizeof(views) / sizeof(views[0]);
                     v++) {
                    checkGeom("ClampViewStart",
                              ChopperView::ClampViewStart(
                                  views[v], sizes[s], zoom),
                              g_clampView(views[v], sizes[s], zoom));
                    for (size_t c = 0;
                         c < sizeof(cursors) / sizeof(cursors[0]); c++) {
                        checkGeom("EnsureCursorVisible",
                                  ChopperView::EnsureCursorVisible(
                                      views[v], cursors[c], sizes[s], zoom),
                                  g_ensureVisible(views[v], cursors[c],
                                                  sizes[s], zoom));
                    }
                }
                for (size_t c = 0;
                     c < sizeof(cursors) / sizeof(cursors[0]); c++) {
                    checkGeom("CenterOnCursor",
                              ChopperView::CenterOnCursor(
                                  cursors[c], sizes[s], zoom),
                              g_center(cursors[c], sizes[s], zoom));
                }
            }
    }

    /* --- frame <-> pixel --- */
    {
        int sizes[] = {0, 1, 288, 1000, 44100, 1000000};
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
            for (size_t z = 0; z < sizeof(G_ZOOMS) / sizeof(G_ZOOMS[0]);
                 z++) {
                int zoom = G_ZOOMS[z];
                int viewStart = g_clampView(100, sizes[s], zoom);
                for (int f = -10; f <= (int)sizes[s] + 10; f += 7) {
                    checkGeom("FrameToPixel",
                              ChopperView::FrameToPixel(
                                  f, viewStart, sizes[s], zoom),
                              g_frameToPixel(f, viewStart, sizes[s], zoom));
                }
                for (int px = -20; px <= 300; px += 11) {
                    checkGeom("PixelToFrame",
                              ChopperView::PixelToFrame(
                                  px, viewStart, sizes[s], zoom),
                              g_pixelToFrame(px, viewStart, sizes[s], zoom));
                }
            }
    }

    /* --- nudge cursor (paso y clamps) --- */
    {
        int sizes[] = {1, 288, 1000, 44100, 1000000};
        int cursors[] = {0, 100, 999, 22050, 44099, 999999};
        int deltas[] = {-5, -1, 0, 1, 3, 50};
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
            for (size_t z = 0; z < sizeof(G_ZOOMS) / sizeof(G_ZOOMS[0]); z++)
                for (size_t c = 0;
                     c < sizeof(cursors) / sizeof(cursors[0]); c++)
                    for (size_t d = 0;
                         d < sizeof(deltas) / sizeof(deltas[0]); d++) {
                        checkGeom("NudgeCursorPixels",
                                  ChopperView::NudgeCursorPixels(
                                      cursors[c], deltas[d], sizes[s],
                                      G_ZOOMS[z]),
                                  g_nudgeCursor(cursors[c], deltas[d],
                                                sizes[s], G_ZOOMS[z]));
                    }
    }

    /* --- nudge zoom (clamps min/max) --- */
    {
        CHECK(ChopperView::NudgeZoom(50, 10, 5, 100) == 60);
        CHECK(ChopperView::NudgeZoom(95, 10, 5, 100) == 100);
        CHECK(ChopperView::NudgeZoom(5, -10, 5, 100) == 5);
        CHECK(ChopperView::NudgeZoom(50, 0, 5, 100) == 50);
        checks += 4;
    }

    /* --- waveform columns vs golden --- */
    {
        const int size = 10000;
        const int channels = 2;
        short buf[size * channels];
        for (int i = 0; i < size; i++) {
            /* canal 0: picos deterministas; canal 1: ruido fijo */
            buf[i * channels] = (short)((i * 37) % 60000 - 30000);
            buf[i * channels + 1] = (short)((i * 13) % 2000 - 1000);
        }
        /* picos exactos en frames conocidos */
        buf[1000 * channels] = 32767;
        buf[1000 * channels + 1] = -32767;
        buf[4321 * channels] = -32768;
        buf[9999 * channels] = 12345;

        struct {
            int viewStart, viewFrames;
        } vcases[] = {
            {0, 10000},      /* ventana completa, todos los picos */
            {1000, 5000},    /* ventana interna */
            {0, 288},        /* frames == cols */
            {500, 200},      /* frames < cols (end<=start) */
            {4990, 20},      /* ventana minima */
            {0, 1},          /* un frame */
        };
        for (size_t v = 0; v < sizeof(vcases) / sizeof(vcases[0]); v++) {
            int viewStart = vcases[v].viewStart;
            int viewFrames = vcases[v].viewFrames;
            int minA[G_WAVE_W], maxA[G_WAVE_W];
            int minB[G_WAVE_W], maxB[G_WAVE_W];
            g_buildWaveform(buf, size, channels, viewStart, viewFrames,
                            minA, maxA);
            bool ok = ChopperView::BuildWaveformColumns(
                buf, size, channels, viewStart, viewFrames, G_WAVE_W, minB,
                maxB);
            CHECK(ok);
            for (int col = 0; col < G_WAVE_W; col++) {
                if (minA[col] != minB[col] || maxA[col] != maxB[col]) {
                    printf("FAIL BuildWaveformColumns viewStart=%d "
                           "viewFrames=%d col=%d: got %d..%d expected "
                           "%d..%d\n",
                           viewStart, viewFrames, col, minB[col], maxB[col],
                           minA[col], maxA[col]);
                    exit(1);
                }
                checks++;
            }
        }
        /* picos clave visibles en la ventana completa */
        {
            int minB[G_WAVE_W], maxB[G_WAVE_W];
            CHECK(ChopperView::BuildWaveformColumns(
                buf, size, channels, 0, size, G_WAVE_W, minB, maxB));
            int colPeak = (int)(((long long)1000 * (G_WAVE_W - 1)) /
                                (size - 1));
            CHECK(maxB[colPeak] == 32767);
            checks += 2;
        }
    }

    printf("chopper_view_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}