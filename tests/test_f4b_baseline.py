#!/usr/bin/env python3
"""F4b baseline: AudioCapabilities - vocabulario declarativo de capacidades.

Verifica que:
1. AudioCapabilities.h (Application/Audio) declara las 8 capacidades del
   objetivo 6 (Stereo Output, Stereo Input, USB Device, USB Host, MIDI,
   Capture, Clock Sync, Hotplug) como bits puros con sus nombres legibles,
   sin depender de GUI/audio/daemons/POSIX/framebuffer.
2. AudioDriverModeTable.h incluye SOLO AudioCapabilities.h y deriva
   AudioDriverModeCapabilities exclusivamente de los primitivos golden
   (AudioDriverModeHasOut/HasIn, identidad del modo y rol gadget vs
   host-role).  El bridge (TreeFrogUac2Bridge.cpp) NO consume capacidades
   todavia: la proyeccion es metadata declarativa, la ruta runtime no se
   toca (F4c conectara el AudioRouter).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CAP_H = ROOT / "source/sources/Application/Audio/AudioCapabilities.h"
TABLE = ROOT / "source/sources/Application/Audio/AudioDriverModeTable.h"
BRIDGE = ROOT / "source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp"

TOKENS = [
    "kAudioCapStereoOutput", "kAudioCapStereoInput", "kAudioCapUsbDevice",
    "kAudioCapUsbHost", "kAudioCapMidi", "kAudioCapCapture",
    "kAudioCapClockSync", "kAudioCapHotplug", "kAudioCapabilityCount",
    "kAudioCapabilityNames", "AudioCapabilityBit", "AudioCapabilityName",
    "AudioCapabilityCount",
]

FORBIDDEN = [
    "Player::GetInstance", "SamplePool::GetInstance", "ModalView",
    "DrawString", "GUITextProperties", "AppWindow", "MixerService",
    "GetInstance", "ColorDefinition", "EPBM_", "TreeFrogGetFramebuffer",
    "viewData_", "View::SetNotification", "GUIRect", "isDirty_",
    "unistd.h", "fcntl.h", "stdio.h", "stdlib.h", "string.h", "sys/",
    "open(", "mkfifo", "mkdir(", "snprintf", "fprintf",
    "TreeFrogUac2Bridge_", "g_driver_mode", "g_sampler_direction_in",
    "U241_", "TREEFROG_UAC2_BRIDGE", "#include",
]

CAP_NAMES = [
    "Stereo Output", "Stereo Input", "USB Device", "USB Host",
    "MIDI", "Capture", "Clock Sync", "Hotplug",
]


def check_capabilities_layer():
    c = CAP_H.read_text()
    for token in TOKENS:
        assert token in c, token
    for token in FORBIDDEN:
        assert token not in c, token
    for line in c.splitlines():
        assert not line.strip().startswith("#include"), line
    print("capabilities layer guards OK")


def check_capability_names():
    c = CAP_H.read_text()
    for name in CAP_NAMES:
        assert f'"{name}"' in c, name
    assert '"Stereo Output", "Stereo Input", "USB Device", "USB Host",' in c
    assert '"MIDI", "Capture", "Clock Sync", "Hotplug"};' in c
    print("capability names golden OK")


def check_mode_table_derivation():
    t = TABLE.read_text()
    assert '#include "Application/Audio/AudioCapabilities.h"' in t
    assert "AudioDriverModeCapabilities(" in t
    # Deriva solo de primitivos golden: sin caps hardcodeadas fuera de los
    # flags, sin rutas, sin hardware.
    for token in ["/mnt/", "/tmp/", "g_usb_out_allowed", "g_driver_mode"]:
        assert token not in t, token
    # La derivacion menciona las reglas golden (rol por modo).
    assert "kAudioDriverModeLocalConsole" in t
    assert "kAudioDriverModeWindows" in t
    assert "kAudioDriverModeAndroid" in t
    assert "kAudioDriverModeUsbOut" in t
    assert "kAudioDriverModeMidi" in t
    assert "kAudioDriverModeSp404In" in t
    print("mode table derivation OK")


def check_bridge_untouched_by_caps():
    b = BRIDGE.read_text()
    assert "AudioDriverModeCapabilities" not in b
    assert "AudioCapabilities" not in b
    assert "#include \"Application/Audio/AudioCapabilities.h\"" not in b
    print("bridge not consuming capabilities (F4c will connect) OK")


check_capabilities_layer()
check_capability_names()
check_mode_table_derivation()
check_bridge_untouched_by_caps()
print("F4B_BASELINE_OK")