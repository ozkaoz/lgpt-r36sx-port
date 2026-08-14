#!/usr/bin/env python3
"""Bacon 1.4 - T8: STEREO_48K como contrato coherente del port.

configfs UAC2, scripts otg_*, bridge TreeFrog y daemon declaran la misma
frecuencia (48000), formato (S16_LE) y numero de canales (2).  Mono solo
existe como opt-in/fallback explicito (MONO_48K), nunca como baseline.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
bridge = (root / "source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp").read_text()
common = (root / "device/otg_u241_common.sh").read_text()
setup = (root / "device/otg_u241_setup_once.sh").read_text()
launcher = (root / "device/lgpt_launcher_u241.sh").read_text()
daemon = (root / "device/r36s_u2523_usb_audio_io.c").read_text()

# 1. Script comun: default STEREO_48K (2 canales), mono solo explicito.
assert "AUDIO_PROFILE=STEREO_48K" in common
stereo_case = common[common.index('STEREO_48K)'):]
assert "AUDIO_CHMASK=3" in stereo_case and "AUDIO_CHANNELS=2" in stereo_case
assert "MONO_48K)" in common and "AUDIO_CHANNELS=1" in common

# 2. Setup once: default STEREO_48K, esperado 2/3; mono = explicito.
mono_case = setup[setup.index('MONO_48K)'):]
assert "EXPECTED_CHANNELS=1" in mono_case and "EXPECTED_CHMASK=1" in mono_case
rest = setup[setup.index('REQUESTED_PROFILE=STEREO_48K'):]
assert "EXPECTED_CHANNELS=2" in rest and "EXPECTED_CHMASK=3" in rest

# 3. Bridge: solo un MONO_48K explicito selecciona 1 canal; default 2.
assert "only an explicit MONO_48K request" in bridge
req = bridge[bridge.index("requested_audio_channels"):]
assert 'file_contains(' in req and '"MONO_48K")' in req
assert "? 1" in req and ": 2;" in req

# 4. configfs: 48000 / ssize 2 / chmask 3 para el perfil stereo.
assert 'echo 48000 > functions/uac2.usb0/p_srate' in common
assert 'echo 48000 > functions/uac2.usb0/c_srate' in common
assert 'echo 2 > functions/uac2.usb0/p_ssize' in common
assert 'echo "$AUDIO_CHMASK" > functions/uac2.usb0/p_chmask' in common

# 5. Daemon: 2 canales / 48000 por defecto y desde el runtime file.
assert "static unsigned g_audio_channels = 2;" in daemon
assert "static unsigned g_audio_rate = 48000;" in daemon
assert 'AUDIO_CHANNELS' in daemon and 'AUDIO_RATE' in daemon

# 6. Launcher provisiona STEREO_48K como baseline del port.
assert "printf 'STEREO_48K\\n'" in launcher

print("TEST_BC14_STEREO_48K_COHERENT_CONTRACT_OK")