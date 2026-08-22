#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PAYLOAD="$REPO_ROOT/sd_root"
OUT="${1:-$REPO_ROOT/dist/LGPT_R36SX_U2524_COPYROOT_UAC2_FULL_SOURCE.zip}"
HOTFIX_OVERLAY_ONLY="${HOTFIX_OVERLAY_ONLY:-0}"

[[ -d "$REPO_ROOT/.git" ]] || {
    echo 'ERROR: build_from_full_clone.sh debe ejecutarse dentro del clone Git completo.' >&2
    exit 2
}
"$SCRIPT_DIR/verify_copy_root_layout.sh" "$PAYLOAD"
"$REPO_ROOT/tests/test_copy_root_launcher.sh"

required_binaries=(
    "cubegm/cores/lgpt_core.so"
    "lgpt/otg/bin/r36s_u241_usb_audio_io"
    "lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/soundcore.ko"
    "lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/snd.ko"
    "lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/snd-timer.ko"
    "lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/snd-pcm.ko"
    "lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/usb_f_uac2.ko"
)
missing=0
for rel in "${required_binaries[@]}"; do
    if [[ ! -s "$PAYLOAD/$rel" ]]; then
        echo "RELEASE_BINARY_MISSING $rel" >&2
        missing=$((missing + 1))
    fi
done
if (( missing > 0 )) && [[ "$HOTFIX_OVERLAY_ONLY" != 1 ]]; then
    echo 'ERROR: release autónomo bloqueado. Compile/copíe los binarios indicados.' >&2
    echo 'Para generar sólo un hotfix que conserva binarios existentes: HOTFIX_OVERLAY_ONLY=1.' >&2
    exit 3
fi

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp -a "$PAYLOAD/." "$STAGE/"
mkdir -p "$STAGE/SOURCE_AND_TOOLS/full_repository"

# Copy the complete repository source, excluding local/generated state.
tar -C "$REPO_ROOT" \
    --exclude='./.git' \
    --exclude='./dist' \
    --exclude='./BUILD' \
    --exclude='./BACKUPS' \
    --exclude='./COLLECTED_LOGS' \
    --exclude='./__pycache__' \
    -cf - . | tar -C "$STAGE/SOURCE_AND_TOOLS/full_repository" -xf -

cat > "$STAGE/SOURCE_AND_TOOLS/RELEASE_MODE.txt" <<MODE
mode=$([[ "$HOTFIX_OVERLAY_ONLY" == 1 ]] && echo hotfix_overlay || echo autonomous)
source_repository=complete clone at build time
payload=sd_root
MODE

python3 "$SCRIPT_DIR/build_copy_root_release.py" "$STAGE" "$OUT"
