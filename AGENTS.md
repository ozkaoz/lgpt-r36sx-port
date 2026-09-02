# AGENTS.md — Reglas permanentes para agentes de IA

**Versión:** 1.0
**Fecha:** 2026-08-21
**Repositorio:** https://github.com/ozkaoz/lgpt-r36sx-port
**Rama principal de desarrollo:** `feature/bacon-1.5-fx`

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
   git branch --show-current
   git rev-parse HEAD
   git status --short
   git log --oneline -5
   ```

3. **Confirmar objetivo e hipótesis** antes de tocar código.

---

## 2. JERARQUÍA DE FUENTES DE VERDAD

Prioridad de autoridad (mayor → menor):

1. **Código** — La única verdad absoluta
2. **CURRENT.md** — Estado operativo actual
3. **DECISIONS.md** — Decisiones técnicas duraderas
4. **CONTEXT_MAP.md** — Mapa de navegación
5. **CHANGELOG.md** — Historial de cambios
6. **README.md** — Documentación pública

**Regla:** Si hay discrepancia entre documentación y código, el **CÓDIGO es la verdad**.

---

## 2b. ENTORNO DE COMPILACIÓN (actualizado 2026-09-01 — ver `docs/TOOLCHAINS.md`)

| Qué | Dónde (WSL) | Dónde (Windows) |
|-----|-------------|-----------------|
| Este repo | `/mnt/d/Github/lgpt-r36sx-port` | `D:\GitHub\lgpt-r36sx-port` |
| Bases de compilación (kernel vanilla 4.4.186, LGPT U2523 source, iteraciones audio) | `/mnt/d/Toolchains/R36SX/` | `D:\Toolchains\R36SX\` |
| Toolchain MIPS (Codescape GNU 6.3.0) | `~/sf3000-work/sf3000toolchain/` — **NO mover** | — (vive en WSL) |
| Backups SD + git bundles | `/mnt/d/R36S/PORT LPTRACKER/BACKUPS/` (siguen ahí) | `D:\R36S\PORT LPTRACKER\BACKUPS\` |
| Evidencia forense FAT32 | — | `D:\R36S\PORT LPTRACKER\PAPELERA_20260811\` (irrepetible) |
| Evidencia física/binaria del port | `physical-evidence/` (en este repo) | idem |

Variables de los scripts de build (defaults desde 2026-09-01):
```bash
PROJECT_ROOT=/mnt/d/Toolchains/R36SX     # antes: /mnt/d/R36S/PORT LPTRACKER
SOURCE=$PROJECT_ROOT/lgpt-u2523-source     # antes: WORK/U2523_SOURCE
TC=~/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
```

Docs históricos (RELEASE_BACON_*, COPY_ROOT) conservan rutas de su época con nota aclaratoria — no corregirlos salvo que se re-edite su contenido.

---

## 3. REGLAS DE SEGURIDAD (NO NEGOCIABLES)

**PROHIBIDO alterar sin evidencia y validación:**
- Código de audio (DSP, mezclador, drivers, callbacks)
- Sistema de input (botones, encoders, USB)
- Drivers USB / Audio / MIDI
- Timing, sample rates, buffers, latencia
- Proceso de instalación / compatibilidad hardware
- Assets (samples, fonts, configs)

**Si encuentras un bug funcional preexistente:**
1. NO lo corrijas
2. Regístralo en `CURRENT.md` como "pendiente fuera de alcance"
2. Continúa con la tarea actual

---

## 4. REGLAS DE INGENIERÍA

1. **Cambios mínimos** — Solo lo necesario para la hipótesis actual
2. **Verificables** — Cada cambio debe tener evidencia (test, build, medición)
3. **Evidencia > Plan** — Si la evidencia contradice el plan, actualiza el contexto PRIMERO
3. **Commits atómicos** — Un cambio lógico por commit, mensaje claro
4. **Sin regresiones** — Ejecutar tests y auditorías antes de declarar éxito
5. **Estilo del proyecto** — Seguir convenciones existentes (naming, formato, comentarios)

---

## 5. VALIDACIONES OBLIGATORIAS

Antes de declarar éxito en cualquier cambio:

| Checkpoint | Acción | Evidencia requerida |
|------------|--------|---------------------|
| **C2** Validación host | Tests y auditorías relevantes | Output de tests, logs |
| **C3** Build | Compilación limpia en WSL | Log de build, binario generado |
| **C4** Candidato HW | Preparar binario para SD | Binario en `/mnt/g/cubegm/cores/` |
| **C5** Validación física | Prueba en R36SX real | Foto/log de funcionamiento |

**No declarar éxito sin evidencia.**

---

## 6. CICLO DE CHECKPOINTS OBLIGATORIOS

| Checkpoint | Nombre | Descripción |
|------------|--------|-------------|
| **C0** | Contexto | Leer AGENTS.md, CURRENT.md, CONTEXT_MAP.md. Verificar git. Confirmar objetivo e hipótesis. |
| **C1** | Implementación mínima | Modificar solo lo necesario para probar la hipótesis actual. |
| **C2** | Validación host | Ejecutar tests y auditorías relevantes. |
| **C3** | Build | Compilar en WSL. Registrar resultado exacto (hash, warnings, errores). |
| **C4** | Candidato HW | Copiar binario a `/mnt/g/cubegm/cores/`. Preparar SD. |
| **C5** | Validación física | Probar en R36SX real. Registrar commit, build y resultado. |
| **C6** | Consolidación | Actualizar CURRENT.md, DECISIONS.md, CONTEXT_MAP.md. Preparar handoff. |

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

## 7. PROHIBICIONES EXPLÍCITAS

| Prohibido | Motivo |
|-----------|--------|
| Asumir que compila = funciona en hardware | Build ≠ validación en dispositivo |
| Corregir bugs fuera del alcance sin registro | Scope creep, riesgo de regresión |
| Asumir que la documentación está actualizada | Código = verdad |
| Modificar build system sin necesidad | Riesgo de romper build |
| "Seguir arreglando" tras parada | Debe pedir intervención humana |
| Marcar "verificado" sin evidencia | Build ≠ validación en hardware |

---

## 8. MANTENIMIENTO DE CONTEXTO (OBLIGATORIO)

**Archivo crítico:** `CURRENT.md` — Debe actualizarse:
- DURANTE el trabajo (no solo al final)
- Después de CADA checkpoint (C0–C6)
- Con evidencia concreta, no intenciones

**Reglas:**
- No marcar "verificado" sin evidencia
- No convertir en changelog
- Diferenciar claramente "validado" vs "pendiente"

**Flujo obligatorio:**
```
Leer contexto → Trabajar → Obtener evidencia → Actualizar contexto → Checkpoint → Continuar
```

---

## 8. ACTUALIZACIÓN DE AGENTS.md

**AGENTS.md cambia SOLO cuando:**
- Cambian reglas permanentes de seguridad/ingeniería
- Cambia el protocolo de checkpoints
- Cambia la jerarquía de fuentes de verdad

**NO se actualiza por:**
- Tareas individuales
- Estado temporal → eso va en CURRENT.md
- Bugs encontrados → se registran en CURRENT.md

---

## 9. HANDOFF FINAL (antes de terminar)

Antes de terminar una sesión, el agente DEBE:

- [ ] Actualizar `CURRENT.md` con estado exacto (rama, commit, validaciones)
- [ ] Diferenciar claramente "validado" vs "pendiente"
- [ ] Registrar inconsistencias encontradas
- [ ] Actualizar `DECISIONS.md` solo si hubo decisiones duraderas
- [ ] Actualizar `CONTEXT_MAP.md` solo si cambió el mapa de recursos
- [ ] Verificar que `AGENTS.md` establece mantenimiento continuo
- [ ] Revisar que ningún archivo contenga afirmaciones sin evidencia
- [ ] Preparar handoff breve con:
  - Archivos creados/modificados y función
  - Estado actual (rama, commit, SHA)
  - Evidencia disponible (logs, tests, hashes)
  - Trabajo pendiente
  - Próximo checkpoint
  - Condición de parada
  - Qué debe leer primero el próximo agente

---

## 10. REGLA PERMANENTE

> **La evidencia tiene prioridad sobre el plan.**
> Si nueva evidencia contradice lo escrito, actualiza el contexto PRIMERO, ajusta la hipótesis, y continúa SOLO desde el siguiente checkpoint válido.
>
> **Compilar no es validar.** Un build exitoso NO es evidencia de funcionamiento correcto en hardware.
>
> **Los archivos de contexto son parte del desarrollo.** Mantenerlos actualizados es tan importante como escribir buen código.