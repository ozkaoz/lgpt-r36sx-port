# Prompt para continuar el desarrollo desde U2.36 estable

Contexto: estoy continuando el port de LittleGPTracker para R36S/TreeFrogUI. La base actual validada en hardware es **U2.36 estable independiente**. Voy a formatear la SD, reinstalar TreeFrogUI y reconstruir desde este ZIP fuente.

Usa esta base como fuente de verdad. No retrocedas a U2.22/U2.23 ni reapliques parches anteriores salvo auditoría explícita. El árbol fuente ya contiene todos los cambios hasta U2.36.

## Estado validado

En runtime deben verse estos marcadores:

```text
Graphical Chopper U2.36
PITCH/ENV U2.36
```

Validado en hardware R36S:

- LGPT arranca y carga proyecto.
- Navegación de menús estable.
- Song `A+B` limpia celda; `Y+X` corta/compacta correctamente.
- Chopper normal estable: crear, borrar, seleccionar y preescuchar chops.
- Persistencia `.u2chop` estable al guardar/cerrar/recargar.
- Phrase muestra `S01/S02/etc.` para instrumentos chopeados.
- Crop Sample estable: `SELECT`, `R1+A`, `L2+Y`, `R1+X`, overlay centrado/progreso visible.
- Pitch/Envelope estable: `L1+R1`, `B` preview, `L2+B` stop, `A` apply, `L1+X` undo/redo, `Scope Sample`, `Scope Chop`, `R2+LEFT/RIGHT` para chop activo.
- Instrument sample menu estable: `Listen Import Manage Exit` en una línea.
- `A` sobre Listen preescucha sin mensaje; `L2+B` detiene.
- Import deduplica por contenido: el mismo WAV no se importa dos veces.
- Import protege contra sobrescritura si mismo nombre pero contenido distinto.
- Project Sample Manager estable: borrar libres `--`, purge conservador, force delete con `X` en dos pasos.

## Rutas y entorno

Ruta recomendada en WSL:

```bash
SRC="/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
```

Build:

```bash
cd "$SRC"
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
```

Core esperado:

```bash
$SRC/dist/lgpt_libretro.so
```

Copia a SD TreeFrogUI:

```text
F:\cubegm\cores\lgpt_libretro.so
```

Ajustar letra de SD según `Get-Volume`.

## Archivos críticos modificados hasta U2.36

- `sources/Application/Views/ModalDialogs/SampleChopperModal.cpp`
- `sources/Application/Views/ModalDialogs/SampleChopperModal.h`
- `sources/Application/Views/ModalDialogs/ImportSampleDialog.cpp`
- `sources/Application/Views/ModalDialogs/SampleManagerDialog.cpp`
- `sources/Application/Views/ModalDialogs/SampleManagerDialog.h`
- `sources/Application/Instruments/SamplePool.cpp`
- `sources/Application/Instruments/SamplePool.h`
- `sources/Application/Views/InstrumentView.cpp`
- `sources/Application/Views/SongView.cpp`
- `projects/Makefile`

Antes de tocar cualquiera de estos archivos, localizar los marcadores `TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE` y mantener compatibilidad con el protocolo final U2.36.

## Próxima fase sugerida: chops renderizados/exportables

Objetivo: hacer que los chops sean portables/exportables dentro de la carpeta del proyecto, sin contaminar la carpeta global de samples.

Requisitos de diseño:

1. Los chops renderizados deben guardarse en una carpeta del proyecto, por ejemplo:

```text
<project>/rendered_chops/<instrument_or_sample>/S01.wav
<project>/rendered_chops/<instrument_or_sample>/S02.wav
```

2. Al guardar el proyecto o modificar chops, los renders deben sobreescribirse de forma controlada.
3. No deben duplicarse renders innecesarios.
4. No debe contaminarse la carpeta global `samples`.
5. `.u2chop` sigue siendo la fuente de edición no destructiva de límites de chops.
6. Los renders son artefactos exportables/portables, no sustituto obligatorio de `.u2chop`.
7. Phrase debe permitir `S01/S02/etc.` cuando el instrumento tenga chops cargados.
8. Evaluar si Phrase debe restringir notas normales cuando el instrumento está en modo chops; no romper compatibilidad con proyectos antiguos.
9. El Sample Manager debe proteger los chops renderizados asociados y ofrecer purge/force delete consciente de esos artefactos.
10. Debe existir un protocolo de prueba específico de exportación: crear chops, guardar, verificar WAVs renderizados, cerrar, recargar, mover/copiar proyecto, comprobar que Phrase y audio siguen funcionando.

## Restricciones

No modificar sin justificación:

- Semántica de `Listen`: solo `A` sobre Listen preescucha; `B` directo en Instrument no preescucha.
- `L2+B` como stop de preview.
- Deduplicación de import por contenido.
- Protección de Sample Manager para chops/usados.
- Overlay centrado de operaciones.
- Previews de Pitch/Envelope ya validadas.

## Cómo trabajar

1. Crear incremento nuevo `U2.37`, no sobrescribir U2.36.
2. Hacer cambios mínimos por lote.
3. Generar patch y script aplicable desde WSL.
4. Compilar.
5. Probar en hardware antes de declarar estable.
6. Mantener README y TEST_PROTOCOL específicos de cada incremento.
