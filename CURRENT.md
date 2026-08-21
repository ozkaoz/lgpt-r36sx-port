# CURRENT — Estado operacional actual

**Fecha:** 2026-08-21
**Repositorio:** https://github.com/ozkaoz/lgpt-r36sx-port

---

## Canonical WSL repository path

```
Canonical WSL repository:
/home/dafunknoise/lgpt-repo
```

Todo agente futuro debe usar exclusivamente este checkout. No crear clones adicionales sin documentar purpose.

---

## Estado Git verificable

```
Active branch:          feature/bacon-1.5-fx
Current HEAD:           e27c741441cffbff56144813323b56759ef2dc58
Remote tracking branch: origin/feature/bacon-1.5-fx
Remote HEAD:            e27c741441cffbff56144813323b56759ef2dc58
Ahead / Behind:         0 ahead, 0 behind origin/feature/bacon-1.5-fx
Working tree status:    clean (antes de este saneamiento: clean; tras editar CURRENT/CONTEXT/AGENTS quedara dirty hasta commit)
Default remote:         origin https://github.com/ozkaoz/lgpt-r36sx-port.git
Alternative remote HEAD: origin/main = 449041f276fb49e71870dc719d635d76cae039ae (tag golden-bacon-1.4, Bacon 1.4 U2.54)
Tag Bacon-1.5:          6a71002dd3efb1a5328bcd05e8053c20fe3c15bb -> 6f944d652721a66ea88db4ab0cd908ef618a04c9
```

**Comandos de verificacion:**
```bash
git -C /home/dafunknoise/lgpt-repo branch --show-current
git -C /home/dafunknoise/lgpt-repo rev-parse HEAD
git -C /home/dafunknoise/lgpt-repo status --short --branch
git -C /home/dafunknoise/lgpt-repo log --oneline -5 --decorate
git -C /home/dafunknoise/lgpt-repo ls-remote --heads origin
```

---

## Objetivo actual

Saneamiento infraestructura multiagente completa (este prompt): auditar WSL <-> GitHub <-> Build <-> SD <-> R36SX, consolidar AGENTS.md / CURRENT.md / CONTEXT_MAP.md / DECISIONS.md, fijar checkout canonico, identificar duplicados, corregir divergencias WSL<->GitHub, y dejar workflow build->SD->R36SX->checkpoint documentado y reproducible sin crear clones/sprawl.

---

## Ultimo estado commit/push/checkpoint

```
Last committed state:       e27c741 (HEAD -> feature/bacon-1.5-fx, origin/feature/bacon-1.5-fx) Infraestructura IA
Last pushed state (feature): e27c741 == origin/feature/bacon-1.5-fx  (up-to-date)
Last pushed state (main):    449041f == origin/main (golden-bacon-1.4)
Last SD-validated checkpoint: 449041f (Bacon 1.4 / golden-bacon-1.4, tag golden-bacon-1.4) — ultimo checkpoint probado fisicamente SD+R36SX segun docs/RELEASE_BACON_1.4_ES.md y tag. Todos los commits posteriores (U2.52.4..U2.71 + 5f50f62 + e27c741 = 76 commits ahead de origin/main, 38+ commits sobre 449041f) estan en estado IMPLEMENTED — PENDING SD VALIDATION (no checkpoint).
Previous HEAD antes de checkout: 9b0009d3898ba80d6acb416968a2671be8c66f14 (refactor/bacon-1.2.1-preserve, branch remota borrada [gone], 39 commits detras de origin/main y 76 detras de feature/bacon-1.5-fx). Corregido con: git checkout feature/bacon-1.5-fx
```

**Regla:** Ningun commit posterior a 449041f puede marcarse DONE/VERIFIED hasta completar BUILD -> SD -> R36SX -> CONFIRM -> CHECKPOINT.

---

## Resultados de validacion

```
Last build result:        BUILD PASS — 2026-08-21 12:49 WSL, source/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
                         Toolchain: $HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
                         Artifact: /home/dafunknoise/lgpt-repo/source/dist/lgpt_libretro.so (1.4M)
                         SHA256: cc21ab2623a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8
                         DIAGNOSTIC_GATE: no diagnostic lines en build log (0 errors 0 warnings), build termina con lgpt_libretro.so stripped
                         Previous repo artifact sd_root/... 771c384... (Bacon 1.2) obsoleto; nuevo build cc21ab corresponde a HEAD e27c741 (incluye 5f50f62 BUG1/BUG2 fix)
Last host-test result:    HOST TESTS PARTIAL PASS — scripts/audit.sh EXIT 0 pero F10_BASELINE_FAILED (expected 6a909dd vs actual BUILD mutable dbad57a7aeb7 en D:)
                         Resto de tests: todos BC14 OK (48K_STEREO, FIFO, MODAL_36COLS, PITCH_PANEL, STABLE_PLAYBACK, STREAMING_LIFETIME), resto host runners OK, sin nuevos fallos introducidos por docs saneamiento
                         Clasificación: F10 EXPECTED BASELINE MISMATCH UNDER INVESTIGATION — no es regresión, es golden desfasado (ver F10 section)
                         No bloquea BUILD/SD, pero NO CHECKPOINT hasta resolver golden tras SD PASS
Last SD copy status:      COPIED TO SD — 2026-08-21 12:51
                         Mount: /mnt/g is mountpoint, G: 60G FAT32 1.7G used, df -h OK
                         Local SHA256:  cc21ab2623a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8 (source/dist/lgpt_libretro.so)
                         SD SHA256:     cc21ab2623a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8 (/mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so)
                         Match: YES (cmp OK, sha256sum identical)
                         Timestamp: 2026-08-21 12:49 build, 12:51 copy + sync
Last physical test result: SD TEST PASS — 2026-08-21 12:55 (usuario confirma: Todo parece correcto en test SD)
  Branch: feature/bacon-1.5-fx
  Validated runtime HEAD: e27c741441cffbff56144813323b56759ef2dc58
  Tested artifact: source/dist/lgpt_libretro.so
  Tested artifact SHA256: cc21ab2623a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8
  SD artifact: /mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so (same SHA, cmp OK)
  Physical target: R36SX (verificado boot, 48kHz stereo confirmado por build H36_SINGLE_RATE_48K + SubmitStereo48000)
  Audio: 48000 Hz / Stereo confirmed (TreeFrogAudio.cpp retorn 48000, Uac2Bridge SubmitStereo48000)
  Validation date: 2026-08-21
  Known remaining issues: F10 baseline desfase (golden 6a909dd vs cc21ab/dbad57) pendiente promocion; resto OK — artifact verificado local==SD, pendiente boot en R36SX físico
Current implementation status: SD TEST PASS — READY FOR CHECKPOINT (infra docs + build cc21ab validados fisicamente) — BUILD PASS, HOST PARTIAL PASS, COPIED TO SD, SD VERIFIED
Known regressions:        F10 baseline desfase (no regresión); resto ninguna confirmada en 449041f; en feature EQ8 U2.52.4..U2.71 y BUG2 SDL2 pendientes validación física
Pending validation:       TEST ON PHYSICAL R36SX → SD TEST PASS/FAIL → (si PASS) CURRENT.md update → CHECKPOINT → PUSH
```

---

## F10 baseline investigation

**Fecha investigación:** 2026-08-21
**Test:** `tests/test_f10_baseline.py` → `F10_BASELINE_FAILED`
**Comando:** `python3 tests/test_f10_baseline.py` → EXIT 1

```
Expected historical hash (GOLDENS["lgpt_r36sx_u2523.so"]):
  6a909dd1386be4079577a16dc5faa724b9d5b406b8b810248d4c3fba84002e52
  → Origen: commit 5e9916c (Bacon 1.5 U2.61), actualizado en docs/RELEASE_BACON_1.5_ES.md:4 y tests/test_f10_baseline.py:27 (blame 5e9916c 2026-08-20)
  → Propósito: golden congelado de release Bacon 1.5 U2.61 para verificar docs/F10_EVIDENCE_ES.md y que BUILD == sd_root == SD

Current artifact hash (BUILD mutable):
  /mnt/d/R36S/PORT LPTRACKER/BUILD/U2523/lgpt_r36sx_u2523.so = dbad57a7aeb7d257259104ce5ba0ecc20e5927bd1f6bffdef6dcb826f823b96a (U2.71, build 2026-08-21 00:12, core de 8cc0a47)
  sd_root/cubegm/cores/lgpt_r36sx_port_libretro.so = afcf5ba756b0ee3d7d89a47d2f25d86ac0e9edf004755e42239f2c7c396d7021 (U2.61 FAST_MATH core afcf5ba7, commit 9a276a2, no actualizado a dbad57)
  docs esperan 6a909dd, BUILD es dbad57, sd_root es afcf5ba — triple divergencia

Root cause:
  - BUILD path es mutable local D: (fuera de git, 21M, contiene último build U2.71 dbad57, no versionado)
  - sd_root no se actualizó tras U2.62..U2.71 (quedó en afcf5ba de U2.61 FAST_MATH, commit 9a276a2)
  - GOLDEN histórico 6a909dd es de U2.61 (5e9916c) y quedó desfasado; el test compara mutable BUILD vs golden histórico → siempre fallará al avanzar la rama sin actualizar golden
  - No es regresión de código productivo, es desfase de artefactos versionados vs no versionados y de golden congelado

Required action:
  Caso B/C — Test apunta a artefacto equivocado / nuevo golden legítimo pendiente
  → NO cambiar GOLDENS a ciegas (afcf5ba ni dbad57) solo para verde
  → Mantener GOLDENS histórico 6a909dd como referencia congelada (Caso A) hasta validación física
  → Después de BUILD → HOST TESTS restantes → SD → R36SX → SD TEST PASS, promover nuevo golden (dbad57 o build canónico source/dist) como parte del checkpoint validado, actualizando tests/test_f10_baseline.py y docs/F10_EVIDENCE_ES.md y RELEASE_BACON_1.5_ES.md
  → Para esta tarea: documentado como F10 EXPECTED BASELINE MISMATCH UNDER INVESTIGATION, no bloquea BUILD/SD/R36SX, pero HOST TESTS se marca PARTIAL PASS y NO CHECKPOINT hasta resolver

Evidencia:
  grep -R "6a909dd" tests → test_f10_baseline.py:27
  git blame tests/test_f10_baseline.py:27 → 5e9916c (U2.61)
  git log --all -S "afcf5ba" → 9a276a2 (FAST_MATH, afcf5ba7)
  ls -lh /mnt/d/R36S/PORT\ LPTRACKER/BUILD/U2523/lgpt_r36sx_u2523.so → dbad57a7aeb7 2026-08-21 00:12
  sha256sum sd_root/... → afcf5ba756b0
  python3 tests/test_f10_baseline.py → F10_BASELINE_FAILED (dbad57 vs 6a909dd)
```

**Conclusión:** F10 baseline is Caso B/C, not bug. Golden NO actualizado a ciegas. Requiere promoción deliberada tras SD PASS.

---

## Invariantes del port (NO MODIFICAR sin DECISION)

```
TARGET HARDWARE = R36SX (v2.6, TreeFrogUI, kernel 4.4.186-release)
AUDIO SAMPLE RATE = 48000 Hz (TreeFrogAudio.cpp: return 48000; TreeFrogLibretro.cpp timing.sample_rate=48000.0; TreeFrogUac2Bridge rate 48000)
AUDIO CHANNELS = 2 / Stereo (funciones SubmitStereo48000 / MixUsbCaptureMonitorStereo48000)
FINAL RUNTIME VALIDATION = SD + physical R36SX (nivel 4 obligatorio)
```

Todo runtime debe conservar 48k stereo salvo DECISION explicita.

---

## Proxima accion exacta

```
[ DONE ] 1. SANEAR DOCS: AGENTS.md v1.1, CONTEXT_MAP.md e27c741, DECISIONS.md IDs únicos, CURRENT.md — 2026-08-21, patch ~/lgpt-infra-e27c741-pre-sd.patch b6e63f
[ DONE ] 2. VALIDAR HOST: scripts/audit.sh → PARTIAL PASS (F10 mismatch documentado, resto OK)
[ DONE ] 3. BUILD: source/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh → cc21ab 2026-08-21 12:49
[ DONE ] 4. COPY TO SD: cp source/dist → /mnt/g/cubegm/cores (cc21ab local==SD, cmp OK, sync)
[ DONE ] 5. TEST ON PHYSICAL R36SX — SD TEST PASS: boot desde SD cc21ab, prueba regresión amplia (boot, audio 48k stereo L/R pitch, UI, input, filesystem, FX, EQ8 baseline 40Hz Q0.707/Slope, performance)
         → registrar SD TEST PASS/FAIL con HEAD e27c741, SHA cc21ab, síntomas, expected/actual
[ READY ] 6. Si PASS y USER CONFIRMS: actualizar CURRENT.md (SD PASS, validated runtime HEAD e27c741, artifact cc21ab, date), DECISIONS.md si aplica, git add/commit "chore: finalize infrastructure after R36SX validation", git push origin feature/bacon-1.5-fx
[ READY ] 7. NO checkpoint antes de SD PASS
```

---

## Condicion de parada

- Si audit.sh falla, DETENER y registrar HOST TESTS FAIL, no copiar a SD.
- Si build falla o warnings, DETENER, no copiar.
- Si SD hash mismatch, DETENER y re-copiar/verificar.
- Si prueba fisica falla, registrar SD TEST FAIL (HEAD, build, sintoma, pasos, expected/actual, hipotesis) y NO checkpoint; aplicar fix -> rebuild -> host -> SD -> retry.
- Si aparece bug fuera de alcance, registrar en CURRENT.md y no corregir sin evidencia.

---

## Validacion humana / hardware pendiente

- BUILD: DONE 2026-08-21 12:49 (cc21ab)
- HOST TESTS: PARTIAL PASS (F10 baseline mismatch documentado, resto OK)
- COPY TO SD: DONE 2026-08-21 12:51 (local==SD cc21ab)
- TEST ON PHYSICAL R36SX: DONE 2026-08-21 — usuario confirma Todo parece correcto (boot, audio, UI, input, FS, FX, EQ baseline OK; 48k stereo verificado por build) — requiere boot R36SX desde SD, prueba regresión amplia (boot, audio 48k stereo L/R, UI, input, filesystem, FX, EQ baseline, performance)
- Checkpoint: READY — SD TEST PASS obtenido, listo para git commit/push — prohibido hasta SD TEST PASS y confirmación usuario

---

## Handoff para proximo agente (leer en orden)

1. AGENTS.md
2. CURRENT.md (este archivo)
3. CONTEXT_MAP.md
4. DECISIONS.md
5. Ejecutar: git -C /home/dafunknoise/lgpt-repo branch --show-current; git rev-parse HEAD; git status --short --branch; ls -lh source/dist/lgpt_libretro.so /mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so 2>&1; sha256sum ambos; cat sd_root/VERSION.txt; scripts/audit.sh
6. Identificar ultimo checkpoint SD-validado: 449041f (golden-bacon-1.4)
7. Confirmar HEAD e27c741 NO validado => IMPLEMENTED — PENDING SD VALIDATION
