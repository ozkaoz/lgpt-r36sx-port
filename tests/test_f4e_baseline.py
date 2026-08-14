#!/usr/bin/env python3
"""F4e baseline: AudioEngine - politica de estado del motor de audio.

Verifica que:
1. AudioEngine.h (Application/Audio) declara la politica golden de estado
   del motor: AudioEngineShouldMute (U2.52.5 ANDROID_NO_MUTE + hasOut &&
   !nomute && raw, con el estado runtime como parametros),
   AudioEngineMonitorStep (ASRC usb/engine con fallback 1.0),
   AudioEngineMonitorApplyGain (75%) y el prebuffer del monitor (960).
2. AudioEngine.h es capa pura: solo include de las capas hermanas
   (AudioDriverModeTable.h, AudioCapabilities.h, AudioRouter.h); sin
   GUI/audio/daemons/POSIX/framebuffer.
3. El bridge delega: should_mute_now usa AudioEngineShouldMute y el bucle
   del monitor usa AudioEngineMonitorStep/AudioEngineMonitorApplyGain; el
   estado runtime (g_usb_raw, nomute_file_present, ring, fifos) queda en
   el bridge.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "source/sources/Application/Audio/AudioEngine.h"
BRIDGE = ROOT / "source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp"

TOKENS = [
    "kAudioEngineMonitorPrebufferSamples",
    "kAudioEngineMonitorGainPercent",
    "AudioEngineShouldMute", "AudioEngineMonitorStep",
    "AudioEngineMonitorApplyGain",
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
    "launch_apply_profile_once", "runtime_ready_fast", "g_usb_raw",
    "nomute_file_present", "monitor_ring", "read_monitor_fifo",
    "refresh_usb_state", "refresh_runtime_audio_profile",
]


def check_engine_layer():
    e = ENGINE.read_text()
    for token in TOKENS:
        assert token in e, token
    for token in FORBIDDEN:
        assert token not in e, token
    includes = [l.strip() for l in e.splitlines()
                if l.strip().startswith("#include")]
    assert includes == [
        '#include "Application/Audio/AudioDriverModeTable.h"',
        '#include "Application/Audio/AudioCapabilities.h"',
        '#include "Application/Audio/AudioRouter.h"',
    ], includes
    print("engine layer guards OK")


def check_golden_policy():
    e = ENGINE.read_text()
    # Regla de mute golden.
    assert "mode == kAudioDriverModeAndroid" in e
    assert "AudioDriverModeHasOut(mode, samplerDirectionIn)" in e
    assert "disableMuteFilePresent" in e
    assert "usbRawPresent" in e
    # ASRC step golden.
    assert "(double)usbRate / (double)engineRate" in e
    assert "engineRate > 0" in e
    # Ganancia golden.
    assert "kAudioEngineMonitorGainPercent" in e
    assert "(sample * kAudioEngineMonitorGainPercent) / 100" in e
    # Prebuffer golden.
    assert "= 960" in e
    assert "= 75" in e
    print("engine golden policy OK")


def check_bridge_delegates():
    b = BRIDGE.read_text()
    assert '#include "Application/Audio/AudioEngine.h"' in b
    assert "AudioEngineShouldMute(" in b
    assert "g_usb_raw, nomute_file_present()" in b
    assert "AudioEngineMonitorStep(g_engine_rate, g_usb_rate)" in b
    assert "AudioEngineMonitorApplyGain(left)" in b
    assert "AudioEngineMonitorApplyGain(right)" in b
    # El estado runtime permanece en el bridge.
    for kept in ["g_was_muted", "log_msg(should ?", "g_monitor_phase",
                 "monitor_ring_peek", "g_monitor_fill", "close_monitor_fifo",
                 "g_usb_monitor_enabled"]:
        assert kept in b, kept
    print("bridge delegates engine policy, runtime intact OK")


check_engine_layer()
check_golden_policy()
check_bridge_delegates()
print("F4E_BASELINE_OK")