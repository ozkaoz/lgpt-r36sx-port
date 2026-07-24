#!/usr/bin/env bash
set -Eeuo pipefail

PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
SD_MOUNT="${SD_MOUNT:-/mnt/f}"
SD_DRIVE="${SD_DRIVE:-F:}"
WORK_ROOT="${WORK_ROOT:-$HOME/lgpt-r36sx-kernel-work}"
TS="$(date +%Y%m%d_%H%M%S)"
LOG_DIR="$PROJECT_ROOT/LOGS"
LOG_FILE="$LOG_DIR/UAC2_STAGE1_AUDIT_${TS}.log"
INPUT_DIR="$WORK_ROOT/input_from_sd"
MOUNTED_BY_SCRIPT=0

mkdir -p "$LOG_DIR" "$INPUT_DIR"
exec > >(tee "$LOG_FILE") 2>&1

section() {
    printf '\n============================================================\n'
    printf '%s\n' "$*"
    printf '============================================================\n'
}

warn() {
    printf 'WARN: %s\n' "$*" >&2
}

have() {
    command -v "$1" >/dev/null 2>&1
}

cleanup() {
    local rc=$?
    if (( MOUNTED_BY_SCRIPT == 1 )); then
        section "SINCRONIZACION Y DESMONTAJE DE LA SD"
        sync || true
        cd / || true
        if sudo umount "$SD_MOUNT"; then
            echo "SD_UNMOUNT_OK=$SD_MOUNT"
        else
            warn "No se pudo desmontar $SD_MOUNT. Cierre Explorer/terminales abiertas en F: y ejecute: sudo umount '$SD_MOUNT'"
        fi
    fi
    echo "AUDIT_EXIT_CODE=$rc"
    echo "AUDIT_LOG=$LOG_FILE"
}
trap cleanup EXIT

section "IDENTIDAD DEL AUDIT"
echo "AUDIT_VERSION=UAC2_STAGE1_AUDIT_R1"
echo "DATE=$(date --iso-8601=seconds 2>/dev/null || date)"
echo "USER=$(id -un)"
echo "PWD=$PWD"
echo "PROJECT_ROOT=$PROJECT_ROOT"
echo "WORK_ROOT=$WORK_ROOT"
echo "SD_DRIVE=$SD_DRIVE"
echo "SD_MOUNT=$SD_MOUNT"

section "HOST WSL/UBUNTU"
uname -a
printf 'WSL_INTEROP=%s\n' "${WSL_INTEROP:-unset}"
printf 'WSL_DISTRO_NAME=%s\n' "${WSL_DISTRO_NAME:-unset}"
if have lsb_release; then
    lsb_release -a 2>/dev/null || true
else
    cat /etc/os-release 2>/dev/null || true
fi
printf 'HOST_ARCH=%s\n' "$(uname -m)"
printf 'FILESYSTEM_PROJECT='; df -T "$PROJECT_ROOT" 2>/dev/null | tail -n 1 || true
printf 'FILESYSTEM_WORK='; df -T "$WORK_ROOT" 2>/dev/null | tail -n 1 || true

section "HERRAMIENTAS DEL HOST"
required_tools=(bash make gcc g++ awk sed grep find xargs file sha256sum readelf objdump nm strings rsync python3)
optional_tools=(git modinfo depmod bc bison flex perl cpio xz zip unzip dtc)
for tool in "${required_tools[@]}"; do
    if have "$tool"; then
        printf 'TOOL_REQUIRED_OK %-12s %s\n' "$tool" "$(command -v "$tool")"
    else
        printf 'TOOL_REQUIRED_MISSING %s\n' "$tool"
    fi
done
for tool in "${optional_tools[@]}"; do
    if have "$tool"; then
        printf 'TOOL_OPTIONAL_OK %-12s %s\n' "$tool" "$(command -v "$tool")"
    else
        printf 'TOOL_OPTIONAL_MISSING %s\n' "$tool"
    fi
done

section "REPOSITORIO LGPT LOCAL"
repo_candidates=()
while IFS= read -r -d '' src; do
    repo="${src%/device/r36s_u2523_usb_audio_io.c}"
    repo_candidates+=("$repo")
done < <(
    find "$PROJECT_ROOT" "$HOME" -maxdepth 8 -type f \
        -path '*/device/r36s_u2523_usb_audio_io.c' -print0 2>/dev/null || true
)

if ((${#repo_candidates[@]} == 0)); then
    echo "REPO_CANDIDATE_COUNT=0"
else
    printf 'REPO_CANDIDATE_COUNT=%d\n' "${#repo_candidates[@]}"
    printf '%s\n' "${repo_candidates[@]}" | awk '!seen[$0]++' | while IFS= read -r repo; do
        echo "REPO_CANDIDATE=$repo"
        if [[ -d "$repo/.git" ]] && have git; then
            git -C "$repo" status --short --branch || true
            git -C "$repo" remote -v || true
            git -C "$repo" log -1 --format='REPO_COMMIT=%H%nREPO_COMMIT_DATE=%cI%nREPO_COMMIT_SUBJECT=%s' || true
        fi
        for f in README.md VERSION scripts/build.sh device/otg_u241_setup_once.sh recovery/u2_38au8_sync_uac2/usb_f_uac2.ko; do
            if [[ -e "$repo/$f" ]]; then
                echo "REPO_FILE_OK=$repo/$f"
            else
                echo "REPO_FILE_MISSING=$repo/$f"
            fi
        done
    done
fi

section "TOOLCHAINS MIPS"
toolchain_compilers=()
known_compilers=(
    "$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot/bin/mips-mti-linux-gnu-gcc"
    "$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot/bin/mipsel-buildroot-linux-gnu-gcc"
    "$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot/bin/mipsel-linux-gnu-gcc"
)
for cc in "${known_compilers[@]}"; do
    [[ -x "$cc" ]] && toolchain_compilers+=("$cc")
done
while IFS= read -r -d '' cc; do
    toolchain_compilers+=("$cc")
done < <(
    find "$HOME/sf3000-work" "$PROJECT_ROOT" -maxdepth 9 -type f -perm -u+x \
        \( -name 'mips-mti-linux-gnu-gcc' -o -name 'mipsel-buildroot-linux-gnu-gcc' -o -name 'mipsel-linux-gnu-gcc' \) \
        -print0 2>/dev/null || true
)

mapfile -t unique_compilers < <(printf '%s\n' "${toolchain_compilers[@]:-}" | sed '/^$/d' | awk '!seen[$0]++')
echo "MIPS_COMPILER_COUNT=${#unique_compilers[@]}"
for cc in "${unique_compilers[@]}"; do
    echo "MIPS_CC=$cc"
    "$cc" -dumpmachine 2>/dev/null | sed 's/^/MIPS_CC_DUMPMACHINE=/' || true
    "$cc" -dumpversion 2>/dev/null | sed 's/^/MIPS_CC_DUMPVERSION=/' || true
    "$cc" --version 2>/dev/null | head -n 2 | sed 's/^/MIPS_CC_VERSION=/' || true
    "$cc" -print-sysroot 2>/dev/null | sed 's/^/MIPS_CC_SYSROOT=/' || true
    prefix="${cc%gcc}"
    for bin in ld as ar nm objcopy objdump strip readelf; do
        [[ -x "${prefix}${bin}" ]] && echo "MIPS_BINUTIL_OK=${prefix}${bin}"
    done
    tmpc="$(mktemp --suffix=.c)"
    tmpo="$(mktemp --suffix=.o)"
    printf 'int r36sx_probe(void){return 0;}\n' > "$tmpc"
    if "$cc" -mips32r2 -EL -c "$tmpc" -o "$tmpo" 2>/tmp/uac2_cc_probe.err; then
        echo "MIPS_COMPILE_PROBE=OK"
        file "$tmpo" || true
        "${prefix}readelf" -h "$tmpo" 2>/dev/null | grep -E 'Class:|Data:|Machine:|Flags:' || true
    else
        echo "MIPS_COMPILE_PROBE=FAILED"
        cat /tmp/uac2_cc_probe.err || true
    fi
    rm -f "$tmpc" "$tmpo" /tmp/uac2_cc_probe.err
    echo "---"
done

section "CANDIDATOS DE FUENTE DEL KERNEL"
kernel_candidates=()
search_roots=("$PROJECT_ROOT" "$HOME/sf3000-work" "$HOME")
for root in "${search_roots[@]}"; do
    [[ -d "$root" ]] || continue
    while IFS= read -r -d '' mk; do
        kdir="${mk%/Makefile}"
        [[ -d "$kdir/arch/mips" ]] || continue
        [[ -d "$kdir/include/linux" ]] || continue
        [[ -d "$kdir/sound/core" ]] || continue
        version="$(awk -F'=' '/^VERSION[[:space:]]*=/{gsub(/[[:space:]]/,"",$2);print $2;exit}' "$mk")"
        patchlevel="$(awk -F'=' '/^PATCHLEVEL[[:space:]]*=/{gsub(/[[:space:]]/,"",$2);print $2;exit}' "$mk")"
        sublevel="$(awk -F'=' '/^SUBLEVEL[[:space:]]*=/{gsub(/[[:space:]]/,"",$2);print $2;exit}' "$mk")"
        if [[ "$version.$patchlevel.$sublevel" == "4.4.186" ]]; then
            kernel_candidates+=("$kdir")
        fi
    done < <(find "$root" -maxdepth 9 -type f -name Makefile -print0 2>/dev/null || true)
done
mapfile -t unique_kernels < <(printf '%s\n' "${kernel_candidates[@]:-}" | sed '/^$/d' | awk '!seen[$0]++')
echo "KERNEL_CANDIDATE_COUNT=${#unique_kernels[@]}"

config_keys='CONFIG_MODULES|CONFIG_MODVERSIONS|CONFIG_MODULE_UNLOAD|CONFIG_PREEMPT|CONFIG_CPU_MIPS32_R2|CONFIG_CPU_LITTLE_ENDIAN|CONFIG_CPU_BIG_ENDIAN|CONFIG_SND|CONFIG_SND_TIMER|CONFIG_SND_PCM|CONFIG_USB_GADGET|CONFIG_USB_LIBCOMPOSITE|CONFIG_USB_CONFIGFS|CONFIG_USB_CONFIGFS_F_UAC2|CONFIG_USB_F_UAC2'
required_symbols=(snd_card_new snd_card_register snd_card_free snd_pcm_new snd_pcm_set_ops snd_pcm_period_elapsed snd_pcm_lib_ioctl snd_pcm_lib_malloc_pages snd_pcm_lib_free_pages snd_pcm_lib_preallocate_pages_for_all snd_pcm_hw_constraint_integer)

for kdir in "${unique_kernels[@]}"; do
    echo "KERNEL_CANDIDATE=$kdir"
    if [[ -d "$kdir/.git" ]] && have git; then
        git -C "$kdir" status --short --branch || true
        git -C "$kdir" log -1 --format='KERNEL_COMMIT=%H%nKERNEL_COMMIT_DATE=%cI%nKERNEL_COMMIT_SUBJECT=%s' || true
    fi
    for artifact in .config Module.symvers System.map vmlinux include/config/kernel.release include/generated/autoconf.h include/generated/utsrelease.h; do
        if [[ -s "$kdir/$artifact" ]]; then
            echo "KERNEL_ARTIFACT_OK=$kdir/$artifact SIZE=$(stat -c %s "$kdir/$artifact" 2>/dev/null || echo unknown)"
        else
            echo "KERNEL_ARTIFACT_MISSING=$kdir/$artifact"
        fi
    done
    if [[ -s "$kdir/.config" ]]; then
        grep -E "^($config_keys)=" "$kdir/.config" || true
        grep -E '^CONFIG_LOCALVERSION=' "$kdir/.config" || true
    fi
    if [[ -s "$kdir/include/config/kernel.release" ]]; then
        printf 'KERNEL_RELEASE_FILE='; cat "$kdir/include/config/kernel.release"
    fi
    for src in \
        sound/core/sound.c \
        sound/core/init.c \
        sound/core/pcm.c \
        sound/core/pcm_lib.c \
        sound/core/timer.c \
        drivers/usb/gadget/function/f_uac2.c \
        drivers/usb/gadget/function/u_audio.c; do
        [[ -f "$kdir/$src" ]] && echo "KERNEL_SOURCE_OK=$kdir/$src" || echo "KERNEL_SOURCE_MISSING=$kdir/$src"
    done
    if [[ -s "$kdir/Module.symvers" ]]; then
        for sym in "${required_symbols[@]}"; do
            if grep -qw "$sym" "$kdir/Module.symvers"; then
                echo "SYMVERS_HAS=$sym"
            else
                echo "SYMVERS_MISSING=$sym"
            fi
        done
    fi
    echo "---"
done

section "MONTAJE E INVENTARIO DE LA SD"
if mountpoint -q "$SD_MOUNT" 2>/dev/null; then
    echo "SD_ALREADY_MOUNTED=$SD_MOUNT"
else
    sudo mkdir -p "$SD_MOUNT"
    if sudo mount -t drvfs "$SD_DRIVE" "$SD_MOUNT"; then
        MOUNTED_BY_SCRIPT=1
        echo "SD_MOUNT_OK=$SD_DRIVE->$SD_MOUNT"
    else
        warn "No se pudo montar $SD_DRIVE en $SD_MOUNT. El audit continuará sin inspeccionar la SD."
    fi
fi

if mountpoint -q "$SD_MOUNT" 2>/dev/null; then
    df -T "$SD_MOUNT" || true
    echo "SD_ROOT_LISTING_BEGIN"
    find "$SD_MOUNT" -maxdepth 2 -mindepth 1 -printf '%y %p %s bytes\n' 2>/dev/null | sort | head -n 300 || true
    echo "SD_ROOT_LISTING_END"

    echo "SD_KERNEL_ARTIFACTS_BEGIN"
    find "$SD_MOUNT" -maxdepth 10 -type f \
        \( -name '*.ko' -o -name 'Module.symvers' -o -name 'System.map*' -o -name 'vmlinux*' -o -name '*config*' -o -name 'uImage*' -o -name 'zImage*' \) \
        -printf '%p %s bytes\n' 2>/dev/null | sort | head -n 2000 || true
    echo "SD_KERNEL_ARTIFACTS_END"

    current_uac2="$(find "$SD_MOUNT" -maxdepth 12 -type f -path '*/u2_38au8_sync_uac2/usb_f_uac2.ko' -print -quit 2>/dev/null || true)"
    if [[ -n "$current_uac2" ]]; then
        echo "SD_CURRENT_UAC2=$current_uac2"
        cp -a "$current_uac2" "$INPUT_DIR/usb_f_uac2.current.ko"
        echo "COPIED_CURRENT_UAC2=$INPUT_DIR/usb_f_uac2.current.ko"
    else
        echo "SD_CURRENT_UAC2=NOT_FOUND"
    fi
fi

section "INSPECCION DEL MODULO UAC2 ACTUAL"
module="$INPUT_DIR/usb_f_uac2.current.ko"
if [[ -s "$module" ]]; then
    file "$module" || true
    sha256sum "$module" || true
    if have modinfo; then
        modinfo "$module" || true
    fi
    readelf -h "$module" || true
    readelf -S "$module" | grep -E '__versions|\.modinfo|__ksymtab|__kcrctab' || true
    strings "$module" | grep -E '^(vermagic=|depends=|name=|r36sx_build=)|R36SX USB AUDIO|4\.4\.186' || true
    if readelf -S "$module" 2>/dev/null | grep -q '__versions'; then
        echo "MODULE_HAS___VERSIONS=YES"
    else
        echo "MODULE_HAS___VERSIONS=NO"
    fi
    echo "MODULE_UNDEFINED_SYMBOLS_BEGIN"
    nm -u "$module" 2>/dev/null | sed 's/^/UNDEF /' || true
    echo "MODULE_UNDEFINED_SYMBOLS_END"
else
    echo "CURRENT_UAC2_MODULE_NOT_AVAILABLE=YES"
fi

section "CLASIFICACION"
missing=0
if ((${#unique_compilers[@]} == 0)); then
    echo "BLOCKER=MIPS_CROSS_COMPILER_NOT_FOUND"
    missing=1
fi
if ((${#unique_kernels[@]} == 0)); then
    echo "BLOCKER=LINUX_4_4_186_MIPS_SOURCE_NOT_FOUND"
    missing=1
else
    ready_candidates=0
    for kdir in "${unique_kernels[@]}"; do
        [[ -s "$kdir/.config" ]] || continue
        if grep -q '^CONFIG_MODVERSIONS=y' "$kdir/.config"; then
            [[ -s "$kdir/Module.symvers" ]] || continue
        fi
        ((ready_candidates+=1))
    done
    echo "KERNEL_READY_CANDIDATE_COUNT=$ready_candidates"
    if (( ready_candidates == 0 )); then
        echo "BLOCKER=NO_KERNEL_TREE_WITH_CONFIG_AND_REQUIRED_SYMVERS"
        missing=1
    fi
fi
if [[ ! -s "$module" ]]; then
    echo "BLOCKER=CURRENT_UAC2_MODULE_NOT_COPIED_FROM_SD"
    missing=1
fi

if (( missing == 0 )); then
    echo "STAGE1_RESULT=READY_FOR_EXACT_SELECTION_AND_BUILD"
else
    echo "STAGE1_RESULT=BLOCKED_SEE_BLOCKERS"
fi

echo "NEXT_INPUT_REQUIRED=$LOG_FILE"
echo "Do not compile or copy modules to the SD until this report is reviewed."
