#ifndef _SAMPLE_EDIT_HISTORY_H_
#define _SAMPLE_EDIT_HISTORY_H_

/*
 * F3-2 (docs/F3_ARCHITECTURE_ES.md): historial logico undo/redo del Chopper
 * extraido del SampleChopperModal como capa pura (header-only, sin
 * dependencias de la vista).
 *
 * Regla de oro: NO cambia el comportamiento de Bacon 1.2.1.  Los
 * algoritmos de stack replican EXACTAMENTE el golden de la vista
 * (pushLogicalUndo/undoLogicalEdit/redoLogicalEdit/clearLogicalRedo/
 * clearLogicalHistory):
 * - Push: si el undo esta lleno (kMaxEntries) descarta la entrada mas
 *   vieja (shift izquierda) y luego INVALIDA el redo (clearLogicalRedo
 *   del golden tras cada push).
 * - Undo: mueve el tope del undo al redo (con captura previa del estado
 *   actual como "redoState") y restaura el nuevo tope del undo.
 * - Redo: operacion espejo.
 *
 * El estado almacenado es un tipo de datos (State) definido por el dueño
 * (LogicalHistoryState en la vista); aqui solo se gestionan stacks de
 * copias planas (memcpy-safe, sin punteros).  La captura del estado
 * actual y la restauracion (refresh de vista/audio/overlay) siguen en el
 * dueño vía captureLogicalState/restoreLogicalState, que reciben la copia
 * del tope con PeekUndo/PeekRedo.
 */

#define LGPT_HISTORY_MAX_ENTRIES 24

template <typename State>
class SampleEditHistory {
public:
    SampleEditHistory() : undoCount_(0), redoCount_(0) {}

    void Clear() {
        undoCount_ = 0;
        redoCount_ = 0;
    }

    /* Golden clearLogicalRedo: vacia solo el redo. */
    void ClearRedo() { redoCount_ = 0; }

    int UndoCount() const { return undoCount_; }
    int RedoCount() const { return redoCount_; }
    static int MaxEntries() { return LGPT_HISTORY_MAX_ENTRIES; }

    /* Copia del tope sin mutar los stacks (para match del sample e
       historia del mensaje "Undo: %s").  Devuelve false si vacio. */
    bool PeekUndo(State &out) const {
        if (undoCount_ <= 0) return false;
        out = undo_[undoCount_ - 1];
        return true;
    }
    bool PeekRedo(State &out) const {
        if (redoCount_ <= 0) return false;
        out = redo_[redoCount_ - 1];
        return true;
    }

    /*
     * Golden pushLogicalUndo:
     *   if (undoHistoryCount_ >= MAX_LOGICAL_HISTORY) shift izquierda;
     *   undoHistory_[undoHistoryCount_] = captured; ++undoHistoryCount_;
     *   clearLogicalRedo();
     * Orden exacto: primero descarta la mas vieja, despues escribe, y el
     * redo se invalida al final (el golden lo hace en pushLogicalUndo
     * despues de capturar).
     */
    void Push(const State &state) {
        if (undoCount_ >= LGPT_HISTORY_MAX_ENTRIES) {
            for (int i = 1; i < LGPT_HISTORY_MAX_ENTRIES; ++i)
                undo_[i - 1] = undo_[i];
            undoCount_ = LGPT_HISTORY_MAX_ENTRIES - 1;
        }
        undo_[undoCount_] = state;
        ++undoCount_;
        redoCount_ = 0;
    }

    /*
     * Golden undoLogicalEdit:
     *   if (redoHistoryCount_ >= MAX) shift izquierda del redo;
     *   redoHistory_[redoHistoryCount_] = redoState;  (captura previa en
     *   el dueño, con el action del tope)
     *   ++redoHistoryCount_; --undoHistoryCount_;
     *   restore(undoHistory_[undoHistoryCount_]); (nuevo tope)
     * El estado redoState lo captura el dueño ANTES de llamar con el
     * action del tope (PeekUndo), igual que el golden.
     */
    bool Undo(const State &redoState) {
        if (undoCount_ <= 0) return false;

        if (redoCount_ >= LGPT_HISTORY_MAX_ENTRIES) {
            for (int i = 1; i < LGPT_HISTORY_MAX_ENTRIES; ++i)
                redo_[i - 1] = redo_[i];
            redoCount_ = LGPT_HISTORY_MAX_ENTRIES - 1;
        }
        redo_[redoCount_] = redoState;
        ++redoCount_;
        --undoCount_;
        return true;
    }

    /* Golden redoLogicalEdit: espejo exacto de Undo(). */
    bool Redo(const State &undoState) {
        if (redoCount_ <= 0) return false;

        if (undoCount_ >= LGPT_HISTORY_MAX_ENTRIES) {
            for (int i = 1; i < LGPT_HISTORY_MAX_ENTRIES; ++i)
                undo_[i - 1] = undo_[i];
            undoCount_ = LGPT_HISTORY_MAX_ENTRIES - 1;
        }
        undo_[undoCount_] = undoState;
        ++undoCount_;
        --redoCount_;
        return true;
    }

private:
    State undo_[LGPT_HISTORY_MAX_ENTRIES];
    State redo_[LGPT_HISTORY_MAX_ENTRIES];
    int undoCount_;
    int redoCount_;
};

#endif