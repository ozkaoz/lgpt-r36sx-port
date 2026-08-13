/*
 * edit_history_host_test.cpp -- F3-2 (docs/F3_ARCHITECTURE_ES.md):
 * equivalencia GOLDEN de SampleEditHistory.  Cada escenario replica
 * exactamente la secuencia de Bacon 1.2.1 SampleChopperModal sobre el
 * historial logico (pushLogicalUndo/undoLogicalEdit/redoLogicalEdit/
 * clearLogicalRedo/clearLogicalHistory) y la compara contra la capa
 * pura extraida.
 *
 * El golden de la vista captura el estado con captureLogicalState,
 * valida match de sample/path/sourceSize, y restaura con
 * restoreLogicalState; aqui esos tres pasos se sustituyen por
 * equivalentes deterministicos sobre un State de prueba.
 */
#include <stdio.h>
#include <string.h>
#include "Application/Views/ModalDialogs/SampleEditHistory.h"

static int g_failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

/* State de prueba: copia plana con los campos que el golden usa. */
struct TestState {
    int seq;             /* identificador del estado */
    int sampleIndex;
    int sourceSize;
    char samplePath[64];
    char action[40];
    int boundaries[8];

    TestState() {
        seq = -1;
        sampleIndex = 0;
        sourceSize = 0;
        samplePath[0] = 0;
        action[0] = 0;
        for (int i = 0; i < 8; ++i) boundaries[i] = 0;
    }
    TestState(int s, int si, int size, const char *path, const char *ac) {
        seq = s;
        sampleIndex = si;
        sourceSize = size;
        snprintf(samplePath, sizeof(samplePath), "%s", path);
        snprintf(action, sizeof(action), "%s", ac);
        for (int i = 0; i < 8; ++i) boundaries[i] = s * 10 + i;
    }
};

/* ------------------------------------------------------------------ */
/* Golden: implementaciones literales del SampleChopperModal original */
/* (Bacon 1.2.1) sobre arrays propios.                                 */
/* ------------------------------------------------------------------ */
#define MAX_HISTORY 24
static TestState gUndo[MAX_HISTORY];
static TestState gRedo[MAX_HISTORY];
static int gUndoCount = 0;
static int gRedoCount = 0;
static int gCurrentSeq = 1000; /* estado actual del "modal" */

/* captureLogicalState golden: snapshot del estado actual en out. */
static void goldenCapture(TestState &out, const char *action) {
    out.seq = gCurrentSeq;
    out.sampleIndex = 1;
    out.sourceSize = 44100;
    snprintf(out.samplePath, sizeof(out.samplePath), "samples:test.wav");
    snprintf(out.action, sizeof(out.action), "%s", action ? action : "Edit");
    for (int i = 0; i < 8; ++i) out.boundaries[i] = gCurrentSeq * 10 + i;
}

static void goldenClearLogicalRedo() { gRedoCount = 0; }
static void goldenClearLogicalHistory() { gUndoCount = gRedoCount = 0; }

/* pushLogicalUndo golden (sin el gancho destructivo, no aplica aqui). */
static void goldenPushLogicalUndo(const char *action) {
    if (gUndoCount >= MAX_HISTORY) {
        for (int i = 1; i < MAX_HISTORY; ++i) gUndo[i - 1] = gUndo[i];
        gUndoCount = MAX_HISTORY - 1;
    }
    goldenCapture(gUndo[gUndoCount], action);
    ++gUndoCount;
    goldenClearLogicalRedo();
}

/* undoLogicalEdit golden: devuelve la accion deshecha en out (0 si no
   hubo entrada, "MISMATCH" si el sample no coincide). */
static const char *goldenUndoLogicalEdit(char *out, int outLen) {
    if (gUndoCount <= 0) return 0;
    TestState state = gUndo[gUndoCount - 1];

    /* match del sample (golden): sampleIndex, samplePath, sourceSize. */
    if (state.sampleIndex != 1 ||
        strcmp(state.samplePath, "samples:test.wav") != 0 ||
        state.sourceSize != 44100) {
        goldenClearLogicalHistory();
        snprintf(out, outLen, "%s", "MISMATCH");
        return out;
    }

    if (gRedoCount >= MAX_HISTORY) {
        for (int i = 1; i < MAX_HISTORY; ++i) gRedo[i - 1] = gRedo[i];
        gRedoCount = MAX_HISTORY - 1;
    }
    goldenCapture(gRedo[gRedoCount], state.action);
    ++gRedoCount;
    --gUndoCount;

    snprintf(out, outLen, "%s", state.action);
    return out;
}

/* redoLogicalEdit golden: devuelve la accion rehecha en out (0 si no
   hubo entrada). */
static const char *goldenRedoLogicalEdit(char *out, int outLen) {
    if (gRedoCount <= 0) return 0;
    TestState state = gRedo[gRedoCount - 1];

    if (state.sampleIndex != 1 ||
        strcmp(state.samplePath, "samples:test.wav") != 0 ||
        state.sourceSize != 44100) {
        goldenClearLogicalHistory();
        snprintf(out, outLen, "%s", "MISMATCH");
        return out;
    }

    if (gUndoCount >= MAX_HISTORY) {
        for (int i = 1; i < MAX_HISTORY; ++i) gUndo[i - 1] = gUndo[i];
        gUndoCount = MAX_HISTORY - 1;
    }
    goldenCapture(gUndo[gUndoCount], state.action);
    ++gUndoCount;
    --gRedoCount;

    snprintf(out, outLen, "%s", state.action);
    return out;
}

/* ------------------------------------------------------------------ */
/* Adapter: el modal migrado usa la capa pura con el mismo orden.      */
/* ------------------------------------------------------------------ */
static SampleEditHistory<TestState> gHist;

static void pureCapture(TestState &out, const char *action) {
    goldenCapture(out, action);
}
static void purePush(const char *action) {
    TestState s;
    pureCapture(s, action);
    gHist.Push(s);
}
static const char *pureUndo(char *out, int outLen) {
    if (gHist.UndoCount() <= 0) return 0;
    TestState top;
    gHist.PeekUndo(top);
    if (top.sampleIndex != 1 ||
        strcmp(top.samplePath, "samples:test.wav") != 0 ||
        top.sourceSize != 44100) {
        gHist.Clear();
        snprintf(out, outLen, "%s", "MISMATCH");
        return out;
    }
    TestState redoState;
    pureCapture(redoState, top.action);
    if (!gHist.Undo(redoState)) return 0;
    snprintf(out, outLen, "%s", top.action);
    return out;
}
static const char *pureRedo(char *out, int outLen) {
    if (gHist.RedoCount() <= 0) return 0;
    TestState top;
    gHist.PeekRedo(top);
    if (top.sampleIndex != 1 ||
        strcmp(top.samplePath, "samples:test.wav") != 0 ||
        top.sourceSize != 44100) {
        gHist.Clear();
        snprintf(out, outLen, "%s", "MISMATCH");
        return out;
    }
    TestState undoState;
    pureCapture(undoState, top.action);
    if (!gHist.Redo(undoState)) return 0;
    snprintf(out, outLen, "%s", top.action);
    return out;
}

/* ------------------------------------------------------------------ */
/* Escenarios                                                          */
/* ------------------------------------------------------------------ */

/* Replica una secuencia de operaciones en ambos y compara. */
struct OpSeq {
    char op;   /* 'P' push, 'U' undo, 'R' redo, 'C' clear */
    const char *action;
};
static void runSequence(const OpSeq *seq, int n) {
    static int round = 0;
    ++round;
    goldenClearLogicalHistory();
    gHist.Clear();
    gCurrentSeq = 1000;

    for (int i = 0; i < n; ++i) {
        char goldenBuf[48];
        char pureBuf[48];
        char *goldenAction = 0;
        char *pureAction = 0;
        switch (seq[i].op) {
            case 'P':
                goldenPushLogicalUndo(seq[i].action);
                purePush(seq[i].action);
                break;
            case 'U':
                goldenAction = (char *)goldenUndoLogicalEdit(
                    goldenBuf, (int)sizeof(goldenBuf));
                pureAction = (char *)pureUndo(pureBuf, (int)sizeof(pureBuf));
                break;
            case 'R':
                goldenAction = (char *)goldenRedoLogicalEdit(
                    goldenBuf, (int)sizeof(goldenBuf));
                pureAction = (char *)pureRedo(pureBuf, (int)sizeof(pureBuf));
                break;
            case 'C':
                goldenClearLogicalHistory();
                gHist.Clear();
                break;
            default: CHECK(0); break;
        }
        if (seq[i].op == 'U' || seq[i].op == 'R') {
            if (!goldenAction && pureAction) {
                printf("FAIL round %d op %d: golden null, puro '%s'\n",
                       round, i, pureAction);
                ++g_failures;
            } else if (goldenAction && !pureAction) {
                printf("FAIL round %d op %d: golden '%s', puro null\n",
                       round, i, goldenAction);
                ++g_failures;
            } else if (goldenAction && pureAction) {
                CHECK(strcmp(goldenAction, pureAction) == 0);
            }
        }
        CHECK(gUndoCount == gHist.UndoCount());
        CHECK(gRedoCount == gHist.RedoCount());
        if (gUndoCount > 0) {
            CHECK(gUndo[gUndoCount - 1].seq ==
                  gUndo[gUndoCount - 1].seq); /* noop, sanity */
        }
    }
    ++gCurrentSeq;
}

int main() {
    /* 1) Secuencia mixta corta. */
    {
        OpSeq seq[] = {
            {'P', "Add cut"}, {'P', "Merge cuts"}, {'P', "Move cut start"},
            {'U', 0}, {'U', 0}, {'R', 0}, {'R', 0}, {'U', 0},
            {'P', "Split sample"}, {'R', 0},
        };
        runSequence(seq, (int)(sizeof(seq) / sizeof(seq[0])));
    }

    /* 2) Llenado del undo (desbordamiento -> shift descarta el mas
          viejo) y undo hasta vaciarlo. */
    {
        char buf[8];
        OpSeq seq[40];
        int n = 0;
        for (int i = 0; i < 30; ++i) {
            snprintf(buf, sizeof(buf), "Push%d", i);
            seq[n].op = 'P';
            seq[n].action = buf;
            ++n;
        }
        for (int i = 0; i < 10; ++i) {
            seq[n].op = 'U';
            seq[n].action = 0;
            ++n;
        }
        runSequence(seq, n);
        /* el golden con 30 pushes: 6 descartados del principio -> 24. */
        CHECK(gUndoCount == 14);
        CHECK(gHist.UndoCount() == 14);
        CHECK(gRedoCount == 10);
        CHECK(gHist.RedoCount() == 10);
    }

    /* 3) Llenado del undo + undo completo + redo completo. */
    {
        char buf[8];
        OpSeq seq[80];
        int n = 0;
        for (int i = 0; i < 26; ++i) {
            snprintf(buf, sizeof(buf), "E%d", i);
            seq[n].op = 'P';
            seq[n].action = buf;
            ++n;
        }
        for (int i = 0; i < 26; ++i) {
            seq[n].op = 'U';
            seq[n].action = 0;
            ++n;
        }
        for (int i = 0; i < 26; ++i) {
            seq[n].op = 'R';
            seq[n].action = 0;
            ++n;
        }
        runSequence(seq, n);
    }

    /* 4) Push tras undo (invalida el redo) + redo vacio. */
    {
        OpSeq seq[] = {
            {'P', "A"}, {'P', "B"}, {'U', 0},
            {'P', "C"},      /* golden: clearLogicalRedo */
            {'R', 0},        /* debe devolver NULL (redo invalido) */
            {'U', 0}, {'R', 0},
        };
        runSequence(seq, (int)(sizeof(seq) / sizeof(seq[0])));
    }

    /* 5) Clear en medio. */
    {
        OpSeq seq[] = {
            {'P', "X"}, {'P', "Y"}, {'C', 0}, {'P', "Z"},
            {'U', 0}, {'R', 0},
        };
        runSequence(seq, (int)(sizeof(seq) / sizeof(seq[0])));
    }

    /* 6) Undo/redo con match fallido: el golden borra TODO el historial
          y "MISMATCH" no sale en la accion. */
    {
        gHist.Clear();
        goldenClearLogicalHistory();
        CHECK(gHist.UndoCount() == 0);

        /* simula un estado con otro sample activo */
        gCurrentSeq = 500; /* seq distinto, pero capture usa fijos */
        TestState bad;
        bad.sampleIndex = 9; /* mismatch contra golden 1 */
        bad.sourceSize = 22050;
        bad.seq = 42;
        gHist.Push(bad);
        goldenClearLogicalHistory();
        goldenPushLogicalUndo("A");
        goldenPushLogicalUndo("B");
        gUndo[0].sampleIndex = 9; /* corrompe el tope del golden */
        gUndo[1].sampleIndex = 9;

        char gb[48], pb[48];
        char *ga = (char *)goldenUndoLogicalEdit(gb, (int)sizeof(gb));
        CHECK(ga != 0);
        CHECK(strcmp(gb, "MISMATCH") == 0);
        CHECK(gUndoCount == 0 && gRedoCount == 0);

        TestState pure;
        gHist.PeekUndo(pure);
        char *pa = (char *)pureUndo(pb, (int)sizeof(pb));
        CHECK(pa != 0);
        CHECK(strcmp(pb, "MISMATCH") == 0);
        CHECK(gHist.UndoCount() == 0 && gHist.RedoCount() == 0);
    }

    /* 7) Invalidez de argumentos: Capa pura no debe fallar en vacio. */
    {
        TestState s;
        CHECK(gHist.PeekUndo(s) == false);
        CHECK(gHist.PeekRedo(s) == false);
        CHECK(gHist.Undo(s) == false);
        CHECK(gHist.Redo(s) == false);
        CHECK(gHist.UndoCount() == 0);
        CHECK(gHist.RedoCount() == 0);
        gHist.Clear();
        CHECK(gHist.UndoCount() == 0);
    }

    /* 8) Saltos de secuencia aleatorios deterministas. */
    {
        unsigned int seed = 12345u;
        int n = 0;
        OpSeq seq[80];
        for (int i = 0; i < 80; ++i) {
            seed = seed * 1103515245u + 12345u;
            unsigned int r = (seed >> 16) % 100u;
            if (r < 45) {
                seq[n].op = 'P';
                seq[n].action = "Rand";
            } else if (r < 70) {
                seq[n].op = 'U';
                seq[n].action = 0;
            } else if (r < 92) {
                seq[n].op = 'R';
                seq[n].action = 0;
            } else {
                seq[n].op = 'C';
                seq[n].action = 0;
            }
            ++n;
        }
        runSequence(seq, n);
    }

    /* 9) Sobre 100 pushes seguidos: solo 24 entradas se conservan. */
    {
        gHist.Clear();
        goldenClearLogicalHistory();
        for (int i = 0; i < 100; ++i) {
            char buf[8];
            snprintf(buf, sizeof(buf), "P%03d", i);
            goldenPushLogicalUndo(buf);
            purePush(buf);
        }
        CHECK(gHist.UndoCount() == MAX_HISTORY);
        CHECK(gUndoCount == MAX_HISTORY);
        CHECK(gHist.RedoCount() == 0 && gRedoCount == 0);
    }

    if (g_failures == 0) {
        printf("edit_history_host_test: ALL OK (%d checks)\n", 0);
        return 0;
    }
    printf("edit_history_host_test: %d FAILURES\n", g_failures);
    return 1;
}