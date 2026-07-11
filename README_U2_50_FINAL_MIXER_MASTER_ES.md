# LGPT R36SX Port — U2.50 FINAL Mixer/Master

Estado: versión estable aprobada en SD después de U2.50 TEST.

Base estable anterior: U2.46 FINAL Phrase Workflow.

Esta entrega consolida:

- Workflow completo de Phrase probado en U2.46.
- Exportación WAV estable por proyecto, Song WAV y multitrack.
- Chopper con edición posterior de chops, release y normalización.
- Columna de volumen por step en Phrase.
- Navegación Phrase/Song con L2 + flechas.
- Mixer rediseñado con paneo por track, tempo desde Mixer y medidores Master L/R.
- Project/Master con medidores verticales reubicados.
- Commands/FX depurados, manteniendo compatibilidad interna para proyectos antiguos.

## Estructura esperada en Windows / WSL

Carpeta de trabajo en Windows:

```text
D:\R36S\PORT LPTRACKER
```

La misma carpeta en WSL:

```bash
/mnt/d/R36S/PORT\ LPTRACKER
```

Repositorio local de GitHub usado en este desarrollo:

```text
D:\R36S\PORT LPTRACKER\GITHUB\lgpt-r36sx-port
```

En WSL:

```bash
/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port
```

## Compilar desde cero en WSL

Copiar este ZIP a:

```text
D:\R36S\PORT LPTRACKER\LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE.zip
```

Luego ejecutar en WSL:

```bash
cd ~

rm -rf lgpt_u250_final_build
mkdir -p lgpt_u250_final_build
cd lgpt_u250_final_build

unzip -q "/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE.zip"

cd LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE

bash VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh
bash BUILD_U2_36_STABLE_WSL.sh
```

El core compilado debería quedar en:

```bash
dist/lgpt_libretro.so
```

## Instalar en SD

Asumiendo que la SD está montada como `F:` en Windows:

```bash
cp -f dist/lgpt_libretro.so "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_50_FINAL.so"

bash INSTALL_U2_36_LGPT_TREEFROGUI_FROM_WSL.sh F "/mnt/d/R36S/PORT LPTRACKER/lgpt_libretro_U2_50_FINAL.so"
```

Si la SD usa otra letra, reemplazar `F` por la letra correcta.

## Verificación rápida de SD

```bash
find /mnt/f/roms -iname "*.lgpt" -print
find /mnt/f/lgpt -maxdepth 3 -type f | grep -i "lgpt_libretro"
find /mnt/f/lgpt/exports -iname "*.wav" -print
```

Entrada esperada del port:

```text
/mnt/f/roms/lgpt/start.lgpt
```

## Controles principales agregados hasta U2.50

### Phrase

```text
Note | Vol | Inst | Cmd1 | Param1 | Cmd2 | Param2
```

- `Y`: preview de la fila actual.
- `L1 + X`: inicia selección.
- `X`: corta la selección activa.
- `L2 + LEFT`: previous used phrase.
- `L2 + RIGHT`: next used phrase.
- `L2 + UP`: previous phrase assignment in current Song channel.
- `L2 + DOWN`: next phrase assignment in current Song channel; crea/linkea nueva asignación si no existe una posterior.
- Doble `A`: abre Pitch/Envelope para el chop asignado.
- `R1 + B` dentro de esa vista Pitch/Envelope: vuelve a Phrase.

### Mixer

- `R1 + A`: solo del canal seleccionado; pulsar de nuevo limpia solo/unmute all.
- `R1 + B`: mute/unmute del canal seleccionado.
- `R1 + LEFT/RIGHT`: paneo del canal seleccionado.
- `R1 + DOWN`: centro de paneo.
- `Y + UP/DOWN`: tempo +/- 1.
- `Y + LEFT/RIGHT`: tempo +/- 10.

La visual de Mixer en U2.50 usa canales más legibles para mezcla, con ayuda de teclas arriba y faders abajo.

### Project/Master

- Medidor vertical Master L/R reubicado en zona inferior izquierda.
- Exportación WAV estable en `lgpt/exports/<ProjectName>/`.

### WAV Export

Song WAV:

```text
F:\lgpt\exports\<ProjectName>\<ProjectName>.wav
```

Multitrack:

```text
F:\lgpt\exports\<ProjectName>\multitrack\<ProjectName>_<InstrumentName>_track_XX.wav
```

Los stems sin instrumento asignado en Song no se exportan.

## Nota sobre binarios

Este paquete contiene el código fuente completo y scripts de build/install. No se incluye `dist/lgpt_libretro.so` precompilado dentro del ZIP final para evitar distribuir un binario no recompilado en el entorno destino. Compilar en WSL con `BUILD_U2_36_STABLE_WSL.sh` genera el core correcto para instalar en la SD.
