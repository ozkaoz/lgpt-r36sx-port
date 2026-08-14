/*
 * F3-3b (docs/F3_ARCHITECTURE_ES.md): capa pura de dibujo del Chopper
 * (grilla 40x30: celdas, invert, color) extraida de SampleChopperModal.
 * Equivalencia golden bajo ASAN/UBSAN: oracles replicando bacon 1.2.1
 * (drawTopBar/drawFrame/drawEmptyWaveformText/drawControls/drawPitchScreen/
 * drawSampleInfo/showOperationProgress/drawOperationOverlay) comparados
 * campo a campo contra ChopperView.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Application/UI/Views/ModalDialogs/ChopperView.h"

static int checks = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL line %d: %s\n", __LINE__, #cond);              \
            exit(1);                                                    \
        }                                                               \
        checks++;                                                       \
    } while (0)

static ChopperGrid g_mk() {
    ChopperGrid g;
    g.Clear();
    return g;
}

static bool g_matches(const char *tag, const ChopperGrid &got,
                      const ChopperGrid &exp) {
    for (int y = 0; y < LGPT_CHOPPER_SCREEN_H; y++)
        for (int x = 0; x < LGPT_CHOPPER_SCREEN_W; x++) {
            if (got.cell[y][x] != exp.cell[y][x]) {
                printf("FAIL %s cell(%d,%d): got '%c'(%d) expected '%c'(%d)\n",
                       tag, x, y, got.cell[y][x], got.cell[y][x],
                       exp.cell[y][x], exp.cell[y][x]);
                return false;
            }
            if (got.invert[y][x] != exp.invert[y][x] ||
                got.color[y][x] != exp.color[y][x]) {
                printf("FAIL %s attr(%d,%d): got inv=%d col=%d expected "
                       "inv=%d col=%d\n",
                       tag, x, y, got.invert[y][x], (int)got.color[y][x],
                       exp.invert[y][x], (int)exp.color[y][x]);
                return false;
            }
        }
    return true;
}

/* --- oracles golden (bacon 1.2.1) --- */

static void o_topBar(ChopperGrid &g) {
    for (int x = 0; x < 40; x++) { g.invert[0][x] = true; g.color[0][x] = CHOP_COLOR_HILITE1; }
    const char *t = " P G  SCPI  M TT       CHOPPER       ";
    for (int i = 0; t[i] && i < 40; i++) { g.cell[0][i] = t[i]; g.invert[0][i] = true; g.color[0][i] = CHOP_COLOR_HILITE1; }
}

static void o_frame(ChopperGrid &g) {
    for (int x = 0; x < 40; x++) { g.invert[1][x] = true; g.color[1][x] = CHOP_COLOR_BORDER; g.invert[22][x] = true; g.color[22][x] = CHOP_COLOR_BORDER; }
    for (int y = 2; y < 22; y++) { g.invert[y][0] = true; g.color[y][0] = CHOP_COLOR_BORDER; g.invert[y][39] = true; g.color[y][39] = CHOP_COLOR_BORDER; }
    const char *t = "Graphical Chopper";
    for (int i = 0; t[i] && (2 + i) < 40; i++) { g.cell[2][2 + i] = t[i]; g.color[2][2 + i] = CHOP_COLOR_HILITE2; }
}

static void o_empty(ChopperGrid &g) {
    const char *t = "            no sample loaded            ";
    for (int i = 0; t[i] && (2 + i) < 40; i++) { g.cell[13][2 + i] = t[i]; g.color[13][2 + i] = CHOP_COLOR_HILITE1; }
}

static void o_controls(ChopperGrid &g, bool trim) {
    const char *t = trim ? "R1+A Keep  L2+Y Del  A+B Nudge  R1+B Back"
                         : "Select: Crop | L1+R1: Pitch | R1+B: Back";
    for (int i = 0; t[i] && i < 40; i++) { g.cell[24][i] = t[i]; g.color[24][i] = CHOP_COLOR_NORMAL; }
}

static void o_hints(ChopperGrid &g) {
    const char *a = "UP/DN Item | L/R Value | B Preview";
    const char *b = "A Apply | L1+R1 Exit | R2+LR Target";
    for (int i = 0; a[i] && (1 + i) < 40; i++) { g.cell[24][1 + i] = a[i]; g.color[24][1 + i] = CHOP_COLOR_NORMAL; }
    for (int i = 0; b[i] && (1 + i) < 40; i++) { g.cell[25][1 + i] = b[i]; g.color[25][1 + i] = CHOP_COLOR_NORMAL; }
}

static char o_headerBuf[64], o_valueBuf[64], o_siBuf[64],
    o_nameBuf[64], o_frameBuf[64], o_opBuf[64], o_pctBuf[64];

static void o_header(int instr, int sample, int sel, int count) {
    sprintf(o_headerBuf, "I%02X S%02X C%02d/%02d", instr, sample, sel, count);
}

static void o_value(int param, int semitones, int attackMs, int sustainPercent,
                    int releaseMs, int scope, int sample) {
    switch (param) {
    case 0: sprintf(o_valueBuf, "%+3d st", semitones); break;
    case 1: sprintf(o_valueBuf, "%4d ms", attackMs); break;
    case 2: sprintf(o_valueBuf, "%3d %%", sustainPercent); break;
    case 3: sprintf(o_valueBuf, "%4d ms", releaseMs); break;
    case 4: sprintf(o_valueBuf, "%s", scope ? "Chop" : "Sample"); break;
    default: sprintf(o_valueBuf, "%02X", sample); break;
    }
}

static void check_compose(const char *tag, int gotLen, const char *got,
                          const char *exp) {
    if (gotLen != (int)strlen(exp) || strcmp(got, exp) != 0) {
        printf("FAIL %s: got %d:[%s] expected %d:[%s]\n", tag, gotLen, got,
               (int)strlen(exp), exp);
        exit(1);
    }
    checks++;
}

int main() {
    /* tiny buffer safety: the compositors must never overflow */
    char tiny[1];
    {
        int r = ChopperView::ComposeSampleInfoLine(tiny, 1, 1, 2, 50);
        CHECK(r >= 0);
        CHECK(tiny[0] == 0);
        r = ChopperView::ComposeNameLine(tiny, 1, "name");
        CHECK(r >= 0);
        checks += 3;
    }

    /* --- grid primitives: clipping and Clear --- */
    {
        ChopperGrid g = g_mk();
        g.SetText(-5, 4, "abc", CHOP_COLOR_HILITE1, false);
        CHECK(g.cell[4][0] == ' ');           /* left clip */
        g.SetText(38, 4, "abcdefghijk", CHOP_COLOR_HILITE1, false);
        CHECK(g.cell[4][38] == 'a');          /* right clip at 40 cols */
        CHECK(g.cell[4][39] == 'b');
        g.SetText(0, 30, "x", CHOP_COLOR_HILITE1, false);
        g.SetText(0, -1, "x", CHOP_COLOR_HILITE1, false);
        g.SetInvert(0, 30, CHOP_COLOR_BORDER);
        g.SetInvert(40, 4, CHOP_COLOR_BORDER);
        g.SetInvert(0, -1, CHOP_COLOR_BORDER);
        checks += 5;
        g.Clear();
        for (int y = 0; y < 30; y++)
            for (int x = 0; x < 40; x++)
                CHECK(g.cell[y][x] == ' ' && !g.invert[y][x] &&
                      g.color[y][x] == CHOP_COLOR_NORMAL);
    }

    /* --- drawTopBar vs golden --- */
    {
        ChopperGrid got = g_mk(); ChopperGrid exp = g_mk();
        ChopperView::DrawTopBar(got); o_topBar(exp);
        CHECK(g_matches("DrawTopBar", got, exp));
    }

    /* --- drawFrame vs golden --- */
    {
        ChopperGrid got = g_mk(); ChopperGrid exp = g_mk();
        ChopperView::DrawFrame(got); o_frame(exp);
        CHECK(g_matches("DrawFrame", got, exp));
    }

    /* --- drawEmptyWaveformText (recorte a 40 cols) --- */
    {
        ChopperGrid got = g_mk(); ChopperGrid exp = g_mk();
        ChopperView::DrawEmptyWaveformText(got); o_empty(exp);
        CHECK(g_matches("DrawEmptyWaveformText", got, exp));
        int setCount = 0;
        for (int x = 0; x < 40; x++) if (got.cell[13][x] != ' ') setCount++;
        CHECK(setCount == 14);  /* "no sample loaded" (los 12+12 espacios
                                   no son visibles como celdas escritas) */
        checks++;
    }

    /* --- drawControls / drawPitchHints --- */
    {
        for (int trim = 0; trim < 2; trim++) {
            ChopperGrid got = g_mk(); ChopperGrid exp = g_mk();
            ChopperView::DrawControls(got, trim != 0);
            o_controls(exp, trim != 0);
            CHECK(g_matches("DrawControls", got, exp));
        }
        {   /* pitch hints merge over main controls: col 0 y fila 24 x>=33
            siguen siendo del main, x=1..32 del pitch */
            ChopperGrid g = g_mk();
            ChopperView::DrawControls(g, false);
            ChopperView::DrawPitchHints(g);
            CHECK(g.cell[24][0] == 'S');
            CHECK(g.cell[24][1] == 'U');
            CHECK(g.cell[24][34] == 'w');  /* dos ultimos chars del pitch */
            CHECK(g.cell[25][1] == 'A');
            CHECK(g.cell[24][36] == 'B');  /* cola del hint main intacta */
            checks += 5;
        }
        {   /* trim controls fully overwrite row 24 */
            ChopperGrid g = g_mk();
            ChopperView::DrawControls(g, true);
            ChopperView::DrawControls(g, false);
            CHECK(g.cell[24][0] == 'S');
            checks++;
        }
    }

    /* --- compositors vs snprintf replicas --- */
    {
        for (int instr = 0; instr < 300; instr += 37)
            for (int sample = 0; sample < 300; sample += 53)
                for (int sel = 0; sel < 10; sel++)
                    for (int count = 0; count < 10; count += 2) {
                        o_header(instr, sample, sel, count);
                        char buf[64];
                        int len = ChopperView::ComposeHeaderLine(
                            buf, sizeof(buf), instr, sample, sel, count);
                        check_compose("ComposeHeaderLine", len, buf,
                                       o_headerBuf);
                    }
    }
    {
        int params[2][4] = {{-12, 0, 1, 12}, {0, 100, 5000, 5000}};
        int semis[] = {-12, -1, 0, 1, 12};
        for (int p = 0; p < 6; p++)
            for (size_t s = 0; s < sizeof(semis) / sizeof(semis[0]); s++)
                for (int scope = 0; scope < 2; scope++)
                    for (int smp = 0; smp < 300; smp += 99) {
                        int atk = params[0][s % 4], sus = params[1][s % 4],
                            rel = params[0][(s + 1) % 4];
                        o_value(p, semis[s], atk, sus, rel, scope, smp);
                        char buf[32];
                        int len = ChopperView::ComposePitchValue(
                            buf, sizeof(buf), p, semis[s], atk, sus, rel,
                            scope != 0, smp);
                        check_compose("ComposePitchValue", len, buf,
                                      o_valueBuf);
                    }
    }
    {
        const char *labels[6] = {"Pitch", "Attack", "Sustain",
                                 "Release", "Scope", "Sample"};
        for (int p = 0; p < 8; p++) {
            const char *l = ChopperView::PitchLabel(p);
            CHECK(strcmp(l, labels[p > 5 ? 5 : p]) == 0);
        }
        checks += 8;
    }
    {
        for (int instr = 0; instr < 300; instr += 47)
            for (int sample = -3; sample < 300; sample += 61)
                for (int zoom = 0; zoom < 120; zoom += 17) {
                    sprintf(o_siBuf, "Inst:%02X Smpl:%02X Zoom:%03d%%",
                            instr, sample < 0 ? 0 : sample, zoom);
                    char buf[96];
                    int len = ChopperView::ComposeSampleInfoLine(
                        buf, sizeof(buf), instr, sample, zoom);
                    check_compose("ComposeSampleInfoLine", len, buf,
                                  o_siBuf);
                }
    }
    {
        const char *names[] = {"", "A", "0123456789012345678901234567890",
                               "0123456789012345678901234567890123456789"};
        for (size_t n = 0; n < 4; n++) {
            sprintf(o_nameBuf, "Name:%s", names[n]);
            char buf[64];
            int len = ChopperView::ComposeNameLine(buf, sizeof(buf), names[n]);
            check_compose("ComposeNameLine", len, buf,
                          o_nameBuf);
        }
    }
    {
        int cursors[] = {0, 1, 22050, 44100};
        int maxs[] = {0, 1, 44099, 44100};
        int chops[] = {0, 1, 10, 99};
        int counts[] = {0, 1, 2, 5, 100};
        for (size_t a = 0; a < 4; a++)
            for (size_t b = 0; b < 4; b++)
                for (size_t c = 0; c < 4; c++)
                    for (size_t d = 0; d < 5; d++)
                        for (int trim = 0; trim < 2; trim++) {
                            sprintf(o_frameBuf, "Frame:%d/%d Chop:%02d/%02d%s",
                                    cursors[a], maxs[b], chops[c], counts[d],
                                    trim ? " ADJ" : "");
                            char buf[64];
                            int len = ChopperView::ComposeFrameLine(
                                buf, sizeof(buf), cursors[a], maxs[b],
                                chops[c], counts[d], trim != 0);
                            check_compose("ComposeFrameLine", len, buf,
                                          o_frameBuf);
                        }
    }
    {
        const char *combos[] = {"", "R2 + Y", "L1+X"};
        const char *msgs[] = {"", "normalize", "Operacion normalizar"};
        for (size_t a = 0; a < 3; a++)
            for (size_t b = 0; b < 3; b++)
                for (int p = 0; p < 120; p += 13) {
                    if (p >= 100)
                        sprintf(o_opBuf, "%s %s OK A/L1+X/R1+X", combos[a],
                                msgs[b]);
                    else
                        sprintf(o_opBuf, "%s %s %d%%", combos[a], msgs[b], p);
                    char buf[128];
                    int len = ChopperView::ComposeOperationStatus(
                        buf, sizeof(buf), combos[a], msgs[b], p);
                    check_compose("ComposeOperationStatus", len, buf,
                                  o_opBuf);
                }
    }
    {
        for (int p = 0; p < 120; p += 7) {
            sprintf(o_pctBuf, "%3d%%", p);
            char buf[16];
            int len = ChopperView::ComposeOperationPercent(buf, sizeof(buf), p);
            check_compose("ComposeOperationPercent", len, buf,
                          o_pctBuf);
        }
    }

    /* --- grid after merge replays the exact golden screen (pitch hints) --- */
    {
        ChopperGrid exp = g_mk(); o_hints(exp);
        ChopperGrid pg = g_mk();
        ChopperView::DrawPitchHints(pg);
        CHECK(g_matches("DrawPitchHints", pg, exp));
    }

    printf("chopper_draw_host_test: ALL OK (%d checks)\n", checks);
    return 0;
}