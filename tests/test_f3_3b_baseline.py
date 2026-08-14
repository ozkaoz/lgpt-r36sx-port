#!/usr/bin/env python3
"""F3-3b baseline (docs/F3_ARCHITECTURE_ES.md): el dibujo textual del
SampleChopperModal (grilla 40x30 de celdas con invert/color) se extrae a
ChopperView como capa pura header-only; la vista solo drena la grilla a la
pantalla (drainChopperGrid: SetColor/DrawString reales).

Audio, mensajes de estado, overlay (g_chopper*), titulo PITCH/ENV y las
posiciones reales (MenuLayout) siguen en la vista. Verifica:

1. ChopperView.h declara la capa de dibujo: constantes 40/30, enum
   ChopperCellColor, struct ChopperGrid, DrawTopBar/DrawFrame/
   DrawEmptyWaveformText/DrawControls/DrawPitchHints y los compositors
   (ComposeHeaderLine/PitchLabel/ComposePitchValue/ComposeSampleInfoLine/
   ComposeNameLine/ComposeFrameLine/ComposeOperationStatus/
   ComposeOperationPercent) con los literales golden.
2. SampleChopperModal.h declara drainChopperGrid y la usa el cpp
   (drainChopperGrid(, grid, props) sobre las celdas de las capas).
3. La vista delega: drawTopBar/drawFrame/drawEmptyWaveformText/drawControls
   construyen ChopperGrid y llaman a las estaticas de ChopperView;
   drawPitchScreen usa ComposeHeaderLine/PitchLabel/ComposePitchValue/
   DrawPitchHints; showOperationProgress usa ComposeOperationStatus;
   drawOperationOverlay usa ComposeOperationPercent.
4. Los literales golden de dibujo viven en ChopperView.h (fila de estado,
   hint main/trim, hint pitch, "no sample loaded", "Graphical Chopper",
   "OPERATION" no) y el titulo PITCH/ENV queda en la vista.
5. El runner host del dibujo existe y esta en audit.sh.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SCM_H = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.h").read_text()
SCM_CPP = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp").read_text()
CV_H = (ROOT / "source/sources/Application/UI/Views/ModalDialogs/ChopperView.h").read_text()
AUDIT = (ROOT / "scripts/audit.sh").read_text()


# ---------------------------------------------------------------------------
# 1. Capa de dibujo en ChopperView
# ---------------------------------------------------------------------------
def check_draw_layer():
    assert "class ChopperView" in CV_H
    for c in [
        "LGPT_CHOPPER_SCREEN_W (40)",
        "LGPT_CHOPPER_SCREEN_H (30)",
        "enum ChopperCellColor",
        "CHOP_COLOR_NORMAL",
        "CHOP_COLOR_HILITE1",
        "CHOP_COLOR_HILITE2",
        "CHOP_COLOR_BORDER",
        "struct ChopperGrid",
        "void Clear()",
        "void SetText(",
        "void SetInvert(",
    ]:
        assert c in CV_H, f"capa de dibujo sin {c}"
    for m in [
        "static void DrawTopBar(",
        "static void DrawFrame(",
        "static void DrawEmptyWaveformText(",
        "static void DrawControls(",
        "static void DrawPitchHints(",
        "static int ComposeHeaderLine(",
        "static const char *PitchLabel(",
        "static int ComposePitchValue(",
        "static int ComposeSampleInfoLine(",
        "static int ComposeNameLine(",
        "static int ComposeFrameLine(",
        "static int ComposeOperationStatus(",
        "static int ComposeOperationPercent(",
    ]:
        assert m in CV_H, f"ChopperView sin {m}"


# ---------------------------------------------------------------------------
# 2. Literales golden de dibujo en ChopperView.h
# ---------------------------------------------------------------------------
def check_golden_literals():
    for g in [
        " P G  SCPI  M TT       CHOPPER       ",
        "Graphical Chopper",
        "            no sample loaded            ",
        "R1+A Keep  L2+Y Del  A+B Nudge  R1+B Back",
        "Select: Crop | L1+R1: Pitch | R1+B: Back",
        "UP/DN Item | L/R Value | B Preview",
        "A Apply | L1+R1 Exit | R2+LR Target",
        '"I%02X S%02X C%02d/%02d"',
        '"Pitch"',
        '"Attack"',
        '"Sustain"',
        '"Release"',
        '"Scope"',
        '"Sample"',
        '"%+3d st"',
        '"%4d ms"',
        '"%3d %%"',
        '"%s"',
        '"%02X"',
        '"Inst:%02X Smpl:%02X Zoom:%03d%%"',
        '"Name:%s"',
        '"Frame:%d/%d Chop:%02d/%02d%s"',
        '" ADJ"',
        '"%s %s OK A/L1+X/R1+X"',
        '"%s %s %d%%"',
        '"%3d%%"',
    ]:
        assert g in CV_H, f"literal golden de dibujo fuera de ChopperView: {g}"


# ---------------------------------------------------------------------------
# 3. Vista: drenado y delegacion
# ---------------------------------------------------------------------------
def check_modal_delegation():
    assert "void drainChopperGrid(const ChopperGrid &grid, GUITextProperties &props);" in SCM_H, (
        "header no declara drainChopperGrid")
    for call in [
        "void SampleChopperModal::drainChopperGrid(",
        "ChopperGrid grid; grid.Clear();",
        "ChopperView::DrawTopBar(grid)",
        "ChopperView::DrawFrame(grid)",
        "ChopperView::DrawEmptyWaveformText(grid)",
        "ChopperView::DrawControls(grid, trimMode_)",
        "ChopperView::DrawPitchHints(hints)",
        "ChopperView::ComposeHeaderLine(",
        "ChopperView::PitchLabel(i)",
        "ChopperView::ComposePitchValue(",
        "ChopperView::ComposeSampleInfoLine(",
        "ChopperView::ComposeNameLine(",
        "ChopperView::ComposeFrameLine(",
        "ChopperView::ComposeOperationStatus(",
        "ChopperView::ComposeOperationPercent(",
    ]:
        assert call in SCM_CPP, f"delegacion de dibujo perdida: {call}"
    # El titulo PITCH/ENV queda en la vista (drawPitchScreen).
    assert '"PITCH/ENV"' in SCM_CPP


# ---------------------------------------------------------------------------
# 4. Sin literales de dibujo duplicados en la vista
# ---------------------------------------------------------------------------
def check_no_duplicated_literals():
    for g in [
        '" P G  SCPI  M TT       CHOPPER       "',
        '"Graphical Chopper"',
        '"            no sample loaded            "',
        '"R1+A Keep  L2+Y Del  A+B Nudge  R1+B Back"',
        '"Select: Crop | L1+R1: Pitch | R1+B: Back"',
        '"UP/DN Item | L/R Value | B Preview"',
        '"A Apply | L1+R1 Exit | R2+LR Target"',
        '"%+3d st"',
        '"%4d ms"',
        '"%3d %%"',
        '"Inst:%02X Smpl:%02X Zoom:%03d%%"',
        '"Name:%s"',
        '"Frame:%d/%d Chop:%02d/%02d%s"',
        '"%s %s OK A/L1+X/R1+X"',
        '"%s %s %d%%"',
        '"%3d%%"',
    ]:
        assert g not in SCM_CPP, f"literal de dibujo duplicado en la vista: {g}"


# ---------------------------------------------------------------------------
# 5. Runner y audit
# ---------------------------------------------------------------------------
def check_runners():
    assert (ROOT / "tests/run_host_chopper_draw.sh").exists(), (
        "runner de dibujo falta")
    assert "run_host_chopper_draw.sh" in AUDIT, (
        "run_host_chopper_draw.sh no esta en audit.sh")


def main():
    check_draw_layer()
    check_golden_literals()
    check_modal_delegation()
    check_no_duplicated_literals()
    check_runners()
    print("test_f3_3b_baseline: ALL OK")


if __name__ == "__main__":
    main()
