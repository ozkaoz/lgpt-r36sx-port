# Continuar desarrollo desde U2.46 FINAL

Este archivo define el punto de partida para continuar el port sin perder el estado estable aprobado.

## Base obligatoria

Usar como base:

```text
LGPT_PORT_U2_46_FINAL_PHRASE_WORKFLOW_SOURCE
```

No continuar desde U2.41, U2.42, U2.43, U2.44 o U2.45 salvo para comparar patches. U2.46 ya integra esos cambios y corrige el mapeo final de navegación en Phrase.

## Árbol de código principal

Archivos y zonas que han sido modificados con mayor probabilidad durante U2.41-U2.46:

```text
sources/Application/Views/SongView.*
sources/Application/Views/PhraseView.*
sources/Application/Views/ChainView.*
sources/Application/Views/MixerView.*
sources/Application/Views/MasterView.*
sources/Application/Views/ModalDialogs/SampleChopperModal.*
sources/Application/Views/InstrumentView.*
sources/Application/Instruments/SamplePool.*
sources/Application/Instruments/Instrument.*
sources/Application/Audio/*
sources/Application/Commands/*
sources/Application/FX/*
projects/Makefile
BUILD_U2_36_STABLE_WSL.sh
INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh
```

## Reglas de compatibilidad

Mantener compatibilidad con proyectos antiguos. Los proyectos sin bloque `VOLUMES` deben cargar cada step como `V--`, es decir, sin override de volumen por step.

Evitar cambios destructivos sin snapshot de undo. Las operaciones destructivas de sample/chop deben integrarse con `R1+X` cuando sea técnicamente viable.

No distribuir `dist/lgpt_libretro.so` dentro del paquete fuente estable. Compilarlo localmente.

## Flujo recomendado para una nueva versión

1. Crear rama a partir de U2.46 FINAL.
2. Aplicar un solo bloque funcional por versión de prueba.
3. Generar ZIP TEST, patch, notas y checksum.
4. Probar en SD.
5. Si se aprueba, generar ZIP FINAL de fuente completa.
6. Actualizar repo GitHub con script desde WSL.

## Convención de versiones siguiente

La siguiente línea de trabajo recomendada es U2.47 TEST, enfocada en commands/FX y Master.

No mezclar cambios de UI de Phrase con DSP/FX si no es necesario. U2.46 queda congelado como referencia estable de workflow.
