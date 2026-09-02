# Troubleshooting

## WSL cannot access `/mnt/f`

Close WSL, run `wsl --shutdown` in PowerShell, and reopen it. Check `findmnt -T /mnt/f`.

## Windows does not detect OTG

Fully power off, connect a USB-C data cable, and boot again. Select `R36SX USB AUDIO 48K`.

## Restore the previous version

```bash
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/restore.sh
```

## Collect logs

```bash
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/collect_logs.sh
```
