# Metodología para continuar el desarrollo

1. No modificar varias áreas a la vez si una prueba falla. Aislar por bloque: input, UI, playback, Phrase, persistencia o escritura WAV.
2. Cada cambio debe entregarse como script aplicable en WSL Ubuntu 24 sobre el árbol estable actual.
3. El script debe crear backup antes de modificar archivos.
4. El script debe verificar patrones críticos antes de compilar y fallar temprano si el árbol no coincide.
5. La compilación se considera válida solo con `BUILD_RC=0` y existencia de `dist/lgpt_libretro.so`.
6. La instalación en SD se considera válida solo si `LOCAL_SHA256` y `SD_SHA256` coinciden.
7. Toda función nueva debe tener protocolo de prueba mínimo y prueba de regresión.
8. No asumir que una combinación de botones funciona: validar en consola. `SELECT`, `R1`, `L1`, `A`, `B`, `X`, `Y`, `R2` y `L2` pueden tener diferencias según flags de build.
9. Evitar cambios destructivos sin undo/redo y sin overlay de operación claro.
10. Mantener la separación conceptual: Chopper corta, Phrase secuencia, PTCH modifica pitch no destructivo por patrón, PITCH SAMPLE modifica el WAV físicamente.

## Rutas de trabajo usadas

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
WORK="/mnt/d/R36S/PORT LPTRACKER"
SD_CORE="F:\cubegm\cores\lgpt_libretro.so"
```
