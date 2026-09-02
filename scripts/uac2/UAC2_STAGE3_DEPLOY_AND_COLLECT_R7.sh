#!/usr/bin/env bash
set -Eeuo pipefail

# LGPT/R36SX UAC2 Stage 3 R7
# Controlled deployment, collection and rollback.
# Fixes:
#   - paths containing spaces
#   - EXIT trap calling exit recursively
#   - automatic recovery of the R5 result directory from the full-source ZIP
#
# Usage:
#   bash UAC2_STAGE3_DEPLOY_AND_COLLECT_R7.sh deploy
#   bash UAC2_STAGE3_DEPLOY_AND_COLLECT_R7.sh collect
#   bash UAC2_STAGE3_DEPLOY_AND_COLLECT_R7.sh restore

PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/Toolchains/R36SX}"
SD_DRIVE="${SD_DRIVE:-F:}"
SD_MOUNT="${SD_MOUNT:-/mnt/f}"

BUILD_ROOT="$PROJECT_ROOT/BUILD"
LOG_ROOT="$PROJECT_ROOT/LOGS"
BACKUP_ROOT="$PROJECT_ROOT/BACKUPS"

KVER="4.4.186-release"
PROFILE="u2_38au8_sync_uac2"
TARGET_REL="lgpt/otg/modules/$KVER/$PROFILE"
EXPECTED_VERMAGIC="4.4.186-release preempt MIPS32_R2 32BIT"

declare -A EXPECTED_SHA256=(
  [soundcore.ko]="5cd5d4dbdb0ce7379c64611d035bf3643d9f6d3097c046bb49214b2f065d5f39"
  [snd.ko]="91742747579d9a6e8ca0fff0e920eed69afdd9f2fcab57029e351ea13c5f95bb"
  [snd-timer.ko]="25cb2142f7bea8f92edfc03d28c3a1e82cc656a1aa09ec5a5e1a99be41c87920"
  [snd-pcm.ko]="48f7d39e97aafb6f61c2ff2f8c9a2a101115875924e22d986f8fc8485f4a2704"
  [usb_f_uac2.ko]="e9062ac5a37aa7706c98a3411a57dbe67bc98ef82621c430c1d4e7a89b2fbefe"
)

MODULES=(soundcore.ko snd.ko snd-timer.ko snd-pcm.ko usb_f_uac2.ko)
MOUNTED_BY_SCRIPT=0
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
ACTION="${1:-invalid}"

mkdir -p "$LOG_ROOT" "$BACKUP_ROOT" "$BUILD_ROOT"
LOG="$LOG_ROOT/UAC2_STAGE3_R7_${ACTION}_${TIMESTAMP}.log"
exec > >(tee -a "$LOG") 2>&1

cleanup() {
  local rc=$?
  trap - EXIT INT TERM
  set +e

  sync
  if (( MOUNTED_BY_SCRIPT == 1 )) && mountpoint -q "$SD_MOUNT"; then
    cd /
    sudo umount "$SD_MOUNT"
    local umount_rc=$?
    if (( umount_rc == 0 )); then
      echo "SD_UNMOUNT_ON_EXIT=$SD_MOUNT"
    else
      echo "WARN_SD_UNMOUNT_ON_EXIT_RC=$umount_rc mount=$SD_MOUNT"
    fi
  fi

  echo "SCRIPT_EXIT_CODE=$rc"
  echo "BUILD_LOG=$LOG"
  return "$rc"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

die() {
  echo "ERROR=$*" >&2
  return 1
}

need_tool() {
  command -v "$1" >/dev/null 2>&1 || die "required tool missing: $1"
}

mount_sd() {
  sudo mkdir -p "$SD_MOUNT"

  if mountpoint -q "$SD_MOUNT"; then
    echo "SD_ALREADY_MOUNTED=$SD_MOUNT"
  else
    echo "SD_MOUNT_REQUEST=$SD_DRIVE->$SD_MOUNT"
    sudo mount -t drvfs "$SD_DRIVE" "$SD_MOUNT"
    MOUNTED_BY_SCRIPT=1
  fi

  mountpoint -q "$SD_MOUNT" || die "SD mount failed"
  [[ -d "$SD_MOUNT/lgpt" ]] ||
    die "$SD_DRIVE does not look like the LGPT SD: missing /lgpt"

  echo "SD_MOUNT_OK=$(findmnt -n -o SOURCE,FSTYPE,TARGET "$SD_MOUNT" 2>/dev/null || true)"
}

unmount_sd_now() {
  sync

  if (( MOUNTED_BY_SCRIPT == 1 )); then
    cd /
    sudo umount "$SD_MOUNT"
    MOUNTED_BY_SCRIPT=0
    echo "SD_UNMOUNT_OK=$SD_MOUNT"
  else
    echo "SD_LEFT_MOUNTED=YES reason=was_already_mounted_before_script"
  fi
}

sha_of() {
  sha256sum "$1" | awk '{print $1}'
}

verify_module() {
  local path="$1"
  local name expected got vm

  name="$(basename "$path")"
  [[ -f "$path" ]] || die "module missing: $path"

  expected="${EXPECTED_SHA256[$name]:-}"
  [[ -n "$expected" ]] || die "no expected checksum registered for $name"

  got="$(sha_of "$path")"
  echo "VERIFY_SHA256[$name]=$got"
  [[ "$got" == "$expected" ]] || die "checksum mismatch for $name"

  file "$path"
  file "$path" | grep -qE 'ELF 32-bit LSB relocatable, MIPS' ||
    die "wrong architecture for $name"

  vm="$(modinfo -F vermagic "$path" 2>/dev/null | sed 's/[[:space:]]*$//')"
  echo "VERIFY_VERMAGIC[$name]=$vm"
  [[ "$vm" == "$EXPECTED_VERMAGIC" ]] ||
    die "vermagic mismatch for $name"
}

latest_matching_path() {
  local root="$1"
  local type="$2"
  local pattern="$3"
  local candidate latest=""

  while IFS= read -r -d '' candidate; do
    if [[ -z "$latest" || "$candidate" -nt "$latest" ]]; then
      latest="$candidate"
    fi
  done < <(find "$root" -maxdepth 1 -mindepth 1 -type "$type" -name "$pattern" -print0 2>/dev/null)

  printf '%s\n' "$latest"
}

recover_r5_from_zip() {
  local zip="$1"
  local recovered tmp source_profile

  recovered="$BUILD_ROOT/UAC2_STAGE2_ALSA_R5_RECOVERED_${TIMESTAMP}"
  tmp="$HOME/lgpt-r36sx-kernel-work/r5-recovery-${TIMESTAMP}"

  echo "R5_RECOVERY_ZIP=$zip"
  echo "R5_RECOVERY_TEMP=$tmp"
  echo "R5_RECOVERY_DEST=$recovered"

  rm -rf "$tmp" "$recovered"
  mkdir -p "$tmp" "$recovered/$TARGET_REL"

  unzip -q "$zip" -d "$tmp"

  source_profile="$(
    find "$tmp" -type d -path "*/$TARGET_REL" -print -quit 2>/dev/null
  )"

  [[ -n "$source_profile" ]] ||
    die "R5 ZIP does not contain $TARGET_REL"

  cp -a "$source_profile/." "$recovered/$TARGET_REL/"
  printf '%s\n' "$recovered"
}

find_or_recover_r5_result() {
  local result zip

  result="$(latest_matching_path "$BUILD_ROOT" d 'UAC2_STAGE2_ALSA_R5_*')"
  if [[ -n "$result" && -d "$result/$TARGET_REL" ]]; then
    printf '%s\n' "$result"
    return 0
  fi

  echo "R5_RESULT_DIRECTORY_NOT_FOUND=YES" >&2

  zip="$(latest_matching_path "$BUILD_ROOT" f 'LGPT_R36SX_UAC2_ALSA_STAGE2_R5_*_FULL_SOURCE.zip')"
  [[ -n "$zip" ]] ||
    die "no valid R5 directory or full-source ZIP found under $BUILD_ROOT"

  recover_r5_from_zip "$zip"
}

find_latest_backup() {
  latest_matching_path "$BACKUP_ROOT" d 'UAC2_PRE_R7_*'
}

deploy() {
  local result source_dir target_dir backup_dir tmp m

  echo "============================================================"
  echo "R7 CONTROLLED DEPLOY"
  echo "============================================================"
  echo "DATE=$(date -Iseconds)"
  echo "PROJECT_ROOT=$PROJECT_ROOT"
  echo "SD_DRIVE=$SD_DRIVE"
  echo "SD_MOUNT=$SD_MOUNT"

  result="$(find_or_recover_r5_result)"
  [[ -n "$result" ]] || die "unable to locate or recover R5 result"

  source_dir="$result/$TARGET_REL"
  target_dir="$SD_MOUNT/$TARGET_REL"
  backup_dir="$BACKUP_ROOT/UAC2_PRE_R7_${TIMESTAMP}"

  echo "R5_RESULT_DIR=$result"
  echo "SOURCE_DIR=$source_dir"

  for m in "${MODULES[@]}"; do
    verify_module "$source_dir/$m"
  done

  mount_sd

  mkdir -p "$backup_dir"
  if [[ -d "$target_dir" ]]; then
    printf '%s\n' 1 >"$backup_dir/TARGET_EXISTED"
    cp -a "$target_dir" "$backup_dir/"
    echo "PC_BACKUP_CREATED=$backup_dir/$PROFILE"
  else
    printf '%s\n' 0 >"$backup_dir/TARGET_EXISTED"
    echo "PC_BACKUP_NOTE=target profile did not previously exist"
  fi

  mkdir -p "$target_dir"

  for m in "${MODULES[@]}"; do
    tmp="$target_dir/.${m}.R7.new"
    cp -f "$source_dir/$m" "$tmp"
    sync
    mv -f "$tmp" "$target_dir/$m"
    echo "DEPLOYED=$target_dir/$m"
  done

  cat >"$target_dir/R7_ALSA_UAC2_DEPLOYMENT.txt" <<EOF
PROFILE=$PROFILE
DEPLOYED_AT=$(date -Iseconds)
SOURCE_RESULT=$result
EXPECTED_VERMAGIC=$EXPECTED_VERMAGIC
soundcore.ko=${EXPECTED_SHA256[soundcore.ko]}
snd.ko=${EXPECTED_SHA256[snd.ko]}
snd-timer.ko=${EXPECTED_SHA256[snd-timer.ko]}
snd-pcm.ko=${EXPECTED_SHA256[snd-pcm.ko]}
usb_f_uac2.ko=${EXPECTED_SHA256[usb_f_uac2.ko]}
EOF

  sync

  for m in "${MODULES[@]}"; do
    verify_module "$target_dir/$m"
  done

  echo "DEPLOY_RESULT=R7_MODULES_COPIED_AND_VERIFIED"
  echo "PC_BACKUP_DIR=$backup_dir"
  echo "DEVICE_TEST_REQUIRED=YES"

  unmount_sd_now
}

collect() {
  local dest target_dir m

  echo "============================================================"
  echo "R7 LOG COLLECTION"
  echo "============================================================"
  echo "DATE=$(date -Iseconds)"

  mount_sd

  dest="$LOG_ROOT/R7_DEVICE_LOGS_${TIMESTAMP}"
  mkdir -p "$dest"

  if [[ -d "$SD_MOUNT/LGPT_OTG_LOGS" ]]; then
    mkdir -p "$dest/LGPT_OTG_LOGS"
    cp -a "$SD_MOUNT/LGPT_OTG_LOGS/." "$dest/LGPT_OTG_LOGS/"
    echo "COLLECTED=$SD_MOUNT/LGPT_OTG_LOGS"
  else
    echo "WARN_MISSING=$SD_MOUNT/LGPT_OTG_LOGS"
  fi

  if [[ -d "$SD_MOUNT/lgpt/otg/logs" ]]; then
    mkdir -p "$dest/lgpt_otg_logs"
    cp -a "$SD_MOUNT/lgpt/otg/logs/." "$dest/lgpt_otg_logs/"
    echo "COLLECTED=$SD_MOUNT/lgpt/otg/logs"
  else
    echo "WARN_MISSING=$SD_MOUNT/lgpt/otg/logs"
  fi

  target_dir="$SD_MOUNT/$TARGET_REL"
  {
    echo "DATE=$(date -Iseconds)"
    echo "TARGET_DIR=$target_dir"

    for m in "${MODULES[@]}"; do
      if [[ -f "$target_dir/$m" ]]; then
        sha256sum "$target_dir/$m"
        modinfo "$target_dir/$m" 2>/dev/null || true
        echo "---"
      else
        echo "MISSING=$target_dir/$m"
      fi
    done
  } >"$dest/R7_SD_MODULE_SNAPSHOT.txt"

  tar -C "$LOG_ROOT" -czf "$dest.tar.gz" "$(basename "$dest")"
  sha256sum "$dest.tar.gz" >"$dest.tar.gz.sha256"

  echo "COLLECT_RESULT=OK"
  echo "COLLECT_DIR=$dest"
  echo "COLLECT_ARCHIVE=$dest.tar.gz"
  echo "COLLECT_ARCHIVE_SHA256=$(cat "$dest.tar.gz.sha256")"

  unmount_sd_now
}

restore() {
  local backup_dir existed target_dir

  echo "============================================================"
  echo "R7 RESTORE PREVIOUS PROFILE"
  echo "============================================================"
  echo "DATE=$(date -Iseconds)"

  backup_dir="$(find_latest_backup)"
  [[ -n "$backup_dir" ]] ||
    die "no UAC2_PRE_R7 backup found under $BACKUP_ROOT"

  [[ -f "$backup_dir/TARGET_EXISTED" ]] ||
    die "backup metadata missing: $backup_dir/TARGET_EXISTED"

  existed="$(cat "$backup_dir/TARGET_EXISTED")"
  [[ "$existed" == "0" || "$existed" == "1" ]] ||
    die "invalid TARGET_EXISTED value"

  echo "RESTORE_BACKUP_DIR=$backup_dir"
  echo "RESTORE_TARGET_EXISTED=$existed"

  mount_sd
  target_dir="$SD_MOUNT/$TARGET_REL"

  rm -rf "$target_dir"

  if [[ "$existed" == "1" ]]; then
    [[ -d "$backup_dir/$PROFILE" ]] ||
      die "backup profile missing: $backup_dir/$PROFILE"

    mkdir -p "$(dirname "$target_dir")"
    cp -a "$backup_dir/$PROFILE" "$target_dir"
    echo "RESTORED=$target_dir"
  else
    echo "RESTORED_STATE=target profile removed because it did not exist before R7"
  fi

  sync
  echo "RESTORE_RESULT=OK"
  unmount_sd_now
}

main() {
  local tool

  for tool in sudo mountpoint findmnt sha256sum file modinfo find tar unzip; do
    need_tool "$tool"
  done

  case "$ACTION" in
    deploy) deploy ;;
    collect) collect ;;
    restore) restore ;;
    *)
      echo "Usage:"
      echo "  bash $(basename "$0") deploy"
      echo "  bash $(basename "$0") collect"
      echo "  bash $(basename "$0") restore"
      return 2
      ;;
  esac
}

main
