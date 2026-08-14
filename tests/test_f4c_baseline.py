#!/usr/bin/env python3
"""F4c baseline: AudioRouter - politica declarativa de routing de backends.

Verifica que:
1. AudioRouter.h (Application/Audio) declara la politica golden de
   seleccion: AudioRouteEffectiveMode (USB_OUT + dir IN -> SP404_IN),
   AudioRouteIsHostRoleMode (derivado de la capacidad UsbHost), y el ciclo
   AudioRouteCycleNext/AudioRouteCyclePrev (secuencia UI de 5, SP404_IN no
   se lista).
2. AudioRouter.h es capa pura: solo include de las capas hermanas
   AudioDriverModeTable.h y AudioCapabilities.h; sin GUI/audio/daemons/
   POSIX/framebuffer.
3. El bridge consume AudioRouter (include + uso en SetDriverMode y
   CycleDriverMode) y conserva la politica runtime intacta (debounce,
   fast-apply, close_fifo, launch_apply_profile_once) delegando solo la
   decision declarativa.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROUTER = ROOT / "source/sources/Application/Audio/AudioRouter.h"
BRIDGE = ROOT / "source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp"

TOKENS = [
    "AudioRouteEffectiveMode", "AudioRouteIsHostRoleMode",
    "AudioRouteCycleNext", "AudioRouteCyclePrev",
    "kAudioDriverModeUsbOut", "kAudioDriverModeSp404In",
    "kAudioCapUsbHost", "kAudioDriverModeUiCount",
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
    "launch_apply_profile_once", "runtime_ready_fast", "monotonic",
]


def check_router_layer():
    r = ROUTER.read_text()
    for token in TOKENS:
        assert token in r, token
    for token in FORBIDDEN:
        assert token not in r, token
    # Solo los dos includes permitidos (capas hermanas puras).
    includes = [l.strip() for l in r.splitlines() if l.strip().startswith("#include")]
    assert includes == [
        '#include "Application/Audio/AudioDriverModeTable.h"',
        '#include "Application/Audio/AudioCapabilities.h"',
    ], includes
    print("router layer guards OK")


def check_golden_policy():
    r = ROUTER.read_text()
    # Mapeo efectivo golden.
    assert "samplerDirectionIn == 1" in r
    assert "kAudioDriverModeSp404In" in r
    # Host-role derivado de la capacidad UsbHost (misma fuente de verdad).
    assert "(AudioDriverModeCapabilities(mode, 0) & kAudioCapUsbHost) != 0" in r
    # Ciclo golden: secuencia UI de 5 modos.
    assert "kAudioDriverModeUiCount" in r
    print("router golden policy OK")


def check_bridge_consumes():
    b = BRIDGE.read_text()
    assert '#include "Application/Audio/AudioRouter.h"' in b
    assert "AudioRouteEffectiveMode(mode, g_sampler_direction_in)" in b
    assert "AudioRouteIsHostRoleMode(g_driver_mode)" in b
    assert "AudioRouteCycleNext(g_driver_mode)" in b
    # La politica runtime sigue intacta en el bridge.
    for kept in [
        "g_pending_driver_mode", "g_last_mode_change_ms", "debounce",
        "close_fifo_if_open", "launch_apply_profile_once",
        "runtime_ready_fast()", "fifo_compatible_with_mode",
        "write_mode_file", "if (g_driver_mode == mode)",
    ]:
        assert kept in b, kept
    print("bridge consumes router, runtime policy intact OK")


check_router_layer()
check_golden_policy()
check_bridge_consumes()
print("F4C_BASELINE_OK")