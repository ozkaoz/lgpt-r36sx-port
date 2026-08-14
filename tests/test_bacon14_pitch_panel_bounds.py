#!/usr/bin/env python3
"""Bacon 1.4 - framebuffer/layout (T4): limites del panel del chopper.

La pantalla es 320x240 con fuente 8x8 (40x30 celdas).  Reglas golden:
- El clear del panel central del Pitch/Env del GUI (drawPitchScreen, otras
  plataformas) termina ANTES de la fila 22: y=60..175.
- U2.53: en TreeFrog el panel Pitch/Env se dibuja por el overlay direct-FB
  a PANTALLA COMPLETA (0,0,320,240): fondo + marco en los bordes, titulo
  fila 2 (y=16), header fila 4 (y=32), items filas 10..15 (y=80+8*i),
  hints filas 26-27 (y=208/216) y status fila 28 (y=224) dentro del panel.
  El panel tapa la cabecera del chopper (graphical chopper, inst, sampl...),
  el status y los hints del char screen.
- El overlay de operacion (Preview B / Apply / Redo-Undo) queda en filas
  8..21 del GUI (otras plataformas).
"""
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
chopper = (root / "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp").read_text()
chview = (root / "source/sources/Application/UI/Views/ModalDialogs/ChopperView.h").read_text()

# 1. El clear del panel pitch del GUI es y=60..175 (0,60,320,116).
assert "tf_rect(0, 60, 320, 116, tf_rgb565(10, 10, 24))" in chopper, \
    "panel pitch del GUI debe ser y=60..175 (tf_rect(0,60,320,116))"
pstart = chopper.index("void SampleChopperModal::drawPitchScreen")
pend = chopper.index("void SampleChopperModal::applyEnvelopeToBuffer", pstart)
pitch = chopper[pstart:pend]
for m in re.finditer(r"tf_rect\((\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*tf_rgb565\(10,\s*10,\s*24\)\)", pitch):
    x, y, w, h = map(int, m.groups())
    assert y + h - 1 <= 175, f"panel pitch del GUI desborda a y={y + h - 1}: {m.group(0)}"

# 2. Marco del pitch del GUI: top y=59, bottom y=176.
assert "tf_rect(0, 59, 320, 1, tf_rgb565(63, 95, 191))" in pitch
assert "tf_rect(0, 176, 320, 1, tf_rgb565(63, 95, 191))" in pitch

# 3. El bloque del menu del GUI: contentTop=8, contentBottom=22, titulo en startY-1.
assert "UiDraw::MakeCenteredMenuLayout(7, 11, 10, 2, 40, 8, 22)" in pitch
assert "DrawCenteredTitleAt(*this, ml.startY - 1, \"PITCH/ENV\")" in pitch
assert "DrawString(ml.labelX, ml.startY, buffer, props)" in pitch
assert "DrawString(ml.labelX, ml.startY + 1 + i, ChopperView::PitchLabel(i), props)" in pitch

# 4. Status en fila 23 y hints en 24-25 del char screen (GUI, otras plataformas).
assert "drawStringAbs(2, 23, statusMessage_, props)" in chopper
assert "g.SetText(1, 24," in chview
assert "g.SetText(1, 25," in chview

# 5. Overlay de operacion del GUI: relleno y=64..175 + borde inferior y=176.
ostart = chopper.index("void SampleChopperModal::drawOperationOverlay")
oend = chopper.index("void SampleChopperModal::DrawView", ostart)
overlay = chopper[ostart:oend]
assert "tf_rect(0, 64, 320, 112, tf_rgb565(10, 10, 24))" in overlay
assert "tf_rect(0, 176, 320, 1, tf_rgb565(63, 95, 191))" in overlay
assert "ClearRect(0, 8, 40, 14)" in overlay

# 6. U2.53: overlay TreeFrog del pitch = pantalla completa, tapa la cabecera.
ovstart = chopper.index("extern \"C\" void TreeFrogChopperOverlayDraw(void) {")
ovend = chopper.index("#else", ovstart)
ov = chopper[ovstart:ovend]
assert "if (g_chopperPitchActive) {" in ov
assert "tf_rect(0, 0, 320, 240, pbg);" in ov, "fondo a pantalla completa"
assert "tf_rect(0, 0, 320, 1, pframe);" in ov, "marco superior y=0"
assert "tf_rect(0, 239, 320, 1, pframe);" in ov, "marco inferior y=239"
assert "tf_rect(0, 1, 1, 238, pframe);" in ov, "marco izquierdo"
assert "tf_rect(319, 1, 1, 238, pframe);" in ov, "marco derecho"
assert 'tf_text(((40 - (int)strlen("PITCH/ENV")) / 2) * 8, 16, "PITCH/ENV", ph1, pbg, 0);' in ov, "titulo fila 2"
assert "tf_text(((40 - (int)strlen(g_chopperPitchHeader)) / 2) * 8, 32, g_chopperPitchHeader, pnorm, pbg, 0);" in ov, "header fila 4"
assert "int y = 80 + i * 8;" in ov, "items filas 10..15 (y=80..128)"
assert "tf_text(64, y, g_chopperPitchLabels[i], pnorm, pbg, 0);" in ov
assert "tf_text(168, y, g_chopperPitchValues[i], sel ? ph2 : ph1, pbg, sel);" in ov
assert "g_chopperPitchHints[0]" in ov and "208" in ov, "hint 1 fila 26 dentro del panel"
assert "g_chopperPitchHints[1]" in ov and "216" in ov, "hint 2 fila 27 dentro del panel"
assert "g_chopperPitchStatus" in ov and "224" in ov, "status fila 28 dentro del panel"
# Todo el contenido del overlay pitch queda dentro de 0..240.
for m in re.finditer(r"tf_rect\((\d+),\s*(\d+),\s*(\d+),\s*(\d+),", ov):
    x, y, w, h = map(int, m.groups())
    if h <= 0 or w <= 0:
        continue
    assert y >= 0 and y + h <= 240, f"overlay pitch fuera de pantalla: {m.group(0)}"
for m in re.finditer(r"tf_text\((\d+),\s*(\d+),", ov):
    x, y = map(int, m.groups())
    assert y >= 0 and y + 8 <= 240, f"texto overlay fuera de pantalla (y={y})"

# 7. El clear de texto del chopper sigue cubriendo la pantalla completa.
assert "View::ClearRect(0, 0, SCREEN_W, SCREEN_H)" in chopper

print("TEST_BC14_PITCH_PANEL_BOUNDS_OK")