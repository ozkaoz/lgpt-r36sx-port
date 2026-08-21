# CURRENT — Estado operacional actual

**Fecha:** 2026-08-21
**Repositorio:** https://github.com/ozkaoz/lgpt-r36sx-port

---

## Infrastructure

```
Infrastructure:          VALIDATED
Canonical WSL:           /home/dafunknoise/lgpt-repo
Active development branch: feature/bacon-1.5-fx
```

---

## Checkpoints y baselines significativos

```
Infrastructure checkpoint:     628484c3bcea780856aab067ae34658e8abbc53d
  → Mensaje: chore: finalize multi-agent infrastructure after R36SX validation
  → Contenido: AGENTS.md v1.1, CURRENT.md, CONTEXT_MAP.md, DECISIONS.md (IDs únicos)
  → Fecha: 2026-08-21
  → Significado: checkpoint documental/multiagente creado DESPUÉS del SD TEST PASS (no contiene runtime nuevo)

Validated runtime HEAD:        e27c741441cffbff56144813323b56759ef2dc58
  → Mensaje: Infraestructura IA: AGENTS.md, CURRENT.md, CONTEXT_MAP.md, DECISIONS.md
  → Significado: código runtime compilado y probado físicamente (incluye 5f50f62 BUG1/BUG2 fix + U2.71)
  → No sustituir por 628484c (628484c es docs-only posterior a la prueba)

Validated runtime artifact:    source/dist/lgpt_libretro.so
  SHA256: cc21ab2623a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8 (1.4M, 1424592 bytes)
  SD TEST: PASS — 2026-08-21 12:55 (usuario: Todo parece correcto)
  SD artifact: /mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so == local (cmp OK, sync, Windows G: verificado)
  Physical target: R36SX — boot OK, 48kHz Stereo confirmado (H36_SINGLE_RATE_48K + SubmitStereo48000 en binario + TreeFrogAudio 48000)
  Audio: 48000 Hz / Stereo CONFIRMED

Previous golden checkpoint:    449041f276fb49e71870dc719d635d76cae039ae
  → Tag: golden-bacon-1.4 — Bacon 1.4 U2.54
  → Significado: último stable golden antes de e27c741, ya no es baseline actual (actual es e27c741/cc21ab)
```

---

## Estado Git actual (siempre verificar con comandos, no hardcodear nuevo SHA)

```
Current repository HEAD:       verificar con `git -C /home/dafunknoise/lgpt-repo rev-parse HEAD`
  → Antes de este commit docs-only: 628484c3bcea780856aab067ae34658e8abbc53d (Infrastructure checkpoint)
  → Después de este commit docs-only: nuevo SHA docs-only (ver git log)
  → No intentar mantener CURRENT.md autorreferencial con SHA de sí mismo (evita ciclo infinito)

Remote tracking branch:        origin/feature/bacon-1.5-fx
Remote HEAD:                   verificar con `git -C /home/dafunknoise/lgpt-repo rev-parse origin/feature/bacon-1.5-fx`
  → Antes de este push: 628484c
  → Después: nuevo SHA (local==remote tras push)

Ahead / Behind:                verificar con `git -C /home/dafunknoise/lgpt-repo status --short --branch`
  → Esperado tras push: 0 ahead, 0 behind, working tree clean

Default remote:                origin https://github.com/ozkaoz/lgpt-r36sx-port.git
Alternative remote HEAD:       origin/main = 449041f276fb49e71870dc719d635d76cae039ae (golden-bacon-1.4)
Tag Bacon-1.5:                 6a71002dd3efb1a5328bcd05e8053c20fe3c15bb -> 6f944d652721a66ea88db4ab0cd908ef618a04c9
```

**Comandos de verificación (siempre):**
```bash
git -C /home/dafunknoise/lgpt-repo branch --show-current
git -C /home/dafunknoise/lgpt-repo rev-parse HEAD
git -C /home/dafunknoise/lgpt-repo status --short --branch
git -C /home/dafunknoise/lgpt-repo log --oneline --decorate -5
git -C /home/dafunknoise/lgpt-repo rev-parse origin/feature/bacon-1.5-fx
git show --stat --oneline 628484c3bcea780856aab067ae34658e8abbc53d
```

---

## Objetivo actual

```
Infrastructure status:       DONE / VALIDATED — 2026-08-21
  → Saneamiento WSL ↔ GitHub ↔ Build ↔ SD ↔ R36SX completado
  → AGENTS.md v1.1 (canonical, invariants 48k/stereo/R36SX), CONTEXT_MAP.md e27c741/cc21ab, DECISIONS.md IDs únicos

Current objective:           Ready to begin EQ8 audit and correction from infrastructure checkpoint 628484c.
  → Runtime baseline: e27c741 / artifact cc21ab / SD TEST PASS 2026-08-21
  → No iniciar EQ8 hasta este checkpoint (ahora validado)

Previous objective (DONE):   Saneamiento infraestructura multiagente completa (auditar, fijar checkout, corregir divergencias, documentar workflow)
```

No modificar EQ todavía dentro de esta tarea.

---

## Resultados de validación (histórico del ciclo que llevó al checkpoint 628484c)

```
Last build result:        BUILD PASS — 2026-08-21 12:49 WSL, source/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
                         Toolchain: $HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
                         Artifact: /home/dafunknoise/lgpt-repo/source/dist/lgpt_libretro.so (1.4M)
                         SHA256: cc21ab2623a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8
                         DIAGNOSTIC_GATE: 0 errors 0 warnings, stripped

Last host-test result:    HOST TESTS PARTIAL PASS — scripts/audit.sh EXIT 0 pero F10_BASELINE_FAILED (expected 6a909dd vs BUILD mutable dbad57a7aeb7 en D:)
                         Resto BC14 OK (48K_STEREO, FIFO, MODAL_36COLS, PITCH_PANEL, STABLE_PLAYBACK, STREAMING_LIFETIME)
                         Clasificación: F10 EXPECTED BASELINE MISMATCH UNDER INVESTIGATION — golden desfasado, no regresión

Last SD copy status:      COPIED TO SD — 2026-08-21 12:51
                         Mount: /mnt/g is mountpoint, G: 60G FAT32 1.7G used (Windows G:\ verified)
                         Local SHA256:  cc21ab2623a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8
                         SD SHA256:     cc21ab2623a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8
                         Match: YES (cmp OK, Windows Get-FileHash + wsl sha256sum)

Last physical test:       SD TEST PASS — 2026-08-21 12:55 — usuario confirma Todo parece correcto
                         Validated runtime HEAD: e27c741
                         Artifact cc21ab SD TEST PASS, R36SX boot OK, 48k/stereo confirmado por build

Infrastructure checkpoint: 628484c — docs-only, creado después del SD TEST PASS, pusheado origin/feature/bacon-1.5-fx

Previous golden:          449041f — golden-bacon-1.4 (stable, ya no baseline actual)
```

---

## F10 baseline investigation

**Fecha:** 2026-08-21
**Test:** `tests/test_f10_baseline.py` → `F10_BASELINE_FAILED` → EXIT 1

```
Expected historical hash (GOLDENS["lgpt_r36sx_u2523.so"]):
  6a909dd1386be4079577a16dc5faa724b9d5b406b8b810248d4c3fba84002e52 (commit 5e9916c U2.61)

Current artifact hash (BUILD mutable):
  /mnt/d/R36S/PORT LPTRACKER/BUILD/U2523/lgpt_r36sx_u2523.so = dbad57a7aeb7d257259104ce5ba0ecc20e5927bd1f6bffdef6dcb826f823b96a (U2.71 dbad57)
  sd_root = afcf5ba756b0ee3d7d89a47d2f25d86ac0e9edf004755e42239f2c7c396d7021 (U2.61 afcf5ba)

Root cause: BUILD D: mutable + sd_root no actualizado + golden congelado desfasado → Caso B/C, no bug

Required action: NO cambiar GOLDENS a ciegas. Mantener 6a909dd hasta SD PASS, luego promover nuevo golden (cc21ab/dbadafter) en checkpoint validado
```

**Conclusión:** Caso B/C, no regresión. Golden NO actualizado en esta tarea docs-only.

---

## Invariantes del port (NO MODIFICAR sin DECISION)

```
TARGET HARDWARE = R36SX (v2.6, TreeFrogUI, kernel 4.4.186-release)
AUDIO SAMPLE RATE = 48000 Hz (TreeFrogAudio.cpp: return 48000; TreeFrogLibretro.cpp timing.sample_rate=48000.0; TreeFrogUac2Bridge rate 48000)
AUDIO CHANNELS = 2 / Stereo (SubmitStereo48000 / MixUsbCaptureMonitorStereo48000)
FINAL RUNTIME VALIDATION = SD + physical R36SX (nivel 4 obligatorio)
```

---

## Próxima acción exacta

```
Next action: Begin EQ8 audit/fix from current repository checkpoint 628484c.

Validated runtime baseline: e27c741
Validated artifact: cc21ab2623a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8
Physical validation: SD TEST PASS 2026-08-21

No pending infrastructure validation.

Any new runtime modification must again follow:
  CHANGE → BUILD → HOST TESTS → COPY TO SD → VERIFY HASH → TEST ON R36SX → USER CONFIRMS PASS → CHECKPOINT → PUSH

Note: This CURRENT.md update itself is docs-only (no runtime bytes changed), so NO NEW SD VALIDATION REQUIRED.
Previous physical validation (e27c741/cc21ab) remains valid.
  Evidence: git diff --name-only shows only CURRENT.md
```

---

## Condición de parada / Notas docs-only

```
Documentation-only metadata update.
No runtime bytes changed (source/, DSP, EQ, drivers, build scripts untouched).
Previous physical validation (e27c741/cc21ab 2026-08-21) remains valid — no repeat SD test needed.
If any runtime file appears in git diff, STOP and investigate.
```

---

## Validación humana / hardware

```
BUILD: DONE 2026-08-21 12:49 (cc21ab)
HOST TESTS: PARTIAL PASS (F10 mismatch documentado, resto OK)
COPY TO SD: DONE 2026-08-21 12:51 (local==SD cc21ab, Windows G: verificado)
R36SX: SD TEST PASS 2026-08-21 12:55 (usuario Todo parece correcto, 48k stereo por build)
CHECKPOINT: DONE 628484c 2026-08-21, pushed origin/feature/bacon-1.5-fx, clean
THIS DOCS UPDATE: NO NEW SD REQUIRED (docs-only)
```

---

## Handoff para próximo agente (leer en orden)

1. AGENTS.md
2. CURRENT.md (este archivo)
3. CONTEXT_MAP.md
4. DECISIONS.md
5. Comandos:
```
git -C /home/dafunknoise/lgpt-repo branch --show-current
git -C /home/dafunknoise/lgpt-repo rev-parse HEAD
git -C /home/dafunknoise/lgpt-repo status --short --branch
git -C /home/dafunknoise/lgpt-repo log --oneline --decorate -5
ls -lh /home/dafunknoise/lgpt-repo/source/dist/lgpt_libretro.so; sha256sum /home/dafunknoise/lgpt-repo/source/dist/lgpt_libretro.so
cat /home/dafunknoise/lgpt-repo/sd_root/VERSION.txt; scripts/audit.sh
```
6. Baseline: Infrastructure 628484c VALIDATED, Validated runtime e27c741 cc21ab SD PASS, Previous golden 449041f
7. No pending infrastructure — ready for EQ8 from 628484c
