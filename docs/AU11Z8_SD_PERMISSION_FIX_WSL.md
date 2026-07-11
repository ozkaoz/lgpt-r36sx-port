# AU11Z8 - Corrección de permisos de escritura en SD desde WSL

Este parche agrega `tools/wsl/04_fix_sd_write_permissions.sh`.

Úsalo cuando la compilación de LGPT sí genera `lgpt_libretro.so`, pero la instalación falla con mensajes como:

```text
mkdir: cannot create directory ‘/mnt/f/lgpt/images’: Permission denied
```

## Uso

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"

bash tools/wsl/04_fix_sd_write_permissions.sh F

bash tools/wsl/00_build_install_r36sx_v26.sh "/mnt/d/R36S/PORT LPTRACKER" F /tmp/lgpt_r36sx_v26
```

Cambia `F` por la letra real de la SD en Windows.

## Rutas que prepara

- `/lgpt`
- `/lgpt/projects`
- `/lgpt/samples`
- `/lgpt/instruments`
- `/lgpt/images`
- `/lgpt/exports`
- `/lgpt/chops`
- `/lgpt/tmp`
- `/lgpt/backups`
- `/lgpt/otg/bin`
- `/lgpt/otg/logs`
- `/roms/lgpt/start.lgpt`
- `/cubegm/cores`
