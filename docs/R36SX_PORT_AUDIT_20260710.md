# Auditoría técnica inicial - LGPT R36SX v2.6 TreeFrogUI

Fecha: 2026-07-10

## Resumen

El paquete `LGPT_R36SX_STABLE_GITHUB_SOURCE_ONLY.zip` contiene una base coherente para un port de LittleGPTracker a R36SX v2.6 sobre TreeFrogUI. La integración técnica actual está orientada a producir un core libretro llamado `lgpt_libretro.so`, no una aplicación standalone.

## Evidencia principal

Archivos agregados frente a LittleGPTracker upstream:

- `projects/Makefile.TREEFROG`
- `sources/Adapters/TREEFROG/Audio/*`
- `sources/Adapters/TREEFROG/GUI/*`
- `sources/Adapters/TREEFROG/Libretro/libretro.h`
- `sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp`
- `sources/Adapters/TREEFROG/System/*`
- `sources/Adapters/TREEFROG/Timer/*`
- `r36sx_package/*`

## Coherencia del port

La fuente es coherente con una estrategia TreeFrog/libretro porque:

1. Existe un adaptador específico `TREEFROG`.
2. Existe un punto de entrada libretro en `TreeFrogLibretro.cpp`.
3. `Makefile.TREEFROG` declara explícitamente el objetivo `lgpt_libretro.so`.
4. El Makefile usa toolchain MIPS little-endian compatible con el ecosistema SF3000/R36SX.

## Problema detectado

La versión `SOURCE_ONLY` reorganizó carpetas del paquete R36SX, pero varios scripts conservaron rutas antiguas del paquete completo:

- Esperaban `r36sx_package/bin/`, pero existe `r36sx_package/wsl_scripts/`.
- Esperaban `r36sx_package/device/`, pero existe `r36sx_package/device_scripts/`.
- Esperaban `r36sx_package/src/`, pero existe `r36sx_package/source_overrides/`.
- Esperaban `r36sx_package/source_full/`, que no existe en la versión `SOURCE_ONLY`.

Por eso se agregaron scripts nuevos en `tools/wsl/`.

## Límite de esta auditoría

No se ejecutó compilación real porque el entorno de revisión no incluye el toolchain MIPS de TreeFrogUI ni acceso físico a la SD. La revisión realizada es estática: estructura, diffs, rutas, Makefile y coherencia del paquete.

## Decisión recomendada

Usar este repositorio como base limpia en GitHub y desarrollar en la rama:

```text
r36sx-v2.6-treefrog-au11z6
```

No mezclar todavía experimentos de puerto OTG/UAC2 con la rama estable. Crear una rama separada si se retoma ese trabajo.
