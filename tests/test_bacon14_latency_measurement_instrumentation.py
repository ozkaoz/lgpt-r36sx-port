#!/usr/bin/env python3
"""Bacon 1.4 - T9: latencia por medicion, no por ensayo y error.

El daemon emite telemetria periodica (BRIDGE_PROGRESS_U2517_ASRC cada 200
periodos) con backlog + min/max, step_ppm, xruns, dropped, clock_hold,
poll_timeouts, short_writes, reconnects, prepare_failures y ahora
avail_min/avail_max (ALSA available frames).  Las constantes de tuning se
mantienen sin cambios (objetivo 2400 frames = 50 ms @48k, correccion
maxima 1200 ppm, ASRC nominal 1:1) hasta que la medicion indique lo
contrario.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
daemon = (root / "device/r36s_u2523_usb_audio_io.c").read_text()

# 1. Telemetria de latencia completa.
for token in [
    "BRIDGE_PROGRESS_U2517_ASRC",
    "backlog_min", "backlog_max", "step_ppm",
    "xruns", "dropped", "clock_hold", "poll_timeouts",
    "short_writes", "prepare_failures", "reconnects",
    "avail_min", "avail_max",
]:
    assert token in daemon, token

# 2. ALSA available frames vía ioctl (solo telemetria).
assert "SNDRV_PCM_IOCTL_STATUS" in daemon
assert "pcm_avail_frames" in daemon

# 3. Eventos de resync / starvation / underrun logueados.
for token in [
    "U2517_ASRC_BACKLOG_RESYNC",
    "PLAYBACK_ASRC_CLOCK_HOLD",
    "U2517_PCM_PLAY_FIFO_FLUSHED_ON_OPEN",
    "PLAYBACK_SHORT_WRITE",
]:
    assert token in daemon, token

# 4. Constantes de tuning sin cambios (medicion primero).
assert "ASRC_TARGET_BACKLOG_FRAMES 2400U" in daemon
assert "ASRC_PRIME_BACKLOG_FRAMES 2400U" in daemon
assert "ASRC_MAX_CORRECTION_PPM 1200" in daemon
assert "asrc_nominal_step_q32" in daemon

print("TEST_BC14_LATENCY_MEASUREMENT_INSTRUMENTATION_OK")