// F3-5a: PhraseGridEdit.h - oraculos golden de la logica de grid/edicion de
// la Phrase (pasos de valor N/V/P/I con scale-snap, pasteLast, seleccion,
// portapapeles e interpolacion).  Comportamiento byte-identico al que vivia
// en PhraseView.cpp (golden Bacon 1.2.1).
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Application/Phrase/PhraseGridEdit.h"
#include "Application/Model/Scale.cpp"

static int g_failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            g_failures++;                                              \
        }                                                              \
    } while (0)

static void test_layout_constants() {
    CHECK(kPhraseColCount == 8);
    // TREEFROG_PHRASE_COLUMNS_V1 golden: 8 columas; kColX/kColHeaderX
    // (posiciones exactas de las celdas del grid centrado 5..35).
    // kColX golden {5, 8, 11, 14, 19, 23, 27, 31}:
    int kColX[8] = {5, 8, 11, 14, 19, 23, 27, 31};
    // kColHeaderX golden {6, 9, 12, 15, 20, 24, 28, 32}:
    int kColHeaderX[8] = {6, 9, 12, 15, 20, 24, 28, 32};
    (void)kColX;
    (void)kColHeaderX;
    // Limites golden:
    CHECK(kPhraseNoteLimit == 119);
    CHECK(kPhraseVolLimit == 0x64);
    CHECK(kPhrasePitchLimit == 24);
    CHECK(kPhraseInstrLimit == 0x8F); // MAX_INSTRUMENT_COUNT-1 = 0x90-1
    // Offsets golden L,R,U,D:
    CHECK(kPhraseStepOffsets[0][0] == -1 && kPhraseStepOffsets[0][1] == 1 &&
          kPhraseStepOffsets[0][2] == 12 && kPhraseStepOffsets[0][3] == -12);
    CHECK(kPhraseStepOffsets[1][0] == -1 && kPhraseStepOffsets[1][1] == 1 &&
          kPhraseStepOffsets[1][2] == 1 && kPhraseStepOffsets[1][3] == -1);
    CHECK(kPhraseStepOffsets[2][0] == -1 && kPhraseStepOffsets[2][1] == 1 &&
          kPhraseStepOffsets[2][2] == 1 && kPhraseStepOffsets[2][3] == -1);
    CHECK(kPhraseStepOffsets[3][0] == -1 && kPhraseStepOffsets[3][1] == 1 &&
          kPhraseStepOffsets[3][2] == 16 && kPhraseStepOffsets[3][3] == -16);
    // Wrap golden: nota e instrumento envuelven; vol y pitch claman.
    CHECK(PhraseWrapFor(kPhraseColNote) == true);
    CHECK(PhraseWrapFor(kPhraseColVol) == false);
    CHECK(PhraseWrapFor(kPhraseColPitch) == false);
    CHECK(PhraseWrapFor(kPhraseColInstr) == true);
    printf("layout constants + limits + offsets OK\n");
}

// Replica golden de View::updateData para el oraculo independiente.
static int oracleClampWrap(int value, int offset, int limit, bool wrap) {
    if (value == 0xFF) value = 0;
    value += offset;
    if (value < 0) value = (wrap ? (limit + 1 + value) : 0);
    if (value > limit) value = (wrap ? value - (limit + 1) : limit);
    return value;
}

static void test_clamp_wrap() {
    // wrap note: 119 + 1 -> 0 ; 0 - 1 -> 119
    CHECK(PhraseClampWrap(119, 1, 119, true) == 0);
    CHECK(PhraseClampWrap(0, -1, 119, true) == 119);
    CHECK(PhraseClampWrap(0xFF, 12, 119, true) == 12); // 0xFF -> 0
    // vol sin wrap: 100 + 1 -> 100 ; 0 - 1 -> 0
    CHECK(PhraseClampWrap(0x64, 1, 0x64, false) == 0x64);
    CHECK(PhraseClampWrap(0, -1, 0x64, false) == 0);
    // instr wrap: 143 + 1 -> 0 ; 0 - 1 -> 143
    CHECK(PhraseClampWrap(143, 1, 143, true) == 0);
    CHECK(PhraseClampWrap(0, -1, 143, true) == 143);
    // equivalencia con el oraculo independiente
    for (int v = 0; v < 256; v++) {
        for (int off = -20; off <= 20; off++) {
            CHECK(PhraseClampWrap(v, off, 119, true) ==
                  oracleClampWrap(v, off, 119, true));
            CHECK(PhraseClampWrap(v, off, 0x64, false) ==
                  oracleClampWrap(v, off, 0x64, false));
            CHECK(PhraseClampWrap(v, off, 143, true) ==
                  oracleClampWrap(v, off, 143, true));
        }
    }
    printf("clamp/wrap OK\n");
}

static void test_step_note_chromatic() {
    // Escala 0 (cromatica): todos los semitonos validos, snap no modifica.
    uchar vol = 0x64, pitch = PITCH_STORED_ZERO;
    // nota 60 (C) + UP -> 72 (una octava)
    {
        uchar note = 60;
        PhraseCellStepResult r =
            PhraseStepCell(kPhraseColNote, &note, &vol, &pitch, 0, PED_UP,
                           false);
        CHECK(note == 72);
        CHECK(r.fillVol == false && r.fillPitch == false);
    }
    // 60 + DOWN -> 48
    {
        uchar note = 60;
        CHECK(PhraseStepCell(kPhraseColNote, &note, &vol, &pitch, 0, PED_DOWN,
                             false).dirtied);
        CHECK(note == 48);
    }
    // 0 + LEFT (wrap) -> 119
    {
        uchar note = 0;
        PhraseStepCell(kPhraseColNote, &note, &vol, &pitch, 0, PED_LEFT,
                       false);
        CHECK(note == 119);
    }
    // 119 + RIGHT (wrap) -> 0
    {
        uchar note = 119;
        PhraseStepCell(kPhraseColNote, &note, &vol, &pitch, 0, PED_RIGHT,
                       false);
        CHECK(note == 0);
    }
    // nota vacia (0xFF) + UP: updateData trata 0xFF como 0 -> 12
    {
        uchar note = 0xFF;
        uchar v = 0xFF, p = PITCH_STORED_NONE;
        PhraseCellStepResult r = PhraseStepCell(kPhraseColNote, &note, &v, &p,
                                                0, PED_UP, false);
        CHECK(note == 12);
        // auto-fill golden: vol 0xFF -> 0x64, pitch NONE -> ZERO
        CHECK(r.fillVol == true && v == kPhraseVolFull);
        CHECK(r.fillPitch == true && p == PITCH_STORED_ZERO);
    }
    printf("step note cromatico + auto-fill OK\n");
}

static void test_step_note_acoustic() {
    // Escala 1 (Acoustic): C C# D D# E F F# G G# A A# B
    // {t,f,t,f,t,f,t,t,f,t,f,f}
    uchar vol = 0x64, pitch = PITCH_STORED_ZERO;
    // 61 (C#) + UP: offset 12 -> (73)%12=1 false -> 13 -> (74)%12=2 true
    // -> nota = 61+13 = 74
    {
        uchar note = 61;
        PhraseStepCell(kPhraseColNote, &note, &vol, &pitch, 1, PED_UP, false);
        CHECK(note == 74);
    }
    // 12 (C) + LEFT: offset -1 -> (11)%12=11 false -> -2 -> (10)%12=10 false
    // -> -3 -> (9)%12=9 true -> nota = 12-3 = 9 (A)
    {
        uchar note = 12;
        PhraseStepCell(kPhraseColNote, &note, &vol, &pitch, 1, PED_LEFT,
                       false);
        CHECK(note == 9);
    }
    // 60 (C) + RIGHT: offset 1 -> (61)%12=1 false -> 2 -> (62)%12=2 true
    // -> nota = 62
    {
        uchar note = 60;
        PhraseStepCell(kPhraseColNote, &note, &vol, &pitch, 1, PED_RIGHT,
                       false);
        CHECK(note == 62);
    }
    printf("step note escala acustica (scale-snap) OK\n");
}

static void test_step_vol() {
    uchar vol = 0x64, pitch = PITCH_STORED_ZERO;
    // 100 LEFT -> 99
    {
        uchar v = 100;
        PhraseStepCell(kPhraseColVol, &v, &vol, &pitch, 0, PED_LEFT, false);
        CHECK(v == 99);
    }
    // 100 UP bigStep: 100+10 -> clamp 100
    {
        uchar v = 100;
        PhraseStepCell(kPhraseColVol, &v, &vol, &pitch, 0, PED_UP, true);
        CHECK(v == 100);
    }
    // 50 UP bigStep -> 60
    {
        uchar v = 50;
        PhraseStepCell(kPhraseColVol, &v, &vol, &pitch, 0, PED_UP, true);
        CHECK(v == 60);
    }
    // 50 UP normal -> 51
    {
        uchar v = 50;
        PhraseStepCell(kPhraseColVol, &v, &vol, &pitch, 0, PED_UP, false);
        CHECK(v == 51);
    }
    // vacio (0xFF): se rellena con 0x64 ANTES del paso; luego offset
    // LEFT -> 0x64-1 = 99
    {
        uchar v = 0xFF;
        PhraseStepCell(kPhraseColVol, &v, &vol, &pitch, 0, PED_LEFT, false);
        CHECK(v == 99);
    }
    // limite: 0 LEFT -> queda 0
    {
        uchar v = 0;
        PhraseStepCell(kPhraseColVol, &v, &vol, &pitch, 0, PED_LEFT, false);
        CHECK(v == 0);
    }
    printf("step vol (paso 1 / bigStep 10, clamp 0..100) OK\n");
}

static void test_step_pitch() {
    uchar vol = 0x64, pitch = PITCH_STORED_ZERO;
    // 0x40 (0) UP -> 0x41 (1)
    {
        uchar p = PITCH_STORED_ZERO;
        PhraseStepCell(kPhraseColPitch, &p, &vol, &pitch, 0, PED_UP, false);
        CHECK(p == PITCH_STORED_ZERO + 1);
    }
    // 0x58 (24) UP big -> clamp 24 -> 0x58
    {
        uchar p = PITCH_STORED_MAX;
        PhraseStepCell(kPhraseColPitch, &p, &vol, &pitch, 0, PED_UP, true);
        CHECK(p == PITCH_STORED_MAX);
    }
    // 0x28 (-24) DOWN big -> clamp -24
    {
        uchar p = PITCH_STORED_MIN;
        PhraseStepCell(kPhraseColPitch, &p, &vol, &pitch, 0, PED_DOWN, true);
        CHECK(p == PITCH_STORED_MIN);
    }
    // 0x40 (0) UP big -> 10 -> 0x4A
    {
        uchar p = PITCH_STORED_ZERO;
        PhraseStepCell(kPhraseColPitch, &p, &vol, &pitch, 0, PED_UP, true);
        CHECK(p == PITCH_STORED_ZERO + 10);
    }
    // 0x40 LEFT -> -1
    {
        uchar p = PITCH_STORED_ZERO;
        PhraseStepCell(kPhraseColPitch, &p, &vol, &pitch, 0, PED_LEFT, false);
        CHECK(p == PITCH_STORED_ZERO - 1);
    }
    // empty (PITCH_STORED_NONE): storedToInt(0x00)=0 -> paso normal
    {
        uchar p = PITCH_STORED_NONE;
        PhraseStepCell(kPhraseColPitch, &p, &vol, &pitch, 0, PED_UP, false);
        CHECK(p == PITCH_STORED_ZERO + 1);
    }
    printf("step pitch (stored -24..+24, pasos 1/10, clamp) OK\n");
}

static void test_step_instr() {
    uchar vol = 0x64, pitch = PITCH_STORED_ZERO;
    // 3 UP -> +16 -> 19
    {
        uchar ins = 3;
        PhraseStepCell(kPhraseColInstr, &ins, &vol, &pitch, 0, PED_UP, false);
        CHECK(ins == 19);
    }
    // 3 DOWN -> 3-16 -> wrap 143+3-16... (0-16 -> wrap 144-16=128... )
    // oracle: v=3; v+=-16=-13; wrap -> 143+1-13=131
    {
        uchar ins = 3;
        PhraseStepCell(kPhraseColInstr, &ins, &vol, &pitch, 0, PED_DOWN,
                       false);
        CHECK(ins == 131);
    }
    // 143 UP -> +16 -> wrap 159-144 = 15
    {
        uchar ins = 143;
        PhraseStepCell(kPhraseColInstr, &ins, &vol, &pitch, 0, PED_UP, false);
        CHECK(ins == 15);
    }
    // 1 LEFT -> 0
    {
        uchar ins = 1;
        PhraseStepCell(kPhraseColInstr, &ins, &vol, &pitch, 0, PED_LEFT,
                       false);
        CHECK(ins == 0);
    }
    // 0 LEFT -> wrap 143
    {
        uchar ins = 0;
        PhraseStepCell(kPhraseColInstr, &ins, &vol, &pitch, 0, PED_LEFT,
                       false);
        CHECK(ins == 143);
    }
    printf("step instrumento (paso 1/16, wrap 0..143) OK\n");
}

static void test_paste_last() {
    // nota vacia -> lastNote; nota llena -> captura
    {
        uchar cell = 0xFF;
        int last = 60;
        CHECK(PhrasePasteLast(kPhraseColNote, &cell, &last) == true);
        CHECK(cell == 60 && last == 60);
        CHECK(PhrasePasteLast(kPhraseColNote, &cell, &last) == false);
        CHECK(cell == 60 && last == 60);
    }
    // vol vacia -> lastVol; llena -> captura
    {
        uchar cell = 0xFF;
        int last = 80;
        CHECK(PhrasePasteLast(kPhraseColVol, &cell, &last) == true);
        CHECK(cell == 80);
    }
    // pitch: NONE -> phrasePitchIntToStored(last); llena -> storedToInt
    {
        uchar cell = PITCH_STORED_NONE;
        int last = 5;
        CHECK(PhrasePasteLast(kPhraseColPitch, &cell, &last) == true);
        CHECK(cell == PITCH_STORED_ZERO + 5);
        CHECK(PhrasePasteLast(kPhraseColPitch, &cell, &last) == false);
        CHECK(last == 5);
    }
    // instr vacia -> lastInstr
    {
        uchar cell = 0xFF;
        int last = 27;
        CHECK(PhrasePasteLast(kPhraseColInstr, &cell, &last) == true);
        CHECK(cell == 27);
    }
    // comando: I_CMD_NONE -> lastCmd; lleno -> captura
    {
        FourCC cell = I_CMD_NONE;
        int last = 0x4D4F544F; // MOTO
        CHECK(PhrasePasteLastCommand(&cell, &last) == true);
        CHECK(cell == 0x4D4F544F);
        CHECK(PhrasePasteLastCommand(&cell, &last) == false);
        CHECK(last == 0x4D4F544F);
    }
    printf("pasteLast por columna OK\n");
}

static void test_selection() {
    // rect normalizado
    PhraseRect r = PhraseNormalizeRect(5, 3, 2, 8);
    CHECK(r.left == 2 && r.top == 3 && r.right == 5 && r.bottom == 8);
    r = PhraseNormalizeRect(2, 8, 5, 3);
    CHECK(r.left == 2 && r.top == 3 && r.right == 5 && r.bottom == 8);

    // extendSelection columna: cursor dentro, expande a columnas 0..7
    {
        PhraseSelectionState s = {3, 5, 2, 5}; // cursor(3,5) ancla(2,5)
        PhraseExtendSelection(s);
        CHECK(s.col == 7 && s.clipCol == 0);
    }
    // extendSelection fila: seleccion ya cubre columnas 0..7 -> filas.
    // cursor 3 < ancla 5 -> cursor a fila 0, ancla a fila 15
    {
        PhraseSelectionState s = {7, 3, 0, 5};
        PhraseExtendSelection(s);
        CHECK(s.row == 0 && s.clipRow == 15);
    }
    // cursor arriba del ancla en filas
    {
        PhraseSelectionState s = {0, 2, 7, 5};
        PhraseExtendSelection(s);
        CHECK(s.clipRow == 15 && s.row == 0);
    }
    printf("seleccion (rect normalize + extend) OK\n");
}

static void test_clipboard() {
    uchar note[16], vol[16], pitch[16], instr[16];
    uint cmd1[16], cmd2[16];
    ushort param1[16], param2[16];
    // fill de una frase de prueba
    for (int i = 0; i < 16; i++) {
        note[i] = (uchar)(60 + i);
        vol[i] = (uchar)(90 + i);
        pitch[i] = (uchar)(PITCH_STORED_ZERO + i);
        instr[i] = (uchar)i;
        cmd1[i] = I_CMD_NONE + i;
        param1[i] = (ushort)(100 + i);
        cmd2[i] = I_CMD_NONE + i + 1;
        param2[i] = (ushort)(200 + i);
    }
    PhraseClipboard cb;
    memset(&cb, 0, sizeof(cb));
    cb.active_ = true;
    cb.col_ = 2;  // ancla (2,3)
    cb.row_ = 3;
    // cursor (5,6) -> rect 2..5 x 3..6
    PhraseFillClipboard(cb, 5, 6, note, vol, pitch, instr, cmd1, param1, cmd2,
                        param2);
    CHECK(cb.col_ == 2 && cb.row_ == 3);
    CHECK(cb.width_ == 4 && cb.height_ == 4);
    CHECK(cb.note_[0] == note[3] && cb.vol_[3] == vol[6]);
    CHECK(cb.pitch_[1] == pitch[4] && cb.instr_[2] == instr[5]);
    CHECK(cb.cmd1_[1] == cmd1[4] && cb.param1_[3] == param1[6]);
    CHECK(cb.cmd2_[0] == cmd2[3] && cb.param2_[3] == param2[6]);
    // PasteClipboard a una posicion vacia
    uchar dnote[16], dvol[16], dpitch[16], dinstr[16];
    uint dcmd1[16], dcmd2[16];
    ushort dparam1[16], dparam2[16];
    memset(dnote, 0xFF, sizeof(dnote));
    memset(dvol, 0xFF, sizeof(dvol));
    memset(dpitch, PITCH_STORED_NONE, sizeof(dpitch));
    memset(dinstr, 0xFF, sizeof(dinstr));
    memset(dcmd1, 0, sizeof(dcmd1));
    memset(dparam1, 0, sizeof(dparam1));
    memset(dcmd2, 0, sizeof(dcmd2));
    memset(dparam2, 0, sizeof(dparam2));
    {
        // destino (0, 2): el switch es por i+cb.col_ (origen), asi que con
        // ancla col 2 solo se copia la columna 2 (pitch) -> logica golden
        PhraseClipboard c2 = cb;
        bool updated = PhrasePasteClipboard(c2, dnote, dvol, dpitch, dinstr,
                                            dcmd1, dparam1, dcmd2, dparam2, 0,
                                            2);
        CHECK(updated);
        CHECK(dnote[2] == 0xFF && dvol[2] == 0xFF); // no copiados
        CHECK(dpitch[2] == pitch[3]);              // ancla col 2 -> pitch
        CHECK(dinstr[2] == instr[3]);              // ancla col 3 -> instr
        // command/param solo copian si el destino es una columna de comando:
        // con destino 4, origen (4 + i) -> i=2,3 -> pasteCol 6,7 (cmd2/param2)
        memset(dcmd1, 0, sizeof(dcmd1));
        memset(dparam1, 0, sizeof(dparam1));
        memset(dcmd2, 0, sizeof(dcmd2));
        memset(dparam2, 0, sizeof(dparam2));
        PhraseClipboard c3 = cb;
        PhrasePasteClipboard(c3, dnote, dvol, dpitch, dinstr, dcmd1, dparam1,
                             dcmd2, dparam2, 4, 2);
        CHECK(dcmd1[2] == 0 && dparam1[3] == 0);
        CHECK(dcmd2[2] != 0);
        CHECK(dparam2[3] != 0);
    }
    // CutSelectionCells limpia con los valores golden
    {
        uchar n[16], v[16], p[16], i2[16];
        uint cc1[16], cc2[16];
        ushort cp1[16], cp2[16];
        for (int k = 0; k < 16; k++) {
            n[k] = 60 + k;
            v[k] = 90 + k;
            p[k] = PITCH_STORED_ZERO + k;
            i2[k] = (uchar)k;
            cc1[k] = 0x11;
            cp1[k] = 0x12;
            cc2[k] = 0x13;
            cp2[k] = 0x14;
        }
        PhraseClipboard c4;
        memset(&c4, 0, sizeof(c4));
        c4.active_ = true;
        c4.col_ = 1;
        c4.row_ = 2;
        c4.width_ = 2;
        c4.height_ = 2;
        PhraseCutSelectionCells(c4, n, v, p, i2, cc1, cp1, cc2, cp2);
        // cols 1..2 (vol/pitch): solo esas columnas se limpian
        CHECK(n[2] == 62 && v[2] == 0xFF && v[3] == 0xFF);
        CHECK(p[2] == PITCH_STORED_NONE && p[3] == PITCH_STORED_NONE);
        CHECK(i2[2] == 2 && i2[3] == 3);
        CHECK(cc1[2] == 0x11 && cp1[3] == 0x12);
        CHECK(cc2[2] == 0x13 && cp2[3] == 0x14);
    }
    printf("portapapeles (fill/paste/cut golden) OK\n");
}

static void test_interpolate() {
    PhraseClipboard cb;
    memset(&cb, 0, sizeof(cb));
    // no activa -> SKIPPED
    CHECK(PhraseInterpolateSelection(cb, 0, 0, 0, 0, 0, 0) ==
          PINTERP_SKIPPED);
    // nota: 60..66 en 3 filas -> 60,63,66
    cb.active_ = true;
    cb.col_ = 0;
    cb.row_ = 0; // ancla (0,0), cursor (0,2)
    uchar note[16];
    for (int i = 0; i < 16; i++) note[i] = 0xFF;
    note[0] = 60;
    note[2] = 66;
    CHECK(PhraseInterpolateSelection(cb, 0, 2, note, 0, 0, 0) == PINTERP_OK);
    CHECK(note[0] == 60 && note[1] == 63 && note[2] == 66);
    // nota con extremo 0xFF -> NO_NOTE_INFO
    {
        uchar n2[16];
        for (int i = 0; i < 16; i++) n2[i] = 0xFF;
        n2[2] = 66;
        cb.col_ = 0;
        cb.row_ = 0;
        CHECK(PhraseInterpolateSelection(cb, 0, 2, n2, 0, 0, 0) ==
              PINTERP_NO_NOTE_INFO);
    }
    // pitch: 0..12 en 3 filas con clamp; stored
    {
        uchar pitch[16];
        memset(pitch, PITCH_STORED_NONE, sizeof(pitch));
        pitch[0] = PITCH_STORED_ZERO;          // 0
        pitch[2] = PITCH_STORED_ZERO + 12;     // +12
        cb.col_ = kPhraseColPitch;
        cb.row_ = 0;
        CHECK(PhraseInterpolateSelection(cb, kPhraseColPitch, 2, 0, pitch, 0,
                                         0) == PINTERP_OK);
        CHECK(phrasePitchStoredToInt(pitch[1]) == 6);
    }
    // param1 (col 5): 100..110 en 3 filas
    {
        ushort param1[16];
        memset(param1, 0, sizeof(param1));
        param1[0] = 100;
        param1[2] = 110;
        cb.col_ = kPhraseColParam1;
        cb.row_ = 0;
        CHECK(PhraseInterpolateSelection(cb, kPhraseColParam1, 2, 0, 0, param1,
                                         0) == PINTERP_OK);
        CHECK(param1[1] == 105);
    }
    // columna invalida (1=vol) -> SKIPPED
    {
        cb.col_ = kPhraseColVol;
        cb.row_ = 0;
        CHECK(PhraseInterpolateSelection(cb, kPhraseColVol, 2, 0, 0, 0, 0) ==
              PINTERP_SKIPPED);
    }
    // una sola fila -> SKIPPED
    {
        cb.col_ = kPhraseColNote;
        cb.row_ = 0;
        CHECK(PhraseInterpolateSelection(cb, kPhraseColNote, 0, 0, 0, 0, 0) ==
              PINTERP_SKIPPED);
    }
    printf("interpolacion (nota/pitch/param + estados) OK\n");
}

int main() {
    test_layout_constants();
    test_clamp_wrap();
    test_step_note_chromatic();
    test_step_note_acoustic();
    test_step_vol();
    test_step_pitch();
    test_step_instr();
    test_paste_last();
    test_selection();
    test_clipboard();
    test_interpolate();
    if (g_failures) {
        printf("PHRASE_GRID_EDIT_HOST_TEST FAILED (%d)\n", g_failures);
        return 1;
    }
    printf("ALL OK\n");
    return 0;
}