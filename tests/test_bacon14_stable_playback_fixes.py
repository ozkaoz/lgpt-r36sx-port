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
print("TEST_BC14_STABLE_PLAYBACK_OK")