# FrogUI Apps Patch — LGPT Phase B (Dual Entry)

**Branch:** `feature/treefrog-apps-lgpt`
**Base:** `f773504` (main, Bacon-1.5)
**FrogUI upstream:** `https://github.com/tzubertowski/FrogUI` `branch r36sx` `028b011c2bdc04e5ea7d8611ac15c01e3016db71` (2026-08-23 HEAD)

## Objective

Phase B adds LGPT to TreeFrogUI Apps while keeping Games→LGPT visible for A/B testing.

```
TREEFROG_APPS_ENTRY_LGPT=1
TREEFROG_GAMES_ENTRY_LGPT=1
```

Phase C will later hide Games entry via `is_app_folder_name()`.

## Protected invariants

- `cubegm/lgpt` Bacon wrapper `ee1ecfe5…` (9006) **must not be overwritten** by TreeFrog's 544-byte `9fc4d0…`.
- `cubegm/cores/lgpt_core.so` `46bd84…` unchanged.
- `lgpt/otg/*`, modules, daemons, H38 APK, Golden Bootstrap unchanged.
- Only intended delta: `cubegm/cores/frogui_libretro.so` (+ LGPT in `app_defs`).

## Source baseline

TreeFrogUI v1.0.15_a shipped `frogui_libretro.so`:

- `sha256:b07bbb4c13b47af714dd1bbf0a8bf84f5e4021f5521743d6ea06b337a5424566` 324664
- tag `v1.0.15` commit `27f3bf33`, submodule `15ea12bb` (no Apps). Shipped binary built from later `r36sx` dev (Apps commits `eab14f8f`, `a80c1576`, `3402fb0c`).

Chosen rebuild base for reproducibility: `r36sx` `028b011c` (latest Apps-capable at audit time). Vanilla rebuild with Ubuntu `mipsel-linux-gnu-gcc 12.4`:

- `frogui_libretro.so.vanilla` `82f9446bf33e9d77496bfcaf93ccfbffcf45be0d5ec24c157952cc0582a5d1f8` 368k (approx) — **not byte-identical** to shipped due to toolchain/sysroot delta (`-mtune=74kc -mdspr2` etc. vs generic). Functional parity verified via strings/symbols/`app_defs` structure (7 entries → `0x8c`).

`FROGUI_REPRODUCIBLE=NO` but `FROGUI_REPRODUCIBILITY_CONFIDENCE=HIGH` (same source, same `APPS`/`scan_apps_tab`/`app_defs` 7-entry layout, only bin size delta from flags).

## Patch (Phase B — Apps entry only)

File: `frogui_libretro.c`

```diff
 app_defs[] = {
     {"activity",…},
     {"frogshell",…,FROGSHELL_BIN},
+    {"lgpt","LGPT",NULL,NULL,LGPT_BIN},
     …
 };
```

```diff
- request_standalone_launch(bin, ROMS_PATH);
+ if (!strcmp(key,"lgpt"))
+     request_standalone_launch(bin, SDCARD_BASE "/roms/lgpt/start.lgpt");
+ else
+     request_standalone_launch(bin, ROMS_PATH);
```

`LGPT_BIN = SDCARD_BASE "/cubegm/lgpt"` already defined. No `is_app_folder_name()` change in Phase B, so `roms/lgpt` remains visible under Games.

Launch semantics:

- `GAMES_LGPT_ARGV0=/mnt/sdcard/cubegm/lgpt` `ARGV1=/mnt/sdcard/roms/lgpt/start.lgpt` (via `launch_by_path` + `is_standalone_bin`)
- `APPS_LGPT_ARGV0=/mnt/sdcard/cubegm/lgpt` `ARGV1=/mnt/sdcard/roms/lgpt/start.lgpt` (patched bin launch) — **identical**
- Bacon wrapper `cubegm/lgpt` does `ROM="${1:-$ROOT/roms/lgpt/start.lgpt}"` → `exec "$PICO" "$CORE" "$ROM"` from `cd $DATA` — same effective runtime.

## Build

Patched source in `/tmp/FrogUI_r36sx` (clone), built with:

```
mipsel-linux-gnu-gcc -mips32r2 -mfp32 -mhard-float -EL -fPIC -G0 -Wall -I./ -Ofast -DNDEBUG -D__LIBRETRO__ -c -o *.lo *.c
mipsel-linux-gnu-gcc -mips32r2 -EL -fPIC -shared *.lo -lm -lc -ldl -lpthread -lgcc -o frogui_libretro.so
```

Result: `frogui_libretro.so.patched` `029584167642cb2d8b0f28e4dc0b8c41aed55d28c3f05184c685c5d18e7e1e36` 368k, `app_defs` `0xa0` (8 entries).

Candidate staged: `build/frogui_candidate/frogui_libretro.so` (ignored by `.gitignore`, SHA above). Patch file: `patches/frogui_apps_lgpt.patch`.

## Static gate

```
LGPT_WRAPPER_CHANGED=NO (ee1ecfe5)
LGPT_CORE_CHANGED=NO (46bd84)
OTG_RUNTIME_CHANGED=NO
AUDIO_MODULES_CHANGED=NO / AUDIO_DAEMONS_CHANGED=NO / ANDROID_H38_CHANGED=NO / GOLDEN_BOOTSTRAP_CHANGED=NO
PICOARCH_CHANGED=N/A / LGPT_ELF_CHANGED=N/A (vendor)
```

Only `frogui_libretro.so` differs by design.

## Deployment (one-file)

On physical SD (already on v1.0.15_a, backup first):

```
backup:  SD:/cubegm/cores/frogui_libretro.so  →  frogui_libretro.so.v1.0.15_a.backup  (store outside scan, e.g. /cubegm/cores/frogui_libretro.so.v1.0.15_a.backup or PC)
copy:    build/frogui_candidate/frogui_libretro.so  →  SD:/cubegm/cores/frogui_libretro.so
verify:  sha256sum SD:/cubegm/cores/frogui_libretro.so == 029584167642cb2d8b0f28e4dc0b8c41aed55d28c3f05184c685c5d18e7e1e36
do NOT copy cubegm/lgpt / lgpt.elf / picoarch / lgpt_core.so / otg
```

Cold boot, expect **dual entry**:

```
Apps → LGPT (new)
Games → LGPT (legacy, still present)
```

Test both A/B routes: launch, local audio, exit to TreeFrog, relaunch — must be identical.

## Next

Phase C will add `lgpt` to `is_app_folder_name()` to hide `roms/lgpt` from Games, leaving `TREEFROG_APPS_ENTRY_LGPT=1` `TREEFROG_GAMES_ENTRY_LGPT=0`. Do not delete `roms/lgpt/start.lgpt` physically — exclusion is sufficient for safe rollback.

## References

- `docs/standalone-apps.md` (TreeFrog docs)
- `/tmp/FrogUI_r36sx` clone at `028b011`
- `patches/frogui_apps_lgpt.patch`
- `build/frogui_candidate/` (vanilla/patched/shipped)
