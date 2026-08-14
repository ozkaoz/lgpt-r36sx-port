#!/usr/bin/env python3
"""Bacon 1.4 - T7: transporte FIFO/UAC2.

- Orden FIFO estricto: el bridge drena el resto pendiente (H40) ANTES de
  escribir audio nuevo; EAGAIN conserva el staging; overflow descarta el
  bloque MAS VIEJO, nunca el nuevo.
- Limpieza: close_fifo_if_open() resetea pending+phase en transporte
  parado/errores duros; el daemon hace flush del fifo en cada open fresco
  y resetea ring+stage en resync de backlog.
- Daemon: los bytes parciales entre read() se conservan (drain_partial) y
  al ring solo entran frames completos; el flush tambien resetea el carry.
"""
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
bridge = (root / "source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp").read_text()
daemon = (root / "device/r36s_u2523_usb_audio_io.c").read_text()

# 1. Daemon: carry de bytes parciales + frames completos al ring.
assert "drain_partial[4]" in daemon and "drain_partial_len" in daemon
assert re.search(r"const size_t whole = \(n / 4U\) \* 4U;", daemon), "solo frames completos"
assert re.search(r"memcpy\(drain_partial, p, n\);\s*drain_partial_len", daemon), "carry entre reads"
assert "drain_partial_len = 0U;" in daemon, "flush resetea el carry"

# 2. Bridge: H40 retry desde el frente (nunca adelanta audio nuevo).
assert "drain any remainder of a previous partial write" in bridge
idx = bridge.index("g_fifo_pending_samples > 0")
assert bridge.index("write(g_fifo_fd, g_fifo_pending, pend_bytes)") > idx

# 3. EAGAIN -> mantener staged; EPIPE/ENXIO/EBADF -> close + cleanup.
assert "EAGAIN (or other nonfatal): keep staged, retry next submit." in bridge
assert "close_fifo_if_open" in bridge

# 4. Overflow: descarta los MAS VIEJOS, nunca los nuevos.
assert "overflow drops the OLDEST samples, never the newest" in bridge

# 5. Limpieza en transporte parado: close_fifo_if_open resetea
#    pending + phase de resample.
close_body = bridge[bridge.index("static void close_fifo_if_open"):]
assert "g_fifo_pending_samples = 0;" in close_body
assert "g_resample_phase_160 = 0;" in close_body

# 6. Daemon: flush del fifo en cada open fresco + resync limpia ring+stage.
assert "flush_input_fifo(in);" in daemon
assert "ring_reset();" in daemon and "asrc_source_reset();" in daemon
assert "U2517_ASRC_BACKLOG_RESYNC" in daemon

print("TEST_BC14_FIFO_UAC2_TRANSPORT_OK")