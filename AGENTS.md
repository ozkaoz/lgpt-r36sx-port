# AGENTS.md — Reglas permanentes para agentes de IA

**Versión:** 1.1
**Fecha:** 2026-08-21
**Repositorio:** https://github.com/ozkaoz/lgpt-r36sx-port
**Rama principal de desarrollo:** `feature/bacon-1.5-fx`
**Canonical WSL repository:** `/home/dafunknoise/lgpt-repo`

> Este archivo debe ser neutral respecto al proveedor del agente (Codex, ChatGPT, Copilot, Claude, Gemini, Cursor). Ninguna regla puede depender de una herramienta concreta.

---

## 1. PROTOCOLO DE INICIO

Antes de realizar cualquier trabajo, el agente DEBE:

1. **Leer en orden obligatorio:**
   - `AGENTS.md` (este archivo) — Reglas permanentes
   - `CURRENT.md` — Estado activo del desarrollo
   - `CONTEXT_MAP.md` — Mapa de navegación
   - `DECISIONS.md` — Memoria técnica (si existe)

2. **Verificar estado del repositorio:**
   ```bash
   git -C /home/dafunknoise/lgpt-repo branch --show-current
   git -C /home/dafunknoise/lgpt-repo rev-parse HEAD
   git -C /home/dafunknoise/lgpt-repo status --short --branch
   git -C /home/dafunknoise/lgpt-repo log --oneline -5 --decorate
   git -C /home/dafunknoise/lgpt-repo remote -v
   git -C /home/dafunknoise/lgpt-repo worktree list
   git -C /home/dafunknoise/lgpt-repo stash list
   ```

3. **Confirmar objetivo e hipótesis** antes de tocar código.

4. **Confirmar canonical WSL path, branch, HEAD, remote y último checkpoint SD-validado** leyendo `CURRENT.md`.

Si existen cambios locales no explicados en `CURRENT.md`:
```
NO empezar a editar todavía
```
Primero determinar qué son (Part II — auditoría no destructiva).

---

## 2. JERARQUÍA DE FUENTES DE VERDAD

Prioridad de autoridad (mayor → menor):

1. **Requisitos explícitos y especificaciones validadas** (incluyendo invariantes de este archivo)
2. **Decisiones técnicas activas documentadas** (`DECISIONS.md` con estado ACTIVE)
3. **Código = comportamiento actual** (puede contener el bug que debe corregirse; no es verdad absoluta)
4. **Documentación histórica** (`CHANGELOG.md`, `README.md`, docs antiguas)

**Regla:** Si hay discrepancia entre documentación y código, investigar cuál contiene el bug. Nunca asumir que "el código es la verdad absoluta" sin evidencia.

---

## 2b. INVARIANTES DEL PORT (NO NEGOCIABLES)

```
TARGET HARDWARE = R36SX (v2.6, TreeFrogUI, kernel 4.4.186-release)
AUDIO SAMPLE RATE = 48000 Hz (TreeFrogAudio.cpp: return 48000; TreeFrogLibretro.cpp timing.sample_rate=48000.0; TreeFrogUac2Bridge rate 48000)
AUDIO CHANNELS = 2 / Stereo (SubmitStereo48000 / MixUsbCaptureMonitorStereo48000)
FINAL RUNTIME VALIDATION = SD + physical R36SX (nivel 4 obligatorio; compilar != validar)
```

Todo runtime del port debe conservar 48k stereo salvo que exista una DECISION explícita que cambie el requisito (con ID DEC-YYYY-MM-DD-NN, evidencia y aprobación).
Buscar regresiones antes de aprobar cambios de audio:
```
44100, 44100.0, mono, 1 channel, fallback sample rate, unexpected resampling
```
No reemplazar valores ciegamente; investigar cada aparición.

---

## 3. REGLAS DE SEGURIDAD (NO NEGOCIABLES)

**PROHIBIDO alterar sin evidencia y validación:**
- Código de audio (DSP, mezclador, drivers, callbacks, EqBiquad, FxEngine, SampleInstrument, BassSynth, PianoSynth)
- Sistema de input (botones, encoders, USB, ChordResolver, NavigationController)
- Drivers USB / Audio / MIDI (TreeFrogUac2Bridge, r36s_u2523_usb_audio_io.c, r36s_sp404_host_audio_io.c)
- Timing, sample rates, buffers, latencia, ASRC, FIFO
- Proceso de instalación / compatibilidad hardware
- Assets (samples, fonts, configs)

**Si encuentras un bug funcional preexistente:**
1. NO lo corrijas automáticamente
2. Regístralo en `CURRENT.md` como "pendiente fuera de alcance"
3. Continúa con la tarea actual

---

## 4. REGLAS DE INGENIERÍA

1. **Cambios mínimos y verificables** — Solo lo necesario para la hipótesis actual, con evidencia (test, build, medición)
2. **Evidencia > Plan** — Si la evidencia contradice el plan, actualiza el contexto PRIMERO
3. **Commits atómicos** — Un cambio lógico por commit, mensaje claro
4. **Sin regresiones** — Ejecutar tests y auditorías antes de declarar éxito
5. **Estilo del proyecto** — Seguir convenciones existentes (naming, formato, comentarios)
6. **Cambios pequeños y verificables** — Un cambio → build → test → SD → PASS → checkpoint antes del siguiente. Evitar acumular fix A + fix B + refactor C antes de una única prueba final, especialmente en DSP/audio/drivers/input/filesystem/timing/threading.

---

## 5. REGLA FUNDAMENTAL — FLUJO OBLIGATORIO

Todo cambio ejecutable DEBE seguir obligatoriamente:

```
CHANGE
  ↓
BUILD
  ↓
HOST TESTS
  ↓
COPY TO SD
  ↓
TEST ON PHYSICAL R36SX
  ↓
CONFIRM RESULT
  ↓
CHECKPOINT
  ↓
CONTINUE
```

Está prohibido invertir este orden:
```
NO: CHANGE → CHECKPOINT → TEST LATER
SI: CHANGE → BUILD → TEST → SD → R36SX → CONFIRM → CHECKPOINT
```

Un cambio no puede considerarse terminado porque compila, pasa tests host, parece matemáticamente correcto, el diff parece correcto o funciona en simulación. La validación final es siempre:
```
SD + R36SX REAL
```

---

## 6. VALIDACIONES OBLIGATORIAS (4 NIVELES)

Antes de declarar éxito en cualquier cambio:

| Nivel | Nombre | Acción | Evidencia requerida |
|-------|--------|--------|---------------------|
| **1** | STATIC REVIEW | diff, code review, math, refs, lint | diff revisado |
| **2** | BUILD | Compilación limpia en WSL | command, result, warnings, artifact, HEAD |
| **3** | HOST TESTS | unit/DSP/audio/audit scripts | output de tests, logs |
| **4** | SD + R36SX | Validación final obligatoria para todo cambio que afecte runtime | foto/log de funcionamiento en R36SX real |

**No declarar éxito sin evidencia de nivel 4 si afecta runtime.** No utilizar estados ambiguos como `DONE`, `COMPLETE`, `VERIFIED`, `VALIDATED` sin haber pasado nivel 4. Usar estados normalizados:
```
NOT STARTED, IN PROGRESS, IMPLEMENTED, BUILD PASS/FAIL, HOST TESTS PASS/FAIL, READY FOR SD, COPIED TO SD, SD TEST PASS/FAIL, PENDING SD VALIDATION, CHECKPOINT READY/CREATED
```
Evitar: `looks good`, `probably fixed`, `should work`, `almost done`.

---

## 7. CICLO DE CHECKPOINTS OBLIGATORIOS

| Checkpoint | Nombre | Descripción |
|------------|--------|-------------|
| **C0** | Contexto | Leer AGENTS.md, CURRENT.md, CONTEXT_MAP.md. Verificar git. Confirmar objetivo e hipótesis. |
| **C1** | Implementación mínima | Modificar solo lo necesario para probar la hipótesis actual. |
| **C2** | Validación host | Ejecutar tests y auditorías relevantes. |
| **C3** | Build | Compilar en WSL. Registrar resultado exacto (hash, warnings, errores). |
| **C4** | Candidato HW | Copiar binario a `/mnt/g/cubegm/cores/` (verificar hash/size/build ID). Preparar SD. |
| **C5** | Validación física | Probar en R36SX real. Registrar commit, build y resultado. |
| **C6** | Consolidación | Actualizar CURRENT.md, DECISIONS.md, CONTEXT_MAP.md. Crear checkpoint y publicar según workflow. |

### Secuencia oficial
```
SD PASS → CONFIRM → CHECKPOINT → CONTINUE
```
Si `SD TEST FAIL`:
```
Registrar HEAD, build, síntoma, pasos, expected/actual, hipótesis → fix → build → host tests → copy to SD → physical test again
```
**Prohibido crear checkpoint antes de SD PASS.** Usar si no probado: `IMPLEMENTED — PENDING SD VALIDATION` o `HOST VALIDATED — SD VALIDATION REQUIRED — NO CHECKPOINT YET`.

### Condiciones de parada OBLIGATORIAS (DEBE detenerse si):

- Aparece una regresión
- Aparece una dependencia inesperada
- Nueva evidencia contradice la hipótesis
- Se necesita modificar otro subsistema no previsto
- El alcance comienza a crecer (scope creep)
- La evidencia es insuficiente
- Se alcanza un checkpoint que requiere intervención humana
- Se requiere validación física que el agente no puede realizar

**PROHIBIDO:** "Seguir arreglando cosas" automáticamente después de una parada. Debe pedir intervención humana.

---

## 8. FLUJO OBLIGATORIO CON LA SD

Definir cuál es la SD de desarrollo y evitar confundirla con copias locales antiguas.

El agente debe saber:
```
artefacto exacto:      source/dist/lgpt_libretro.so (build canonico) y sd_root/cubegm/cores/lgpt_r36sx_port_libretro.so (payload)
ruta origen:           /home/dafunknoise/lgpt-repo/source/dist/lgpt_libretro.so
ruta destino en SD:    /mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so
cómo comprobar que es el build nuevo: sha256sum, size, build ID / VERSION, commit SHA en sd_root/VERSION.txt
cómo arrancarlo:       boot R36SX desde esa SD, verificar cubegm/lgpt y VERSION
qué probar:            boot, audio L/R, UI, controls, EQ, FX, filesystem, loading/saving, performance, crashes/glitches según cambio
```

**Antes de copiar a SD verificar:** branch correcta, HEAD correcto, working tree conocido, build PASS, artefacto correcto.
**Después de copiar verificar:** size/hash/build ID entre build local y SD.
**Prueba física:** arrancar R36SX desde esa SD y probar exactamente la funcionalidad afectada. No declarar PASS sin observar comportamiento relevante. Asociar build a commit SHA / build ID / timestamp verificable siempre que sea viable.

**Artefacto único:** Después de cada build debe ser posible responder sin búsqueda manual:
```
¿Cuál archivo debo copiar a SD? → /home/dafunknoise/lgpt-repo/source/dist/lgpt_libretro.so
```

Directorios `BUILD/`, `buildTREEFROG/`, `source/dist/`, `sd_root/` usan `.gitignore` para no confundirse con SOURCE. `SOURCE` vs `BUILD` vs `DEPLOY/STAGING` vs `SD` deben quedar claros.

---

## 9. PROHIBICIONES EXPLÍCITAS

| Prohibido | Motivo |
|-----------|--------|
| Asumir que compila = funciona en hardware | Build ≠ validación en dispositivo |
| Corregir bugs fuera del alcance sin registro | Scope creep, riesgo de regresión |
| Asumir que la documentación está actualizada | Código puede contener bug |
| Modificar build system sin necesidad | Riesgo de romper build |
| "Seguir arreglando" tras parada | Debe pedir intervención humana |
| Marcar "verificado" sin evidencia | Build ≠ validación en hardware |
| Crear checkpoint antes de SD PASS | Viola regla fundamental |
| Crear clones duplicados innecesarios | Un solo checkout canónico + worktrees si hace falta |
| Ejecutar rm -rf / reset --hard / clean -fd sin auditoría no destructiva | Riesgo de pérdida de trabajo único |

---

## 10. MANTENIMIENTO DE CONTEXTO (OBLIGATORIO)

**Archivo crítico:** `CURRENT.md` — Debe actualizarse:
- DURANTE el trabajo (no solo al final)
- Después de CADA checkpoint (C0–C6)
- Con evidencia concreta, no intenciones

Debe contener como mínimo (Parte V):
```
Canonical WSL repo, Active branch, Current HEAD, Remote tracking, Working tree status,
Current objective, Last committed state, Last pushed state, Last SD-validated checkpoint,
Last build result, Last host-test result, Last physical test result,
Current implementation status, Known regressions, Pending validation, Next exact action
```

**Reglas:**
- No marcar "verificado" sin evidencia
- No convertir en changelog
- Diferenciar claramente "validado" vs "pendiente"

**Flujo obligatorio:**
```
Leer contexto → Trabajar → Obtener evidencia → Actualizar contexto → Checkpoint → Continuar
```

---

## 11. ACTUALIZACIÓN DE AGENTS.md

**AGENTS.md cambia SOLO cuando:**
- Cambian reglas permanentes de seguridad/ingeniería / protocolo de checkpoints / jerarquía / invariantes

**NO se actualiza por:**
- Tareas individuales → CURRENT.md
- Bugs encontrados → CURRENT.md
- Estado temporal → CURRENT.md

---

## 12. HANDOFF FINAL (antes de terminar)

Antes de terminar una sesión, el agente DEBE:

- [ ] Actualizar `CURRENT.md` con estado exacto (canonical path, branch, HEAD, remote, git status, qué se cambió, build/host/SD/physical results, known failures, pending, next exact action)
- [ ] Diferenciar claramente "validado" vs "pendiente"
- [ ] Registrar inconsistencias encontradas
- [ ] Actualizar `DECISIONS.md` solo si hubo decisiones duraderas (ID DEC-YYYY-MM-DD-NN, Date, Status ACTIVE/SUPERSEDED/DEPRECATED, Context, Decision, Reason, Consequences, Evidence, Related files)
- [ ] Actualizar `CONTEXT_MAP.md` solo si cambió el mapa de recursos
- [ ] Verificar que `AGENTS.md` establece mantenimiento continuo
- [ ] Revisar que ningún archivo contenga afirmaciones sin evidencia
- [ ] Preparar handoff breve con: archivos creados/modificados y función, estado actual (rama, commit, SHA), evidencia (logs, tests, hashes), trabajo pendiente, próximo checkpoint, condición de parada, qué debe leer primero el próximo agente
- [ ] No depender del contexto de chat del agente anterior

---

## 13. REGLA PERMANENTE

> **La evidencia tiene prioridad sobre el plan.**
> Si nueva evidencia contradice lo escrito, actualiza el contexto PRIMERO, ajusta la hipótesis, y continúa SOLO desde el siguiente checkpoint válido.
>
> **Compilar no es validar.** Un build exitoso NO es evidencia de funcionamiento correcto en hardware.
>
> **Los archivos de contexto son parte del desarrollo.** Mantenerlos actualizados es tan importante como escribir buen código.
>
> **Un solo checkout canónico + historial git coherente + documentación multiagente actualizada + builds identificables + validación real en SD/R36SX + cero pérdida de trabajo local.**

Arquitectura final:
```
               GitHub
                 │
                 │ versioned source
                 ▼
      Canonical WSL Git checkout (/home/dafunknoise/lgpt-repo)
                 │
                 │ build
                 ▼
         Build artifacts (source/dist/lgpt_libretro.so)
                 │
                 │ deploy
                 ▼
                SD (/mnt/g)
                 │
                 │ boot/test
                 ▼
              R36SX
                 │
                 │ result
                 ▼
         CONFIRMATION
                 │
                 ▼
            CHECKPOINT
                 │
                 ▼
              GitHub
```
