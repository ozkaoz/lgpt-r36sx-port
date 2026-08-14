// F3-5b: PhraseUndo.h - oraculos golden de la historia snapshot/restore de
// la frase (golden TREEFROG_GLOBAL_UNDO_V2/V8/V9 de PhraseView.cpp, Bacon
// 1.2.1): capture, equal (dedup V9), push (shift + cap 16 + clear redo +
// guard de reentrada), restore (V8: publica el indice de frase, no el
// cursor) y el paso undo/redo compartido (GlobalUndo/GlobalRedo).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "System/System/System.h"
#include "Application/Phrase/PhraseUndo.h"
#include "Application/Model/Phrase.cpp"

// Stub de System para que Phrase.cpp pueda SYS_MALLOC/SYS_FREE en host.
class HostSystem : public System {
public:
    unsigned long GetClock() { return 0; }
    int GetBatteryLevel() { return 0; }
    void *Malloc(unsigned size) { return ::malloc(size); }
    void Free(void *ptr) { ::free(ptr); }
    void Memset(void *addr, char value, int size) { ::memset(addr, value, size); }
    void *Memcpy(void *s1, const void *s2, int n) { return ::memcpy(s1, s2, n); }
    void PostQuitMessage() {}
    unsigned int GetMemoryUsage() { return 0; }
};

static int g_failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            g_failures++;                                              \
        }                                                              \
    } while (0)

static PhraseUndoSnapshot g_undo[kPhraseUndoHistorySize];
static int g_undoCount = 0;
static PhraseUndoSnapshot g_redo[kPhraseUndoHistorySize];
static int g_redoCount = 0;
static bool g_pushActive = false;

static void historyReset() {
    memset(g_undo, 0, sizeof(g_undo));
    memset(g_redo, 0, sizeof(g_redo));
    g_undoCount = 0;
    g_redoCount = 0;
    g_pushActive = false;
}

static void writeStep(Phrase *p, int phrase, int step, uchar note, uchar vol,
                      uchar pitch, uchar instr, FourCC cmd, ushort param) {
    int off = 16 * phrase;
    p->note_[off + step] = note;
    p->vol_[off + step] = vol;
    p->pitch_[off + step] = pitch;
    p->instr_[off + step] = instr;
    p->cmd1_[off + step] = cmd;
    p->param1_[off + step] = param;
}

static bool stepMatches(Phrase *p, int phrase, int step, uchar note, uchar vol,
                        uchar pitch, uchar instr, FourCC cmd, ushort param) {
    int off = 16 * phrase;
    return p->note_[off + step] == note && p->vol_[off + step] == vol &&
           p->pitch_[off + step] == pitch && p->instr_[off + step] == instr &&
           p->cmd1_[off + step] == cmd && p->param1_[off + step] == param;
}

static void test_capture() {
    historyReset();
    Phrase p;
    // Frase 3, paso 5 con valores no triviales (cmd2/cmd3/param2/param3
    // tambien, clave para la robustez del snapshot completo).
    int off = 16 * 3;
    p.note_[off + 5] = 48;
    p.vol_[off + 5] = 0x64;
    p.pitch_[off + 5] = 0x38; // +8 (stored)
    p.instr_[off + 5] = 0x42;
    p.cmd1_[off + 5] = 'F';
    p.param1_[off + 5] = 0x1234;
    p.cmd2_[off + 5] = 'Q';
    p.param2_[off + 5] = 0x0100;
    p.cmd3_[off + 5] = 'R';
    p.param3_[off + 5] = 0x00FE;
    p.note_[off + 11] = 0x7F;

    PhraseUndoSnapshot e = PhraseUndoCapture(&p, 3);
    CHECK(e.note[5] == 48 && e.note[11] == 0x7F);
    CHECK(e.vol[5] == 0x64);
    CHECK(e.pitch[5] == 0x38);
    CHECK(e.instr[5] == 0x42);
    CHECK(e.cmd1[5] == 'F' && e.param1[5] == 0x1234);
    CHECK(e.cmd2[5] == 'Q' && e.param2[5] == 0x0100);
    CHECK(e.cmd3[5] == 'R' && e.param3[5] == 0x00FE);
    CHECK(e.currentPhrase == 3);
    // Los 16 pasos se capturan (los no escritos son el fill del ctor).
    CHECK(e.note[0] == 0xFF && e.vol[0] == 0xFF && e.pitch[0] == 0x00);
    printf("capture snapshot OK\n");
}

static void test_snapshot_equal() {
    Phrase p;
    int off = 16 * 1;
    p.note_[off + 2] = 60;
    p.param3_[off + 15] = 0xABCD;
    PhraseUndoSnapshot a = PhraseUndoCapture(&p, 1);
    PhraseUndoSnapshot b = PhraseUndoCapture(&p, 1);
    CHECK(PhraseUndoSnapshotEqual(a, b) == true);
    CHECK(PhraseUndoSnapshotEqual(a, a) == true);
    b.note[2] = 61;
    CHECK(PhraseUndoSnapshotEqual(a, b) == false);
    b = a;
    b.vol[0] = 3;
    CHECK(PhraseUndoSnapshotEqual(a, b) == false);
    b = a;
    b.currentPhrase = 2;
    CHECK(PhraseUndoSnapshotEqual(a, b) == false);
    b = a;
    b.param2[9] = 1;
    CHECK(PhraseUndoSnapshotEqual(a, b) == false);
    printf("snapshot equal (dedup V9) OK\n");
}

static void test_push() {
    historyReset();
    Phrase p;
    // Estado A (frase 0): un push captura y encabeza la historia.
    writeStep(&p, 0, 0, 60, 0x60, 0x40, 0x10, 'A', 1);
    bool pushed = PhraseUndoPush(&p, 0, g_undo, &g_undoCount, &g_redoCount,
                                 &g_pushActive);
    CHECK(pushed == true);
    CHECK(g_undoCount == 1);
    CHECK(g_undo[0].note[0] == 60 && g_undo[0].vol[0] == 0x60);
    CHECK(g_undo[0].currentPhrase == 0);
    CHECK(g_pushActive == false);

    // Estado B (mutacion real): segundo push apila, undo[0]=B, undo[1]=A.
    writeStep(&p, 0, 0, 61, 0x50, 0x40, 0x10, 'B', 2);
pushed = PhraseUndoPush(&p, 0, g_undo, &g_undoCount, &g_redoCount,
                                &g_pushActive);
    CHECK(pushed == true);
    CHECK(g_undoCount == 2);
    CHECK(g_undo[0].note[0] == 61);
    CHECK(g_undo[1].note[0] == 60);

    // Dedup V9: push del mismo estado sin mutar -> no anade nada.
pushed = PhraseUndoPush(&p, 0, g_undo, &g_undoCount, &g_redoCount,
                                &g_pushActive);
    CHECK(pushed == false);
    CHECK(g_undoCount == 2);

    // Un undo deja A en redo; un push nuevo borra el redo.
    int cur = 0;
    PhraseUndoStep(&p, g_undo, &g_undoCount, g_redo, &g_redoCount, &cur);
    CHECK(g_undoCount == 1 && g_redoCount == 1);
    writeStep(&p, 0, 0, 62, 0x40, 0x40, 0x10, 'C', 3);
PhraseUndoPush(&p, 0, g_undo, &g_undoCount, &g_redoCount, &g_pushActive);
    CHECK(g_undoCount == 2 && g_redoCount == 0);

    // Guard de reentrada: con pushActive=true el push se ignora tal cual.
    g_pushActive = true;
pushed = PhraseUndoPush(&p, 0, g_undo, &g_undoCount, &g_redoCount,
                                &g_pushActive);
    CHECK(pushed == false);
    CHECK(g_undoCount == 2);
    g_pushActive = false;
    printf("push (shift + dedup + clear redo + guard) OK\n");
}

static void test_push_cap() {
    historyReset();
    Phrase p;
    // 20 estados distintos (note[0] unico por iteracion): la historia se
    // capa en 16 y sobreviven los ultimos 16 pushes (valores 5..20).
    for (int i = 0; i < 20; i++) {
        p.note_[0] = (uchar)(i + 1);
        PhraseUndoPush(&p, 0, g_undo, &g_undoCount, &g_redoCount,
                       &g_pushActive);
    }
    CHECK(g_undoCount == kPhraseUndoHistorySize);
    CHECK(g_undo[0].note[0] == 20); // el ultimo push queda al tope.
    CHECK(g_undo[kPhraseUndoHistorySize - 1].note[0] == 5);
    // El push de un estado identico al tope (dedup V9) no crece.
    PhraseUndoPush(&p, 0, g_undo, &g_undoCount, &g_redoCount, &g_pushActive);
    CHECK(g_undoCount == kPhraseUndoHistorySize);
    printf("push cap a %d OK\n", kPhraseUndoHistorySize);
}

static void test_restore() {
    Phrase p;
    // Snapshot manual con valores de la frase 5.
    PhraseUndoSnapshot e;
    memset(&e, 0, sizeof(e));
    e.currentPhrase = 5;
    for (int i = 0; i < 16; i++) {
        e.note[i] = (uchar)(i + 1);
        e.instr[i] = (uchar)(i + 0x20);
        e.vol[i] = (uchar)(0x60 - i);
        e.pitch[i] = (uchar)(0x28 + i);
        e.cmd1[i] = 'C';
        e.param1[i] = (ushort)(0x100 + i);
        e.cmd2[i] = 'D';
        e.param2[i] = (ushort)(0x200 + i);
        e.cmd3[i] = 'E';
        e.param3[i] = (ushort)(0x300 + i);
    }
    int cur = 2;
    PhraseUndoRestore(&p, e, &cur);
    CHECK(cur == 5); // V8: publica el indice de frase editada.
    int off = 16 * 5;
    for (int i = 0; i < 16; i++) {
        CHECK(p.note_[off + i] == (uchar)(i + 1));
        CHECK(p.instr_[off + i] == (uchar)(i + 0x20));
        CHECK(p.vol_[off + i] == (uchar)(0x60 - i));
        CHECK(p.pitch_[off + i] == (uchar)(0x28 + i));
        CHECK(p.cmd1_[off + i] == 'C' && p.param1_[off + i] == (ushort)(0x100 + i));
        CHECK(p.cmd2_[off + i] == 'D' && p.param2_[off + i] == (ushort)(0x200 + i));
        CHECK(p.cmd3_[off + i] == 'E' && p.param3_[off + i] == (ushort)(0x300 + i));
    }
    // El cursor de edicion NO se mueve (V8); solo currentPhrase se publica.
    CHECK(cur == 5);
    printf("restore (V8, sin mover cursor) OK\n");
}

static void test_undo_redo_cycle() {
    historyReset();
    Phrase p;
    int cur = 0;

    // Golden: cada mutacion captura el estado pre-edit ANTES de aplicarse.
    // A -> push(A) -> muta a B -> push(B) -> muta a C (sin push).
    writeStep(&p, 0, 0, 60, 0x60, 0x40, 0x10, 'A', 1);
    PhraseUndoPush(&p, 0, g_undo, &g_undoCount, &g_redoCount, &g_pushActive);
    writeStep(&p, 0, 0, 61, 0x50, 0x40, 0x10, 'B', 2);
    PhraseUndoPush(&p, 0, g_undo, &g_undoCount, &g_redoCount, &g_pushActive);
    writeStep(&p, 0, 0, 62, 0x40, 0x40, 0x10, 'C', 3);
    // Pila: [B, A]; modelo actual: C (edicion sin snapshot posterior).
    CHECK(g_undoCount == 2 && g_redoCount == 0);

    // Undo #1: desapila el tope (B) y lo restaura -> modelo=B, B al redo.
    bool ok = PhraseUndoStep(&p, g_undo, &g_undoCount, g_redo, &g_redoCount,
                             &cur);
    CHECK(ok == true);
    CHECK(stepMatches(&p, 0, 0, 61, 0x50, 0x40, 0x10, 'B', 2));
    CHECK(g_undoCount == 1 && g_redoCount == 1);
    CHECK(g_redo[0].note[0] == 61);

    // Undo #2: desapila A y la restaura -> modelo=A; pila undo vacia.
    ok = PhraseUndoStep(&p, g_undo, &g_undoCount, g_redo, &g_redoCount, &cur);
    CHECK(ok == true);
    CHECK(stepMatches(&p, 0, 0, 60, 0x60, 0x40, 0x10, 'A', 1));
    CHECK(g_undoCount == 0 && g_redoCount == 2);

    // Undo #3 sobre pila vacia: devuelve true y no toca nada (golden).
    ok = PhraseUndoStep(&p, g_undo, &g_undoCount, g_redo, &g_redoCount, &cur);
    CHECK(ok == true);
    CHECK(g_undoCount == 0 && g_redoCount == 2);
    CHECK(stepMatches(&p, 0, 0, 60, 0x60, 0x40, 0x10, 'A', 1));

    // Redo #1: desapila A (tope del redo) y la restaura -> modelo=A de nuevo.
    ok = PhraseUndoStep(&p, g_redo, &g_redoCount, g_undo, &g_undoCount, &cur);
    CHECK(ok == true);
    CHECK(stepMatches(&p, 0, 0, 60, 0x60, 0x40, 0x10, 'A', 1));
    CHECK(g_undoCount == 1 && g_redoCount == 1);

    // Redo #2: desapila B y la restaura -> modelo=B.
    ok = PhraseUndoStep(&p, g_redo, &g_redoCount, g_undo, &g_undoCount, &cur);
    CHECK(ok == true);
    CHECK(stepMatches(&p, 0, 0, 61, 0x50, 0x40, 0x10, 'B', 2));
    CHECK(g_undoCount == 2 && g_redoCount == 0);

    // Un push nuevo tras el ciclo limpia el redo (politica golden).
    writeStep(&p, 0, 0, 63, 0x30, 0x40, 0x10, 'D', 4);
    PhraseUndoPush(&p, 0, g_undo, &g_undoCount, &g_redoCount, &g_pushActive);
    CHECK(g_undoCount == 3 && g_redoCount == 0);
    printf("undo/redo cycle (V8) OK\n");
}

static void test_undo_restores_other_phrase() {
    historyReset();
    Phrase p;
    // El undo desapila el tope y restaura la FRASE DE ESE SNAPSHOT (f7),
    // aunque el cursor actual de la vista sea otro (el caller lo decide).
    writeStep(&p, 7, 3, 90, 0x64, 0x50, 0x19, 'G', 7);
    PhraseUndoPush(&p, 7, g_undo, &g_undoCount, &g_redoCount, &g_pushActive);
    writeStep(&p, 7, 3, 91, 0x64, 0x50, 0x19, 'G', 7);
    PhraseUndoPush(&p, 7, g_undo, &g_undoCount, &g_redoCount, &g_pushActive);
    int cur = 2;
    PhraseUndoStep(&p, g_undo, &g_undoCount, g_redo, &g_redoCount, &cur);
    CHECK(stepMatches(&p, 7, 3, 91, 0x64, 0x50, 0x19, 'G', 7));
    CHECK(cur == 7); // currentPhrase del snapshot publicado (V8).
    PhraseUndoStep(&p, g_undo, &g_undoCount, g_redo, &g_redoCount, &cur);
    CHECK(stepMatches(&p, 7, 3, 90, 0x64, 0x50, 0x19, 'G', 7));
    printf("undo restaura la frase del snapshot OK\n");
}

int main() {
    HostSystem system;
    System::Install(&system);
    test_capture();
    test_snapshot_equal();
    test_push();
    test_push_cap();
    test_restore();
    test_undo_redo_cycle();
    test_undo_restores_other_phrase();
    if (g_failures == 0) {
        printf("ALL OK\n");
        return 0;
    }
    printf("FAILURES: %d\n", g_failures);
    return 1;
}