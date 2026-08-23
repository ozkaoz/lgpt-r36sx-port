# BACON 1.5 Golden Bootstrap Physical PASS

**Date:** 2026-08-23
**Core SHA:** `46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6` (1559548, `cubegm/cores/lgpt_core.so`)
**Launcher SHA:** `EE1ECFE53BF9C94915ABC696271561B7CCBF157F9C80300438F720DBEDAD896C` (`cubegm/lgpt` + `lgpt/otg/bin/lgpt_launcher_u241.sh`, SD_WRITEABLE diagnostic)
**OTG common SHA:** `CDE737B55E099E5EE947C10C0B2C4621CD0C8C0DA6E9EF052C5C6623511CCEBC` (`lgpt/otg/bin/otg_u241_common.sh`, diagnostic closure extended)
**H37 apply SHA:** `76B50C9879A7DF9929A287960276C9519EE58A279B7A7CC444DF580F413AD697` (baseline `f3862cf5`, CONFIG_MODULE_UNLOAD=n shared ALSA)
**Test ZIP:** `LGPT_R36SX_Bacon-1.5_GOLDEN_BOOTSTRAP_TEST.zip` `7138546` `C5C77A0212E4784A9D0E6D0EDDC4DE1A8BBE0943B9EBEF8B13A18A82A6B9CB1E` `56` files

## Physical Results (staged Golden Bootstrap, exFAT repaired Healthy/SD_WRITEABLE=YES)

```
LOCAL=PASS
WINDOWS: DETECT=PASS PLAYBACK=PASS RECORD/INPUT=PASS (R36SX USB Audio 48K, Dev: Windows, USB active)
SP404: DETECT=PASS PLAYBACK=PASS (Dev: SP404MKII, USB out active)
ANDROID: BRIDGE=PASS PLAYBACK=PASS RECORD=PASS (H38, no Runtime is not ready)
```

No manual sentinel/profile/mode creation. No post-install patches beyond bootstrap staging.

## Golden Bootstrap Baseline (packaged, 6 files)

Persistent, deterministic, after extracting ZIP before first launch:

| Path | Size | SHA256 | Content |
|---|---|---|---|
| `lgpt/otg/enable_lgpt_uac2_bridge` | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | empty |
| `lgpt/otg/audio_usb_profile` | 11 | `30174a9ce7486ef59c06c9e1ada862290619b3ff4cfb4012456246dd7c2ba1ce` | `STEREO_48K\n` |
| `lgpt/otg/audio_driver_mode` | 14 | `4504f293237d97028c19447b96a6bcbb20e61c75a3d97931b95a5e29a46a1edf` | `LOCAL_CONSOLE\n` |
| `lgpt/otg/audio_driver_policy` | 14 | `4504f293237d97028c19447b96a6bcbb20e61c75a3d97931b95a5e29a46a1edf` | `LOCAL_CONSOLE\n` |
| `lgpt/otg/active_audio_branch` | 27 | `c3d87122fb891e48ff563e4f37b65ebcaf382af7ec0592a812319be0188d5447` | `audio_driver_local_console\n` |
| `lgpt/otg/branches/audio_driver_local_console/MODE` | 14 | `4504f293237d97028c19447b96a6bcbb20e61c75a3d97931b95a5e29a46a1edf` | `LOCAL_CONSOLE\n` |

No volatile runtime state packaged (no `daemon_pid`, `setup_result`, `sp404_card`, FIFO, logs).

## Support Payload (physically validated, unchanged)

- **Launcher:** `cubegm/lgpt` `EE1ECF` (SD_WRITEABLE diagnostic, self-healing sentinel/profile)
- **OTG scripts:** `otg_u241_common.sh` `CDE737` (diagnostic `LGPT_OTG_LOGS/runtime_state/`), `otg_h37_apply_driver_mode.sh` `76B50C` (baseline host `b_host`), `otg_u241_setup_once.sh` `0c729e...` `8436`, `otg_u241_apply_profile_once.sh` `b2bbe0...`, `otg_h37_host_device_detect.sh` `de44ec...`, `otg_h37_host_runtime_supervisor.sh` `e04541...`, `otg_h37_android_runtime_supervisor.sh` `d6b8e3...`
- **Daemons:** `r36s_u241_usb_audio_io` `f7140072...` `4470572` (ABI7), `r36s_sp404_host_audio_io` `0fc2bb7f...` `4510084` (ABI1), `r36s_aoa_bulk_audio_io_h36` `a967603e...` `645280` (ABI4), `r36s_aoa_bulk_receiver_h36` `e1f910...` `636328`, `r36s_midi_host_io` `3f0ea7a2...` `4397188`
- **Modules:** `host_usb_audio` 8 `snd*.ko` (`09ec1a`/`b7013d` etc.) + `u2_38au8_sync_uac2` `soundcore` `5cd5d4` `snd` `917427` `snd-timer` `25cb21` `snd-pcm` `48f7d3` `usb_f_uac2` `e9062ac...` `22020` (vermagic `4.4.186-release`)
- **APK:** `ANDROID/LGPTUsbAudioBridge-H38-debug.apk` `89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a` `298118` (H38 only, no H36)
- **VERSION:** `sd_root/VERSION.txt` `46bd84ebb0d1b1be` `514dbb9f`

No personal projects, logs, or temp files included. `GOLDEN_BOOTSTRAP_PHYSICAL_PASS=YES`.
