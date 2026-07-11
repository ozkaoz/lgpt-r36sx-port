#!/usr/bin/env bash
set -euo pipefail

SD_LETTER="${1:-}"
if [[ -z "$SD_LETTER" ]]; then
  echo "Uso: bash tools/wsl/04_fix_sd_write_permissions.sh F" >&2
  echo "Cambia F por la letra real de la SD en Windows." >&2
  exit 2
fi

SD_LETTER="${SD_LETTER%:}"
SD="/mnt/${SD_LETTER,,}"

if [[ ! -d "$SD" ]]; then
  echo "ERROR: no existe $SD. Revisa la letra de unidad en Windows." >&2
  exit 1
fi

if [[ ! -d "$SD/cubegm" || ! -d "$SD/frogui" ]]; then
  echo "ERROR: $SD no parece ser la SD con Stock OS + TreeFrogUI. Faltan cubegm/ o frogui/." >&2
  exit 1
fi

echo "Revisando escritura en SD: $SD"

if touch "$SD/.lgpt_wsl_write_test" 2>/dev/null; then
  rm -f "$SD/.lgpt_wsl_write_test" 2>/dev/null || true
  echo "[OK] La raíz de la SD permite escritura directa desde WSL."
else
  echo "[WARN] Tu usuario de WSL no puede escribir directamente en la raíz de la SD. Se intentará con sudo."
fi

# Crear/normalizar carpetas requeridas por LGPT. Usamos sudo solo si el mkdir normal falla.
make_dir() {
  local d="$1"
  if mkdir -p "$d" 2>/dev/null; then
    return 0
  fi
  echo "[INFO] mkdir normal falló en $d; intentando con sudo..."
  sudo mkdir -p "$d"
}

DIRS=(
  "$SD/lgpt"
  "$SD/lgpt/projects"
  "$SD/lgpt/samples"
  "$SD/lgpt/instruments"
  "$SD/lgpt/images"
  "$SD/lgpt/exports"
  "$SD/lgpt/chops"
  "$SD/lgpt/tmp"
  "$SD/lgpt/backups"
  "$SD/lgpt/otg/bin"
  "$SD/lgpt/otg/logs"
  "$SD/roms/lgpt"
  "$SD/cubegm/cores"
)

for d in "${DIRS[@]}"; do
  make_dir "$d"
done

# Si las carpetas quedaron creadas por root en WSL, devolverlas al usuario actual.
# En FAT/exFAT puede no aplicar; por eso no es fatal si falla.
sudo chown -R "$(id -u):$(id -g)" "$SD/lgpt" "$SD/roms/lgpt" "$SD/cubegm/cores" 2>/dev/null || true
sudo chmod -R u+rwX "$SD/lgpt" "$SD/roms/lgpt" "$SD/cubegm/cores" 2>/dev/null || true

# Prueba final sobre la ruta problemática.
TEST_DIR="$SD/lgpt/.wsl_permission_test"
if mkdir -p "$TEST_DIR" 2>/dev/null; then
  rmdir "$TEST_DIR" 2>/dev/null || true
  echo "[OK] Ya se puede crear subcarpetas dentro de $SD/lgpt"
else
  echo "[ERROR] Aún no se puede escribir dentro de $SD/lgpt." >&2
  echo "Posibles causas: SD montada en solo lectura, unidad bloqueada por Windows, tarjeta dañada, o permisos heredados raros." >&2
  echo "Prueba en PowerShell de Windows: chkdsk ${SD_LETTER^^}: /f" >&2
  exit 3
fi

cat > "$SD/roms/lgpt/start.lgpt" <<'EOF'
LGPT_START
EOF

cat > "$SD/roms/lgpt/filelist.csv" <<'EOF'
start.lgpt,LGPT,LGPT
EOF

cat > "$SD/lgpt/AU11Z8_SD_PERMISSION_FIX.txt" <<EOF
FIX=AU11Z8_SD_PERMISSION_FIX
DATE=$(date --iso-8601=seconds)
SD=$SD
USER=$(id -un)
REQUIRED_ROOT=/lgpt
VISIBLE_ENTRY=/roms/lgpt/start.lgpt
EOF

sync || true

echo "[OK] Carpetas LGPT preparadas. Ahora vuelve a ejecutar el build/install."
echo "Comando sugerido:"
echo "  bash tools/wsl/00_build_install_r36sx_v26.sh \"/mnt/d/R36S/PORT LPTRACKER\" ${SD_LETTER^^} /tmp/lgpt_r36sx_v26"
