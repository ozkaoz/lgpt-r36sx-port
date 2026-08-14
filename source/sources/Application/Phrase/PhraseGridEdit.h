#ifndef _PHRASE_GRID_EDIT_H_
#define _PHRASE_GRID_EDIT_H_

// F3-5a (docs/F3_ARCHITECTURE_ES.md): capa pura de la logica de grid/edicion
// de la Phrase.  Declara la geometria de columnas (posiciones, offsets de
// stepping), la matematica golden del paso de valores de las columnas de
// datos N/V/P/I (limites, wrap, scale-snap, encoding de pitch y reglas de
// auto-fill), las reglas de pasteLast por columna, y la logica de seleccion/
// portapapeles/interpolacion.  No depende de GUI, audio, Player, del estado
// de navegacion de la vista, de Song ni del framebuffer: solo Types.h,
// Phrase.h (encoding de pitch), Scale.h (scaleSteps) y CommandList.h
// (I_CMD_NONE).
// Todo el comportamiento es byte-identico al que vivia en PhraseView.cpp
// (golden Bacon 1.2.1).
#include "Foundation/Types/Types.h"
#include "Application/Model/Phrase.h"
#include "Application/Model/Scale.h"
#include "Application/Instruments/CommandList.h"

// Replica golden de View::updateData (clamp/wrap con offset).
inline int PhraseClampWrap(int value, int offset, int limit, bool wrap) {
    if (value == 0xFF) value = 0;  // Uninitialized data
    value += offset;
    if (value < 0)
        value = (wrap ? (limit + 1 + value) : 0);
    if (value > limit)
        value = (wrap ? value - (limit + 1) : limit);
    return value;
}

// Column layout (8 columns): row# | N | V | P | I | FX1 | P1 | FX2 | P2
// TREEFROG_PHRASE_COLUMNS_V1: FX3 column removed (user decision, H38.5).
// TREEFROG_PHRASE_PITCH_COLUMN_V1 (H38.7): dedicated pitch column (P) between
// volume and instrument. X positions in character cells (each cell = 8px).
static const int kPhraseColCount = 8;

// Columnas de datos (edicion con paso de valor) y de comando.
static const int kPhraseColNote = 0;
static const int kPhraseColVol = 1;
static const int kPhraseColPitch = 2;
static const int kPhraseColInstr = 3;
static const int kPhraseColCmd1 = 4;
static const int kPhraseColParam1 = 5;
static const int kPhraseColCmd2 = 6;
static const int kPhraseColParam2 = 7;

// Limites golden por columna de datos (updateCursorValue):
//   nota: 0..119 wrap; vol: 0..0x64 (0xFF = empty) sin wrap;
//   pitch: -24..+24 (stored encoding) sin wrap;
//   instr: 0..MAX_INSTRUMENT_COUNT-1 (0x80 sample + 0x10 midi = 144) wrap.
static const uchar kPhraseNoteLimit = 119;
static const uchar kPhraseVolLimit = 0x64;
static const int kPhrasePitchLimit = 24;
static const int kPhraseInstrLimit =
    (0x80 + 0x10) - 1; // MAX_INSTRUMENT_COUNT-1 (Song.h)

// TREEFROG_PHRASE_VOL_V3: row volume is 0..100 (0x64), 0xFF = empty.
// 100 = full scale (100%), linear mapping.
static const uchar kPhraseVolFull = 0x64;

// Offsets para note(0), volume(1), pitch(2) y instrument(3) value stepping:
// L, R, U, D.  TREEFROG_PHRASE_VOL_EDIT_V1: volume steps by 1 in every
// direction (A+UP/DOWN steps by 10 inside StepCell).  Pitch follows the
// same scheme.
static const short kPhraseStepOffsets[4][4] = {{-1, 1, 12, -12},
                                               {-1, 1, 1, -1},
                                               {-1, 1, 1, -1},
                                               {-1, 1, 16, -16}};

// Limite golden por columna de datos (0..3).
inline uchar PhraseLimitFor(int col);
// Wrap golden por columna de datos (0..3): nota e instrumento envuelven.
inline bool PhraseWrapFor(int col);

// Direccion de un paso de edicion (mismo orden que kPhraseStepOffsets).
enum PhraseEditDirection {
    PED_LEFT = 0,
    PED_RIGHT = 1,
    PED_UP = 2,
    PED_DOWN = 3
};

// Replica golden de updateCursorValue (datos, col 0..3): aplica el paso de
// valor sobre la celda apuntada por cell, con scale-snap para la columna de
// nota y reglas de auto-fill sobre vol/pitch cuando una nota vacia (0xFF)
// pasa a tener valor.  Devuelve los flags de auto-fill.
struct PhraseCellStepResult {
    bool fillVol;    // vol debe pasar de 0xFF a kPhraseVolFull
    bool fillPitch;  // pitch debe pasar de PITCH_STORED_NONE a ZERO
    bool dirtied;    // la celda (o su auto-fill) cambiaron
};

inline PhraseCellStepResult PhraseStepCell(int col, uchar *cell, uchar *vol,
                                           uchar *pitch, int scale,
                                           PhraseEditDirection dir,
                                           bool bigStep) {
    PhraseCellStepResult r = {false, false, true};
    if (col == kPhraseColPitch) {
        // TREEFROG_PHRASE_PITCH_COLUMN_V2 (H38.7): the pitch column stores
        // -24..+24 semitones with an offset so that -1 (0xFF) does not
        // collide with the "no pitch" marker. 0x00 = none, values 0x28..0x58
        // map to -24..+24. Steps by 1 per direction, A+UP/DOWN by 10.
        int p = phrasePitchStoredToInt(*cell);
        int step = (dir == PED_UP || dir == PED_DOWN) && bigStep ? 10 : 1;
        int offset = 0;
        switch (dir) {
        case PED_LEFT:
            offset = -1;
            break;
        case PED_RIGHT:
            offset = 1;
            break;
        case PED_UP:
            offset = step;
            break;
        case PED_DOWN:
            offset = -step;
            break;
        }
        p += offset;
        if (p < -kPhrasePitchLimit) p = -kPhrasePitchLimit;
        if (p > kPhrasePitchLimit) p = kPhrasePitchLimit;
        *cell = phrasePitchIntToStored(p);
        return r;
    }
    bool noteWasEmpty = (col == kPhraseColNote) && (*cell == 0xFF);
    if ((col == kPhraseColVol) && (*cell == 0xFF)) {
        *cell = kPhraseVolFull;
        r.dirtied = true;
    }
    int offset =
        kPhraseStepOffsets[col == kPhraseColInstr ? 3
                                                  : (col == kPhraseColVol ? 1 : 0)][dir];
    if (col == kPhraseColVol && (dir == PED_UP || dir == PED_DOWN)) {
        int step = bigStep ? 10 : 1;
        offset = (dir == PED_UP) ? step : -step;
    }
    // If note column apply the selected musical scale only for normal,
    // non-chopped instruments.  NOTE: el modulo se normaliza a
    // [0..11] (el golden original hacia `% 12` sin normalizar y
    // leia scaleSteps con indice negativo - UB - al bajar octava desde
    // notas bajas o al alejarse del borde inferior; el resultado
    // definido es identico para todo caso bien definido).
    if (col == kPhraseColNote) {
        int snapIndex = (*cell + offset) % 12;
        if (snapIndex < 0) snapIndex += 12;
        while (!scaleSteps[scale][snapIndex]) {
            offset > 0 ? offset++ : offset--;
            snapIndex = (*cell + offset) % 12;
            if (snapIndex < 0) snapIndex += 12;
        }
    }
    *cell = (uchar)PhraseClampWrap(*cell, offset, PhraseLimitFor(col),
                                   PhraseWrapFor(col));
    if (noteWasEmpty && (*cell != 0xFF)) {
        if (vol && (*vol == 0xFF)) {
            *vol = kPhraseVolFull;
            r.fillVol = true;
        }
        if (pitch && (*pitch == PITCH_STORED_NONE)) {
            *pitch = PITCH_STORED_ZERO;
            r.fillPitch = true;
        }
    }
    return r;
}

// Limite golden por columna de datos (0..3).
inline uchar PhraseLimitFor(int col) {
    switch (col) {
    case kPhraseColNote:
        return kPhraseNoteLimit;
    case kPhraseColVol:
        return kPhraseVolLimit;
    case kPhraseColPitch:
        return (uchar)kPhrasePitchLimit;
    default:
        return (uchar)kPhraseInstrLimit;
    }
}

// Wrap golden por columna de datos (0..3): nota e instrumento envuelven.
inline bool PhraseWrapFor(int col) {
    return (col == kPhraseColNote) || (col == kPhraseColInstr);
}

// Regla golden de pasteLast para una celda de datos: si la celda esta vacia
// la rellena con el ultimo valor editado (devuelve true = dirty); si ya
// tiene valor, captura el actual como "ultimo" (devuelve false).  lastValue
// apunta al int del ultimo valor segun columna.
inline bool PhrasePasteLast(int col, uchar *cell, int *lastValue) {
    switch (col) {
    case kPhraseColNote:
        if (*cell == 0xFF) {
            *cell = (uchar)*lastValue;
            return true;
        }
        *lastValue = *cell;
        return false;
    case kPhraseColVol:
        if (*cell == 0xFF) {
            *cell = (uchar)*lastValue;
            return true;
        }
        *lastValue = *cell;
        return false;
    case kPhraseColPitch:
        if (*cell == PITCH_STORED_NONE) {
            *cell = phrasePitchIntToStored(*lastValue);
            return true;
        }
        *lastValue = phrasePitchStoredToInt(*cell);
        return false;
    default:  // instrument
        if (*cell == 0xFF) {
            *cell = (uchar)*lastValue;
            return true;
        }
        *lastValue = *cell;
        return false;
    }
}

// Regla golden de pasteLast para una celda de comando (col 4/6):
// I_CMD_NONE se rellena con el ultimo comando; si no, se captura.
inline bool PhrasePasteLastCommand(FourCC *cell, int *lastValue) {
    if (*cell == I_CMD_NONE) {
        *cell = (FourCC)*lastValue;
        return true;
    }
    *lastValue = (int)*cell;
    return false;
}

// --- Seleccion / portapapeles (golden PhraseView.cpp) ---

// Rectangulo normalizado de una seleccion.
struct PhraseRect {
    int left, top, right, bottom;
};

inline PhraseRect PhraseNormalizeRect(int col1, int row1, int col2, int row2) {
    PhraseRect r;
    r.left = (col1 < col2) ? col1 : col2;
    r.top = (row1 < row2) ? row1 : row2;
    r.right = (col1 < col2) ? col2 : col1;
    r.bottom = (row1 < row2) ? row2 : row1;
    return r;
}

// Replica golden de extendSelection: si la seleccion no cubre ambas orlas de
// columnas la expande en columnas (0..7); si ya las cubre, en filas.
// Navegar hacia la orla opuesta del ancla extiende hasta el borde.
struct PhraseSelectionState {
    int col, row;          // posicion del cursor
    int clipCol, clipRow;  // ancla del portapapeles
};

inline void PhraseExtendSelection(PhraseSelectionState &s) {
    PhraseRect rect = PhraseNormalizeRect(s.clipCol, s.clipRow, s.col, s.row);
    if (rect.left > 0 || rect.right < (kPhraseColCount - 1)) {
        if (s.col < s.clipCol) {
            s.col = 0;
            s.clipCol = kPhraseColCount - 1;
        } else {
            s.col = kPhraseColCount - 1;
            s.clipCol = 0;
        }
    } else {
        if (s.row < s.clipRow) {
            s.row = 0;
            s.clipRow = 15;
        } else {
            s.clipRow = 0;
            s.row = 15;
        }
    }
}

// Portapapeles golden (mismo layout que el struct clipboard de la vista).
struct PhraseClipboard {
    bool active_;
    int col_;
    int row_;
    int width_;
    int height_;
    uchar note_[16];
    uchar instr_[16];
    uchar vol_[16];
    uchar pitch_[16];
    uint cmd1_[16];
    ushort param1_[16];
    uint cmd2_[16];
    ushort param2_[16];
    uint cmd3_[16];
    ushort param3_[16];
};

// Replica golden de fillClipboardData: calcula el rect normalizado desde el
// ancla del portapapeles hasta el cursor, y copia ese rango de las 8
// columnas de la frase actual al portapapeles.
inline void PhraseFillClipboard(PhraseClipboard &cb, int cursorCol,
                                int cursorRow, const uchar *note,
                                const uchar *vol, const uchar *pitch,
                                const uchar *instr, const uint *cmd1,
                                const ushort *param1, const uint *cmd2,
                                const ushort *param2) {
    PhraseRect selRect =
        PhraseNormalizeRect(cb.col_, cb.row_, cursorCol, cursorRow);
    cb.width_ = selRect.right - selRect.left + 1;
    cb.height_ = selRect.bottom - selRect.top + 1;
    cb.row_ = selRect.top;
    cb.col_ = selRect.left;
    for (int i = 0; i < cb.height_; i++) {
        cb.note_[i] = note[cb.row_ + i];
        cb.vol_[i] = vol[cb.row_ + i];
        cb.pitch_[i] = pitch[cb.row_ + i];
        cb.instr_[i] = instr[cb.row_ + i];
        cb.cmd1_[i] = cmd1[cb.row_ + i];
        cb.param1_[i] = param1[cb.row_ + i];
        cb.cmd2_[i] = cmd2[cb.row_ + i];
        cb.param2_[i] = param2[cb.row_ + i];
    }
}

// Replica golden del bucle de cutSelection: limpia celdas dentro del
// rectangulo seleccionado (note/vol/instr=0xFF, pitch=PITCH_STORED_NONE,
// cmd=I_CMD_NONE, param=0).
inline void PhraseCutSelectionCells(PhraseClipboard &cb, uchar *note,
                                    uchar *vol, uchar *pitch, uchar *instr,
                                    uint *cmd1, ushort *param1, uint *cmd2,
                                    ushort *param2) {
    for (int i = 0; i < cb.width_; i++) {
        for (int j = 0; j < cb.height_; j++) {
            switch (i + cb.col_) {
            case kPhraseColNote:
                note[j + cb.row_] = 0xFF;
                break;
            case kPhraseColVol:
                vol[j + cb.row_] = 0xFF;
                break;
            case kPhraseColPitch:
                pitch[j + cb.row_] = PITCH_STORED_NONE;
                break;
            case kPhraseColInstr:
                instr[j + cb.row_] = 0xFF;
                break;
            case kPhraseColCmd1:
                cmd1[j + cb.row_] = I_CMD_NONE;
                break;
            case kPhraseColParam1:
                param1[j + cb.row_] = 0x0000;
                break;
            case kPhraseColCmd2:
                cmd2[j + cb.row_] = I_CMD_NONE;
                break;
            case kPhraseColParam2:
                param2[j + cb.row_] = 0x0000;
                break;
            }
        }
    }
}

// Replica golden de pasteClipboard: copia el portapapeles a la posicion del
// cursor.  Devuelve true si se escribio alguna celda.
inline bool PhrasePasteClipboard(PhraseClipboard &cb, uchar *note, uchar *vol,
                                 uchar *pitch, uchar *instr, uint *cmd1,
                                 ushort *param1, uint *cmd2, ushort *param2,
                                 int destCol, int destRow) {
    int height = cb.height_;
    uint *noCmd = (uint *)-1;
    ushort *noPrm = (ushort *)-1;
    uint *srcCmd[8] = {noCmd, noCmd, noCmd, noCmd, cb.cmd1_, noCmd, cb.cmd2_,
                       noCmd};
    ushort *srcPrm[8] = {noPrm, noPrm, noPrm, noPrm, noPrm, cb.param1_, noPrm,
                         cb.param2_};
    uint *dstCmd[8] = {noCmd, noCmd, noCmd, noCmd, cmd1, noCmd, cmd2, noCmd};
    ushort *dstPrm[8] = {noPrm, noPrm, noPrm, noPrm, noPrm, param1, noPrm,
                         param2};

    bool wasUpdated = false;

    for (int i = 0; i < cb.width_; i++) {
        for (int j = 0; j < height; j++) {
            int pasteCol = destCol + i;
            switch (i + cb.col_) {
            case kPhraseColNote:
                note[(j + destRow) % 16] = cb.note_[j];
                wasUpdated = true;
                break;
            case kPhraseColVol:
                vol[(j + destRow) % 16] = cb.vol_[j];
                wasUpdated = true;
                break;
            case kPhraseColPitch:
                pitch[(j + destRow) % 16] = cb.pitch_[j];
                wasUpdated = true;
                break;
            case kPhraseColInstr:
                instr[(j + destRow) % 16] = cb.instr_[j];
                wasUpdated = true;
                break;
            case kPhraseColCmd1:
            case kPhraseColCmd2:
                if (pasteCol == kPhraseColCmd1 || pasteCol == kPhraseColCmd2) {
                    // Don't allow commands in notes, etc
                    dstCmd[pasteCol][(destRow + j) % 16] =
                        srcCmd[cb.col_ + i][j];
                    wasUpdated = true;
                }
                break;
            case kPhraseColParam1:
            case kPhraseColParam2:
                if (pasteCol == kPhraseColParam1 ||
                    pasteCol == kPhraseColParam2) {
                    dstPrm[pasteCol][(destRow + j) % 16] =
                        srcPrm[cb.col_ + i][j];
                    wasUpdated = true;
                }
                break;
            }
        }
    }
    return wasUpdated;
}

// Estados de interpolateSelection (replica golden de los retornos).
enum PhraseInterpResult {
    PINTERP_OK = 0,
    PINTERP_SKIPPED = 1,      // no activa, columna invalida o 1 sola fila
    PINTERP_NO_NOTE_INFO = 2  // 0xFF en algun extremo de la columna de nota
};

// Replica golden de interpolateSelection: columnas 0 (nota), 2 (pitch) y
// 5/7 (parametros).  Nota: lineal sin clamp, 0xFF en extremos aborta con
// PINTERP_NO_NOTE_INFO.  Pitch: semitonos con clamp -24..+24 (stored).
// Params: lineal sin clamp.
inline PhraseInterpResult PhraseInterpolateSelection(
    PhraseClipboard &cb, int cursorCol, int cursorRow, uchar *note,
    uchar *pitch, ushort *param1, ushort *param2) {
    if (!cb.active_) {
        return PINTERP_SKIPPED;
    }
    PhraseRect rect =
        PhraseNormalizeRect(cb.col_, cb.row_, cursorCol, cursorRow);
    // Only interpolate if we're in note (0), pitch (2) or param (5, 7)
    // columns
    int col = rect.left;
    if (col != rect.right ||
        (col != kPhraseColNote && col != kPhraseColPitch &&
         col != kPhraseColParam1 && col != kPhraseColParam2)) {
        return PINTERP_SKIPPED;
    }
    int startRow = rect.top;
    int endRow = rect.bottom;
    // Need at least 2 rows to interpolate
    if (endRow - startRow < 1) {
        return PINTERP_SKIPPED;
    }
    if (col == kPhraseColNote) {
        uchar startNote = note[startRow];
        uchar endNote = note[endRow];
        if (startNote == 0xFF || endNote == 0xFF) {
            return PINTERP_NO_NOTE_INFO;
        }
        int numSteps = endRow - startRow;
        int noteDiff = (int)endNote - (int)startNote;
        for (int step = 0; step <= numSteps; step++) {
            int row = startRow + step;
            int value = startNote + (noteDiff * step) / (numSteps);
            note[row] = (uchar)value;
        }
    } else if (col == kPhraseColPitch) {
        int startPitch = phrasePitchStoredToInt(pitch[startRow]);
        int endPitch = phrasePitchStoredToInt(pitch[endRow]);
        int numSteps = endRow - startRow;
        int pitchDiff = endPitch - startPitch;
        for (int step = 0; step <= numSteps; step++) {
            int row = startRow + step;
            int value = startPitch + (pitchDiff * step) / (numSteps);
            if (value < -kPhrasePitchLimit) value = -kPhrasePitchLimit;
            if (value > kPhrasePitchLimit) value = kPhrasePitchLimit;
            pitch[row] = phrasePitchIntToStored(value);
        }
    } else {
        // Parameter columns (5 or 7)
        ushort *paramData =
            (col == kPhraseColParam1) ? param1 : param2;
        ushort startParam = paramData[startRow];
        ushort endParam = paramData[endRow];
        int numSteps = endRow - startRow;
        int paramDiff = (int)endParam - (int)startParam;
        for (int step = 0; step <= numSteps; step++) {
            int row = startRow + step;
            int value = startParam + (paramDiff * step) / (numSteps);
            paramData[row] = (ushort)value;
        }
    }
    return PINTERP_OK;
}

#endif