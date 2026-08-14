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
#define LGPT_CHOPPER_SCREEN_W (40)
#define LGPT_CHOPPER_SCREEN_H (30)

/* F3-3b: colores de celda del golden (mapeados a CD_* por el dueno al
   drenar la grilla; la capa pura no referencia tipos de la UI). */
enum ChopperCellColor {
    CHOP_COLOR_NORMAL = 0,
    CHOP_COLOR_HILITE1,
    CHOP_COLOR_HILITE2,
    CHOP_COLOR_BORDER
};

/* F3-3b: grilla de texto 40x30 (celdas del golden: char, invert, color). */
struct ChopperGrid {
    char cell[LGPT_CHOPPER_SCREEN_H][LGPT_CHOPPER_SCREEN_W];
    bool invert[LGPT_CHOPPER_SCREEN_H][LGPT_CHOPPER_SCREEN_W];
    ChopperCellColor color[LGPT_CHOPPER_SCREEN_H][LGPT_CHOPPER_SCREEN_W];

    void Clear() {
        for (int y = 0; y < LGPT_CHOPPER_SCREEN_H; y++)
            for (int x = 0; x < LGPT_CHOPPER_SCREEN_W; x++) {
                cell[y][x] = ' ';
                invert[y][x] = false;
                color[y][x] = CHOP_COLOR_NORMAL;
            }
    }

    /* Escribe texto con recorte a los 40 chars; las celdas fuera de rango
       se descartan (el golden las recorta en el borde de pantalla). */
    void SetText(int x, int y, const char *text, ChopperCellColor col,
                 bool inv) {
        if (y < 0 || y >= LGPT_CHOPPER_SCREEN_H) return;
        for (int i = 0; text && text[i] && (x + i) < LGPT_CHOPPER_SCREEN_W;
             i++) {
            if (x + i < 0) continue;
            cell[y][x + i] = text[i];
            invert[y][x + i] = inv;
            color[y][x + i] = col;
        }
    }

    void SetInvert(int x, int y, ChopperCellColor col) {
        if (y < 0 || y >= LGPT_CHOPPER_SCREEN_H) return;
        if (x < 0 || x >= LGPT_CHOPPER_SCREEN_W) return;
        invert[y][x] = true;
        color[y][x] = col;
    }
};

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

/* ============================ F3-3b: dibujo ========================== */
    /* Golden drawTopBar: fila 0 completa (invert, CD_HILITE1). */
    static void DrawTopBar(ChopperGrid &g) {
        for (int x = 0; x < LGPT_CHOPPER_SCREEN_W; x++)
            g.SetInvert(x, 0, CHOP_COLOR_HILITE1);
        g.SetText(0, 0, " P G  SCPI  M TT       CHOPPER       ",
                  CHOP_COLOR_HILITE1, true);
    }

    /* Golden drawFrame (RC4 P6): borde de celdas solidas filas 1/22,
       columnas 0/39 filas 2..21 (CD_BORDER invert) y titulo (2,2)
       CD_HILITE2. */
    static void DrawFrame(ChopperGrid &g) {
        for (int x = 0; x < LGPT_CHOPPER_SCREEN_W; x++) {
            g.SetInvert(x, 1, CHOP_COLOR_BORDER);
            g.SetInvert(x, 22, CHOP_COLOR_BORDER);
        }
        for (int y = 2; y < 22; y++) {
            g.SetInvert(0, y, CHOP_COLOR_BORDER);
            g.SetInvert(39, y, CHOP_COLOR_BORDER);
        }
        g.SetText(2, 2, "Graphical Chopper", CHOP_COLOR_HILITE2, false);
    }

    /* Golden drawEmptyWaveformText: placeholder 40 chars en (2,13)
       CD_HILITE1 (recorte incluido). */
    static void DrawEmptyWaveformText(ChopperGrid &g) {
        g.SetText(2, 13, "            no sample loaded            ",
                  CHOP_COLOR_HILITE1, false);
    }

    /* Golden drawControls: fila 24, CD_NORMAL; trim mode swap. */
    static void DrawControls(ChopperGrid &g, bool trimMode) {
        g.SetText(0, 24,
                  trimMode ? "R1+A Keep  L2+Y Del  A+B Nudge  R1+B Back"
                           : "Select: Crop | L1+R1: Pitch | R1+B: Back",
                  CHOP_COLOR_NORMAL, false);
    }

    /* Golden drawPitchScreen hint lines (CD_NORMAL). */
    static void DrawPitchHints(ChopperGrid &g) {
        g.SetText(1, 24, "UP/DN Item | L/R Value | B Preview",
                  CHOP_COLOR_NORMAL, false);
        g.SetText(1, 25, "A Apply | L1+R1 Exit | R2+LR Target",
                  CHOP_COLOR_NORMAL, false);
    }

    /* Golden header pitch: "I%02X S%02X C%02d/%02d". */
    static int ComposeHeaderLine(char *buf, int bufLen, int instrumentIndex,
                                 int sampleIndex, int selectedChop,
                                 int boundaryCount) {
        return snprintf(buf, bufLen, "I%02X S%02X C%02d/%02d",
                        instrumentIndex, sampleIndex, selectedChop,
                        boundaryCount);
    }

    /* Golden labels[6] del pitch screen. */
    static const char *PitchLabel(int param) {
        static const char *labels[6] = {"Pitch", "Attack", "Sustain",
                                        "Release", "Scope", "Sample"};
        if (param < 0 || param > 5) param = 5;
        return labels[param];
    }

    /* Golden value[16] del pitch screen (mismos formatos snprintf). */
    static int ComposePitchValue(char *buf, int bufLen, int param,
                                 int semitones, int attackMs,
                                 int sustainPercent, int releaseMs,
                                 bool scope, int pitchSampleIndex) {
        switch (param) {
        case 0: return snprintf(buf, bufLen, "%+3d st", semitones);
        case 1: return snprintf(buf, bufLen, "%4d ms", attackMs);
        case 2: return snprintf(buf, bufLen, "%3d %%", sustainPercent);
        case 3: return snprintf(buf, bufLen, "%4d ms", releaseMs);
        case 4: return snprintf(buf, bufLen, "%s", scope ? "Chop" : "Sample");
        default: return snprintf(buf, bufLen, "%02X", pitchSampleIndex);
        }
    }

    /* Golden fila de info de sample (drawSampleInfo): "Inst:%02X Smpl:%02X
       Zoom:%03d%%". */
    static int ComposeSampleInfoLine(char *buf, int bufLen,
                                     int instrumentIndex, int sampleIndex,
                                     int zoomPercent) {
        return snprintf(buf, bufLen, "Inst:%02X Smpl:%02X Zoom:%03d%%",
                        instrumentIndex, sampleIndex < 0 ? 0 : sampleIndex,
                        zoomPercent);
    }

    /* Golden fila de nombre (drawSampleInfo): "Name:%s" (el dueno recorta
       el nombre a 31 y la linea a 37, como el golden). */
    static int ComposeNameLine(char *buf, int bufLen, const char *name) {
        return snprintf(buf, bufLen, "Name:%s", name ? name : "");
    }

    /* Golden fila de frame (drawSampleInfo): "Frame:%d/%d Chop:%02d/%02d%s"
       (trailing " ADJ" en trim). */
    static int ComposeFrameLine(char *buf, int bufLen, int cursorFrame,
                                int maxFrame, int chopIndex, int chopCount,
                                bool trimMode) {
        return snprintf(buf, bufLen, "Frame:%d/%d Chop:%02d/%02d%s",
                        cursorFrame, maxFrame, chopIndex, chopCount,
                        trimMode ? " ADJ" : "");
    }

    /* Golden status del progress (showOperationProgress): OK o porcentaje. */
    static int ComposeOperationStatus(char *buf, int bufLen,
                                      const char *comboLabel,
                                      const char *message, int percent) {
        if (percent >= 100)
            return snprintf(buf, bufLen, "%s %s OK A/L1+X/R1+X",
                            comboLabel ? comboLabel : "",
                            message ? message : "");
        return snprintf(buf, bufLen, "%s %s %d%%",
                        comboLabel ? comboLabel : "",
                        message ? message : "", percent);
    }

    /* Golden fila de porcentaje del overlay (drawOperationOverlay):
       "%3d%%". */
    static int ComposeOperationPercent(char *buf, int bufLen, int percent) {
        return snprintf(buf, bufLen, "%3d%%", percent);
    }

private:
    static int ClampInt(int v, int lo, int hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
};

#endif