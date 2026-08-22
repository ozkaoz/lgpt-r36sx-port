# Single Startup Actions Checkpoint — 2026-08-22

**DATE=2026-08-22**
**BASE_COMMIT=b7b2e46b9061bdfd71e16c8aea21b65368ddc305**
**BASE_TAG=single-lgpt-functional**
**BRANCH=feature/startup-project-actions**
**NEW_COMMIT=single-startup-actions (to be created)**
**PHYSICAL_TREEFROGUI_VERSION=v1.0.14_a**
**PHYSICAL_TEST=PASS**
**VISIBLE_LGPT_ENTRIES=1**

## Active Core

**ACTIVE_CORE_PATH=/mnt/sdcard/cubegm/cores/lgpt_core.so (G:\cubegm\cores\lgpt_core.so)**

**CORE_SHA256=46bd84ebb0d1b1be8caec7c76fecbe6fb4baa8e9bbd603b44488bcc929dedec6**
**CORE_SIZE=1559548**
**PREVIOUS_TEST_CORE_SHA=6cfcad55767c58aa21311c9673d4f623d1b4e6daa4d8212e5f09025a1a292c99**
**GOLDEN_CORE_SHA=7d99987d5e2f71b4d4eb6ab822ee2888c38a863b3a8fbd433902cf79fa1218a3 (sd_root/cubegm/cores/lgpt_core.so at single-lgpt-functional)**

Core is MIPS32r2, stripped, dynamically linked,.Flags 0x70001007, built with TC=$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot, PLATFORM=TREEFROG, TREEFROG_ENABLE_SELECT=1, TREEFROG_ENABLE_START=1.

**SP404_DAEMON_SHA256=0fc2bb7f27a1f7100ce31cf72e0d43633180cda26ef567890b3b2a5684328e64** `lgpt/otg/bin/r36s_sp404_host_audio_io` unchanged (nice +1, MUSB restore, P3)
**STOCK_LGPT_ELF_SHA256=d5c467627b6807a8fe5f8db170f5d119be8a88362f87ebcae2cbbf233fc1e2da** `cubegm/lgpt.elf` preserved (528988)
**FROGUI_BINARY_SHA256=be0c9170b9c512b7dfdbe0009305d2b0105ee786cb1efb726bb6d8d1e6567679** `frogui_libretro.so` unchanged
**TREEFROGUI_VERSION=v1.0.14_a** (cubegm/version.txt, physical)

## Startup Project Management — Centralized under SELECT

**Previous:** R1+A (Rename/Export/Delete) and A+B (Delete) duplicated functionality.
**Final:** All project management centralized under **PLAIN SELECT** (`mask == EPBM_SELECT`).

```
SELECT (mask == 512, EPBT_SELECT=9, 1<<9)
  -> PROJECT
     Rename      -> PA_RENAME=1   -> OnRenameProject() -> TreeFrogTextEditor
     Duplicate   -> PA_DUPLICATE=4 -> OnDuplicateProject() -> lgpt_<base>_c, "_c" exact, Copy exists / Name too long / Duplicate failed, sync, refresh
     Export      -> PA_EXPORT=2   -> StartProjectExport() -> Export Mode picker (Full/Multitrack)
     Delete      -> PA_DELETE=3   -> MessageBox YES/NO -> OnDeleteProject()
  B cancels, UP/DOWN wraps, bounded SetWindow (TreeFrogProjectActionModal)

A (mask == 64, plain) -> Load project (no fallthrough from R1+A/A+B)
R1+A -> early return, no menu, no Load
A+B -> early return, no Delete, no Load
SELECT+R1/R2 -> early return if(mask & EPBM_SELECT), preserved global help/audio
```

**Deferred pattern preserved:** `StartupProjectActionMenuCallback -> DeferProjectAction(code) -> pendingAction_ -> OnFrameUpdate -> launchProjectAction` (avoids nested modal SAFE_DELETE).

**Duplicate:** source `GetCurrentProjectPath()` validated `TreeFrogV40IsLgptProjectName` + `IsDirectory` + `HasSaveFile`, dst `lgpt_<base>_c`, length check `kMaxStem=24` -> `Name too long`, `Exists()` -> `Copy exists`, `RecursiveCopyDirectory` -> `sync()` -> `setCurrentFolder` -> cursor follows, no auto-load.

## Startup New — A Random / START Confirm

**Startup New (SelectProjectDialog New):** `new NewProjectDialog(*this, currentPath_, true)`
- `A` -> `getRandomName()` loop `setInitialText` until `!currentPath_.Descend(GetName()).Exists()` (bounded 100), `isDirty_`, stays in dialog
- `START` (`TFSP_START` via `GetAdditionalActionMask`) -> validates non-empty, not busy (`Name busy`), then `EndModal(1)` -> `OnNewProject` creates `projects/lgpt_<name>/samples`
- Hint: `A random START confirm B erase`, `R1+LEFT` still cancels (base FSM), `B` erases, arrows, `L1+X` case, `X+UP/DOWN` +-5 preserved
- Source: `Application/Utils/RandomNames.h` `adjective+verb` <=25, srand(time), while >MAX_NAME_LENGTH

**Save Song As (ProjectView):** `new NewProjectDialog(*this, "root:projects")` default `startupRandomMode=false`
- `A` -> `EndModal(1)` (confirm, not random), `START` not handled, preserves original
- Destination fixed: `Path projectsRoot("root:projects"); str_dstprjdir = projectsRoot.GetName()+"/"+npd.GetName()` => `root:projects/lgpt_<name>` and `root:projects/lgpt_<name>/samples`, not `root:/lgpt_<name>`
- Collision now checks `projects/lgpt_<name>` correctly, `Name busy` if exists, samples copy, `sync()`, `OnSaveAsProject`
- Verified: `root:projects` alias resolves via `Path::GetPath()` to `LGPT_TREEFROG_ROOT+"/projects"`, physical `G:\lgpt\projects\lgpt_<name>`

**TreeFrogTextEditor minimal hooks:** `GetAdditionalActionMask()` (default 0, startup New returns TFSP_START), `HandlePhysicalAction()` (default false), `GetActionHintLine()` (default `A confirm...`, startup New returns `A random...`), `processPhysicalInput` ORs mask and calls handle before default. No second FSM, no global A change.

## Physical PASS (R36SX)

```
TreeFrogUI 1 LGPT PASS
Startup:
  A Load PASS
  SELECT overlay (Rename/Duplicate/Export/Delete) PASS
  R1+A disabled/no fallthrough PASS
  A+B disabled/no fallthrough PASS
  Duplicate KaOz->KaOz_c PASS, collision PASS
  Export (Full/Multitrack) PASS
  Delete YES/NO PASS, Duplicate not overwrite
New:
  A Random PASS, START Confirm PASS, B erase, R1+LEFT cancel PASS
Save Song As:
  A Confirm PASS, destination projects/lgpt_<name> PASS, not root:/ PASS, collision Name busy PASS, visible in startup list PASS, loads PASS
Local PASS, Playback PASS, Mixer PASS, EQ8 PASS, Analyzer PASS, Peak PASS, Pitch PASS, Chopper PASS, Sampler/SP404 PASS, Sampler Mixer/EQ8 PASS, Sampler->Local PASS, Projects/Samples/Menus PASS
```

No audio/Mixer/EQ8/Analyzer/Pitch/Chopper/Sampler/ASRC/FIR8/ALSA/MUSB/driver/TreeFrogLibretro/FrogUI changes beyond startup UI.

## Payload

`sd_root/` direct-copy overlay (56 files, 7.6M ZIP):
- `cubegm/cores/lgpt_core.so` (46bd84, 1559548, non-enumerated)
- `cubegm/lgpt` (e240e500, 8141, launcher), `cubegm/lgpt.elf` (d5c467, 528988, stock), `cubegm/picoarch` (93dbe7, 415120)
- `frogui/core_overrides.txt` (c7bfce2a, both lgpt_core)
- `roms/lgpt/start.lgpt` (94)
- `lgpt/otg/*` (daemons, scripts, modules), `lgpt/config.xml`, `lgpt/projects/.keep` etc.
- `LGPT_R36SX_Bacon-1.5_Android.apk` at ZIP root (H38, 298118, 89a99d...), also in `ANDROID/`
- `README_INSTALL.txt`, `SHA256SUMS.txt`, `VERSION.txt`

TreeFrog base (`cubegm/cores/libemu_*.so`, frogui icons) not duplicated in ZIP (vendor payload, recorded in PHYSICAL_SD manifest).

## Tests

- `tests/test_startup_project_actions_static.py` ALL_STATIC_CHECKS_PASS (72 + SaveAs root:projects)
- `tests/test_startup_duplicate_functional.py` ALL_FUNCTIONAL_TESTS_PASS
- `tests/host/startup_project_actions_host_test.cpp` STARTUP_PROJECT_ACTIONS_HOST_TEST_OK
- `tests/host_syntax_check.sh` HOST_SYNTAX_CHECK_U2523_OK
- `tests/run_host_startup_project_actions.sh` RUN_HOST_STARTUP_PROJECT_ACTIONS_OK
- `check_new_syntax.sh` TTE/NPD/SPD/Modal/ProjectView OK

No source functional changes after final physical test except packaging/docs sync.

## Next

Tag `single-startup-actions` is checkpoint before Bacon-1.5 release update. Ancestry: `sp404-functional-p3` -> `single-lgpt-functional` (b7b2e46) -> `single-startup-actions` (46bd84). Bacon-1.5 tag will be force-moved to this commit and release converted to LATEST.
