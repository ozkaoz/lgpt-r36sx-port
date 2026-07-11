# Comandos U2.36 FINAL pre-OTG

## Verificar fuente del paquete

```bash
cd "/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_U2_36_FINAL_ESTABLE_PRE_OTG_CHOPPER"
bash ./VERIFY_U2_36_FINAL_PRE_OTG_SOURCE.sh
```

## Compilar core TreeFrog/R36SX

```bash
cd "/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_U2_36_FINAL_ESTABLE_PRE_OTG_CHOPPER"
bash ./BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
```

Core resultante esperado:

```text
dist/lgpt_libretro.so
```

## Instalar en SD Stock + TreeFrogUI con instalador limpio incluido

```bash
cd "/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_U2_36_FINAL_ESTABLE_PRE_OTG_CHOPPER/installers/stock_treefrogui_clean/LGPT_U2_36_STOCK_TREEFROGUI_CLEAN_INSTALLER"

bash ./INSTALL_U2_36_STOCK_TREEFROGUI_FROM_WSL.sh F "/home/dafunknoise/r36sx-lgpt-port/dev_sources/LGPT_PORT_U2_36_FINAL_ESTABLE_PRE_OTG_CHOPPER/dist/lgpt_libretro.so"

bash ./VERIFY_U2_36_STOCK_TREEFROGUI_FROM_WSL.sh F
```

Ajustar `F` según la letra real de la SD.
