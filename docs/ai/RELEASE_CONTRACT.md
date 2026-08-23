# RELEASE_CONTRACT — Installation Invariant

**Status:** Permanent. Applies to every Bacon-1.5 public release.
**Evidence base:** `docs/BACON_1_5_GOLDEN_BOOTSTRAP_PHYSICAL_PASS.md` (2026-08-23), `docs/BACON_1_5_RELEASE_MANIFEST.md`.

---

## Installation Invariant

```
Stock OS
+ TreeFrogUI
+ contents of LGPT_R36SX_Bacon-1.5_SD_ROOT.zip copied to SD root
= fully functional PORT
```

Required:

```
POST_INSTALL_MANUAL_FIXES=0
```

No manual creation of sentinel/profile/mode, no post-install patches beyond what the ZIP already contains. Extracting the ZIP before first boot is the entire install.

---

## Validated Payload Identity (Bacon-1.5 Golden)

```
Core:  46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6 (1559548, cubegm/cores/lgpt_core.so)
ZIP:   LGPT_R36SX_Bacon-1.5_SD_ROOT.zip — C5C77A0212E4784A9D0E6D0EDDC4DE1A8BBE0943B9EBEF8B13A18A82A6B9CB1E (7138546, 56 files)
APK:   ANDROID/LGPTUsbAudioBridge-H38-debug.apk — 89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a (298118, H38-only; H36 must remain absent)
```

Manifest: `LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt` (56 entries), `docs/RELEASE_SD_INCLUDED_FILES.txt` (56 files), `sd_root/` canonical overlay.

---

## Persistent Installation Baseline (packaged, 6 files)

Persistent, deterministic, present after ZIP extraction before first launch:

| Path | SHA256 | Content |
|---|---|---|
| `lgpt/otg/enable_lgpt_uac2_bridge` | `e3b0c442...` | empty (0 bytes) |
| `lgpt/otg/audio_usb_profile` | `30174a9c...` | `STEREO_48K\n` |
| `lgpt/otg/audio_driver_mode` | `4504f293...` | `LOCAL_CONSOLE\n` |
| `lgpt/otg/audio_driver_policy` | `4504f293...` | `LOCAL_CONSOLE\n` |
| `lgpt/otg/active_audio_branch` | `c3d87122...` | `audio_driver_local_console\n` |
| `lgpt/otg/branches/audio_driver_local_console/MODE` | `4504f293...` | `LOCAL_CONSOLE\n` |

These are **not** volatile runtime state. Golden Bootstrap proves they are legitimate package content when physically required for deterministic clean install.

**Must NOT be packaged (volatile):** FIFO, PID, `daemon_pid`, `daemon_version`, `capture_abi`, `setup_result`, `sp404_card`, `aoa_*` runtime state, device enumeration state, `/tmp` files, runtime logs.

Check: `python3 tests/test_release_audio_bootstrap.py` → `RELEASE_AUDIO_BOOTSTRAP: PASS`.

---

## Golden State Model

- **SOURCE GOLDEN:** repo commit `4429d4e` + merge `b616a5b` that builds core `46bd84` without modification during bootstrap repair.
- **PHYSICAL GOLDEN:** exact payload `C5C77A...` installed cleanly and passed physical matrix (staged bootstrap, exFAT Healthy/SD_WRITEABLE=YES, no manual fixes).
- **RELEASE GOLDEN:** payload that was `clean-installed + physically passed + published + downloaded back + REMOTE_SHA==LOCAL_SHA + unzip PASS + bootstrap PASS + manifest consistent`.

```
WORKS ON DEVELOPMENT SD != RELEASE PACKAGE COMPLETE
```

---

## Physical Validation Matrix (required for RELEASE GOLDEN)

```
LOCAL            = PASS (boot, 48kHz Stereo, UI)
WINDOWS          = DETECT PASS + PLAYBACK PASS + RECORD/INPUT PASS (R36SX USB Audio 48K, Dev: Windows)
SP404 / Sampler  = DETECT PASS + PLAYBACK PASS (Dev: SP404MKII)
ANDROID          = BRIDGE PASS + PLAYBACK PASS + RECORD PASS (H38, no Runtime is not ready)
SWITCHING        = LOCAL→WINDOWS→LOCAL→SP404→LOCAL→ANDROID→LOCAL no crash / no stale runtime (REGRESSION PASS)
```

Switching sequence tested: `LOCAL → WINDOWS → LOCAL → SP404 → LOCAL → ANDROID → LOCAL`.

---

## Release Identity Rule

```
ONE CURRENT ARTIFACT NAME = ONE AUTHORITATIVE CURRENT SHA
```

These must agree:

```
GitHub release body SHA
SHA256SUMS.txt entry
Release manifest (BACON_1_5_RELEASE_MANIFEST.md)
Included-files manifest (RELEASE_SD_INCLUDED_FILES.txt)
Downloaded asset bytes
```

After publishing:

```
DOWNLOAD-BACK IS REQUIRED
REMOTE_SHA == LOCALLY_VALIDATED_SHA   → REMOTE_IDENTICAL=YES
```

Historical SHAs (e.g. `31aa6d7e...` 7865366, `71dc6b61...`, `f3862cf5...` 7454962) remain only if explicitly marked historical/superseded in manifest (current authoritative is `C5C77A...`).

Evidence after publish:

```
REMOTE_DOWNLOAD_SHA=C5C77A0212E4784A9D0E6D0EDDC4DE1A8BBE0943B9EBEF8B13A18A82A6B9CB1E
REMOTE_DOWNLOAD_SIZE=7138546
REMOTE_IDENTICAL=YES
UNZIP_TEST_REMOTE=PASS
BOOTSTRAP_TEST_REMOTE=PASS
MANIFEST_CONSISTENT=YES
```

---

## SD Filesystem Precondition

Before blaming runtime: verify SD `mounted && healthy && writable && not read-only`.
Dirty exFAT previously made `/mnt/sdcard` read-only and mimicked USB/bootstrap failure.
Diagnostic layering required:

```
Detection != Runtime READY != PCM flow != physical PASS
```

Do NOT auto-run `fsck`/`chkdsk` — explicit user authorization required.

---

## Reference

- Physical PASS: `docs/BACON_1_5_GOLDEN_BOOTSTRAP_PHYSICAL_PASS.md`
- Manifest: `docs/BACON_1_5_RELEASE_MANIFEST.md`
- File list: `docs/RELEASE_SD_INCLUDED_FILES.txt` (56 files)
- Hashes: `LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt`
- Bootstrap test: `tests/test_release_audio_bootstrap.py`
