#!/usr/bin/env bash
set -euo pipefail
BASE_DIR="${1:-/mnt/d/R36S/PORT LPTRACKER}"
SD_LETTER="${2:-}"
BUILD_DIR="${3:-/tmp/lgpt_r36sx_v26_build}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ -z "$SD_LETTER" ]]; then
  echo "Uso: bash tools/wsl/00_build_install_r36sx_v26.sh \"/mnt/d/R36S/PORT LPTRACKER\" F /tmp/lgpt_r36sx_v26" >&2
  echo "Cambia F por la letra real de la SD en Windows." >&2
  exit 2
fi
SD_LETTER="${SD_LETTER%:}"
SD="/mnt/${SD_LETTER,,}"
if [[ ! -d "$SD" ]]; then
  echo "No existe $SD. Monta la SD en Windows y vuelve a intentar." >&2
  exit 1
fi
if [[ ! -d "$SD/cubegm/cores" ]]; then
  echo "No encuentro $SD/cubegm/cores. Instala primero Stock OS + TreeFrogUI para R36SX v2.6." >&2
  exit 1
fi
mkdir -p "$BUILD_DIR"
cd "$ROOT/projects"
MAKEFILE="Makefile.TREEFROG"
if [[ ! -f "$MAKEFILE" ]]; then
  echo "No existe projects/Makefile.TREEFROG" >&2
  exit 1
fi
TOOLCHAIN_DEFAULT="$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot"
export TOOLCHAIN="${TOOLCHAIN:-$TOOLCHAIN_DEFAULT}"
cat <<MSG
Compilación LGPT R36SX v2.6 / TreeFrogUI
Repo:       $ROOT
Makefile:   $ROOT/projects/$MAKEFILE
Toolchain:  $TOOLCHAIN
SD:         $SD
MSG
if [[ ! -d "$TOOLCHAIN" ]]; then
  cat <<MSG >&2

No encontré el toolchain MIPS esperado en:
  $TOOLCHAIN

Instala o clona primero el toolchain SF3000/R36SX usado por TreeFrogUI, o define TOOLCHAIN manualmente:
  export TOOLCHAIN=/ruta/al/toolchain

MSG
  exit 1
fi
make -f "$MAKEFILE" clean || true
make -f "$MAKEFILE" -j"$(nproc)"
OUT="$ROOT/lgpt_libretro.so"
if [[ ! -f "$OUT" ]]; then
  echo "No se generó $OUT" >&2
  exit 1
fi
install -Dm755 "$OUT" "$SD/cubegm/cores/lgpt_libretro.so"
mkdir -p "$SD/roms/lgpt" "$SD/roms/LGPT"
cat > "$SD/roms/lgpt/README_LGPT_R36SX.txt" <<'MSG'
LGPT R36SX v2.6 / TreeFrogUI

Core instalado en:
  cubegm/cores/lgpt_libretro.so

Si TreeFrogUI no muestra esta carpeta o no asocia el core automáticamente, falta registrar la asociación de carpeta/core en FrogUI.
MSG
sync
cat <<MSG

Instalación terminada.
Core copiado en:
  $SD/cubegm/cores/lgpt_libretro.so

Reinicia la R36SX y revisa si TreeFrogUI muestra la carpeta LGPT.
Si no aparece, el siguiente trabajo es integrar la asociación de FrogUI: carpeta LGPT -> lgpt_libretro.so.
MSG
