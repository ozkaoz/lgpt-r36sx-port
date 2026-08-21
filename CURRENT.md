# CURRENT — Estado actual del desarrollo

**Fecha:** 2026-08-21
**Rama:** feature/bacon-1.5-fx
**Commit:** 8cc0a47e913b5a031051fed02f15549d312063a2

## Objetivo actual
Implementar infraestructura de contexto y continuidad para agentes de IA (AGENTS.md, CURRENT.md, CONTEXT_MAP.md, DECISIONS.md) sin alterar comportamiento funcional del port LGPT para R36SX.

## Contexto inmediato
- Repositorio: https://github.com/ozkaoz/lgpt-r36sx-port
- Rama activa: feature/bacon-1.5-fx
- Commit actual: 8cc0a47 (Bacon 1.5 U2.71: fix all types <80Hz Q limit, double EqBiquad)
- SD montada en G: (accesible desde WSL como /mnt/g)
- Core actual en SD: DBAD57A7 (U2.71) — pero commit actual es 8cc0a47 con hash DBAD57A7
- Working tree clean, sin cambios pendientes

## Hipótesis vigente
La infraestructura de contexto (AGENTS.md, CURRENT.md, CONTEXT_MAP.md, DECISIONS.md) puede implementarse sin alterar NINGÚN comportamiento funcional del port. Los bugs críticos (EQ < -80 dB y SDL2 audio drivers) son tareas separadas ya documentadas en el issue tracker, NO parte de esta infraestructura.

## Implementación realizada
- [x] AGENTS.md creado con reglas permanentes para agentes
- [ ] CURRENT.md (este archivo) — en progreso
- [ ] CONTEXT_MAP.md — pendiente
- [ ] DECISIONS.md — pendiente

## Evidencia obtenida
- `git status` → working tree clean
- `git log --oneline -1` → 8cc0a47 Bacon 1.5 U2.71
- `git branch --show-current` → feature/bacon-1.5-fx
- `git remote -v` → origin https://github.com/ozkaoz/lgpt-r36sx-port.git
- SD G: verificada — core actual DBAD57A7AEB7D257259104CE5BA0ECC20E5927BD1F6BFFDEF6DCB826F823B96A en /mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so
- Proyecto lgpt_KAOZ en SD con EQ configurado (hipass 40Hz en banda 0)

## Validado
- [x] Working tree clean (git status)
- [x] Rama feature/bacon-1.5-fx al día con origin
- [x] SD G: montada y accesible desde WSL (/mnt/g)
- [x] Core actual en SD coincide con commit 8cc0a47 (DBAD57A7)
- [x] AGENTS.md creado con reglas permanentes

## Pendiente de validación
- [ ] CONTEXT_MAP.md creado y validado
- [ ] DECISIONS.md creado
- [ ] Verificación de que no se alteró código productivo
- [ ] Commit y push de archivos de infraestructura

## Riesgos / Bloqueos
- Ninguno identificado para la infraestructura de contexto
- Bugs críticos (EQ < -80 dB y SDL2 audio drivers) son tareas separadas, NO bloquean esta infraestructura

## Siguiente acción
Crear CONTEXT_MAP.md con mapa de navegación del repositorio

## Próximo checkpoint
C1 — Implementación mínima (crear CONTEXT_MAP.md)

## Condición de parada
Si se detecta cualquier modificación accidental a código productivo (source/sources/**, no documentación), DETENERSE inmediatamente.

## Validación humana / hardware pendiente
Ninguna para esta infraestructura. Pruebas de bugs críticos (EQ < -80 dB y drivers SDL2) requieren sesión separada con dispositivo físico.