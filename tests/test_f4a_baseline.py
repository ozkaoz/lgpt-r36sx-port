#!/usr/bin/env python3
"""F4a baseline: tabla declarativa de modos de audio del driver UAC2.

Verifica que:
1. AudioDriverModeTable.h (Application/Audio) declara los 6 modos golden del
   driver (ids 0..5 identicos al enum U241_* del bridge) como datos puros:
   nombre, descripcion, token de modo, token de politica OTG, rama del
   daemon, selectable y capacidades declarativas de direccion (out/in) con
   el toggle del sampler como parametro.  Conteo de UI = 5 (SP404_IN no se
   lista), fallback default = LOCAL_CONSOLE.
2. AudioDriverModeTable.h NO depende de GUI/audio/daemons/POSIX/framebuffer
   (capa pura: solo tipos integrados de C++03, sin includes).
3. TreeFrogUac2Bridge.cpp delega: incluye la capa pura y sus helpers
   static (mode_name, mode_desc, mode_token, policy_token,
   branch_name_for_mode, selectable_mode, mode_has_out/mode_has_in) y
   GetDriverModeCount son delegados one-line; los literales golden ya no
   viven en el bridge (MOVED_OUT).
4. El enum U241_* del bridge sigue declarando los mismos ids que la capa
   pura (contrato de valores intacto).
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
TABLE = ROOT / "source/sources/Application/Audio/AudioDriverModeTable.h"
BRIDGE = ROOT / "source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp"

TOKENS = [
    "kAudioDriverModeLocalConsole", "kAudioDriverModeWindows",
    "kAudioDriverModeAndroid", "kAudioDriverModeUsbOut",
    "kAudioDriverModeMidi", "kAudioDriverModeSp404In",
    "kAudioDriverModeUiCount", "AudioDriverModeInfo",
    "kAudioDriverModes", "kAudioDriverModeFallback",
    "AudioDriverModeInfoFor", "AudioDriverModeName",
    "AudioDriverModeDescription", "AudioDriverModeToken",
    "AudioDriverModePolicyToken", "AudioDriverModeBranchName",
    "AudioDriverModeIsSelectable", "AudioDriverModeHasOut",
    "AudioDriverModeHasIn", "AudioDriverModeCount",
]

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "DrawString", "GUITextProperties", "AppWindow", "MixerService",
    "GetInstance", "ColorDefinition", "EPBM_", "TreeFrogGetFramebuffer",
    "viewData_", "View::SetNotification", "GUIRect", "isDirty_",
    "unistd.h", "fcntl.h", "stdio.h", "stdlib.h", "string.h", "sys/",
    "open(", "mkfifo", "mkdir(", "snprintf", "fprintf",
    "TreeFrogUac2Bridge_", "g_driver_mode", "g_sampler_direction_in",
    "U241_", "TREEFROG_UAC2_BRIDGE",
]

GOLDEN_STRINGS = [
    '"Local Console"', '"Console sound, OTG may stay connected"',
    '"Duplex UAC2 gadget (PC host)"',
    '"Duplex UAC2 gadget (phone host)"',
    '"SP404: console sound to sampler (EXT SOURCE)"',
    '"SP404 IN: sampler->console, recording only"',
    '"MIDI: USB piano/controller"',
    '"USB_DUPLEX_OTG"', '"USB_IN_OTG"', '"USB_OUT_OTG"', '"MIDI_OTG"',
    '"audio_driver_usb_duplex"', '"audio_driver_usb_in"',
    '"audio_driver_usb_out"', '"audio_driver_sp404_in"',
    '"audio_driver_midi"', '"audio_driver_local_console"',
]

DELEGATES = [
    "static const char *mode_name(int mode) { return AudioDriverModeName(mode); }",
    "static const char *mode_token(int mode) { return AudioDriverModeToken(mode); }",
    "static const char *policy_token(int mode) { return AudioDriverModePolicyToken(mode); }",
    "static const char *branch_name_for_mode(int mode) { return AudioDriverModeBranchName(mode); }",
    "static const char *mode_desc(int mode) {\n    return AudioDriverModeDescription(mode);\n}",
    "static int selectable_mode(int mode) {\n    return AudioDriverModeIsSelectable(mode);\n}",
    "static int mode_has_out(int mode) {\n    return AudioDriverModeHasOut(mode, g_sampler_direction_in);\n}",
    "static int mode_has_in(int mode) {\n    return AudioDriverModeHasIn(mode, g_sampler_direction_in);\n}",
    "int TreeFrogUac2Bridge_GetDriverModeCount(void) { return AudioDriverModeCount(); }",
]


def check_layer():
    t = TABLE.read_text()
    for token in TOKENS:
        assert token in t, token
    for token in FORBIDDEN:
        assert token not in t, token
    # F4a es una capa pura: el unico include permitido es la capa hermana
    # AudioCapabilities.h (F4b), tambien pura.  Nada mas.
    for line in t.splitlines():
        stripped = line.strip()
        if stripped.startswith("#include"):
            assert stripped == '#include "Application/Audio/AudioCapabilities.h"', stripped
    print("layer guards OK")


def check_golden_values():
    t = TABLE.read_text()
    for s in GOLDEN_STRINGS:
        assert s in t, s
    assert "static const AudioDriverModeInfo kAudioDriverModes[6]" in t
    assert "static const AudioDriverModeInfo kAudioDriverModeFallback" in t
    print("golden values present OK")


def check_bridge_delegates():
    b = BRIDGE.read_text()
    assert '#include "Application/Audio/AudioDriverModeTable.h"' in b
    for d in DELEGATES:
        assert d in b, d
    for s in GOLDEN_STRINGS:
        assert s not in b, s
    print("bridge delegates OK")


def check_enum_contract():
    b = BRIDGE.read_text()
    m = re.search(r"enum\s+\w*\s*\{([^}]*)\}", b)
    assert m, "U241 enum not found"
    body = m.group(1)
    pairs = {
        "U241_LOCAL_CONSOLE": "kAudioDriverModeLocalConsole",
        "U241_WINDOWS": "kAudioDriverModeWindows",
        "U241_ANDROID": "kAudioDriverModeAndroid",
        "U241_USB_OUT": "kAudioDriverModeUsbOut",
        "U241_MIDI": "kAudioDriverModeMidi",
        "U241_SP404_IN": "kAudioDriverModeSp404In",
    }
    t = TABLE.read_text()
    for bridge_enum, layer_const in pairs.items():
        vm = re.search(rf"{bridge_enum}\s*=\s*(\d+)", body)
        assert vm, f"{bridge_enum} value not found"
        tm = re.search(rf"{layer_const}\s*=\s*(\d+)", t)
        assert tm, f"{layer_const} value not found"
        assert int(vm.group(1)) == int(tm.group(1)), (
            f"{bridge_enum} != {layer_const}")
    assert "U241_USB_DUPLEX = U241_WINDOWS" in body
    assert "U241_USB_IN = U241_ANDROID" in body
    print("enum contract OK")


check_layer()
check_golden_values()
check_bridge_delegates()
check_enum_contract()
print("F4A_BASELINE_OK")
