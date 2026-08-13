#ifndef _CHOPPER_VIEW_H_
#define _CHOPPER_VIEW_H_

/* F3-3a (docs/F3_ARCHITECTURE_ES.md): geometria de scroll/zoom/cursor y
 * waveform del Chopper extraidas del SampleChopperModal como capa pura
 * (header-only).  Los nombres/constantes del golden: WAVE_W=288,
 * MIN_ZOOM_PERCENT=5, MAX_ZOOM_PERCENT=100 (enum del modal).
 *
 * Regla de oro: replicas EXACTAS de bacon 1.2.1 (mismos divisores, mismos
 * clamps, mismos long long).  El dibujo (DrawString/SetColor/tf_rect) y
 * el estado mutable del modal quedan en el dueno; aqui solo matematicas
 * enteras puras verificables en host.
 */

#define LGPT_CHOPPER_WAVE_W (288)
#define LGPT_CHOPPER_MIN_ZOOM_PERCENT (5)
#define LGPT_CHOPPER_MAX_ZOOM_PERCENT (100)
#define LGPT_CHOPPER_MAX_COLUMNS (288)

class ChopperView {
public:
    /* Golden getZoomFactor (zoom/5, minimo 1). */
    static int GetZoomFactor(int zoomPercent) {
        int z = zoomPercent / 5;
        if (z < 1) z = 1;
        return z;
    }

    /* Golden getViewFrameCount (size/zoom, minimo WAVE_W, tope size). */
    static int GetViewFrameCount(int sourceSize, int zoomPercent) {
        if (sourceSize <= 0) return 0;
        int frames = sourceSize / GetZoomFactor(zoomPercent);
        if (frames < LGPT_CHOPPER_WAVE_W) frames = LGPT_CHOPPER_WAVE_W;
        if (frames > sourceSize) frames = sourceSize;
        return frames;
    }

    /* Golden clampViewStart. */
    static int ClampViewStart(int viewStart, int sourceSize, int zoomPercent) {
        if (sourceSize <= 0) return 0;
        int viewFrames = GetViewFrameCount(sourceSize, zoomPercent);
        int maxStart = sourceSize - viewFrames;
        if (maxStart < 0) maxStart = 0;
        return ClampInt(viewStart, 0, maxStart);
    }

    /* Golden centerViewOnCursor (cursor - frames/2, luego clamp). */
    static int CenterOnCursor(int cursorFrame, int sourceSize,
                              int zoomPercent) {
        if (sourceSize <= 0) return 0;
        int viewFrames = GetViewFrameCount(sourceSize, zoomPercent);
        return ClampViewStart(cursorFrame - viewFrames / 2, sourceSize,
                              zoomPercent);
    }

    /* Golden ensureCursorVisible (entra a la vista o se alinea, luego
       clamp). */
    static int EnsureCursorVisible(int viewStart, int cursorFrame,
                                   int sourceSize, int zoomPercent) {
        if (sourceSize <= 0) return viewStart;
        int viewFrames = GetViewFrameCount(sourceSize, zoomPercent);
        if (cursorFrame < viewStart) viewStart = cursorFrame;
        if (cursorFrame >= viewStart + viewFrames)
            viewStart = cursorFrame - viewFrames + 1;
        return ClampViewStart(viewStart, sourceSize, zoomPercent);
    }

    /* Golden frameToPixel (-1 fuera de la ventana; escala long long). */
    static int FrameToPixel(int frame, int viewStart, int sourceSize,
                            int zoomPercent) {
        int viewFrames = GetViewFrameCount(sourceSize, zoomPercent);
        if (viewFrames <= 1) return -1;
        if (frame < viewStart) return -1;
        if (frame > viewStart + viewFrames - 1) return -1;
        long long rel =
            (long long)(frame - viewStart) *
            (long long)(LGPT_CHOPPER_WAVE_W - 1);
        rel /= (long long)(viewFrames - 1);
        return ClampInt((int)rel, 0, LGPT_CHOPPER_WAVE_W - 1);
    }

    /* Golden pixelToFrame (clamp px, escala long long, clamp al sample). */
    static int PixelToFrame(int px, int viewStart, int sourceSize,
                            int zoomPercent) {
        if (sourceSize <= 0) return 0;
        int viewFrames = GetViewFrameCount(sourceSize, zoomPercent);
        if (viewFrames <= 1) return viewStart;
        px = ClampInt(px, 0, LGPT_CHOPPER_WAVE_W - 1);
        long long frame =
            (long long)viewStart +
            ((long long)px * (long long)(viewFrames - 1)) /
                (long long)(LGPT_CHOPPER_WAVE_W - 1);
        if (frame < 0) frame = 0;
        if (frame >= sourceSize) frame = sourceSize - 1;
        return (int)frame;
    }

    /* Golden nudgeCursorPixels (paso = frames/WAVE_W, minimo 1; clamp
       entero).  El dueno conserva el guard sourceSize<=0 del golden. */
    static int NudgeCursorPixels(int cursorFrame, int deltaPx,
                                 int sourceSize, int zoomPercent) {
        int viewFrames = GetViewFrameCount(sourceSize, zoomPercent);
        int step = viewFrames / LGPT_CHOPPER_WAVE_W;
        if (step < 1) step = 1;
        long long deltaFrames = (long long)step * (long long)deltaPx;
        long long next = (long long)cursorFrame + deltaFrames;
        if (next < 0) next = 0;
        if (next >= sourceSize) next = sourceSize - 1;
        return (int)next;
    }

    /* Golden nudgeZoomPercent: clamp a [minZoom, maxZoom]; el dueno
       conserva la rama de status/center (misma logica). */
    static int NudgeZoom(int zoomPercent, int deltaPercent, int minZoom,
                         int maxZoom) {
        return ClampInt(zoomPercent + deltaPercent, minZoom, maxZoom);
    }

    /* Golden prepareWaveformPreview: min/max por columna sobre el canal 0.
       Devuelve true si pudo construir las columnas. */
    static bool BuildWaveformColumns(const short *samples, int size,
                                     int channels, int viewStart,
                                     int viewFrames, int cols, int *minColumn,
                                     int *maxColumn) {
        if (!samples || size <= 0 || channels <= 0 || cols <= 0)
            return false;
        for (int col = 0; col < cols; col++) {
            int start = viewStart + (col * viewFrames) / cols;
            int end = viewStart + ((col + 1) * viewFrames) / cols;
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
        return true;
    }

private:
    static int ClampInt(int v, int lo, int hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
};

#endif