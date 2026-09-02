#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$ROOT/scripts/audit.sh"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}" bash "$ROOT/scripts/prepare_source.sh"
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}" bash "$ROOT/scripts/build.sh"
SD_MOUNT="${SD_MOUNT:-/mnt/f}" PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}" bash "$ROOT/scripts/install.sh"
SD_MOUNT="${SD_MOUNT:-/mnt/f}" PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}" bash "$ROOT/scripts/verify.sh"
echo BUILD_INSTALL_U2523_COMPLETE
