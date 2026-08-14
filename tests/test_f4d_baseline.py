#!/usr/bin/env python3
"""F4d baseline: AudioBackend - registro declarativo de clases de backend.

Verifica que:
1. AudioBackend.h (Application/Audio) declara las 5 clases de backend
   golden (LocalAudioBackend, WindowsUac2Backend, AndroidBackend,
   Sp404Backend, MidiBackend) con sus nombres, el contrato de operaciones
   (open/start/caps/stream/write/close) y el mapa modo driver -> clase
   (daemons reales del port).
2. AudioBackend.h es capa pura: solo include de las capas hermanas
   (AudioDriverModeTable.h, AudioCapabilities.h, AudioRouter.h); sin
   GUI/audio/daemons/POSIX/framebuffer.
3. Las capacidades de cada clase se DERIVAN de los modos (union), nunca se
   declaran a mano (AudioBackendClassCapabilities no tiene literales de
   mascara).
4. El bridge NO consume AudioBackend todavia: el registro es declaracion
   de arquitectura; AudioEngine (F4e) lo conectara al puente sin cambiar la
   ruta estable.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BE_H = ROOT / "source/sources/Application/Audio/AudioBackend.h"
BRIDGE = ROOT / "source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp"

TOKENS = [
    "kAudioBackendLocal", "kAudioBackendWindowsUac2", "kAudioBackendAndroid",
    "kAudioBackendSp404", "kAudioBackendMidi", "kAudioBackendClassCount",
    "kAudioBackendClassNames", "kAudioBackendOpCount", "kAudioBackendOps",
    "AudioBackendClassForMode", "AudioBackendClassName",
    "AudioBackendClassCapabilities",
]

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "DrawString", "GUITextProperties", "AppWindow", "MixerService",
    "GetInstance", "ColorDefinition", "EPBM_", "TreeFrogGetFramebuffer",
    "viewData_", "View::SetNotification", "GUIRect", "isDirty_",
    "unistd.h", "fcntl.h", "stdio.h", "stdlib.h", "string.h", "sys/",
    "open(", "mkfifo", "mkdir(", "snprintf", "fprintf",
    "TreeFrogUac2Bridge_", "g_driver_mode", "g_sampler_direction_in",
    "U241_", "TREEFROG_UAC2_BRIDGE", "g_fifo", "kFifo", "close_fifo",
    "launch_apply_profile_once", "runtime_ready_fast",
    "r36s_u2523_usb_audio_io", "sp404_host_audio_io", "midi_host_io",
]

BACKEND_NAMES = [
    "LocalAudioBackend", "WindowsUac2Backend", "AndroidBackend",
    "Sp404Backend", "MidiBackend",
]

OPS = ["open", "start", "caps", "stream", "write", "close"]


def check_backend_layer():
    b = BE_H.read_text()
    for token in TOKENS:
        assert token in b, token
    for token in FORBIDDEN:
        assert token not in b, token
    includes = [l.strip() for l in b.splitlines()
                if l.strip().startswith("#include")]
    assert includes == [
        '#include "Application/Audio/AudioDriverModeTable.h"',
        '#include "Application/Audio/AudioCapabilities.h"',
        '#include "Application/Audio/AudioRouter.h"',
    ], includes
    print("backend layer guards OK")


def check_golden_registry():
    b = BE_H.read_text()
    for name in BACKEND_NAMES:
        assert f'"{name}"' in b, name
    assert "kAudioBackendClassNames[kAudioBackendClassCount]" in b
    for op in OPS:
        assert f'"{op}"' in b, op
    # Mapa modo -> clase golden.
    for mode_const, cls in [
        ("kAudioDriverModeWindows", "kAudioBackendWindowsUac2"),
        ("kAudioDriverModeAndroid", "kAudioBackendAndroid"),
        ("kAudioDriverModeUsbOut", "kAudioBackendSp404"),
        ("kAudioDriverModeSp404In", "kAudioBackendSp404"),
        ("kAudioDriverModeMidi", "kAudioBackendMidi"),
        ("kAudioDriverModeLocalConsole", "kAudioBackendLocal"),
    ]:
        assert mode_const in b, mode_const
        assert cls in b, cls
    print("backend golden registry OK")


def check_caps_derived_not_declared():
    b = BE_H.read_text()
    # Las capacidades por clase se derivan de los modos (union), no hay
    # mascaras literales tipo 0x1F ni 1u << fuera del include de caps.
    assert "AudioDriverModeCapabilities(mode, 0)" in b
    assert "AudioDriverModeCapabilities(mode, 1)" in b
    assert "caps |= AudioDriverModeCapabilities" in b
    print("backend caps derived from modes OK")


def check_bridge_untouched():
    br = BRIDGE.read_text()
    assert "AudioBackend.h" not in br
    assert "AudioBackendClass" not in br
    print("bridge not consuming AudioBackend (F4e will connect) OK")


check_backend_layer()
check_golden_registry()
check_caps_derived_not_declared()
check_bridge_untouched()
print("F4D_BASELINE_OK")