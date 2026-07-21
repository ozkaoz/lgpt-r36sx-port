# Instalación en español

## Requisitos

- Consola R36SX con TreeFrogUI instalado.
- Tarjeta SD accesible desde Windows/WSL.
- Copia del ZIP binario `LGPT_R36SX_U2523.zip` descargado desde GitHub Releases.

TreeFrogUI y `picoarch` no están incluidos.

## Instalación

1. Apague completamente la consola y retire la SD.
2. Inserte la SD en el PC.
3. Extraiga el ZIP del release.
4. En WSL, entre en la carpeta extraída.
5. Ejecute:

```bash
SD_MOUNT=/mnt/f bash INSTALL_TO_SD.sh
SD_MOUNT=/mnt/f bash VERIFY_SD_INSTALL.sh
sync
```

El instalador crea un respaldo antes de copiar archivos.

## Primera prueba

1. Expulse la SD de forma segura.
2. Inserte la SD y encienda sin OTG.
3. Abra LGPT y compruebe el audio local y los controles.
4. Apague completamente.
5. Conecte USB-C OTG al PC, encienda y seleccione `R36SX USB AUDIO 48K` en Windows.

La configuración estable es mono, 48 kHz, 16 bits, 480 frames y cuatro periodos.
