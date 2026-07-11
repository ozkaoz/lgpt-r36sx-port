#!/usr/bin/env bash
set -euo pipefail
LETTER="${1:-}"
if [[ -z "$LETTER" ]]; then
  echo "Uso: bash tools/wsl/02_verify_treefrog_sd.sh F" >&2
  echo "Cambia F por la letra real de la SD en Windows." >&2
  exit 2
fi
LETTER="${LETTER%:}"
SD="/mnt/${LETTER,,}"
if [[ ! -d "$SD" ]]; then
  echo "No existe $SD. Revisa que la SD esté montada en Windows y visible en WSL." >&2
  exit 1
fi
printf 'SD detectada: %s\n' "$SD"
missing=0
for p in cubegm cubegm/cores; do
  if [[ -e "$SD/$p" ]]; then
    printf '[OK] %s\n' "$p"
  else
    printf '[FALTA] %s\n' "$p"
    missing=1
  fi
done
if [[ -d "$SD/frogui" ]]; then
  echo "[OK] frogui"
elif [[ -d "$SD/roms" ]]; then
  echo "[OK] roms"
else
  echo "[AVISO] No veo frogui/ ni roms/. Puede que TreeFrog no esté instalado todavía o use otra estructura."
fi
if [[ -f "$SD/cubegm/cores/lgpt_libretro.so" ]]; then
  echo "[OK] LGPT ya está instalado como cubegm/cores/lgpt_libretro.so"
else
  echo "[INFO] LGPT todavía no está instalado. Eso es normal antes de compilar/copiar el core."
fi
if [[ "$missing" -ne 0 ]]; then
  echo "La SD no parece tener una instalación TreeFrogUI/Stock lista para recibir el core." >&2
  exit 1
fi
