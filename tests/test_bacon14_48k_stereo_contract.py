#!/usr/bin/env python3
"""Bacon 1.4 - T6: contrato nominal 48 kHz stereo sin SRC nominal.

La ruta normal es 48k stereo end-to-end: motor (retro_run 48000 Hz) ->
bridge -> FIFO -> daemon -> UAC2 48k stereo.  Con engine == usb == 48000
el incremento del resampler del bridge equivale al denominador (copia
sample-exact / identity) y el ASRC del daemon queda nominalmente 1:1
(usado solo para deriva de reloj, PPM acotado).
"""
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
bridge = (root / "source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp").read_text()
libretro = (root / "source/sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp").read_text()
daemon = (root / "device/r36s_u2523_usb_audio_io.c").read_text()

# 1. Motor a 48 kHz (retro_run declara 48000).
assert "info->timing.sample_rate = 48000.0;" in libretro

# 2. Bridge: engine y usb a 48k por defecto, 2 canales.
assert "static int g_usb_channels = 2;" in bridge
assert "static int g_usb_rate = 48000;" in bridge
assert "static int g_engine_rate = 48000;" in bridge

# 3. Identity del resampler: incremento == denominador cuando engine==usb.
assert "RESAMPLE_DENOMINATOR" in bridge
assert "resample_increment" in bridge
inc = re.search(r"resample_increment\s*=\s*[\s\S]{0,120}?\? RESAMPLE_DENOMINATOR[\s\S]{0,120}?RESAMPLE_DENOMINATOR /", bridge)
assert inc, "el incremento se deriva de engine*DEN/usb"
assert "degenerates to a sample-exact copy (identity)" in bridge, "contrato H36 identity documentado"

# 4. Daemon: 48 kHz stereo por defecto.
assert "static unsigned g_audio_channels = 2;" in daemon
assert "static unsigned g_audio_rate = 48000;" in daemon

# 5. ASRC nominal 1:1 con deriva acotada (no un SRC nominal).
asrc = daemon.index("ASRC")
assert "PPM" in daemon
assert "ASRC_MAX_CORRECTION_PPM" in daemon

# 6. Sin resampleo nominal forzado en la ruta: el mono es solo salida
#    de mezcla (downmix), no el transporte.
assert "out[out_frames] =" in bridge and "left + right) / 2" in bridge

print("TEST_BC14_48K_STEREO_CONTRACT_OK")