#!/usr/bin/env python3
"""Bacon 1.4 - estabilidad de reproduccion (entrecortado en Windows).

- Daemon: guard de flush en reopens rapidos.  El reset de transporte de
  Windows (UAC2 re-enum) cierra/abre el PCM en pocos ms; el flush
  incondicional descartaba el fifo fresco (50-100 ms de audio) + re-prime
  = cortes audibles.  Ahora solo se flushea si el PCM estuvo caido >= 250 ms
  (caso real de backlog stale U2.63.1); un reopen rapido conserva fifo y
  ring (el drenado continuo mantiene el anillo lleno) => continuidad.
- Chopper: el menu Pitch/Envelope debe ser visible aunque el sample este
  detenido.  El gate `hasWaveform_` (solo refleja el ultimo build del
  waveform) se retira de togglePitchMode; las operaciones de pitch siguen
  validando los buffers del sample.
"""
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
daemon = (root / "device/r36s_u2523_usb_audio_io.c").read_text()
chopper = (root / "source/sources/Application/UI/Views/ModalDialogs/SampleChopperModal.cpp").read_text()

# 1. Daemon: reloj de caida del PCM.
assert "long long pcm_down_since_ms = 0;" in daemon, "reloj de caida del PCM"

# 2. Daemon: marcar caida en todos los cierres.
assert "pcm_down_since_ms = monotonic_milliseconds();" in daemon
n_closes = daemon.count("pcm_down_since_ms = monotonic_milliseconds();")
assert n_closes >= 2, f"cierre de caida registrado en {n_closes} puntos"

# 3. Daemon: guard de flush en el open fresco (solo backlog stale).
assert "PCM_PLAY_REOPEN_KEEP_FIFO" in daemon, "log de conservacion de fifo"
assert re.search(r"now_ms - pcm_down_since_ms\) >= 250LL", daemon), "umbral 250 ms"
assert "stale_backlog" in daemon

# 4. Daemon: el flush sigue existiendo para el caso stale (U2.63.1).
assert "flush_input_fifo(in);" in daemon

# 7. Daemon: estabilidad del stream (diagnostico de la SD del 14-08-2026).
#    El print BRIDGE_PROGRESS cada 200 periodos (2 s, ~510 B a la SD)
#    detenia el daemon y underflowaba el buffer ALSA (secuencia print ->
#    POLLERR -> close+reopen cada 2 s en el log U2517_USB_AUDIO_DAEMON.log:
#    121 reopens en 24 s, down_ms=132-140, xruns=14, backlog oscilando
#    350<->24000, clock_hold=160).  Fixes: print a 2000 periodos (20 s),
#    buffer ALSA 8 periodos (80 ms), y recuperacion IN PLACE (PREPARE) en
#    el poll error antes de close+reopen.
assert re.search(r"const int periods = 8;", daemon), "buffer ALSA 8 periodos"
assert re.search(r"period_writes % 2000\) == 0", daemon), "progreso a 2000 periodos"
assert "recover_xrun_in_place(pcm, 0, EPIPE);" in daemon, "recuperacion in-place"
assert re.search(r"if \(inplace == 0\) \{\n\s+good_write_streak = 0;\n\s+continue;\n\s+\}", daemon), "continuar sin close en recuperacion in-place"
assert "(disconnect real)" in daemon, "comentario BACON14 del poll-error"

# 5. Chopper: togglePitchMode sin el gate hasWaveform_ (el sample detenido
#    no bloquea la entrada al menu Pitch/Envelope).
togg = chopper[chopper.index("void SampleChopperModal::togglePitchMode()"):]
togg = togg[:togg.index("publishOverlayState") + len("publishOverlayState")] if "publishOverlayState" in togg else togg[:4000]
assert "No sample for pitch" in togg, "sigue exigiendo sample asignado"
assert re.search(r"hasWaveform_\), \{ setStatus\(\"No waveform loaded\"\)", togg) is None, "gate hasWaveform_ eliminado"
assert "No waveform loaded" not in togg, "texto del gate eliminado del toggle"
assert "removing the waveform gate is safe" in togg, "comentario BACON14 presente"
assert re.search(r"sourceSize_ <= 1 \|\| sourceChannels_ <= 0 \|\| sourceRate_ <= 0", togg), "sigue validando sample util"

# 6. Chopper: el panel de pitch se dibuja con pitchMode_ (independiente del
#    waveform): drawPitchScreen existe y DrawView no condiciona pitch a
#    hasWaveform_.
assert "void SampleChopperModal::drawPitchScreen" in chopper
dv = chopper[chopper.index("void SampleChopperModal::DrawView"):]
dv = dv[:dv.index("void SampleChopperModal::ProcessButtonMask")]
assert "if (!hasWaveform_ && !pitchMode_)" in dv, "DrawView: pitch prevalece sobre waveform"

# 8. Chopper: BACON14 PITCH OVERLAY (U2.52).  Con el proyecto detenido el
#    char screen no se refresca de forma fiable en R36S (el toggle solo
#    actualiza el status), asi que el panel Pitch/Env se renderiza en el
#    overlay direct-FB cada frame (mismo mecanismo que el waveform).
assert '#include "UIFramework/BasicDatas/FontConfig.h"' in chopper, "fuente global disponible"
assert "static int g_chopperPitchActive = 0;" in chopper
assert "static int g_chopperPitchSelected = 0;" in chopper
assert "static char g_chopperPitchHeader[40];" in chopper
assert "static char g_chopperPitchLabels[6][24];" in chopper
assert "static char g_chopperPitchValues[6][20];" in chopper
assert "static char g_chopperPitchHints[2][40];" in chopper, "hints del panel full-screen"
assert "static char g_chopperPitchStatus[40];" in chopper, "status del panel full-screen"
assert re.search(r"static void tf_text\(int x, int y, const char \*s, unsigned short fg, unsigned short bg, int invert\) \{\n", chopper), "render de texto a pixeles"
assert "&font[*c * 8];" in chopper, "glifos de la fuente global 8x8"
assert "g[r * FONT_WIDTH + b] == 0" in chopper, "ZERO_IS_INK: tinta = byte 0 (TreeFrogGUIWindowImp.cpp:13)"
assert "ZERO_IS_INK (TreeFrogGUIWindowImp.cpp:13)" in chopper
ov = chopper[chopper.index("extern \"C\" void TreeFrogChopperOverlayDraw(void) {"):]
ov = ov[:ov.index("static void SampleChopperModal::clearOverlayState")] if "static void SampleChopperModal::clearOverlayState" in ov else ov[:6000]
assert "if (g_chopperPitchActive) {" in ov, "rama del panel en el overlay"
assert "tf_rect(0, 0, 320, 240, pbg);" in ov, "panel a pantalla completa"
assert "tf_rect(0, 239, 320, 1, pframe);" in ov, "marco inferior"
assert 'tf_text(((40 - (int)strlen("PITCH/ENV")) / 2) * 8, 16, "PITCH/ENV", ph1, pbg, 0);' in ov, "titulo centrado fila 2"
assert "tf_text(((40 - (int)strlen(g_chopperPitchHeader)) / 2) * 8, 32, g_chopperPitchHeader, pnorm, pbg, 0);" in ov, "header centrado fila 4"
assert "int y = 80 + i * 8;" in ov, "items filas 10..15"
assert "tf_text(64, y, g_chopperPitchLabels[i], pnorm, pbg, 0);" in ov
assert "tf_text(168, y, g_chopperPitchValues[i], sel ? ph2 : ph1, pbg, sel);" in ov, "valor seleccionado invertido (HILITE2)"
assert "tf_text(((40 - (int)strlen(g_chopperPitchHints[0])) / 2) * 8, 208, g_chopperPitchHints[0], pnorm, pbg, 0);" in ov, "hint 1 fila 26"
assert "tf_text(((40 - (int)strlen(g_chopperPitchHints[1])) / 2) * 8, 216, g_chopperPitchHints[1], pnorm, pbg, 0);" in ov, "hint 2 fila 27"
assert "tf_text(((40 - (int)strlen(g_chopperPitchStatus)) / 2) * 8, 224, g_chopperPitchStatus, ph1, pbg, 0);" in ov, "status fila 28"
pub = chopper[chopper.index("void SampleChopperModal::publishOverlayState()"):]
pub = pub[:pub.index("void SampleChopperModal::clearOverlayState()")]
assert "g_chopperPitchActive = (!suspended_ && !operationActive_ && pitchMode_) ? 1 : 0;" in pub, "gate del panel (suspended/operation)"
assert "g_chopperPitchSelected = pitchEnvTool_.EditParam();" in pub
assert "ChopperView::ComposeHeaderLine(g_chopperPitchHeader, sizeof(g_chopperPitchHeader)," in pub
assert "ChopperView::PitchLabel(i)" in pub
assert "ChopperView::ComposePitchValue(g_chopperPitchValues[i], sizeof(g_chopperPitchValues[i]), i," in pub
assert 'snprintf(g_chopperPitchHints[0], sizeof(g_chopperPitchHints[0]), "%s", ChopperView::PitchHint(0));' in pub, "hint 1 publicado"
assert 'snprintf(g_chopperPitchHints[1], sizeof(g_chopperPitchHints[1]), "%s", ChopperView::PitchHint(1));' in pub, "hint 2 publicado"
assert 'snprintf(g_chopperPitchStatus, sizeof(g_chopperPitchStatus), "%s", statusMessage_[0] ? statusMessage_ : "");' in pub, "status publicado"
clr = chopper[chopper.index("void SampleChopperModal::clearOverlayState()"):]
clr = clr[:clr.index("void SampleChopperModal::setPreviewPlaybackRange")]
assert "g_chopperPitchActive = 0;" in clr, "clearOverlayState apaga el panel"
print("TEST_BC14_STABLE_PLAYBACK_OK")