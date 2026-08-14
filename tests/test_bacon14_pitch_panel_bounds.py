#!/usr/bin/env python3
"""Bacon 1.4 - framebuffer/layout (T4): limites del panel del chopper.

La pantalla es 320x240 con fuente 8x8 (40x30 celdas).  Reglas golden:
- El clear del panel central del Pitch/Env (tf_rect) termina ANTES de la
  fila 22: rango aproximado y=60..175, nunca hasta y=239.
- La fila 22 es el marco (char frame), la 23 el status, 24-25 los hints:
  ningun clear pixel-level de panel puede solaparlas.
- El bloque del menu pitch vive dentro del panel: contentTop=8,
  contentBottom=22 (MakeCenteredMenuLayout(7,11,10,2,40,8,22)) y el
  titulo en startY-1.
- El overlay de operacion (Preview B / Apply / Redo-Undo) tambien queda
  dentro de filas 8..21.
- Ningun tf_rect de relleno de panel puede extenderse hasta y>=176
  (inicio de la fila 22) y mucho menos hasta y=239.
"""
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
chopper = (root / "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp").read_text()

# 1. El clear del panel pitch es y=60..175 (0,60,320,116).
assert "tf_rect(0, 60, 320, 116, tf_rgb565(10, 10, 24))" in chopper, \
    "panel pitch debe ser y=60..175 (tf_rect(0,60,320,116))"
pstart = chopper.index("void SampleChopperModal::drawPitchScreen")
pend = chopper.index("void SampleChopperModal::applyEnvelopeToBuffer", pstart)
pitch = chopper[pstart:pend]
# Los rects de relleno (con color tf_rgb565(10,10,24)) del pitch no pasan de y=175.
for m in re.finditer(r"tf_rect\((\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*tf_rgb565\(10,\s*10,\s*24\)\)", pitch):
    x, y, w, h = map(int, m.groups())
    assert y + h - 1 <= 175, f"panel pitch desborda a y={y + h - 1}: {m.group(0)}"

# 2. Marco del pitch: top y=59, bottom y=176 (linea de 1px, no relleno).
assert "tf_rect(0, 59, 320, 1, tf_rgb565(63, 95, 191))" in pitch
assert "tf_rect(0, 176, 320, 1, tf_rgb565(63, 95, 191))" in pitch

# 3. El bloque del menu: contentTop=8, contentBottom=22, titulo en startY-1.
assert "UiDraw::MakeCenteredMenuLayout(7, 11, 10, 2, 40, 8, 22)" in pitch
assert "DrawCenteredTitleAt(*this, ml.startY - 1, \"PITCH/ENV\")" in pitch
# Header en startY, labels/valores en startY+1..startY+6 -> filas 13..18.
assert "DrawString(ml.labelX, ml.startY, buffer, props)" in pitch
assert "DrawString(ml.labelX, ml.startY + 1 + i, ChopperView::PitchLabel(i), props)" in pitch

# 4. Status en fila 23 y hints en 24-25 (fuera del panel).
assert "drawStringAbs(2, 23, statusMessage_, props)" in chopper
assert "g.SetText(1, 24," in (root / "source/sources/Application/UI/Views/ModalDialogs/ChopperView.h").read_text()
assert "g.SetText(1, 25," in (root / "source/sources/Application/UI/Views/ModalDialogs/ChopperView.h").read_text()

# 5. Overlay de operacion: relleno y=64..175 + borde inferior y=176.
ostart = chopper.index("void SampleChopperModal::drawOperationOverlay")
oend = chopper.index("void SampleChopperModal::DrawView", ostart)
overlay = chopper[ostart:oend]
assert "tf_rect(0, 64, 320, 112, tf_rgb565(10, 10, 24))" in overlay
assert "tf_rect(0, 176, 320, 1, tf_rgb565(63, 95, 191))" in overlay
assert "ClearRect(0, 8, 40, 14)" in overlay, "el overlay limpia filas 8..21 del char screen"

# 6. Ningun relleno de panel (10,10,24) en todo el modal pasa de y=175.
for m in re.finditer(r"tf_rect\((\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*tf_rgb565\(10,\s*10,\s*24\)\)", chopper):
    x, y, w, h = map(int, m.groups())
    assert y + h - 1 <= 175, f"panel desborda a y={y + h - 1}: {m.group(0)}"
# Ningun rect del modal baja de y=192 (fila 24, hints) ni a y=239.
for m in re.finditer(r"tf_rect\((\d+),\s*(\d+),\s*(\d+),\s*(\d+),", chopper):
    x, y, w, h = map(int, m.groups())
    if h <= 0:
        continue
    assert y < 192, f"rect invade la zona de hints (y={y}): {m.group(0)}"

# 7. El clear de texto del chopper sigue cubriendo la pantalla completa
#    (char screen), no es un clear pixel-level.
assert "View::ClearRect(0, 0, SCREEN_W, SCREEN_H)" in chopper

print("TEST_BC14_PITCH_PANEL_BOUNDS_OK")