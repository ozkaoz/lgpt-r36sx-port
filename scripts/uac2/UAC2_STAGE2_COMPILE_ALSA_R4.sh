#!/usr/bin/env bash
set -Eeuo pipefail

# LGPT R36SX - Stage 2 R4
# Compila los módulos ALSA que necesita el usb_f_uac2.ko ya verificado.
# No instala ni sustituye nada en la SD.

PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}"
REPO_ROOT="${REPO_ROOT:-$PROJECT_ROOT/GITHUB/lgpt-r36sx-port}"
KERNEL_SRC="${KERNEL_SRC:-$PROJECT_ROOT/KERNEL/source/linux-4.4.186}"
SDK_ROOT="${SDK_ROOT:-$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}"
CROSS_COMPILE="${CROSS_COMPILE:-$SDK_ROOT/bin/mips-mti-linux-gnu-}"
WORK_ROOT="${WORK_ROOT:-$HOME/lgpt-r36sx-kernel-work/stage2-alsa}"
OUTPUT_ROOT="${OUTPUT_ROOT:-$PROJECT_ROOT/BUILD}"
SD_DRIVE="${SD_DRIVE:-F:}"
SD_MOUNT="${SD_MOUNT:-/mnt/f}"
JOBS="${JOBS:-$(nproc)}"
EXPECTED_RELEASE="${EXPECTED_RELEASE:-4.4.186-release}"
EXPECTED_UAC2_SHA256="${EXPECTED_UAC2_SHA256:-e9062ac5a37aa7706c98a3411a57dbe67bc98ef82621c430c1d4e7a89b2fbefe}"
INCLUDE_FULL_REPO_SOURCE="${INCLUDE_FULL_REPO_SOURCE:-1}"

TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
KERNEL_WORK_SRC="$WORK_ROOT/kernel-source-${TIMESTAMP}"
BUILD_DIR="$WORK_ROOT/build-${TIMESTAMP}"
INPUT_DIR="$WORK_ROOT/input-${TIMESTAMP}"
RESULT_DIR="$OUTPUT_ROOT/UAC2_STAGE2_ALSA_${TIMESTAMP}"
LOG_DIR="$PROJECT_ROOT/LOGS"
LOG_FILE="$LOG_DIR/UAC2_STAGE2_ALSA_BUILD_${TIMESTAMP}.log"
ZIP_FILE="$OUTPUT_ROOT/LGPT_R36SX_UAC2_ALSA_STAGE2_R4_${TIMESTAMP}_FULL_SOURCE.zip"

mkdir -p "$WORK_ROOT" "$BUILD_DIR" "$INPUT_DIR" "$RESULT_DIR" "$LOG_DIR" "$OUTPUT_ROOT"
exec > >(tee -a "$LOG_FILE") 2>&1

cleanup_mount=0
cleanup() {
    local rc=$?
    set +e
    if [[ "$cleanup_mount" == 1 ]] && mountpoint -q "$SD_MOUNT"; then
        sync
        sudo umount "$SD_MOUNT"
        echo "SD_UNMOUNT_ON_EXIT=$SD_MOUNT"
    fi
    echo "SCRIPT_EXIT_CODE=$rc"
    echo "BUILD_LOG=$LOG_FILE"
    exit "$rc"
}
trap cleanup EXIT

section() {
    printf '\n============================================================\n%s\n============================================================\n' "$1"
}

die() {
    echo "ERROR: $*" >&2
    exit 1
}

require_file() {
    [[ -s "$1" ]] || die "Archivo ausente o vacío: $1"
}

require_dir() {
    [[ -d "$1" ]] || die "Directorio ausente: $1"
}

section "IDENTIDAD"
echo "DATE=$(date -Iseconds)"
echo "PROJECT_ROOT=$PROJECT_ROOT"
echo "REPO_ROOT=$REPO_ROOT"
echo "KERNEL_SRC_ORIGINAL=$KERNEL_SRC"
echo "KERNEL_WORK_SRC=$KERNEL_WORK_SRC"
echo "SDK_ROOT=$SDK_ROOT"
echo "CROSS_COMPILE=$CROSS_COMPILE"
echo "WORK_ROOT=$WORK_ROOT"
echo "BUILD_DIR=$BUILD_DIR"
echo "RESULT_DIR=$RESULT_DIR"
echo "EXPECTED_RELEASE=$EXPECTED_RELEASE"
echo "JOBS=$JOBS"

section "PREFLIGHT"
require_dir "$REPO_ROOT"
require_dir "$KERNEL_SRC"
require_file "$KERNEL_SRC/Makefile"
require_file "$KERNEL_SRC/sound/sound_core.c"
require_file "$KERNEL_SRC/sound/core/init.c"
require_file "$KERNEL_SRC/sound/core/pcm.c"
require_file "$KERNEL_SRC/sound/core/pcm_lib.c"
require_file "$KERNEL_SRC/sound/core/timer.c"
require_file "${CROSS_COMPILE}gcc"
require_file "${CROSS_COMPILE}nm"
require_file "${CROSS_COMPILE}readelf"

for tool in bash make gcc python3 rsync file sha256sum zip unzip modinfo mountpoint; do
    command -v "$tool" >/dev/null 2>&1 || die "Herramienta ausente: $tool"
done

section "COPIAR ARBOL DEL KERNEL A WSL SIN ESPACIOS"
[[ "$KERNEL_WORK_SRC" != *" "* && "$KERNEL_WORK_SRC" != *":"* ]] || \
    die "La ruta de compilación del kernel contiene espacios o dos puntos: $KERNEL_WORK_SRC"
rm -rf "$KERNEL_WORK_SRC"
mkdir -p "$KERNEL_WORK_SRC"
rsync -a --delete --exclude='.git/' "$KERNEL_SRC/" "$KERNEL_WORK_SRC/"
require_file "$KERNEL_WORK_SRC/Makefile"
require_file "$KERNEL_WORK_SRC/sound/sound_core.c"
require_file "$KERNEL_WORK_SRC/sound/core/init.c"
echo "KERNEL_SOURCE_COPY=$KERNEL_SRC->$KERNEL_WORK_SRC"
echo "KERNEL_SOURCE_COPY_FILES=$(find "$KERNEL_WORK_SRC" -type f | wc -l)"

section "PARCHE DE COMPATIBILIDAD DTC PARA GCC HOST MODERNO"
# Linux 4.4 contiene una definición duplicada de yylloc en el lexer DTC generado.
# GCC 10+ usa -fno-common por defecto y el enlazador falla con:
#   multiple definition of `yylloc'
# El arreglo equivalente al aplicado upstream elimina únicamente la definición
# duplicada del lexer; el parser conserva la definición real.
DTC_LEXER_SHIPPED="$KERNEL_WORK_SRC/scripts/dtc/dtc-lexer.lex.c_shipped"
require_file "$DTC_LEXER_SHIPPED"
DTC_PATCH_COUNT="$(python3 - "$DTC_LEXER_SHIPPED" <<'PY_DTC'
from pathlib import Path
import re, sys
p = Path(sys.argv[1])
s = p.read_text(errors="strict")
pattern = re.compile(r"(?m)^[ \t]*YYLTYPE[ \t]+yylloc;[ \t]*\n")
s2, count = pattern.subn("", s)
if count not in (0, 1):
    raise SystemExit(f"unexpected YYLTYPE yylloc definition count: {count}")
if count == 1:
    p.write_text(s2)
print(count)
PY_DTC
)"
echo "DTC_YYLLOC_DEFINITIONS_REMOVED=$DTC_PATCH_COUNT"
if grep -Eq '^[[:space:]]*YYLTYPE[[:space:]]+yylloc;' "$DTC_LEXER_SHIPPED"; then
    die "El lexer DTC todavía contiene la definición duplicada de yylloc"
fi
echo "DTC_GCC10_COMPAT_PATCH=OK"

KERNEL_VERSION="$(make -s -C "$KERNEL_WORK_SRC" kernelversion)"
[[ "$KERNEL_VERSION" == "4.4.186" ]] || die "El árbol declara kernel $KERNEL_VERSION; se esperaba 4.4.186"
echo "KERNEL_VERSION=$KERNEL_VERSION"

CC_MACHINE="$(${CROSS_COMPILE}gcc -dumpmachine)"
CC_VERSION="$(${CROSS_COMPILE}gcc -dumpversion)"
echo "CROSS_MACHINE=$CC_MACHINE"
echo "CROSS_GCC_VERSION=$CC_VERSION"
[[ "$CC_MACHINE" == "mips-mti-linux-gnu" ]] || die "Tripleta inesperada: $CC_MACHINE"

PROBE_C="$INPUT_DIR/probe.c"
PROBE_O="$INPUT_DIR/probe.o"
printf 'int r36sx_stage2_probe(void){return 0;}\n' > "$PROBE_C"
"${CROSS_COMPILE}gcc" -c -mips32r2 -EL -mabi=32 "$PROBE_C" -o "$PROBE_O"
file "$PROBE_O"
file "$PROBE_O" | grep -q 'ELF 32-bit LSB relocatable, MIPS' || die "El compilador no produjo MIPS little-endian ELF32"

section "LOCALIZAR MODULO UAC2 VERIFICADO"
UAC2_REPO="$REPO_ROOT/recovery/u2_38au8_sync_uac2/usb_f_uac2.ko"
UAC2_INPUT="$INPUT_DIR/usb_f_uac2.current.ko"

if [[ -s "$UAC2_REPO" ]]; then
    cp -a "$UAC2_REPO" "$UAC2_INPUT"
    echo "UAC2_SOURCE=repository:$UAC2_REPO"
else
    echo "El módulo no está en el repositorio; se copiará desde la SD."
fi

section "REFERENCIAS ALSA PREEXISTENTES OPCIONALES"
# Si existen módulos antiguos en el proyecto, sólo se inventarían y compararían.
# Nunca se sustituyen por ellos los módulos recién compilados.
REFERENCE_DIR="$INPUT_DIR/reference-modules"
mkdir -p "$REFERENCE_DIR"
for module_name in snd-pcm.ko snd-timer.ko; do
    reference_path="$(find "$PROJECT_ROOT" -type f -name "$module_name" \
        ! -path '*/BUILD/*' ! -path '*/LOGS/*' -print 2>/dev/null | head -n 1 || true)"
    if [[ -n "$reference_path" ]]; then
        cp -a "$reference_path" "$REFERENCE_DIR/$module_name"
        echo "REFERENCE_MODULE=$module_name SOURCE=$reference_path SHA256=$(sha256sum "$reference_path" | awk '{print $1}') VERMAGIC=$(modinfo -F vermagic "$reference_path" | sed 's/[[:space:]]*$//')"
    else
        echo "REFERENCE_MODULE_NOT_FOUND=$module_name"
    fi
done

section "OBTENER CONFIGURACION STOCK EXACTA"
KERNEL_IMAGE="$INPUT_DIR/vmlinux.uImage"
BASE_CONFIG="$INPUT_DIR/config.stock"
EXTRACT_IKCONFIG="$KERNEL_WORK_SRC/scripts/extract-ikconfig"

mount_sd_read() {
    sudo mkdir -p "$SD_MOUNT"
    if mountpoint -q "$SD_MOUNT"; then
        echo "SD_ALREADY_MOUNTED=$SD_MOUNT"
        cleanup_mount=0
    else
        sudo mount -t drvfs "$SD_DRIVE" "$SD_MOUNT"
        cleanup_mount=1
        echo "SD_MOUNTED=$SD_DRIVE->$SD_MOUNT"
    fi
}

mount_sd_read

if [[ -s "$SD_MOUNT/cubegm/vmlinux.uImage" ]]; then
    cp -a "$SD_MOUNT/cubegm/vmlinux.uImage" "$KERNEL_IMAGE"
    echo "KERNEL_IMAGE_COPIED=$SD_MOUNT/cubegm/vmlinux.uImage"
else
    echo "KERNEL_IMAGE_NOT_FOUND=$SD_MOUNT/cubegm/vmlinux.uImage"
fi

if [[ ! -s "$UAC2_INPUT" && -s "$SD_MOUNT/lgpt/otg/modules/$EXPECTED_RELEASE/u2_38au8_sync_uac2/usb_f_uac2.ko" ]]; then
    cp -a "$SD_MOUNT/lgpt/otg/modules/$EXPECTED_RELEASE/u2_38au8_sync_uac2/usb_f_uac2.ko" "$UAC2_INPUT"
    echo "UAC2_SOURCE=sd:$SD_MOUNT/lgpt/otg/modules/$EXPECTED_RELEASE/u2_38au8_sync_uac2/usb_f_uac2.ko"
fi

sync
if [[ "$cleanup_mount" == 1 ]]; then
    sudo umount "$SD_MOUNT"
    cleanup_mount=0
    echo "SD_UNMOUNT_OK=$SD_MOUNT"
fi

require_file "$UAC2_INPUT"
UAC2_SHA="$(sha256sum "$UAC2_INPUT" | awk '{print $1}')"
echo "UAC2_SHA256=$UAC2_SHA"
[[ "$UAC2_SHA" == "$EXPECTED_UAC2_SHA256" ]] || die "El UAC2 no coincide con el binario verificado"
file "$UAC2_INPUT"
UAC2_VERMAGIC="$(modinfo -F vermagic "$UAC2_INPUT" | sed 's/[[:space:]]*$//')"
echo "UAC2_VERMAGIC=$UAC2_VERMAGIC"
[[ "$UAC2_VERMAGIC" == "$EXPECTED_RELEASE preempt MIPS32_R2 32BIT" ]] || die "Vermagic UAC2 inesperado: $UAC2_VERMAGIC"

extract_ok=0
if [[ -s "$KERNEL_IMAGE" && -f "$EXTRACT_IKCONFIG" ]]; then
    chmod +x "$EXTRACT_IKCONFIG" 2>/dev/null || true
    if "$EXTRACT_IKCONFIG" "$KERNEL_IMAGE" > "$BASE_CONFIG" 2>"$INPUT_DIR/extract-ikconfig.stderr"; then
        if grep -q '^CONFIG_MIPS=y' "$BASE_CONFIG" && grep -q '^CONFIG_MODULES=y' "$BASE_CONFIG"; then
            extract_ok=1
            echo "CONFIG_SOURCE=stock-kernel-image"
        fi
    fi
fi

if [[ "$extract_ok" != 1 ]]; then
    echo "IKCONFIG_EXTRACTION_FAILED=YES"
    cat "$INPUT_DIR/extract-ikconfig.stderr" 2>/dev/null || true

    mapfile -t CONFIG_CANDIDATES < <(
        find \
            "$PROJECT_ROOT/KERNEL" \
            "$HOME/r36sx-kernel-u2414" \
            "$HOME/r36sx-kernel-u2534-android-uac1" \
            "$HOME/.cache/lgpt-r36sx" \
            "$REPO_ROOT" \
            -type f \( -name '.config' -o -name 'config-*' -o -name '*defconfig' \) \
            -size +2048c 2>/dev/null | sort -u
    )

    BEST_CONFIG="$(python3 - "${CONFIG_CANDIDATES[@]}" <<'PY'
import pathlib, sys
best = None
for raw in sys.argv[1:]:
    p = pathlib.Path(raw)
    try:
        text = p.read_text(errors="ignore")
    except OSError:
        continue
    score = 0
    checks = {
        "CONFIG_MIPS=y": 40,
        "CONFIG_CPU_MIPS32_R2=y": 25,
        "CONFIG_PREEMPT=y": 20,
        "CONFIG_MODULES=y": 20,
        'CONFIG_LOCALVERSION="-release"': 30,
        "CONFIG_32BIT=y": 10,
        "CONFIG_CPU_LITTLE_ENDIAN=y": 10,
    }
    for token, points in checks.items():
        if token in text:
            score += points
    if "4.4.186" in str(p):
        score += 10
    if p.name == ".config":
        score += 5
    item = (score, str(p))
    if best is None or item > best:
        best = item
if best and best[0] >= 100:
    print(best[1])
PY
)"

    [[ -n "$BEST_CONFIG" ]] || die "No se pudo extraer IKCONFIG ni localizar una configuración MIPS32R2 suficientemente exacta"
    cp -a "$BEST_CONFIG" "$BASE_CONFIG"
    echo "CONFIG_SOURCE=local:$BEST_CONFIG"
fi

require_file "$BASE_CONFIG"
echo "BASE_CONFIG_SHA256=$(sha256sum "$BASE_CONFIG" | awk '{print $1}')"
for required in 'CONFIG_MIPS=y' 'CONFIG_MODULES=y' 'CONFIG_PREEMPT=y'; do
    grep -q "^$required$" "$BASE_CONFIG" || die "La configuración base no contiene $required"
done

section "CONFIGURAR ALSA COMO MODULOS"
cp -a "$BASE_CONFIG" "$BUILD_DIR/.config"

python3 - "$BUILD_DIR/.config" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
lines = p.read_text(errors="strict").splitlines()
changes = {
    "CONFIG_SOUND": "m",
    "CONFIG_SND": "m",
    "CONFIG_SND_TIMER": "m",
    "CONFIG_SND_PCM": "m",
    "CONFIG_LOCALVERSION": '"-release"',
    "CONFIG_LOCALVERSION_AUTO": "n",
    "CONFIG_MODVERSIONS": "n",
    "CONFIG_MODULE_SIG": "n",
    "CONFIG_MODULE_COMPRESS": "n",
}
seen = set()
out = []
for line in lines:
    key = None
    if line.startswith("CONFIG_") and "=" in line:
        key = line.split("=", 1)[0]
    elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
        key = line[2:].split(" ", 1)[0]
    if key in changes:
        if key in seen:
            continue
        value = changes[key]
        out.append(f"# {key} is not set" if value == "n" else f"{key}={value}")
        seen.add(key)
    else:
        out.append(line)
for key, value in changes.items():
    if key not in seen:
        out.append(f"# {key} is not set" if value == "n" else f"{key}={value}")
p.write_text("\n".join(out) + "\n")
PY

make -C "$KERNEL_WORK_SRC" O="$BUILD_DIR" ARCH=mips CROSS_COMPILE="$CROSS_COMPILE" olddefconfig

for required in \
    'CONFIG_SOUND=m' \
    'CONFIG_SND=m' \
    'CONFIG_SND_TIMER=m' \
    'CONFIG_SND_PCM=m' \
    'CONFIG_PREEMPT=y' \
    'CONFIG_MODULES=y'; do
    grep -q "^$required$" "$BUILD_DIR/.config" || die "Kconfig no conservó $required"
done

if grep -q '^CONFIG_MODVERSIONS=y' "$BUILD_DIR/.config"; then
    die "CONFIG_MODVERSIONS quedó habilitado; no coincide con el UAC2 verificado"
fi

KERNEL_RELEASE="$(make -s -C "$KERNEL_WORK_SRC" O="$BUILD_DIR" ARCH=mips CROSS_COMPILE="$CROSS_COMPILE" kernelrelease)"
echo "KERNEL_RELEASE=$KERNEL_RELEASE"
[[ "$KERNEL_RELEASE" == "$EXPECTED_RELEASE" ]] || die "Kernel release generado inesperado: $KERNEL_RELEASE"

section "COMPILACION DETERMINISTA"
echo "BUILD_COMMAND=make -C $KERNEL_WORK_SRC O=$BUILD_DIR ARCH=mips CROSS_COMPILE=$CROSS_COMPILE -j$JOBS vmlinux modules"
make -C "$KERNEL_WORK_SRC" \
    O="$BUILD_DIR" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS_COMPILE" \
    HOSTCC=gcc \
    HOSTCXX=g++ \
    -j"$JOBS" \
    vmlinux modules

require_file "$BUILD_DIR/Module.symvers"
require_file "$BUILD_DIR/vmlinux"

section "LOCALIZAR Y VALIDAR MODULOS ALSA"
find_one_module() {
    local name="$1"
    local found
    mapfile -t found < <(find "$BUILD_DIR" -type f -name "$name" -print)
    [[ "${#found[@]}" -eq 1 ]] || die "Se esperaba exactamente un $name; encontrados: ${#found[@]}"
    printf '%s\n' "${found[0]}"
}

SOUNDCORE_KO="$(find_one_module soundcore.ko)"
SND_KO="$(find_one_module snd.ko)"
SND_TIMER_KO="$(find_one_module snd-timer.ko)"
SND_PCM_KO="$(find_one_module snd-pcm.ko)"

MODULES=("$SOUNDCORE_KO" "$SND_KO" "$SND_TIMER_KO" "$SND_PCM_KO")
for module in "${MODULES[@]}"; do
    require_file "$module"
    echo "MODULE=$module"
    file "$module"
    file "$module" | grep -q 'ELF 32-bit LSB relocatable, MIPS' || die "Arquitectura incorrecta: $module"
    vermagic="$(modinfo -F vermagic "$module" | sed 's/[[:space:]]*$//')"
    echo "VERMAGIC[$(basename "$module")]=$vermagic"
    [[ "$vermagic" == "$EXPECTED_RELEASE preempt MIPS32_R2 32BIT" ]] || die "Vermagic incorrecto en $module"
done

section "COMPARAR CON MODULOS ALSA PREEXISTENTES"
for built_module in "$SND_TIMER_KO" "$SND_PCM_KO"; do
    module_name="$(basename "$built_module")"
    reference_module="$REFERENCE_DIR/$module_name"
    if [[ -s "$reference_module" ]]; then
        echo "COMPARE_MODULE=$module_name"
        echo "BUILT_SHA256=$(sha256sum "$built_module" | awk '{print $1}')"
        echo "REFERENCE_SHA256=$(sha256sum "$reference_module" | awk '{print $1}')"
        echo "BUILT_VERMAGIC=$(modinfo -F vermagic "$built_module" | sed 's/[[:space:]]*$//')"
        echo "REFERENCE_VERMAGIC=$(modinfo -F vermagic "$reference_module" | sed 's/[[:space:]]*$//')"
        "${CROSS_COMPILE}nm" -g --defined-only "$built_module" | awk '{print $NF}' | sort -u > "$INPUT_DIR/${module_name}.built.symbols"
        "${CROSS_COMPILE}nm" -g --defined-only "$reference_module" | awk '{print $NF}' | sort -u > "$INPUT_DIR/${module_name}.reference.symbols"
        if diff -u "$INPUT_DIR/${module_name}.reference.symbols" "$INPUT_DIR/${module_name}.built.symbols" > "$INPUT_DIR/${module_name}.symbols.diff"; then
            echo "REFERENCE_EXPORT_SET_MATCH=$module_name"
        else
            echo "REFERENCE_EXPORT_SET_DIFF=$module_name FILE=$INPUT_DIR/${module_name}.symbols.diff"
        fi
    fi
done

section "COMPROBAR CONTRATO DE SIMBOLOS UAC2 -> ALSA"
DEFINED_SYMBOLS="$INPUT_DIR/alsa-defined-symbols.txt"
: > "$DEFINED_SYMBOLS"
for module in "$SND_KO" "$SND_TIMER_KO" "$SND_PCM_KO"; do
    "${CROSS_COMPILE}nm" -g --defined-only "$module" | awk '{print $NF}' >> "$DEFINED_SYMBOLS"
done
sort -u -o "$DEFINED_SYMBOLS" "$DEFINED_SYMBOLS"

REQUIRED_SND_SYMBOLS="$INPUT_DIR/uac2-required-snd-symbols.txt"
"${CROSS_COMPILE}nm" -u "$UAC2_INPUT" | awk '{print $NF}' | grep '^snd_' | sort -u > "$REQUIRED_SND_SYMBOLS"
require_file "$REQUIRED_SND_SYMBOLS"

missing_symbols=0
while IFS= read -r symbol; do
    if grep -qx "$symbol" "$DEFINED_SYMBOLS"; then
        echo "SYMBOL_OK=$symbol"
    else
        echo "SYMBOL_MISSING=$symbol"
        missing_symbols=$((missing_symbols + 1))
    fi
done < "$REQUIRED_SND_SYMBOLS"
[[ "$missing_symbols" -eq 0 ]] || die "$missing_symbols símbolos requeridos por UAC2 no fueron exportados por ALSA"

section "PREPARAR PAQUETE COPY-ROOT Y FUENTES"
PROFILE_REL="lgpt/otg/modules/$EXPECTED_RELEASE/u2_38au8_sync_uac2"
PROFILE_DIR="$RESULT_DIR/$PROFILE_REL"
SOURCE_DIR="$RESULT_DIR/SOURCE_AND_TOOLS"
mkdir -p "$PROFILE_DIR" "$SOURCE_DIR/kernel_build" "$SOURCE_DIR/kernel_source_subset"

cp -a "$SOUNDCORE_KO" "$PROFILE_DIR/soundcore.ko"
cp -a "$SND_KO" "$PROFILE_DIR/snd.ko"
cp -a "$SND_TIMER_KO" "$PROFILE_DIR/snd-timer.ko"
cp -a "$SND_PCM_KO" "$PROFILE_DIR/snd-pcm.ko"
cp -a "$UAC2_INPUT" "$PROFILE_DIR/usb_f_uac2.ko"

cp -a "$BASE_CONFIG" "$SOURCE_DIR/kernel_build/config.stock"
cp -a "$BUILD_DIR/.config" "$SOURCE_DIR/kernel_build/config.alsa-uac2"
cp -a "$BUILD_DIR/Module.symvers" "$SOURCE_DIR/kernel_build/Module.symvers"
cp -a "$REQUIRED_SND_SYMBOLS" "$SOURCE_DIR/kernel_build/uac2-required-snd-symbols.txt"
cp -a "$DEFINED_SYMBOLS" "$SOURCE_DIR/kernel_build/alsa-defined-symbols.txt"
cp -a "$LOG_FILE" "$SOURCE_DIR/kernel_build/"
cp -a "${BASH_SOURCE[0]}" "$SOURCE_DIR/UAC2_STAGE2_COMPILE_ALSA_R4.sh"
printf '%s\n' \
    'Linux 4.4 DTC compatibility fix: removed duplicate YYLTYPE yylloc definition from scripts/dtc/dtc-lexer.lex.c_shipped.' \
    > "$SOURCE_DIR/kernel_build/DTC_GCC10_COMPAT_PATCH.txt"
if compgen -G "$REFERENCE_DIR/*.ko" >/dev/null; then
    mkdir -p "$SOURCE_DIR/reference_modules"
    cp -a "$REFERENCE_DIR"/*.ko "$SOURCE_DIR/reference_modules/"
    cp -a "$INPUT_DIR"/*.symbols.diff "$SOURCE_DIR/reference_modules/" 2>/dev/null || true
fi
printf '%s\n' "$KERNEL_SRC" > "$SOURCE_DIR/kernel_build/kernel-source-original-path.txt"
printf '%s\n' "$KERNEL_WORK_SRC" > "$SOURCE_DIR/kernel_build/kernel-source-build-path.txt"
( cd "$KERNEL_WORK_SRC" && find . -type f -print0 | sort -z | xargs -0 sha256sum ) \
    > "$SOURCE_DIR/kernel_build/kernel-source-manifest.sha256"

# Incluye el árbol completo del kernel usado para producir los módulos.
# El directorio de salida O= está separado, por lo que esta copia contiene fuentes, no objetos de build.
mkdir -p "$SOURCE_DIR/full_kernel_source"
rsync -a --exclude='.git/' "$KERNEL_WORK_SRC/" "$SOURCE_DIR/full_kernel_source/"

# Conserva además un subconjunto pequeño para revisión rápida de ALSA.
rsync -a "$KERNEL_WORK_SRC/sound/core/" "$SOURCE_DIR/kernel_source_subset/sound-core/"
cp -a "$KERNEL_WORK_SRC/sound/sound_core.c" "$SOURCE_DIR/kernel_source_subset/sound_core.c"
cp -a "$KERNEL_WORK_SRC/sound/Makefile" "$SOURCE_DIR/kernel_source_subset/sound-Makefile"
cp -a "$KERNEL_WORK_SRC/sound/Kconfig" "$SOURCE_DIR/kernel_source_subset/sound-Kconfig"
rsync -a "$KERNEL_WORK_SRC/include/sound/" "$SOURCE_DIR/kernel_source_subset/include-sound/"
if [[ -d "$KERNEL_WORK_SRC/include/uapi/sound" ]]; then
    rsync -a "$KERNEL_WORK_SRC/include/uapi/sound/" "$SOURCE_DIR/kernel_source_subset/include-uapi-sound/"
fi

if [[ "$INCLUDE_FULL_REPO_SOURCE" == 1 ]]; then
    mkdir -p "$SOURCE_DIR/full_repository"
    rsync -a \
        --exclude='.git/' \
        --exclude='dist/' \
        --exclude='BUILD/' \
        --exclude='BACKUPS/' \
        --exclude='COLLECTED_LOGS/' \
        --exclude='__pycache__/' \
        "$REPO_ROOT/" "$SOURCE_DIR/full_repository/"
fi

cat > "$RESULT_DIR/README_PRIMERO_ES.md" <<README
# LGPT R36SX - ALSA Stage 2 R4

Este paquete contiene los cuatro módulos ALSA compilados para:

- kernel: $EXPECTED_RELEASE
- arquitectura: MIPS32r2 little-endian, o32
- preemption: PREEMPT
- UAC2 conservado: sha256 $EXPECTED_UAC2_SHA256

## Contenido para copiar a la raíz de la SD

Copiar la carpeta \`lgpt\` de este paquete sobre la raíz de la SD y combinar archivos.
No realizar la copia hasta revisar el log de compilación y los checksums.

## Orden explícito de carga

1. soundcore.ko
2. snd.ko
3. snd-timer.ko
4. snd-pcm.ko
5. usb_f_uac2.ko

El setup existente ya busca estos módulos en el directorio del perfil.

## Fuentes

\`SOURCE_AND_TOOLS\` contiene el repositorio completo disponible, la configuración
Stock, la configuración ALSA resultante, Module.symvers, el script reproducible,
el log y el árbol completo del kernel y el subconjunto de revisión rápida utilizado para ALSA.
README

(
    cd "$RESULT_DIR"
    find . -type f ! -name SHA256SUMS.txt -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS.txt
)

# Validación final del árbol antes de crear el ZIP.
for required in \
    "$PROFILE_DIR/soundcore.ko" \
    "$PROFILE_DIR/snd.ko" \
    "$PROFILE_DIR/snd-timer.ko" \
    "$PROFILE_DIR/snd-pcm.ko" \
    "$PROFILE_DIR/usb_f_uac2.ko" \
    "$SOURCE_DIR/UAC2_STAGE2_COMPILE_ALSA_R4.sh" \
    "$SOURCE_DIR/kernel_build/config.alsa-uac2" \
    "$SOURCE_DIR/kernel_build/Module.symvers" \
    "$SOURCE_DIR/full_kernel_source/Makefile"; do
    require_file "$required"
done

rm -f "$ZIP_FILE"
(
    cd "$RESULT_DIR"
    zip -qr "$ZIP_FILE" .
)
require_file "$ZIP_FILE"
sha256sum "$ZIP_FILE" | tee "$ZIP_FILE.sha256"

VERIFY_DIR="$WORK_ROOT/verify-${TIMESTAMP}"
rm -rf "$VERIFY_DIR"
mkdir -p "$VERIFY_DIR"
unzip -q "$ZIP_FILE" -d "$VERIFY_DIR"
(
    cd "$VERIFY_DIR"
    sha256sum -c SHA256SUMS.txt
)

section "RESULTADO"
echo "STAGE2_RESULT=BUILD_AND_PACKAGE_OK"
echo "RESULT_DIR=$RESULT_DIR"
echo "ZIP_FILE=$ZIP_FILE"
echo "ZIP_SHA256_FILE=$ZIP_FILE.sha256"
echo "BUILD_LOG=$LOG_FILE"
echo "SD_WRITES=NONE"
echo "NEXT_STEP=REVIEW_BUILD_LOG_BEFORE_SD_DEPLOYMENT"
