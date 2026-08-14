#ifndef _PHRASEUNDO_H_
#define _PHRASEUNDO_H_

// F3-5b: PhraseUndo - historia snapshot/restore de la frase (golden
// TREEFROG_GLOBAL_UNDO_V2/V8/V9, Bacon 1.1.1/1.2.1) extraida de
// PhraseView.cpp.  Capa pura: solo dependencias de datos (Types.h,
// Phrase.h).  No toca GUI, audio, Player, ViewData ni el framebuffer.
//
// La vista (PhraseView) conserva la POLITICA de push (que acciones capturan
// snapshot y cuando: updateCursorValue, pasteLast, cut/paste clipboard,
// interpolate, chop, command selector, VM_NEW A, VM_CLONE L+A) y el efecto
// de undo/redo sobre el cursor local de la vista; esta capa implementa la
// mecanica exacta de la historia.

#include "Foundation/Types/Types.h"
#include "Application/Model/Phrase.h"
#include <string.h>

// golden: PhraseView::kPhraseHistorySize = 16
static const int kPhraseUndoHistorySize = 16;

// Layout identico al struct PhraseView::PhraseEdit original (golden
// TREEFROG_GLOBAL_UNDO_V2): snapshots de los 16 pasos de la frase editada
// mas el indice de frase (V8: el cursor de edicion ya no viaja en el
// snapshot; undo/redo no devuelven el cursor al punto de edicion).
struct PhraseUndoSnapshot {
    uchar note[16];
    uchar instr[16];
    uchar vol[16];
    uchar pitch[16];
    FourCC cmd1[16];
    ushort param1[16];
    FourCC cmd2[16];
    ushort param2[16];
    FourCC cmd3[16];
    ushort param3[16];
    uchar currentPhrase;
};

// Captura el estado golden de la frase actual (bloque de 16 pasos en el
// offset 16*currentPhrase de los arrays del modelo) en un snapshot.
static inline PhraseUndoSnapshot PhraseUndoCapture(const Phrase *phrase,
                                                   int currentPhrase) {
    PhraseUndoSnapshot e;
    memcpy(e.note, phrase->note_ + 16 * currentPhrase, 16);
    memcpy(e.instr, phrase->instr_ + 16 * currentPhrase, 16);
    memcpy(e.vol, phrase->vol_ + 16 * currentPhrase, 16);
    memcpy(e.pitch, phrase->pitch_ + 16 * currentPhrase, 16);
    memcpy(e.cmd1, phrase->cmd1_ + 16 * currentPhrase, 16 * sizeof(FourCC));
    memcpy(e.param1, phrase->param1_ + 16 * currentPhrase, 16 * sizeof(ushort));
    memcpy(e.cmd2, phrase->cmd2_ + 16 * currentPhrase, 16 * sizeof(FourCC));
    memcpy(e.param2, phrase->param2_ + 16 * currentPhrase, 16 * sizeof(ushort));
    memcpy(e.cmd3, phrase->cmd3_ + 16 * currentPhrase, 16 * sizeof(FourCC));
    memcpy(e.param3, phrase->param3_ + 16 * currentPhrase, 16 * sizeof(ushort));
    e.currentPhrase = (uchar)currentPhrase;
    return e;
}

// Igualdad byte a byte de dos snapshots (golden: dedup del push, V9).
static inline bool PhraseUndoSnapshotEqual(const PhraseUndoSnapshot &a,
                                           const PhraseUndoSnapshot &b) {
    if (a.currentPhrase != b.currentPhrase) return false;
    return memcmp(a.note, b.note, sizeof(a.note)) == 0 &&
           memcmp(a.instr, b.instr, sizeof(a.instr)) == 0 &&
           memcmp(a.vol, b.vol, sizeof(a.vol)) == 0 &&
           memcmp(a.pitch, b.pitch, sizeof(a.pitch)) == 0 &&
           memcmp(a.cmd1, b.cmd1, sizeof(a.cmd1)) == 0 &&
           memcmp(a.param1, b.param1, sizeof(a.param1)) == 0 &&
           memcmp(a.cmd2, b.cmd2, sizeof(a.cmd2)) == 0 &&
           memcmp(a.param2, b.param2, sizeof(a.param2)) == 0 &&
           memcmp(a.cmd3, b.cmd3, sizeof(a.cmd3)) == 0 &&
           memcmp(a.param3, b.param3, sizeof(a.param3)) == 0;
}

// Push golden de PhraseView::pushPhraseUndo (tramo de historia, sin el
// guard de reentrada g_phraseUndoPushActive, que es politica de la vista y
// se pasa por parametro): captura, dedup V9 contra el tope (una repeticion
// del MISMO estado pre-edit dentro del mismo gesto no anada nada), shift de
// la pila undo, cap kPhraseUndoHistorySize y limpiado del redo.
// Devuelve true si el snapshot entro en la historia.
static inline bool PhraseUndoPush(const Phrase *phrase, int currentPhrase,
                                  PhraseUndoSnapshot *undo, int *undoCount,
                                  int *redoCount, bool *pushActive) {
    if (*pushActive) return false;
    *pushActive = true;
    PhraseUndoSnapshot e = PhraseUndoCapture(phrase, currentPhrase);
    if (*undoCount > 0 && PhraseUndoSnapshotEqual(e, undo[0])) {
        *pushActive = false;
        return false;
    }
    for (int i = kPhraseUndoHistorySize - 1; i > 0; i--)
        undo[i] = undo[i - 1];
    undo[0] = e;
    (*undoCount)++;
    if (*undoCount > kPhraseUndoHistorySize) *undoCount = kPhraseUndoHistorySize;
    *redoCount = 0;
    *pushActive = false;
    return true;
}

// Restore golden de phraseUndoRestore: escribe el snapshot en el bloque
// 16*e.currentPhrase del modelo y publica el indice de frase editada
// (V8: el corriente se queda donde el usuario lo dejo; no se mueve).
static inline void PhraseUndoRestore(Phrase *phrase,
                                     const PhraseUndoSnapshot &e,
                                     int *currentPhraseOut) {
    memcpy(phrase->note_ + 16 * e.currentPhrase, e.note, 16);
    memcpy(phrase->instr_ + 16 * e.currentPhrase, e.instr, 16);
    memcpy(phrase->vol_ + 16 * e.currentPhrase, e.vol, 16);
    memcpy(phrase->pitch_ + 16 * e.currentPhrase, e.pitch, 16);
    memcpy(phrase->cmd1_ + 16 * e.currentPhrase, e.cmd1, 16 * sizeof(FourCC));
    memcpy(phrase->param1_ + 16 * e.currentPhrase, e.param1,
           16 * sizeof(ushort));
    memcpy(phrase->cmd2_ + 16 * e.currentPhrase, e.cmd2, 16 * sizeof(FourCC));
    memcpy(phrase->param2_ + 16 * e.currentPhrase, e.param2,
           16 * sizeof(ushort));
    memcpy(phrase->cmd3_ + 16 * e.currentPhrase, e.cmd3, 16 * sizeof(FourCC));
    memcpy(phrase->param3_ + 16 * e.currentPhrase, e.param3,
           16 * sizeof(ushort));
    *currentPhraseOut = e.currentPhrase;
}

// Un paso de historia golden (PhraseView::GlobalUndo / GlobalRedo): saca el
// tope de la pila FROM, la corre, la inserta en TO (shift + cap) y restaura
// el modelo.  Cuando la pila FROM esta vacia devuelve true sin tocar nada
// (idem golden: GlobalUndo/GlobalRedo devuelven true siempre).
static inline bool PhraseUndoStep(Phrase *phrase, PhraseUndoSnapshot *from,
                                  int *fromCount, PhraseUndoSnapshot *to,
                                  int *toCount, int *currentPhraseOut) {
    if (*fromCount == 0) return true;
    PhraseUndoSnapshot e = from[0];
    for (int i = 0; i < *fromCount - 1; i++) from[i] = from[i + 1];
    (*fromCount)--;
    for (int i = kPhraseUndoHistorySize - 1; i > 0; i--) to[i] = to[i - 1];
    to[0] = e;
    (*toCount)++;
    if (*toCount > kPhraseUndoHistorySize) *toCount = kPhraseUndoHistorySize;
    PhraseUndoRestore(phrase, e, currentPhraseOut);
    return true;
}

#endif // _PHRASEUNDO_H_