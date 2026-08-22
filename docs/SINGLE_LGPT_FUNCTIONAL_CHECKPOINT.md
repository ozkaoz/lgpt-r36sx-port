# Single LGPT Functional Checkpoint — 2026-08-22

**DATE=2026-08-22**
**BASE_COMMIT=beb8a12**
**BASE_TAG=sp404-functional-p3**
**BRANCH=feature/canonical-lgpt-core**
**PHYSICAL_TREEFROGUI_VERSION=v1.0.14_a**
**PHYSICAL_TEST=PASS**
**VISIBLE_LGPT_ENTRIES=1**

## Active Core

**ACTIVE_CORE_PATH=/mnt/sdcard/cubegm/cores/lgpt_core.so**

**CORE_SHA256=7d99987d5e2f71b4d4eb6ab822ee2888c38a863b3a8fbd433902cf79fa1218a3**

Byte-identical to P3 validated core `lgpt_libretro.so` / `lgpt_r36sx_port_libretro.so` (1541388).

**SP404_DAEMON_SHA256=0fc2bb7f27a1f7100ce31cf72e0d43633180cda26ef567890b3b2a5684328e64**

**STOCK_LGPT_ELF_SHA256=d5c467627b6807a8fe5f8db170f5d119be8a88362f87ebcae2cbbf233fc1e2da**

`cubegm/lgpt.elf` preserved, not deleted.

**FROGUI_BINARY_SHA256=be0c9170b9c512b7dfdbe0009305d2b0105ee786cb1efb726bb6d8d1e6567679** `frogui_libretro.so` unchanged, no rebuild.

## Architecture

```
TreeFrogUI
  ? hardcoded LGPT standalone registration console_mappings {"lgpt", "/mnt/sdcard/cubegm/lgpt"}
  ? /mnt/sdcard/cubegm/lgpt (PORT P3 launcher 8141 e240e500)
  ? picoarch /mnt/sdcard/cubegm/picoarch
  ? /mnt/sdcard/cubegm/cores/lgpt_core.so (7d99987d, 1541388, non-enumerated)
```

`frogui/core_overrides.txt`:
```
/mnt/sdcard/roms/lgpt|/mnt/sdcard/cubegm/cores/lgpt_core.so
/mnt/sdcard/roms/lgpt/start.lgpt|/mnt/sdcard/cubegm/cores/lgpt_core.so
```

## Why lgpt_core.so

FrogUI `frogui_libretro.c:313` `build_core_choices()` scans `cubegm/cores` and auto-enumerates every `*_libretro.so`:
```c
if (l <13 || strcmp(n+l-12,"_libretro.so")!=0) continue;
```
`lgpt_libretro.so` and `lgpt_r36sx_port_libretro.so` are enumerated ? `CoreChoice` entries named `lgpt` duplicate the hardcoded standalone `lgpt?/mnt/sdcard/cubegm/lgpt` (dedup by path, not name) ? **TWO** `lgpt` entries in `SELECT` core picker.

`lgpt_core.so` does **NOT** end in `_libretro.so` ? `LGPT_CORE_SO_AUTO_ENUMERATED=NO` ? only `1` picker entry remains: standalone `cubegm/lgpt` which is our port wrapper ? single visible LGPT = PORT.

`PICOARCH_REQUIRES_LIBRETRO_SUFFIX=NO` (`core.c dlopen` arbitrary path), `CORE_OVERRIDE_REQUIRES=NO`, `LAUNCHER_REQUIRES=NO`.

## Physical PASS

- BEFORE: `LGPT / LGPT` (2 entries) — `lgpt_libretro.so` enumerated + standalone
- AFTER:  `LGPT` (1 entry)

```
TreeFrogUI ? LGPT PASS
Local PASS
Playback PASS
Mixer PASS
EQ8 PASS
Analyzer PASS
Pitch PASS
Chopper PASS
Sampler/SP404 PASS
Sampler Mixer PASS
Sampler EQ8 PASS
Sampler ? Local PASS
Menus/projects/samples PASS
```

MUSB restore `cold_local_musb_role` + P3 `nice -n 1` for SP404 daemon only preserved.

## Payload

Active production payload `sd_root/`:

```
sd_root/cubegm/cores/lgpt_core.so (7d99987d, 1541388)
sd_root/cubegm/lgpt (e240e500, LF, CORE=lgpt_core.so)
sd_root/lgpt/otg/bin/lgpt_launcher_u241.sh (e240e500)
sd_root/lgpt/otg/bin/otg_u241_common.sh (411b56ca reconciled)
sd_root/frogui/core_overrides.txt (c7bfce2a, both lgpt_core)
device/lgpt_launcher_u241.sh / device/frogui/core_overrides.txt mirrors
```

**NOT active** (would re-create duplicate if left in `cubegm/cores/`):
- `lgpt_libretro.so` — kept as ignored artifact in repo history, not in final payload
- `lgpt_r36sx_port_libretro.so` — moved to `lgpt/backup/` on physical SD (`7d99987d` backups)

Physical backups outside active cores on G:
```
G:\lgpt\backup\lgpt_libretro.so.canonical-phase1 (7d99987d)
G:\lgpt\backup\lgpt_r36sx_port_libretro.so.p3-legacy (7d99987d)
```

## Tests

- `canonical_lgpt_core_migration_host_test` `CANONICAL_LGPT_CORE_MIGRATION_OK`
- `canonical_core_binary_identity_host_test` `PHYSICAL_G_MATCH=YES` `7d99987d cmp PASS`
- `lgpt_core_single_entry_host_test` `LGPT_CORE_SINGLE_ENTRY_OK` `EXPECTED 1` `lgpt_core not enumerated`
- `run_host_lgpt_core_single.sh` `RUN_HOST_LGPT_CORE_SINGLE_OK`
- `test_canonical_install_simulation.sh` `INSTALL_SIMULATION_OK` `ENUMERATED_LIBRETRO_COUNT=1` `EXPECTED 1`
- `test_copy_root_launcher.sh` `TEST_LAUNCHER_AUTOCREATES_SAMPLELIB_OK`
- `host_syntax_check` `source diff 0`

No `source/` functional changes, no `lgpt.elf` change, no `frogui` rebuild.

## Next

Tag `single-lgpt-functional` is checkpoint before `update.zip` packaging. Next phase starts from this tag and only addresses TreeFrogUI-compatible `update.zip` installation/rollback/version without changing validated runtime.