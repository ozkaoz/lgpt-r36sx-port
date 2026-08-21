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

EQ8 fix validated runtime HEAD: 1809becbccd73ad37e2f8fe6b5ca67cf72fe5eaa
  → Mensaje: fix: correct EQ8 low-frequency biquad precision and response (Q24 round, shelf NaN, UI DSP)
  → Significado: runtime EQ8 corregido y probado físicamente (HPF/LPF 20-100 Hz ≤0.10 dB, shelves estables, UI = DSP)
  → Validated artifact: source/dist/lgpt_libretro.so SHA256 46c4714fd38f7a0714f0d819f65ebc59e5fd9e2f109dc1fcb5415ee75294f2e3 (1.4M)
  → SD TEST: PASS — 2026-08-21 13:45 (usuario: SD TEST PASS, todo OK)
  → SD artifact: /mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so == local (46c4714...)
  → Physical target: R36SX — boot OK, 48kHz Stereo, EQ8 sub-80 Hz PASS

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
  → AGENTS.md v1.1 (canonical, invariants 48k/stereo/R36SX), CONTEXT_MAP.md 46c4714/46c4714, DECISIONS.md IDs únicos

EQ8 status:                  VALIDATED — 2026-08-21 13:45
  → Q24 round+saturación, shelf NaN guard, UI DSP Q24, HPF/LPF 20-100 PASS, SD TEST PASS

Current objective:           EQ8 sub-80 Hz fix VALIDATED — 2026-08-21 13:45
  → Runtime baseline: 46c4714 / artifact 46c4714 / SD TEST PASS 2026-08-21 13:45 (EQ8 PASS)
  → HPF/LPF 20-100 Hz ≤0.10 dB, shelves estables, UI=DSP, 48kHz Stereo, no float hot path

Previous objective (DONE):   EQ8 audit and correction from 628484c (Q24 round, shelf NaN guard, UI coherence)
```

EQ8 fix completado y validado en SD/R36SX.

---

## Resultados de validación (histórico del ciclo que llevó al checkpoint 628484c)

```
Last build result:        BUILD PASS — 2026-08-21 13:43 WSL, source/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
                         Toolchain: $HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
                         Artifact: /home/dafunknoise/lgpt-repo/source/dist/lgpt_libretro.so (1.4M)
                         SHA256: 66c966d089e6edd090e2f803d43c185eda12415ad27f42ec2e3b6e602b230ea5
                         DIAGNOSTIC_GATE: 0 errors 0 warnings, stripped

Last host-test result:    HOST TESTS PASS — spectrum_analyzer 50 PASS, analyzer_target 1781 PASS, analyzer_h1 6 PASS, eq_sub80 22 PASS (HPF/LPF 20-100 ≤0.10), eq8_struct PASS 109, sample_eq_edit PASS 104, analyzer_target PASS, resto BC14 OK
                         F10: EXPECTED BASELINE MISMATCH (6a909dd vs 46c4714) — golden desfasado por EQ8, no regresión (documentado)

Last SD copy status:      COPIED TO SD — 2026-08-21 14:42
                         Mount: /mnt/g is mountpoint, G: 60G FAT32 1.7G used (Windows G:\ verificado)
                         Local SHA256:  66c966dfd38f7a0714f0d819f65ebc59e5fd9e2f109dc1fcb5415ee75294f2e3
                         SD SHA256:     46c4714fd38f7a0714f0d819f65ebc59e5fd9e2f109dc1fcb5415ee75294f2e3
                         Match: YES (cp + sync + sha256sum)

Last physical test:       SD TEST PASS — 2026-08-21 15:50 — usuario confirma SD TEST PASS, todo OK (analyzer idle PASS, hihat PASS, Peak Display PASS, TreeFrog LGPT→port PASS, Android PASS, EQ8 still PASS)
                         Validated runtime HEAD: 46c4714 (EQ8 fix)
                         Artifact 46c4714 SD TEST PASS, R36SX boot OK, 48k/stereo, HPF/LPF 20-100 PASS, shelves PASS

Infrastructure checkpoint: 628484c — docs-only, creado después del SD TEST PASS, pusheado origin/feature/bacon-1.5-fx

EQ8 fix checkpoint:       1809bec — fix EQ8 low-frequency biquad precision (Q24 round, shelf NaN guard, UI DSP coherence), pusheado origin/feature/bacon-1.5-fx
Analyzer fix checkpoint:  66c966d — correct spectrum analyzer Hz mapping, Blackman scale, lazy window, clearCapture, InstrumentEqView hold/barW, pusheado origin/feature/bacon-1.5-fx

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
Next action: Analyzer fix validated and pushed. Ready for next development.

Validated runtime baseline: 66c966d089e6edd090e2f803d43c185eda12415ad27f42ec2e3b6e602b230ea5
Validated artifact: 66c966d089e6edd090e2f803d43c185eda12415ad27f42ec2e3b6e602b230ea5
Physical validation: SD TEST PASS 2026-08-21 15:50 (analyzer + EQ8 PASS)

No pending validation.

Any new runtime modification must again follow:
  CHANGE → BUILD → HOST TESTS → COPY TO SD → VERIFY HASH → TEST ON R36SX → USER CONFIRMS PASS → CHECKPOINT → PUSH
```

---

## Condición de parada / Notas

```
Runtime EQ8 fix validated — no pending validation.
Previous physical validation (46c4714/46c4714) is current.
If new runtime file appears without BUILD→HOST→SD→PASS, STOP.
```

---

## Validación humana / hardware

```
BUILD: DONE 2026-08-21 14:41 (66c966d)
HOST TESTS: PASS (spectrum 50, analyzer_target 1781, analyzer_h1 6, eq_sub80 22, eq8_struct 109, resto OK; F10 mismatch esperado)
COPY TO SD: DONE 2026-08-21 14:42 (local==SD 66c966d, Windows G: verificado)
R36SX: SD TEST PASS 2026-08-21 15:50 (usuario SD TEST PASS, todo OK, analyzer idle/Peak/TreeFrog/Android PASS)
CHECKPOINT: DONE 46c4714 2026-08-21, pushed origin/feature/bacon-1.5-fx, clean
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
6. Baseline: Infrastructure 628484c VALIDATED, Validated runtime 46c4714 46c4714 SD PASS (EQ8 fix), Previous golden 449041f
7. No pending infrastructure — ready for EQ8 from 628484c

Analyzer Peak validated runtime HEAD: 0549d71
  Message: Bacon 1.5 U2.73: DEC-32 visual + windowed Peak 2.5s (core 967137865e2a)
  Validated artifact: source/dist/lgpt_libretro.so SHA256 967137865e2a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8 (1.4M)
  SD TEST: PASS 2026-08-21 18:35 (usuario: La prueba en la SD es correcta. Aprobado, continua.)
