# BACON 1.5 Release Manifest — TreeFrog Apps Migration

**Build:** 2026-08-24
**Core:** `46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6` (1559548, cubegm/cores/lgpt_core.so)
**Launcher:** `ee1ecfe53bf9c94915abc696271561b7ccbf157f9c80300438f720dbedad896c` (cubegm/lgpt, SD_WRITEABLE diagnostic)
**APK H38:** `89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a` (298118, ANDROID/LGPTUsbAudioBridge-H38-debug.apk)
**FrogUI:** `76034bd3c142a9fe24df8729a1ef0dee6f1d8c6b4e5e046db05ebc890b54a0ef` (326700, cubegm/cores/frogui_libretro.so) — derived from https://github.com/tzubertowski/FrogUI r36sx 028b011c2bdc04e5ea7d8611ac15c01e3016db71, CC BY-NC-SA 4.0, patch `patches/frogui_apps_lgpt.patch` (LGPT Apps + hide from Games)
**TreeFrogUI required:** `v1.0.15_a` (Apps-capable)
**ZIP:** `LGPT_R36SX_Bacon-1.5_SD_ROOT.zip` `7295274` `faf7a230c06660b2299664f819f8d517c139311d5bbe8e8a0cbc421623ba0dec` (57 files)

## Golden Bootstrap Baseline (persistent, before first launch)

| Path | Size | SHA256 | Content |
|---|---|---|---|
| `lgpt/otg/enable_lgpt_uac2_bridge` | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | empty |
| `lgpt/otg/audio_usb_profile` | 11 | `30174a9ce7486ef59c06c9e1ada862290619b3ff4cfb4012456246dd7c2ba1ce` | `STEREO_48K` |
| `lgpt/otg/audio_driver_mode` | 14 | `4504f293237d97028c19447b96a6bcbb20e61c75a3d97931b95a5e29a46a1edf` | `LOCAL_CONSOLE` |
| `lgpt/otg/audio_driver_policy` | 14 | `4504f293237d97028c19447b96a6bcbb20e61c75a3d97931b95a5e29a46a1edf` | `LOCAL_CONSOLE` |
| `lgpt/otg/active_audio_branch` | 27 | `c3d87122fb891e48ff563e4f37b65ebcaf382af7ec0592a812319be0188d5447` | `audio_driver_local_console` |
| `lgpt/otg/branches/audio_driver_local_console/MODE` | 14 | `4504f293237d97028c19447b96a6bcbb20e61c75a3d97931b95a5e29a46a1edf` | `LOCAL_CONSOLE` |

Physical PASS (Apps-only): LOCAL PASS / WINDOWS DETECT+PLAYBACK+RECORD PASS / SP404 DETECT+PLAYBACK PASS / ANDROID BRIDGE+PLAYBACK+RECORD PASS (no Runtime is not ready), core `46bd84` unchanged. TreeFrogUI Apps → LGPT visible, Games → LGPT absent (VISIBLE_LGPT_ENTRIES_TOTAL=1).

**ZIP contents (57):** `cubegm/cores/lgpt_core.so` (`46bd84`), `cubegm/cores/frogui_libretro.so` (`76034b`), `cubegm/lgpt` (`ee1ecfe5`), `frogui/core_overrides.txt`, `lgpt/otg/*` (daemons `0fc2bb7f`/`f71400`/`a96760`/`e1f910`/`3f0ea7`), `lgpt/otg/modules/*`, `lgpt/config.xml`, `roms/lgpt/start.lgpt`, `VERSION.txt`, `SHA256SUMS.txt`

**Installation contract:** `Stock OS + TreeFrogUI v1.0.15_a + ZIP contents to SD root = Apps→LGPT` `POST_INSTALL_MANUAL_FIXES=0`

**Previous ZIP identities (historical):** `c5c77a0212e4784a9d0e6d0eddc4de1a8bbe0943b9ebef8b13a18a82a6b9cb1e` (7138546, 56 files, pre-Apps, Games→LGPT), `31aa6d7e...` (7865366, docs stale), `71dc6b61...` (historical), `f3862cf5...` (7454962, pre-bootstrap). Current authoritative is `faf7a230c06660b2299664f819f8d517c139311d5bbe8e8a0cbc421623ba0dec` (`7295274`, 57 files, Apps→LGPT).

**FrogUI attribution:** FrogUI / TreeFrogUI by Tomasz Zubertowski and contributors, source https://github.com/tzubertowski/FrogUI r36sx 028b011, build SF3000 SDK mips-mti-linux-gnu-gcc 6.3.0, patch `patches/frogui_apps_lgpt.patch`, license CC BY-NC-SA 4.0 (non-commercial, ShareAlike).

See `RELEASE_SD_INCLUDED_FILES.txt` (57 files) and `LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt` (57 entries) for per-file hashes. `unzip -t` PASS, `test_release_audio_bootstrap` PASS, `test_treefrog_apps_lgpt_release` PASS, H38 only.

**Release Golden:** `Bacon-1.5` `27edc78` (tag) → `ba43a71` (commit), `LATEST=YES` `PRERELEASE=NO` `DRAFT=NO`, `ZIP faf7a230` `7295274` published `2026-08-24`, `DOWNLOAD-BACK PASS` `REMOTE_SHA==faf7a230` `REMOTE_IDENTICAL=YES`, `TRUE_PHYSICAL_CLEAN_INSTALL PASS` `POST_INSTALL_MANUAL_FIXES=0`.
