# Solución de problemas

## WSL no puede acceder a `/mnt/f`

Cierre WSL, ejecute `wsl --shutdown` en PowerShell y vuelva a abrir. Compruebe `findmnt -T /mnt/f`.

## Windows no reconoce OTG

Apague completamente la consola, conecte un cable USB-C de datos y vuelva a encender. Verifique el dispositivo `R36SX USB AUDIO 48K`.

## Restaurar la versión previa

```bash
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/restore.sh
```

## Recolectar logs

```bash
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/collect_logs.sh
```
