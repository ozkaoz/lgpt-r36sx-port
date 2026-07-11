# LGPT R36S TreeFrogUI — U2.36 FINAL estable pre-OTG / chopper integrado

Estado de este paquete: **congelación estable de código fuente antes de continuar U2.37-OTG-AUDIO**.

Este ZIP consolida la base estable U2.36 independiente, el instalador LGPT/TreeFrogUI incluido en R2 y el instalador limpio Stock + TreeFrogUI validado para la ruta en minúsculas. No introduce cambios experimentales de USB/OTG dentro del core LGPT.

## Decisión de release

La fuente de verdad seleccionada es:

```text
LGPT_PORT_U2_36_ESTABLE_INDEPENDIENTE_R2_CON_INSTALADOR_LGPT.zip
```

Motivo: este árbol tiene las mismas fuentes que `LGPT_PORT_U2_36_ESTABLE_INDEPENDIENTE.zip`, pero añade instalador. Frente a los ZIP anteriores de CHOP base / CHOP integrado, contiene los incrementos posteriores U2.23-U2.36: corrección Song Y+X, fixes de preview/progreso del Chopper, Pitch/Envelope, restauración de Listen/Import, Sample Manager y deduplicación de importación por contenido.

## Marcadores obligatorios en runtime

```text
Graphical Chopper U2.36
PITCH/ENV U2.36
```

Marcador de fuente obligatorio:

```text
TREEFROG_U2_36_IMPORT_DEDUP_LISTEN_LAYOUT_USAGE
```

## Alcance funcional congelado

Incluido y conservado:

- Chopper gráfico estable hasta U2.36.
- Persistencia `.u2chop`.
- Phrase mostrando `S01`, `S02`, etc. para instrumentos con chops.
- Crop Sample con progreso visible, undo/redo y borrado de selección.
- Pitch/Envelope con `Scope: Sample` y `Scope: Chop`.
- Preview con `B` y stop con `L2+B` donde corresponde.
- Menú `Listen Import Manage Exit` en una sola línea.
- Importación con deduplicación por contenido y protección anti-sobrescritura por nombre.
- Project Sample Manager con borrado libre, purge conservador y force delete en dos pasos.
- Instalador LGPT/TreeFrogUI R2.
- Instalador limpio Stock + TreeFrogUI bajo `installers/stock_treefrogui_clean/`.

No incluido como cambio de core:

- Audio USB/OTG.
- Captura USB desde ALSA hacia LGPT.
- Módulos kernel/rootfs para ALSA/UAC.
- U2.37D/E/F como código de producción.

## Build recomendado

```bash
cd "/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT"
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
```

Core esperado:

```text
dist/lgpt_libretro.so
```

## Instalación limpia validada Stock + TreeFrogUI

Desde WSL, usando la letra real de la SD:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER"
rm -rf LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER
unzip -o LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER.zip -d .

bash LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/INSTALL_U2_36_STOCK_TREEFROGUI_FROM_WSL.sh F "/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_V101_DEV_NEXT/dist/lgpt_libretro.so"

bash LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/VERIFY_U2_36_STOCK_TREEFROGUI_FROM_WSL.sh F
```

El mismo instalador también queda incluido dentro de este paquete en:

```text
installers/stock_treefrogui_clean/LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER/
```

## Regla para continuar

El siguiente desarrollo debe partir desde este ZIP y crear un incremento nuevo. No reaplicar parches históricos U2.10-U2.36 encima de este árbol salvo auditoría explícita: ya están integrados en la fuente.

Para continuar por OTG/audio, usar:

```text
PROMPT_CONTINUAR_DESARROLLO_U2_37_OTG_AUDIO_ES.md
OTG_AUDIO_STATUS_U2_37_PRE_PATCH_ES.md
```
