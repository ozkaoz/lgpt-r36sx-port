/*
 * chop_model_host_test.cpp -- F3-1 (docs/F3_ARCHITECTURE_ES.md):
 * equivalencia GOLDEN de ChopModel.  Cada escenario replique exactamente
 * la secuencia de Bacon 1.2.1 SampleChopperModal sobre el estado de
 * boundaryes (los algoritmos extraidos no cambian: mismos clamps, misma
 * constante, mismo orden de escrituras).
 */
#include <stdio.h>
#include <string.h>
#include "Application/UI/Views/ModalDialogs/ChopModel.h"

static int g_failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

/* Golden initializeChopsIfNeeded. */
static void goldenInit(ChopModel &m, int sourceSize) {
    m.boundaryCount = 2;
    m.boundaries[0] = 0;
    m.boundaries[1] = sourceSize - 1;
    m.selected = 0;
}

/* Golden sortBoundaries (bubble). */
static void goldenSort(ChopModel &m) {
    for (int i = 0; i < m.boundaryCount - 1; i++) {
        for (int j = i + 1; j < m.boundaryCount; j++) {
            if (m.boundaries[j] < m.boundaries[i]) {
                int t = m.boundaries[i];
                m.boundaries[i] = m.boundaries[j];
                m.boundaries[j] = t;
            }
        }
    }
}

/* Golden split body (sin el mute de parte). */
static void goldenSplit(ChopModel &m, int parts, int sourceSize) {
    if (parts < 2 || parts > 32) parts = 4;
    int step = sourceSize / parts;
    m.boundaryCount = 0;
    for (int i = 0; i < parts; i++) {
        if (m.boundaryCount >= ChopModel::kMaxBoundaries) break;
        m.boundaries[m.boundaryCount++] = i * step;
    }
    int last = sourceSize - 1;
    if (m.boundaryCount == 0 || m.boundaries[m.boundaryCount - 1] != last) {
        if (m.boundaryCount < ChopModel::kMaxBoundaries)
            m.boundaries[m.boundaryCount++] = last;
        else
            m.boundaries[m.boundaryCount - 1] = last;
    }
    goldenSort(m);
    m.selected = 0;
}

static void assertSameModel(const ChopModel &a, const ChopModel &b,
                            const char *label) {
    if (a.boundaryCount != b.boundaryCount || a.selected != b.selected) {
        printf("FAIL %s: count/selected diverge (%d/%d vs %d/%d)\n", label,
               a.boundaryCount, a.selected, b.boundaryCount, b.selected);
        ++g_failures;
        return;
    }
    for (int i = 0; i < ChopModel::kMaxBoundaries; i++) {
        if (a.boundaries[i] != b.boundaries[i]) {
            printf("FAIL %s: boundaries[%d] %d vs %d\n", label, i,
                   a.boundaries[i], b.boundaries[i]);
            ++g_failures;
            return;
        }
    }
}

/* Escenario: Init -> Append x3 -> Sort -> Find -> movidas de seleccion. */
static void testInitAppendSortFind() {
    ChopModel gold, mdl;
    const int size = 44100;
    goldenInit(gold, size);
    mdl.InitRange(size);
    assertSameModel(gold, mdl, "InitRange");

    gold.boundaries[gold.boundaryCount++] = 1000;
    mdl.Append(1000);
    gold.boundaries[gold.boundaryCount++] = 500;
    mdl.Append(500);
    assertSameModel(gold, mdl, "Append x2");
    goldenSort(gold);
    mdl.Sort();
    assertSameModel(gold, mdl, "Sort");

    CHECK(mdl.Find(500) == 1);
    CHECK(mdl.Find(44099) == mdl.boundaryCount - 1);
    CHECK(mdl.Find(999) == -1);
    CHECK(gold.Find(500) == mdl.Find(500));

    /* Seleccion golden vs modelo (selectChop: clamp(sel+delta,0,count-2)). */
    int delta;
    delta = 5;
    int maxChop = mdl.boundaryCount - 2;
    int gsel = ChopModel::ClampInt(0 + delta, 0, maxChop);
    int msel = ChopModel::ClampInt(mdl.selected + delta, 0, maxChop);
    CHECK(gsel == msel);
}

/* Escenario: deleteSelectedChop (RemoveChop + ClampSelectedToChops). */
static void testRemoveChop() {
    ChopModel gold, mdl;
    const int size = 44100;
    int removeIdx;

    for (int trial = 0; trial < 3; trial++) {
        goldenInit(gold, size);
        mdl.InitRange(size);
        int frames[4] = {100, 2000, 3000};
        for (int i = 0; i < 3; i++) {
            gold.boundaries[gold.boundaryCount++] = frames[i];
            mdl.Append(frames[i]);
        }
        goldenSort(gold);
        mdl.Sort();
        gold.selected = trial;
        mdl.selected = trial;

        /* golden delete block: shift, count--, <2 reinit. */
        removeIdx = (gold.selected > 0) ? gold.selected : 1;
        for (int i = removeIdx; i < gold.boundaryCount - 1; i++)
            gold.boundaries[i] = gold.boundaries[i + 1];
        gold.boundaryCount--;
        if (gold.boundaryCount < 2) {
            gold.boundaryCount = 2;
            gold.boundaries[0] = 0;
            gold.boundaries[1] = size > 0 ? size - 1 : 0;
        }
        if (gold.selected > gold.boundaryCount - 2)
            gold.selected = gold.boundaryCount - 2;
        if (gold.selected < 0) gold.selected = 0;

        mdl.RemoveChop(removeIdx, size);
        mdl.ClampSelectedToChops();

        char label[48];
        snprintf(label, sizeof(label), "RemoveChop trial %d", trial);
        assertSameModel(gold, mdl, label);
    }
}

/* Escenario: split/cycle split partes validas y fuera de rango. */
static void testSplitIntoEqualParts() {
    ChopModel gold, mdl;
    for (int parts = 1; parts <= 35; parts++) {
        const int size = 10000;
        goldenSplit(gold, parts == 1 ? 4 : (parts > 32 ? 4 : parts), size);
        mdl.SplitIntoEqualParts(parts, size);
        char label[48];
        snprintf(label, sizeof(label), "Split parts=%d", parts);
        assertSameModel(gold, mdl, label);
    }
    /* Caso cierre con last (sourceSize no divisible). */
    ChopModel a, b;
    a.SplitIntoEqualParts(4, 999);
    b.SplitIntoEqualParts(4, 999);
    CHECK(a.boundaryCount == 5);
    CHECK(a.boundaries[0] == 0);
    CHECK(a.boundaries[4] == 998);

    /* Golden con step=0 (partes > sourceSize): el bloque interior mantiene
     * los parts ceros + cierre con last (el guard "Sample too small" de
     * step<1 vive en la vista, no en el modelo). */
    ChopModel c;
    c.SplitIntoEqualParts(8, 3);
    CHECK(c.boundaryCount == 9);
    CHECK(c.boundaries[0] == 0);
    CHECK(c.boundaries[8] == 2);
}

/* Escenario: clearAllChops. */
static void testClearAll() {
    ChopModel gold, mdl;
    const int size = 44100;
    goldenInit(gold, size);
    for (int i = 0; i < 10; i++) gold.boundaries[gold.boundaryCount++] = i * 1000;
    gold.selected = 4;
    mdl.InitRange(size);
    for (int i = 0; i < 10; i++) mdl.Append(i * 1000);
    mdl.selected = 4;

    gold.boundaryCount = 2;
    gold.boundaries[0] = 0;
    gold.boundaries[1] = size - 1;
    for (int i = 2; i < ChopModel::kMaxBoundaries; i++) gold.boundaries[i] = 0;
    gold.selected = 0;
    mdl.ClearAll(size);
    assertSameModel(gold, mdl, "ClearAll");
}

/* Escenario: StartFrameForSelected / EndFrameForSelected (golden clamps). */
static void testSelectedFrames() {
    ChopModel m;
    m.InitRange(44100);
    CHECK(m.StartFrameForSelected() == 0);
    CHECK(m.EndFrameForSelected(44100) == 44099);
    m.Append(1000);
    m.Sort();
    m.selected = 0;
    int idx0 = ChopModel::ClampInt(m.selected, 0, m.boundaryCount - 2);
    CHECK(m.StartFrameForSelected() == m.boundaries[idx0]);
    m.selected = 99;
    int idxE =
        ChopModel::ClampInt(m.selected + 1, 1, m.boundaryCount - 1);
    CHECK(m.EndFrameForSelected(44100) == m.boundaries[idxE]);
}

int main() {
    testInitAppendSortFind();
    testRemoveChop();
    testSplitIntoEqualParts();
    testClearAll();
    testSelectedFrames();

    if (g_failures != 0) {
        printf("ChopModel host test: %d FAILURES\n", g_failures);
        return 1;
    }
    printf("ChopModel host test: OK (equivalencia golden)\n");
    return 0;
}