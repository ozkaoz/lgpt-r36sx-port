# Auditoría de consolidación U2.36 FINAL pre-OTG

## Entradas revisadas

- `LGPT_PORT_U2_36_ESTABLE_INDEPENDIENTE_R2_CON_INSTALADOR_LGPT.zip`
- `LGPT_PORT_U2_36_ESTABLE_INDEPENDIENTE.zip`
- `LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER.zip`
- `PORT_Version_Estable_con_CHOP_Integrado.zip`
- `PORT Versión Estable con Chop base.zip`
- `PORT LGPT Mixer U1 Base Limpia.zip`

## Resultado

Se seleccionó R2 como base de release porque conserva el árbol fuente U2.36 completo y añade instalador. La comparación fuente entre R2 y U2.36 independiente no arroja diferencias. La comparación con el ZIP “CHOP integrado” muestra que R2 contiene cambios posteriores en archivos críticos de U2.33-U2.36.

## Archivos críticos verificados

- `sources/Application/Views/ModalDialogs/SampleChopperModal.cpp`
- `sources/Application/Views/ModalDialogs/SampleChopperModal.h`
- `sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp`
- `sources/Application/Views/ModalDialogs/ImportSampleDialog.h`
- `sources/Application/Views/ModalDialogs/SampleManagerDialog.cpp`
- `sources/Application/Views/ModalDialogs/SampleManagerDialog.h`
- `sources/Application/Instruments/SamplePool.cpp`
- `sources/Application/Instruments/SamplePool.h`
- `sources/Application/Views/InstrumentView.cpp`
- `sources/Application/Views/SongView.cpp`
- `sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp`
- `projects/Makefile`

## Verificación local ejecutada

Se ejecutó:

```bash
bash ./VERIFY_U2_36_SOURCE.sh
```

Resultado: `OK: fuente U2.36 verificada.`

Limitación: no se recompiló `lgpt_libretro.so` en este entorno porque no está disponible el toolchain TreeFrog/SF3000 (`mipsel-buildroot-linux-gnu_sdk-buildroot`). El paquete mantiene los scripts de build para compilar localmente en WSL.

## Conclusión

Este paquete es una congelación estable fuente/instalador. Es apto como punto de partida para U2.37-OTG-AUDIO, pero OTG/audio sigue siendo una línea experimental separada y dependiente de kernel/rootfs.
