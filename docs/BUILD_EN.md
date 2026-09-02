# English build guide

## Requirements

- WSL/Ubuntu.
- `rsync`, `make`, `gcc`, `g++`, `python3`.
- R36SX MIPS toolchain:

```text
$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
```

## Audit, build and install

```bash
bash scripts/audit.sh
PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/build.sh
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/install.sh
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/verify.sh
```

Full workflow:

```bash
SD_MOUNT=/mnt/f PROJECT_ROOT="/mnt/d/Toolchains/R36SX" bash scripts/build_install.sh
```
