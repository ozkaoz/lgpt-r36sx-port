# LGPT R36SX U2.46 FINAL - Phrase workflow estable

Este paquete contiene el código fuente completo del port LGPT para R36SX/R36S en el estado aprobado como U2.46 FINAL. La base funcional estable incluye el flujo de exportación WAV de U2.41 y los cambios posteriores aprobados en Phrase, Chopper, Pitch/Envelope y navegación.

No incluye un binario precompilado `lgpt_libretro.so`. El core debe compilarse en WSL para evitar distribuir binarios obsoletos o generados en un entorno distinto.

## Estado aprobado

U2.46 queda como punto estable para continuar desarrollo. Los cambios validados por prueba en SD son:

- Exportación WAV organizada por proyecto: `lgpt/exports/<ProjectName>/`.
- Exportación multitrack con nombres de proyecto, instrumento y track.
- Pantalla de progreso no bloqueante para exportación.
- Columna de volumen en Phrase entre Note e Instrument.
- Mute/Solo coherente: `R1+A` solo/unmute all, `R1+B` mute.
- Preview en Phrase con `Y`.
- Selección/corte en Phrase: `L1+X` inicia selección, `X` corta selección activa.
- Doble `A` en Phrase abre Pitch/Envelope para el chop asignado.
- En ese Pitch/Envelope abierto desde Phrase, `R1+B` vuelve a Phrase.
- Navegación en Phrase:
  - `L2+LEFT` = previous used phrase.
  - `L2+RIGHT` = next used phrase.
  - `L2+UP` = previous phrase assignment in current Song channel.
  - `L2+DOWN` = next phrase assignment in current Song channel.
  - `L2+DOWN` crea/vincula un nuevo phrase si no hay asignación posterior en el canal actual.
- Chopper con edición posterior de chops, release, zero-crossing snap y undo/redo.
- Pitch/Envelope con normalización de Sample/Chop.

## Directorios esperados en Windows/WSL

Ruta de trabajo en Windows:

```text
D:\R36S\PORT LPTRACKER
```

La misma ruta en WSL:

```text
/mnt/d/R36S/PORT LPTRACKER
```

SD en Windows, ejemplo:

```text
F:\
```

SD en WSL:

```text
/mnt/f
```

El port queda instalado en:

```text
/mnt/f/roms/lgpt/start.lgpt
/mnt/f/lgpt/
```

Las exportaciones WAV quedan en:

```text
/mnt/f/lgpt/exports/<ProjectName>/
```

En Windows:

```text
F:\lgpt\exports\<ProjectName>\
```

## Compilar desde WSL

Copiar este ZIP a `D:\R36S\PORT LPTRACKER` y ejecutar:

```bash
cd ~

rm -rf lgpt_u246_final_build
mkdir -p lgpt_u246_final_build
cd lgpt_u246_final_build

unzip -q "/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_46_FINAL_PHRASE_WORKFLOW_SOURCE.zip"

cd LGPT_PORT_U2_46_FINAL_PHRASE_WORKFLOW_SOURCE

bash VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh
bash BUILD_U2_36_STABLE_WSL.sh
```

Al terminar debe existir:

```text
dist/lgpt_libretro.so
```

Copiar el core compilado a la carpeta de trabajo:

```bash
cp -f dist/lgpt_libretro.so "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_46_FINAL.so"
```

## Instalar en SD

Ajustar `F` si la SD usa otra letra:

```bash
bash INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh F "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_46_FINAL.so"
```

Verificar launchers:

```bash
find /mnt/f/roms -iname "*.lgpt" -print
```

Lo esperado es conservar solo:

```text
/mnt/f/roms/lgpt/start.lgpt
```

Verificar exportaciones:

```bash
find /mnt/f/lgpt/exports -iname "*.wav" -print
cat /mnt/f/lgpt/wav_export_debug.log
```

## Actualizar repo GitHub local

Ver `GITHUB_UPDATE_U2_46_ES.md` y `scripts/UPDATE_GITHUB_U2_46_FROM_WSL.sh`.

## Continuar desarrollo

Ver `CONTINUE_DEVELOPMENT_U2_46_ES.md` y `PROMPT_CONTINUAR_DESARROLLO_U2_47_DSP_MASTER_ES.md`.
