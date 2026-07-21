#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

bash "$ROOT/scripts/00_PREPARAR_FUENTE_KERNEL_44186.sh"
bash "$ROOT/scripts/01_COMPILAR_U2414_AU8_SYNC.sh"
bash "$ROOT/scripts/02_INSTALAR_U2414_AU8_SYNC_SD.sh"
bash "$ROOT/scripts/03_VERIFICAR_U2414_SD.sh"

echo "U2414_BUILD_INSTALL_COMPLETE"
