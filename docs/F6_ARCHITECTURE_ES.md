# F6 - Arquitectura objetivo (estructura de carpetas)

Tramo del refactor `refactor/bacon-1.2.1-preserve` (golden Bacon 1.2.1,
core MIPS byte-identico `7709b665`).

## Objetivo

Aplicar la estructura objetivo:

```
Application/Audio                    <- F4 (AudioDriverModeTable, AudioCapabilities,
                                          AudioRouter, AudioBackend, AudioEngine)
Application/UI/Input                 <- ya existia
Application/UI/Navigation            <- ya existia
Application/UI/Views                 <- F6: movido desde Application/Views
Services/Storage                     <- F5 (StoragePolicy)
Services/Audio, Midi, Controllers, Time  <- ya existian
Platform                             <- Adapters/TREEFROG + Adapters/Unix + device/
```

Regla del tramo: movimientos = movimientos puros + include fixes, en
commits separados por area; nunca mezclar con cambios de comportamiento.

## Que se hizo

- `git mv source/sources/Application/Views -> Application/UI/Views` (las
  subcarpetas ModalDialogs y BaseClasses incluidas).
- Include fixes en `source/sources/` (35 archivos): `Application/Views/`
  -> `Application/UI/Views/` y un include relativo (`Views/UIController.h`
  -> `UI/Views/UIController.h`).
- `source/projects/Makefile`: COMMONDIRS actualizado
  (`../sources/Application/UI/Views`, `/ModalDialogs`, `/BaseClasses`).
- Proyectos de IDE legacy que referenciaban la ruta vieja:
  `lgpt.xcodeproj`, `lgpt64.xcodeproj`, `lgpt.vcproj`, `lgpt.vcxproj`,
  `lgptest.dev` (mencion de carpetas/paths de ficheros).
- Tests y scripts: `tests/*.py` (40 baselines con rutas), runners host,
  `scripts/rc3_base_syntax_check.sh`, `tests/host_syntax_check.sh`.

No se movio nada mas: Services/Audio|Midi|Controllers|Time, Storage y
los adapters Platform ya estaban en su lugar objetivo; la regla de
"movimientos separados por area" se respeto (F6 es un solo movimiento
puro de la jerarquia de vistas).

## Evidencia

- Audit completo: `AUDIT_CLEAN_MAIN_U2523_OK` (todos los baselines F3,
  F4, F5 y los runner host pasan tras los include fixes).
- Build MIPS **byte-identico** `7709b665`: el movimiento de ficheros no
  cambia ninguna unidad de traduccion ni el codigo emitido.
- Gate diag `NO_DIAGNOSTICS_OUTSIDE_DEVICE`; deploy SD == build; backup
  `LGPT_BEFORE_U2523_20260813_230223`.
- Commit: 1ed9b30.