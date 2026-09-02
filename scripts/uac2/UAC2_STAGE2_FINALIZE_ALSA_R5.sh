#!/usr/bin/env bash
set -Eeuo pipefail

# LGPT R36SX - Stage 2 R5
# Reanuda el workspace R4 y finaliza exclusivamente los cuatro módulos ALSA.
# No recompila todo el kernel, no monta y no escribe la SD.

PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}"
REPO_ROOT="${REPO_ROOT:-$PROJECT_ROOT/GITHUB/lgpt-r36sx-port}"
SDK_ROOT="${SDK_ROOT:-$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot}"
CROSS_COMPILE="${CROSS_COMPILE:-$SDK_ROOT/bin/mips-mti-linux-gnu-}"
WORK_ROOT="${WORK_ROOT:-$HOME/lgpt-r36sx-kernel-work/stage2-alsa}"
OUTPUT_ROOT="${OUTPUT_ROOT:-$PROJECT_ROOT/BUILD}"
LOG_ROOT="${LOG_ROOT:-$PROJECT_ROOT/LOGS}"
EXPECTED_RELEASE="${EXPECTED_RELEASE:-4.4.186-release}"
EXPECTED_UAC2_SHA256="${EXPECTED_UAC2_SHA256:-e9062ac5a37aa7706c98a3411a57dbe67bc98ef82621c430c1d4e7a89b2fbefe}"
JOBS="${JOBS:-$(nproc)}"
RESUME_TIMESTAMP="${RESUME_TIMESTAMP:-}"
INCLUDE_FULL_REPO_SOURCE="${INCLUDE_FULL_REPO_SOURCE:-1}"
INCLUDE_EXACT_KERNEL_SOURCE="${INCLUDE_EXACT_KERNEL_SOURCE:-1}"

RUN_TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="$LOG_ROOT/UAC2_STAGE2_ALSA_FINALIZE_R5_${RUN_TIMESTAMP}.log"
RESULT_DIR="$OUTPUT_ROOT/UAC2_STAGE2_ALSA_R5_${RUN_TIMESTAMP}"
ZIP_FILE="$OUTPUT_ROOT/LGPT_R36SX_UAC2_ALSA_STAGE2_R5_${RUN_TIMESTAMP}_FULL_SOURCE.zip"
INPUT_DIR="$WORK_ROOT/r5-input-${RUN_TIMESTAMP}"

mkdir -p "$LOG_ROOT" "$OUTPUT_ROOT" "$RESULT_DIR" "$INPUT_DIR"
exec > >(tee -a "$LOG_FILE") 2>&1

cleanup() {
    local rc=$?
    set +e
    echo "SCRIPT_EXIT_CODE=$rc"
    echo "BUILD_LOG=$LOG_FILE"
    echo "SD_MOUNT_ACTION=NONE"
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

section "IDENTIDAD R5"
echo "DATE=$(date -Iseconds)"
echo "PROJECT_ROOT=$PROJECT_ROOT"
echo "REPO_ROOT=$REPO_ROOT"
echo "WORK_ROOT=$WORK_ROOT"
echo "OUTPUT_ROOT=$OUTPUT_ROOT"
echo "CROSS_COMPILE=$CROSS_COMPILE"
echo "EXPECTED_RELEASE=$EXPECTED_RELEASE"
echo "JOBS=$JOBS"
echo "PURPOSE=finalize only sound modules from the verified R4 workspace"
echo "SD_ACCESS=not required"

section "PREFLIGHT"
require_dir "$REPO_ROOT"
require_dir "$WORK_ROOT"
for tool in bash make gcc g++ python3 file modinfo readelf sha256sum rsync zip unzip find sort awk sed grep diff; do
    command -v "$tool" >/dev/null 2>&1 || die "Herramienta ausente: $tool"
done
for tool in gcc nm readelf objdump strip; do
    require_file "${CROSS_COMPILE}${tool}"
done

section "SELECCIONAR WORKSPACE R4 EXACTO"
if [[ -z "$RESUME_TIMESTAMP" ]]; then
    mapfile -t BUILD_CANDIDATES < <(
        find "$WORK_ROOT" -maxdepth 1 -type d -name 'build-20??????_??????' -printf '%T@ %f\n' 2>/dev/null \
            | sort -nr | awk '{print $2}'
    )
    for candidate in "${BUILD_CANDIDATES[@]}"; do
        ts="${candidate#build-}"
        src="$WORK_ROOT/kernel-source-$ts"
        bld="$WORK_ROOT/build-$ts"
        if [[ -s "$bld/.config" \
           && -s "$bld/include/generated/autoconf.h" \
           && -s "$bld/include/config/kernel.release" \
           && -s "$src/Makefile" \
           && -s "$bld/sound/core/snd.o" \
           && -s "$bld/sound/core/snd-timer.o" \
           && -s "$bld/sound/core/snd-pcm.o" ]]; then
            RESUME_TIMESTAMP="$ts"
            break
        fi
    done
fi

[[ -n "$RESUME_TIMESTAMP" ]] || die "No se encontró un workspace R4 con los objetos ALSA ya enlazados"
KERNEL_WORK_SRC="$WORK_ROOT/kernel-source-$RESUME_TIMESTAMP"
BUILD_DIR="$WORK_ROOT/build-$RESUME_TIMESTAMP"

require_dir "$KERNEL_WORK_SRC"
require_dir "$BUILD_DIR"
require_file "$BUILD_DIR/.config"
require_file "$BUILD_DIR/include/generated/autoconf.h"
require_file "$BUILD_DIR/include/config/kernel.release"
require_file "$BUILD_DIR/sound/sound_core.o"
require_file "$BUILD_DIR/sound/core/snd.o"
require_file "$BUILD_DIR/sound/core/snd-timer.o"
require_file "$BUILD_DIR/sound/core/snd-pcm.o"

echo "RESUME_TIMESTAMP=$RESUME_TIMESTAMP"
echo "KERNEL_WORK_SRC=$KERNEL_WORK_SRC"
echo "BUILD_DIR=$BUILD_DIR"
echo "PREEXISTING_SOUND_OBJECTS=OK"

KERNEL_RELEASE="$(cat "$BUILD_DIR/include/config/kernel.release" | tr -d '\r\n')"
echo "KERNEL_RELEASE_FROM_BUILD=$KERNEL_RELEASE"
[[ "$KERNEL_RELEASE" == "$EXPECTED_RELEASE" ]] || die "Release inesperado en workspace: $KERNEL_RELEASE"

for required in \
    'CONFIG_MIPS=y' \
    'CONFIG_CPU_MIPS32_R2=y' \
    'CONFIG_MODULES=y' \
    'CONFIG_PREEMPT=y' \
    'CONFIG_SOUND=m' \
    'CONFIG_SND=m' \
    'CONFIG_SND_TIMER=m' \
    'CONFIG_SND_PCM=m'; do
    grep -qx "$required" "$BUILD_DIR/.config" || die "El workspace no conserva $required"
done
if grep -qx 'CONFIG_MODVERSIONS=y' "$BUILD_DIR/.config"; then
    die "CONFIG_MODVERSIONS está habilitado; no coincide con el UAC2 verificado"
fi

section "DIAGNOSTICO DEL FALLO DE R4"
UPPER_TCPMSS="$KERNEL_WORK_SRC/net/netfilter/xt_TCPMSS.c"
LOWER_TCPMSS="$KERNEL_WORK_SRC/net/netfilter/xt_tcpmss.c"
if [[ -f "$LOWER_TCPMSS" && ! -f "$UPPER_TCPMSS" ]]; then
    echo "SOURCE_CASE_COLLISION_CONFIRMED=YES"
    echo "PRESENT=$LOWER_TCPMSS"
    echo "MISSING=$UPPER_TCPMSS"
    echo "R4_FULL_KERNEL_FAILURE=NTFS_CASE_COLLISION_IN_NETFILTER"
else
    echo "SOURCE_CASE_COLLISION_CONFIRMED=NO_OR_TREE_ALREADY_REPAIRED"
    [[ -f "$UPPER_TCPMSS" ]] && echo "PRESENT=$UPPER_TCPMSS"
    [[ -f "$LOWER_TCPMSS" ]] && echo "PRESENT=$LOWER_TCPMSS"
fi

echo "R5_SCOPE=sound only; net/netfilter is not traversed"

section "LOCALIZAR UAC2 VERIFICADO Y REFERENCIAS"
UAC2_REPO="$REPO_ROOT/recovery/u2_38au8_sync_uac2/usb_f_uac2.ko"
require_file "$UAC2_REPO"
UAC2_SHA256="$(sha256sum "$UAC2_REPO" | awk '{print $1}')"
UAC2_VERMAGIC="$(modinfo -F vermagic "$UAC2_REPO" | sed 's/[[:space:]]*$//')"
echo "UAC2_PATH=$UAC2_REPO"
echo "UAC2_SHA256=$UAC2_SHA256"
echo "UAC2_VERMAGIC=$UAC2_VERMAGIC"
[[ "$UAC2_SHA256" == "$EXPECTED_UAC2_SHA256" ]] || die "El UAC2 no coincide con el binario verificado"
[[ "$UAC2_VERMAGIC" == "$EXPECTED_RELEASE preempt MIPS32_R2 32BIT" ]] || die "Vermagic UAC2 inesperado"

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

section "FINALIZAR EXCLUSIVAMENTE EL SUBARBOL SOUND"
# La compilación completa R4 ya generó headers, configuración y objetos ALSA.
# Este comando limita kbuild a sound/ y ejecuta modpost sólo para esos módulos.
# CONFIG_MODVERSIONS=n; KBUILD_MODPOST_WARN=1 permite que modpost no exija un
# Module.symvers global de vmlinux, mientras conserva las dependencias entre
# los módulos ALSA compilados en el mismo lote.
echo "TARGETED_BUILD_COMMAND=make -C $KERNEL_WORK_SRC O=$BUILD_DIR ARCH=mips CROSS_COMPILE=$CROSS_COMPILE M=sound KBUILD_MODPOST_WARN=1 -j$JOBS modules"
make -C "$KERNEL_WORK_SRC" \
    O="$BUILD_DIR" \
    ARCH=mips \
    CROSS_COMPILE="$CROSS_COMPILE" \
    HOSTCC=gcc \
    HOSTCXX=g++ \
    KBUILD_MODPOST_WARN=1 \
    M=sound \
    -j"$JOBS" \
    modules

section "LOCALIZAR MODULOS RESULTANTES"
find_one_module() {
    local name="$1"
    local -a found=()
    mapfile -t found < <(find "$BUILD_DIR/sound" -type f -name "$name" -print 2>/dev/null | sort -u)
    if [[ "${#found[@]}" -eq 0 ]]; then
        mapfile -t found < <(find "$KERNEL_WORK_SRC/sound" -type f -name "$name" -print 2>/dev/null | sort -u)
    fi
    [[ "${#found[@]}" -eq 1 ]] || {
        printf 'FOUND_%s_COUNT=%s\n' "$name" "${#found[@]}"
        printf '%s\n' "${found[@]:-}"
        die "Se esperaba exactamente un $name"
    }
    printf '%s\n' "${found[0]}"
}

SOUNDCORE_KO="$(find_one_module soundcore.ko)"
SND_KO="$(find_one_module snd.ko)"
SND_TIMER_KO="$(find_one_module snd-timer.ko)"
SND_PCM_KO="$(find_one_module snd-pcm.ko)"
MODULES=("$SOUNDCORE_KO" "$SND_KO" "$SND_TIMER_KO" "$SND_PCM_KO")

for module in "${MODULES[@]}"; do
    require_file "$module"
    name="$(basename "$module")"
    echo "MODULE_PATH[$name]=$module"
    echo "MODULE_SHA256[$name]=$(sha256sum "$module" | awk '{print $1}')"
    file "$module"
    file "$module" | grep -q 'ELF 32-bit LSB relocatable, MIPS' || die "Arquitectura incorrecta: $module"
    vermagic="$(modinfo -F vermagic "$module" | sed 's/[[:space:]]*$//')"
    depends="$(modinfo -F depends "$module" | tr -d '\r\n')"
    echo "VERMAGIC[$name]=$vermagic"
    echo "DEPENDS[$name]=$depends"
    [[ "$vermagic" == "$EXPECTED_RELEASE preempt MIPS32_R2 32BIT" ]] || die "Vermagic incorrecto en $name"
    if readelf -S "$module" | grep -q '__versions'; then
        die "$name contiene __versions pero CONFIG_MODVERSIONS=n"
    fi
done

# Contratos mínimos observados en los módulos de referencia.
[[ "$(modinfo -F depends "$SND_TIMER_KO" | tr -d '\r\n')" == *snd* ]] || die "snd-timer.ko no declara dependencia de snd"
SND_PCM_DEPENDS="$(modinfo -F depends "$SND_PCM_KO" | tr -d '\r\n')"
[[ "$SND_PCM_DEPENDS" == *snd* && "$SND_PCM_DEPENDS" == *snd-timer* ]] || die "snd-pcm.ko no declara snd y snd-timer"

section "VALIDAR CONTRATO UAC2 -> ALSA"
DEFINED_SYMBOLS="$INPUT_DIR/alsa-defined-symbols.txt"
REQUIRED_SYMBOLS="$INPUT_DIR/uac2-required-snd-symbols.txt"
: > "$DEFINED_SYMBOLS"
for module in "$SND_KO" "$SND_TIMER_KO" "$SND_PCM_KO"; do
    "${CROSS_COMPILE}nm" -g --defined-only "$module" | awk '{print $NF}' >> "$DEFINED_SYMBOLS"
done
sort -u -o "$DEFINED_SYMBOLS" "$DEFINED_SYMBOLS"
"${CROSS_COMPILE}nm" -u "$UAC2_REPO" | awk '{print $NF}' | grep '^snd_' | sort -u > "$REQUIRED_SYMBOLS"
require_file "$REQUIRED_SYMBOLS"

missing=0
while IFS= read -r symbol; do
    if grep -qx "$symbol" "$DEFINED_SYMBOLS"; then
        echo "SYMBOL_OK=$symbol"
    else
        echo "SYMBOL_MISSING=$symbol"
        missing=$((missing + 1))
    fi
done < "$REQUIRED_SYMBOLS"
[[ "$missing" -eq 0 ]] || die "$missing símbolos UAC2 no están exportados por los módulos ALSA"

section "COMPARAR CON MODULOS PREEXISTENTES"
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

section "LOCALIZAR MODULE.SYMVERS DEL BUILD DIRIGIDO"
mapfile -t SYMVERS_FILES < <(find "$BUILD_DIR" "$KERNEL_WORK_SRC/sound" -type f -name Module.symvers -size +0c -print 2>/dev/null | sort -u)
if [[ "${#SYMVERS_FILES[@]}" -gt 0 ]]; then
    for symvers in "${SYMVERS_FILES[@]}"; do
        echo "MODULE_SYMVERS=$symvers SHA256=$(sha256sum "$symvers" | awk '{print $1}')"
    done
else
    echo "MODULE_SYMVERS=NONE"
    echo "NOTE=CONFIG_MODVERSIONS is disabled; runtime symbol resolution uses names, matching the verified UAC2 module"
fi

section "PREPARAR PAQUETE COPY-ROOT Y FUENTES"
PROFILE_REL="lgpt/otg/modules/$EXPECTED_RELEASE/u2_38au8_sync_uac2"
PROFILE_DIR="$RESULT_DIR/$PROFILE_REL"
SOURCE_DIR="$RESULT_DIR/SOURCE_AND_TOOLS"
mkdir -p "$PROFILE_DIR" "$SOURCE_DIR/kernel_build" "$SOURCE_DIR/reference_modules"

cp -a "$SOUNDCORE_KO" "$PROFILE_DIR/soundcore.ko"
cp -a "$SND_KO" "$PROFILE_DIR/snd.ko"
cp -a "$SND_TIMER_KO" "$PROFILE_DIR/snd-timer.ko"
cp -a "$SND_PCM_KO" "$PROFILE_DIR/snd-pcm.ko"
cp -a "$UAC2_REPO" "$PROFILE_DIR/usb_f_uac2.ko"

cp -a "$BUILD_DIR/.config" "$SOURCE_DIR/kernel_build/config.alsa-uac2"
cp -a "$LOG_FILE" "$SOURCE_DIR/kernel_build/"
cp -a "$REQUIRED_SYMBOLS" "$SOURCE_DIR/kernel_build/"
cp -a "$DEFINED_SYMBOLS" "$SOURCE_DIR/kernel_build/"
cp -a "${BASH_SOURCE[0]}" "$SOURCE_DIR/UAC2_STAGE2_FINALIZE_ALSA_R5.sh"
printf '%s\n' "$KERNEL_WORK_SRC" > "$SOURCE_DIR/kernel_build/kernel-source-build-path.txt"
printf '%s\n' "$BUILD_DIR" > "$SOURCE_DIR/kernel_build/kernel-output-build-path.txt"
printf '%s\n' \
    'R4 full build stopped outside ALSA because the NTFS-derived source tree lacked net/netfilter/xt_TCPMSS.c while retaining xt_tcpmss.c.' \
    'R5 deliberately builds only sound/ from the already configured R4 workspace.' \
    'CONFIG_MODVERSIONS=n; modules are validated by architecture, vermagic, dependencies and the exact UAC2 snd_* symbol contract.' \
    > "$SOURCE_DIR/kernel_build/R5_BUILD_SCOPE_AND_SOURCE_INTEGRITY.txt"

if [[ "${#SYMVERS_FILES[@]}" -gt 0 ]]; then
    mkdir -p "$SOURCE_DIR/kernel_build/symvers"
    index=0
    for symvers in "${SYMVERS_FILES[@]}"; do
        cp -a "$symvers" "$SOURCE_DIR/kernel_build/symvers/Module.symvers.$index"
        index=$((index + 1))
    done
fi

if compgen -G "$REFERENCE_DIR/*.ko" >/dev/null; then
    cp -a "$REFERENCE_DIR"/*.ko "$SOURCE_DIR/reference_modules/"
    cp -a "$INPUT_DIR"/*.symbols.diff "$SOURCE_DIR/reference_modules/" 2>/dev/null || true
fi

# Fuente exacta mínima y completa del subsistema compilado.
mkdir -p "$SOURCE_DIR/kernel_source_sound"
rsync -a "$KERNEL_WORK_SRC/sound/" "$SOURCE_DIR/kernel_source_sound/sound/"
rsync -a "$KERNEL_WORK_SRC/include/sound/" "$SOURCE_DIR/kernel_source_sound/include-sound/"
if [[ -d "$KERNEL_WORK_SRC/include/uapi/sound" ]]; then
    rsync -a "$KERNEL_WORK_SRC/include/uapi/sound/" "$SOURCE_DIR/kernel_source_sound/include-uapi-sound/"
fi
cp -a "$KERNEL_WORK_SRC/Makefile" "$SOURCE_DIR/kernel_source_sound/kernel-Makefile"
cp -a "$KERNEL_WORK_SRC/scripts/dtc/dtc-lexer.lex.c_shipped" "$SOURCE_DIR/kernel_build/dtc-lexer.lex.c_shipped.patched"

if [[ "$INCLUDE_EXACT_KERNEL_SOURCE" == 1 ]]; then
    mkdir -p "$SOURCE_DIR/exact_kernel_source_used"
    rsync -a --exclude='.git/' "$KERNEL_WORK_SRC/" "$SOURCE_DIR/exact_kernel_source_used/"
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
# LGPT R36SX - ALSA/UAC2 Stage 2 R5

Este paquete contiene el stack modular validado para:

- kernel: $EXPECTED_RELEASE
- arquitectura: MIPS32r2 little-endian, o32
- preemption: PREEMPT
- CONFIG_MODVERSIONS: deshabilitado
- UAC2 conservado: SHA-256 $EXPECTED_UAC2_SHA256

## Orden de carga

1. soundcore.ko
2. snd.ko
3. snd-timer.ko
4. snd-pcm.ko
5. usb_f_uac2.ko

## Estado

Los módulos fueron compilados y validados, pero este paquete todavía no debe copiarse
a la SD hasta revisar el log R5. La siguiente etapa será un despliegue controlado con
backup, carga individual, dmesg después de cada módulo y desmontaje seguro de F:.

## Fuente incluida

SOURCE_AND_TOOLS contiene el repositorio completo disponible, el script R5, el log,
la configuración, los símbolos, el subárbol sound completo y el árbol exacto del
kernel usado cuando INCLUDE_EXACT_KERNEL_SOURCE=1.
README

(
    cd "$RESULT_DIR"
    find . -type f ! -name SHA256SUMS.txt -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS.txt
)

for required in \
    "$PROFILE_DIR/soundcore.ko" \
    "$PROFILE_DIR/snd.ko" \
    "$PROFILE_DIR/snd-timer.ko" \
    "$PROFILE_DIR/snd-pcm.ko" \
    "$PROFILE_DIR/usb_f_uac2.ko" \
    "$SOURCE_DIR/UAC2_STAGE2_FINALIZE_ALSA_R5.sh" \
    "$SOURCE_DIR/kernel_build/config.alsa-uac2" \
    "$SOURCE_DIR/kernel_source_sound/sound/Makefile" \
    "$RESULT_DIR/SHA256SUMS.txt"; do
    require_file "$required"
done

rm -f "$ZIP_FILE" "$ZIP_FILE.sha256"
(
    cd "$RESULT_DIR"
    zip -qr "$ZIP_FILE" .
)
require_file "$ZIP_FILE"
sha256sum "$ZIP_FILE" | tee "$ZIP_FILE.sha256"

VERIFY_DIR="$WORK_ROOT/r5-verify-${RUN_TIMESTAMP}"
rm -rf "$VERIFY_DIR"
mkdir -p "$VERIFY_DIR"
unzip -q "$ZIP_FILE" -d "$VERIFY_DIR"
(
    cd "$VERIFY_DIR"
    sha256sum -c SHA256SUMS.txt
)

section "RESULTADO"
echo "STAGE2_R5_RESULT=TARGETED_ALSA_BUILD_AND_PACKAGE_OK"
echo "RESUMED_R4_TIMESTAMP=$RESUME_TIMESTAMP"
echo "RESULT_DIR=$RESULT_DIR"
echo "ZIP_FILE=$ZIP_FILE"
echo "ZIP_SHA256_FILE=$ZIP_FILE.sha256"
echo "BUILD_LOG=$LOG_FILE"
echo "SD_WRITES=NONE"
echo "SD_MOUNT_ACTION=NONE"
echo "NEXT_STEP=REVIEW_R5_LOG_BEFORE_CONTROLLED_SD_DEPLOYMENT"
