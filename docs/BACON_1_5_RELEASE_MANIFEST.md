# Bacon 1.5 Release Manifest — 2026-08-22

**FUNCTIONAL_CHECKPOINT_COMMIT=557b26d186a9d4bf78cb5d8c3bdd20d83ed7e836**
**FUNCTIONAL_CHECKPOINT_TAG=single-startup-actions**
**RELEASE_TAG=Bacon-1.5**
**BASE_TAG=single-lgpt-functional**
**BASE_COMMIT=b7b2e46b9061bdfd71e16c8aea21b65368ddc305**
**ANCESTRY=sp404-functional-p3 (beb8a12) -> single-lgpt-functional (b7b2e46) -> single-startup-actions (557b26d)

## Physical Core
**PHYSICAL_CORE_SHA=46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6**
**PHYSICAL_CORE_SIZE=1559548**
**CHECKPOINT_CORE_SHA=46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6**
**CORE_IDENTICAL=YES (sd_root/cubegm/cores/lgpt_core.so byte-identical to G:\cubegm\cores\lgpt_core.so and ZIP)**

## Support Binaries
**SP404_DAEMON_SHA=0fc2bb7f27a1f7100ce31cf72e0d43633180cda26ef567890b3b2a5684328e64**
**SP404_DAEMON_SHA_EXPECTED=0fc2bb7f27a1f7100ce31cf72e0d43633180cda26ef567890b3b2a5684328e64**
**UAC2_DAEMON=r36s_u241_usb_audio_io (physical) + r36s_sp404_host_audio_io**
**MIDI_DAEMON=r36s_midi_host_io (3f0ea7a...)**

## TreeFrog / Stock
**TREEFROG_VERSION=v1.0.14_a**
**LGPT_ELF_SHA=d5c467627b6807a8fe5f8db170f5d119be8a88362f87ebcae2cbbf233fc1e2da**
**LGPT_ELF_SIZE=528988**
**FROGUI_SHA=be0c9170b9c512b7dfdbe0009305d2b0105ee786cb1efb726bb6d8d1e6567679** (frogui_libretro.so, not rebuilt)

## Android APK
**ANDROID_APK_SOURCE=sd_root/ANDROID/LGPTUsbAudioBridge-H38-debug.apk**
**ANDROID_APK_FILENAME=LGPT_R36SX_Bacon-1.5_Android.apk**
**ANDROID_APK_SIZE=298118**
**ANDROID_APK_SHA256=89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a**
**APK_VALID=YES (file: Android package, unzip -t OK, 298118 bytes)**

APK included two ways:
- Inside ZIP at root: LGPT_R36SX_Bacon-1.5_Android.apk (same SHA)
- As separate GitHub Release asset

## SD Root ZIP
**SD_ROOT_ZIP=LGPT_R36SX_Bacon-1.5_SD_ROOT.zip**
**SD_ROOT_ZIP_SIZE=7454962
**SD_ROOT_ZIP_SHA=f3862cf5d1c3e624c566c86eabf951312224fe6c262a0acaf262158b415642be
**ZIP_ROOT_LAYOUT_OK=YES (no wrapper, cubegm/cores/lgpt_core.so at root, APK/README/SHA present)**
**ZIP_APK_INCLUDED=YES**
**ZIP_USER_DATA_CLEAN=YES (no lgpt/projects/lgpt_* user projects, no logs, no crash dumps, no backups)**
**PACKAGE_RUNTIME_MATCH_PHYSICAL=YES (core, daemon, launcher, overrides verified via sha256sum after unzip)**

Included files: see docs/RELEASE_SD_INCLUDED_FILES.txt (56 files)
Excluded user files: see docs/RELEASE_SD_EXCLUDED_USER_FILES.txt (4721 files, includes TreeFrog base vendor files not in port overlay)
Full physical manifest: docs/PHYSICAL_SD_FINAL_TREE.txt, docs/PHYSICAL_SD_FINAL_SHA256SUMS.txt (4758 files)

## Release Assets (published to Bacon-1.5)
- LGPT_R36SX_Bacon-1.5_SD_ROOT.zip (7454962, 71dc6b61f7d28f41646ce8a1d988ee88f8848fa58128fcfb32256320a22551e8)
- LGPT_R36SX_Bacon-1.5_Android.apk (298118, 89a99d50571948788f1f0d2a3c1ebf313e6bf7a531874b5fc0885ba754ef1c3a)
- LGPT_R36SX_Bacon-1.5_SHA256SUMS.txt (SHA for ZIP + APK + core)
- Optionally: LGPT_R36SX_Bacon-1.5_lgpt_core.so (core standalone)

## Host Tests
ALL_PASS: test_startup_project_actions_static.py, test_startup_duplicate_functional.py, startup_project_actions_host_test.cpp, host_syntax_check.sh
SELECT_MENU_ITEMS=Rename/Duplicate/Export/Delete, R1+A/A+B removed, plain A Load, SaveAs root:projects

## Tags
- single-lgpt-functional -> b7b2e46 (unchanged)
- sp404-functional-p3 -> beb8a12 (unchanged)
- Bacon-1.5-U2523-legacy -> 6f944d6 (preserved old Bacon-1.5)
- single-startup-actions -> 557b26d186a9d4bf78cb5d8c3bdd20d83ed7e836
- Bacon-1.5 -> was force-moved to 557b26d186a9d4bf78cb5d8c3bdd20d83ed7e836

## Release Flags (final)
**RELEASE_TITLE=LGPT R36SX - Bacon 1.5 - Sinths and EQ8**
**RELEASE_PRERELEASE=false**
**RELEASE_LATEST=true**
**LATEST_URL=https://github.com/ozkaoz/lgpt-r36sx-port/releases/latest -> Bacon-1.5**

**RELEASE_PACKAGING_COMMIT=see Bacon-1.5 tag (hygiene descendant of 557b26d)**