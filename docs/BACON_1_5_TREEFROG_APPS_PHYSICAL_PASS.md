# Bacon-1.5 TreeFrog Apps — Physical Clean-Install PASS

**Date:** 2026-08-24 (user local 2026-08-23 clean-install, recorded 2026-08-24 UTC)
**TreeFrogUI required:** `v1.0.15_a`
**FrogUI:** `76034bd3c142a9fe24df8729a1ef0dee6f1d8c6b4e5e046db05ebc890b54a0ef` (326700, `cubegm/cores/frogui_libretro.so`) — derived from https://github.com/tzubertowski/FrogUI r36sx 028b011c2bdc04e5ea7d8611ac15c01e3016db71, patch `patches/frogui_apps_lgpt.patch`, CC BY-NC-SA 4.0, built SF3000 SDK mips-mti 6.3.0
**RC ZIP:** `LGPT_R36SX_Bacon-1.5_SD_ROOT.zip` `faf7a230c06660b2299664f819f8d517c139311d5bbe8e8a0cbc421623ba0dec` `7295274` `57` files
**Core:** `46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6` (1559548) unchanged
**Wrapper:** `ee1ecfe53bf9c94915abc696271561b7ccbf157f9c80300438f720dbedad896c` (9006) unchanged
**APK H38:** `89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a` (298118) unchanged

## Installation

```
Stock OS
+ TreeFrogUI v1.0.15_a
+ ONLY contents of RC ZIP (faf7a230) copied to SD root
= Apps→LGPT (no inherited Bacon files, no development SD, no manual fixes)
```

`POST_INSTALL_MANUAL_FIXES=0`

## Physical Matrix (R36SX, clean media, cold boot)

```
TREEFROGUI_BOOT=PASS
TREEFROG_APPS_ENTRY_LGPT=1
TREEFROG_GAMES_ENTRY_LGPT=0
VISIBLE_LGPT_ENTRIES_TOTAL=1

LGPT_APPS_LAUNCH=PASS
LGPT_EXIT_TO_TREEFROG=PASS
LGPT_RELAUNCH_FROM_APPS=PASS

LGPT_FUNCTIONAL_REGRESSION=PASS (projects, samples, controls, SELECT+R1/R2, Mixer, EQ8, Spectrum, Pitch, Chopper, Sampler, BassSynth, PianoSynth)

LOCAL=PASS
WINDOWS_DETECT=PASS
WINDOWS_PLAYBACK=PASS
WINDOWS_RECORD=PASS   (R36SX UAC2 48K Stereo)
SP404_DETECT=PASS
SP404_PLAYBACK=PASS   (SP404MKII)
ANDROID_CONNECT=PASS
ANDROID_PLAYBACK=PASS
ANDROID_RECORD=PASS   (H38, no Runtime is not ready)

SWITCHING_REGRESSION=PASS (LOCAL→WINDOWS→LOCAL→SP404→LOCAL→ANDROID→LOCAL, no crash/stale FIFO/daemon, no leakage)

TRUE_PHYSICAL_CLEAN_INSTALL=PASS
```

Wrapped binary launches via `Apps→LGPT` → `/tmp/frogui_launch.txt` (`standalone` / `/mnt/sdcard/cubegm/lgpt` / `/mnt/sdcard/roms/lgpt/start.lgpt`) → `cubegm/lgpt` (Bacon wrapper, `ROOT/roms/lgpt/start.lgpt` default) → `picoarch` `lgpt_core.so` — identical to previous Games route except presentation. `roms/lgpt/start.lgpt` (94, `3ac7a539`) remains present as launch argument, hidden from Games via `is_app_folder_name`.

## Evidence

- `build/release_candidate/LGPT_R36SX_Bacon-1.5_SD_ROOT.zip` `faf7a230` `7295274` `unzip -t PASS`
- `tests/test_treefrog_apps_lgpt_release.py` PASS (frogui 76034b, wrapper/core/start.lgpt preserved, bootstrap PASS, no volatile)
- `tests/test_frogui_apps_lgpt.py` PASS (Apps-only, hide)
- `sd_root/cubegm/cores/frogui_libretro.so` `76034b` == RC payload

## Previous Payload

`c5c77a0212e4784a9d0e6d0eddc4de1a8bbe0943b9ebef8b13a18a82a6b9cb1e` `7138546` `56` files (Games→LGPT, TreeFrogUI v1.0.14_a) remains historical; new authoritative is `faf7a230` `57` files.

**Attribution:** FrogUI / TreeFrogUI by Tomasz Zubertowski, CC BY-NC-SA 4.0, source commit 028b011, patch `patches/frogui_apps_lgpt.patch`.

**Next:** Download-back `REMOTE_SHA==faf7a230` → `RELEASE_GOLDEN PASS`, move `Bacon-1.5` tag to release commit.
