# AU11Z9 — instalador simple para reinstalar LGPT desde PC

Objetivo: crear un instalador reutilizable a partir de una SD donde LGPT ya carga correctamente.

Ruta de trabajo asumida en Windows:

```text
D:\R36S\PORT LPTRACKER
```

Ruta equivalente en WSL:

```bash
/mnt/d/R36S/PORT\ LPTRACKER
```

## Crear el instalador desde la SD funcional

Con la SD montada como `F:`:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"
chmod +x tools/wsl/*.sh
bash tools/wsl/05_create_pc_reinstall_package_from_sd.sh F "/mnt/d/R36S/PORT LPTRACKER"
```

Salida esperada:

```text
D:\R36S\PORT LPTRACKER\INSTALLERS\LGPT_R36SX_AU11Z9_PC_INSTALLER
D:\R36S\PORT LPTRACKER\LGPT_R36SX_AU11Z9_PC_INSTALLER.zip
```

## Reinstalar después de formatear la SD

1. Formatear SD.
2. Instalar Stock OS.
3. Instalar TreeFrogUI.
4. Conectar SD al PC.
5. Entrar al instalador generado.
6. Ejecutar en PowerShell:

```powershell
.\INSTALL_LGPT_R36SX_TO_SD.ps1 -SdDrive F
.\VERIFY_LGPT_R36SX_SD.ps1 -SdDrive F
```

Cambiar `F` por la letra real de la SD.

## Qué guarda el instalador

Por defecto guarda una versión mínima:

- `lgpt/config.xml`
- estructura mínima de carpetas de LGPT
- `roms/lgpt/start.lgpt`
- `cubegm/cores/lgpt_libretro.so`
- alias/lanzadores `cubegm/lgpt_libretro.so`, `cubegm/lgpt`, `cubegm/lgpt.elf`

Para guardar también proyectos, samples e instrumentos de la SD, usar modo `full`:

```bash
bash tools/wsl/05_create_pc_reinstall_package_from_sd.sh F "/mnt/d/R36S/PORT LPTRACKER" LGPT_R36SX_AU11Z9_PC_INSTALLER_FULL full
```
