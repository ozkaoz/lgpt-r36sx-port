# Compilación en español

## Requisitos

- WSL/Ubuntu.
- `rsync`, `make`, `gcc`, `g++`, `python3`.
- Toolchain MIPS usado por R36SX:

```text
$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
```

## Auditoría, compilación e instalación

```bash
bash scripts/audit.sh
PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/build.sh
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/install.sh
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/verify.sh
```

Todo el proceso:

```bash
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/build_install.sh
```
