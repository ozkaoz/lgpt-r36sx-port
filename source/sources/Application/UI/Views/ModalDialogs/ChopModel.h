/*
 * ChopModel.h -- F3-1 (docs/F3_ARCHITECTURE_ES.md): estado puro de los
 * cortes del chopper, extraido de SampleChopperModal SIN cambiar su
 * logica.  Cada metodo replica exactamente el algoritmo golden
 * (Bacon 1.2.1, SampleChopperModal.cpp): mismo orden de escrituras,
 * mismos clamps, mismas constantes.  La vista conserva todo lo demas
 * (mensajes de estado, preview, historia, cursor, dibujo).
 *
 * Invariante: kMaxBoundaries == MAX_CHOP_BOUNDARIES de la vista.  Los
 * campos conservan los nombres golden (boundaryCount/selected/boundaries).
 */
#ifndef APPLICATION_VIEWS_MODALDIALOGS_CHOPMODEL_H_
#define APPLICATION_VIEWS_MODALDIALOGS_CHOPMODEL_H_

class ChopModel {
  public:
    /* Golden: MAX_CHOP_BOUNDARIES = 101. */
    static const int kMaxBoundaries = 101;

    /* Golden SampleChopperModal: boundaryCount_, selectedChop_,
     * boundaries_[].  Datos publicos como el struct golden; los algoritmos
     * de mutacion viven aqui. */
    int boundaryCount;
    int selected;
    int boundaries[kMaxBoundaries];

    ChopModel() : boundaryCount(0), selected(0) {
        for (int i = 0; i < kMaxBoundaries; ++i) {
            boundaries[i] = 0;
        }
    }

    /* Golden clampInt (SampleChopperModal). */
    static int ClampInt(int value, int minValue, int maxValue) {
        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        return value;
    }

    /* Golden initializeChopsIfNeeded (parte de estado): rango completo
     * [0, sourceSize-1] con 2 boundaryes y seleccion 0. */
    void InitRange(int sourceSize) {
        boundaryCount = 2;
        boundaries[0] = 0;
        boundaries[1] = sourceSize > 1 ? sourceSize - 1 : 0;
        selected = 0;
    }

    /* Golden addChopAtCursor: boundaries_[count++] = frame. */
    void Append(int frame) {
        boundaries[boundaryCount++] = frame;
    }

    /* Golden sortBoundaries (bubble identico, mismas comparaciones). */
    void Sort() {
        for (int i = 0; i < boundaryCount - 1; i++) {
            for (int j = i + 1; j < boundaryCount; j++) {
                if (boundaries[j] < boundaries[i]) {
                    int t = boundaries[i];
                    boundaries[i] = boundaries[j];
                    boundaries[j] = t;
                }
            }
        }
    }

    /* Golden findBoundaryIndex (lineal). */
    int Find(int frame) const {
        for (int i = 0; i < boundaryCount; i++)
            if (boundaries[i] == frame) return i;
        return -1;
    }

    /* Golden deleteSelectedChop (parte de estado): desplaza el resto a la
     * izquierda, decrementa y re-inicializa a rango minimo si quedo < 2.
     * El llamador clampa la seleccion despues (ClampSelectedToChops). */
    void RemoveChop(int index, int sourceSize) {
        for (int i = index; i < boundaryCount - 1; i++)
            boundaries[i] = boundaries[i + 1];
        boundaryCount--;
        if (boundaryCount < 2) {
            boundaryCount = 2;
            boundaries[0] = 0;
            boundaries[1] = sourceSize > 0 ? sourceSize - 1 : 0;
        }
    }

    /* Golden clamp de seleccion tras mutaciones:
     *   if (selected > count - 2) selected = count - 2;
     *   if (selected < 0) selected = 0; */
    void ClampSelectedToChops() {
        if (selected > boundaryCount - 2) selected = boundaryCount - 2;
        if (selected < 0) selected = 0;
    }

    /* Golden clearAllChops (parte de estado): rango minimo + cero del
     * resto del array. */
    void ClearAll(int sourceSize) {
        boundaryCount = 2;
        boundaries[0] = 0;
        boundaries[1] = sourceSize > 1 ? sourceSize - 1 : 0;
        for (int i = 2; i < kMaxBoundaries; i++) boundaries[i] = 0;
        selected = 0;
    }

    /* Golden selectedChopStartFrame: boundaries[clamp(selected, 0,
     * count-2)] con count<2 -> 0. */
    int StartFrameForSelected() const {
        if (boundaryCount < 2) return 0;
        int idx = ClampInt(selected, 0, boundaryCount - 2);
        return boundaries[idx];
    }

    /* Golden selectedChopEndFrame: boundaries[clamp(selected+1, 1,
     * count-1)] con count<2 -> sourceSize-1 (o 0). */
    int EndFrameForSelected(int sourceSize) const {
        if (boundaryCount < 2) return sourceSize > 0 ? sourceSize - 1 : 0;
        int idx = ClampInt(selected + 1, 1, boundaryCount - 1);
        return boundaries[idx];
    }

    /* Golden splitSampleIntoEqualParts (parte de estado): partes 2..32
     * (fuera de rango -> 4), step = sourceSize/parts, cierre con
     * last = sourceSize-1 (o override del ultimo si el array esta lleno),
     * sort y seleccion 0. */
    void SplitIntoEqualParts(int parts, int sourceSize) {
        if (parts < 2 || parts > 32) parts = 4;
        int step = sourceSize / parts;
        boundaryCount = 0;
        for (int i = 0; i < parts; i++) {
            if (boundaryCount >= kMaxBoundaries) break;
            boundaries[boundaryCount++] = i * step;
        }
        int last = sourceSize - 1;
        if (boundaryCount == 0 || boundaries[boundaryCount - 1] != last) {
            if (boundaryCount < kMaxBoundaries)
                boundaries[boundaryCount++] = last;
            else
                boundaries[boundaryCount - 1] = last;
        }
        Sort();
        selected = 0;
    }
};

#endif  /* APPLICATION_VIEWS_MODALDIALOGS_CHOPMODEL_H_ */