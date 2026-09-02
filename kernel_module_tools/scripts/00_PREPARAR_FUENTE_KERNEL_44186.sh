#!/usr/bin/env bash
set -Eeuo pipefail

PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}"
DEST="${KERNEL_SRC_DEST:-$PROJECT_ROOT/KERNEL/linux-4.4.186}"
TARBALL="$PROJECT_ROOT/KERNEL/linux-4.4.186.tar.xz"
URL="https://cdn.kernel.org/pub/linux/kernel/v4.x/linux-4.4.186.tar.xz"

mkdir -p "$PROJECT_ROOT/KERNEL"

is_kernel_tree(){
    [[ -f "$1/Makefile" &&
       -f "$1/drivers/usb/gadget/function/f_uac2.c" &&
       -d "$1/arch/mips" ]]
}

CANDIDATES=(
    "${KERNEL_SRC:-}"
    "/tmp/r36sx_kernel_nospace_u241/linux-4.4.186"
    "$PROJECT_ROOT/KERNEL/linux-4.4.186"
    "$PROJECT_ROOT/linux-4.4.186"
)

for c in "${CANDIDATES[@]}"; do
    [[ -n "$c" ]] || continue
    if is_kernel_tree "$c"; then
        echo "KERNEL_SOURCE_FOUND=$c"
        printf '%s\n' "$c" > "$PROJECT_ROOT/KERNEL/U2414_KERNEL_SOURCE_PATH.txt"
        exit 0
    fi
done

found="$(
    find "$PROJECT_ROOT" -maxdepth 7 -type f \
        -path '*/linux-4.4.186/drivers/usb/gadget/function/f_uac2.c' \
        -print -quit 2>/dev/null || true
)"
if [[ -n "$found" ]]; then
    tree="${found%/drivers/usb/gadget/function/f_uac2.c}"
    echo "KERNEL_SOURCE_FOUND=$tree"
    printf '%s\n' "$tree" > "$PROJECT_ROOT/KERNEL/U2414_KERNEL_SOURCE_PATH.txt"
    exit 0
fi

echo "No se encontró el árbol linux-4.4.186. Se descargará desde kernel.org."

if [[ ! -s "$TARBALL" ]]; then
    if command -v curl >/dev/null; then
        curl -L --fail --retry 4 --retry-delay 3 \
            "$URL" -o "$TARBALL"
    elif command -v wget >/dev/null; then
        wget -O "$TARBALL" "$URL"
    else
        echo "ERROR: se requiere curl o wget." >&2
        exit 2
    fi
fi

mkdir -p "$(dirname "$DEST")"
rm -rf "$DEST"
tar -xJf "$TARBALL" -C "$(dirname "$DEST")"

is_kernel_tree "$DEST" || {
    echo "ERROR: el árbol extraído no es válido: $DEST" >&2
    exit 3
}

printf '%s\n' "$DEST" > "$PROJECT_ROOT/KERNEL/U2414_KERNEL_SOURCE_PATH.txt"
echo "KERNEL_SOURCE_READY=$DEST"
