#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"; DRIVE="${DRIVE%:}"
BUILD_OUT="${2:-/tmp/r36s_u2_38au11z4/U2_38AU11_BUILD_OUT}"
PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SD="/mnt/${DRIVE,,}"
CORE="$BUILD_OUT/lgpt_libretro_au11u_cleanroom_root_diag.so"
DAEMON="$BUILD_OUT/r36s_au11_usb_audio_io"
[ -f "$CORE" ] || { echo "ERROR missing core $CORE"; exit 2; }
[ -x "$DAEMON" ] || { echo "ERROR missing daemon $DAEMON"; exit 3; }
[ -d "$SD" ] || { echo "ERROR SD mount not found: $SD"; exit 4; }

# AU11Z4: no usar cp -a/cp -p hacia SD montada por WSL. En FAT/exFAT/NTFS puede fallar al preservar tiempos.
safe_cp_file() {
  local src="$1" dst="$2"
  mkdir -p "$(dirname "$dst")" 2>/dev/null || true
  cp -f --no-preserve=all "$src" "$dst" 2>/dev/null || cp -f "$src" "$dst"
}
safe_cp_tree() {
  local src="$1" dst="$2"
  rm -rf "$dst" 2>/dev/null || true
  mkdir -p "$(dirname "$dst")" 2>/dev/null || true
  cp -r --no-preserve=all "$src" "$dst" 2>/dev/null || cp -r "$src" "$dst"
}

# AU11Z6: escritura robusta de marcadores de perfil sobre SD montada por WSL.
# En algunas tarjetas/drvfs, la redirección `echo > archivo` puede devolver EINVAL
# sobre nombres residuales. Se evita que el instalador falle por archivos opcionales.
safe_write_text_required() {
  local text="$1" dst="$2" dir tmp
  dir="$(dirname "$dst")"
  mkdir -p "$dir" 2>/dev/null || true
  rm -f "$dst" 2>/dev/null || true
  tmp="$dir/.au11z6_write_$$_$(basename "$dst")"
  if printf '%s
' "$text" > "$tmp" 2>/dev/null && mv -f "$tmp" "$dst" 2>/dev/null; then
    echo "OK_WRITE_MARKER=$dst"
    return 0
  fi
  rm -f "$tmp" 2>/dev/null || true
  if printf '%s
' "$text" > "$dst" 2>/dev/null; then
    echo "OK_WRITE_MARKER_DIRECT=$dst"
    return 0
  fi
  echo "ERROR_WRITE_MARKER=$dst"
  return 1
}
safe_write_text_optional() {
  local text="$1" dst="$2"
  if safe_write_text_required "$text" "$dst"; then
    return 0
  fi
  echo "WARN_OPTIONAL_MARKER_NOT_WRITTEN=$dst"
  echo "WARN_AU11Z6_PROFILE_MARKER_DEVICE_WILL_RECREATE=YES"
  return 0
}

restore_au11_modules_from_backup() {
  echo "AU11Z4_MODULE_RESTORE_CHECK=YES"
  mkdir -p "$SD/lgpt/otg"
  if find "$SD/lgpt/otg/modules" -type f -name 'soundcore.ko' 2>/dev/null | grep -q .; then
    echo "AU11Z4_MODULES_ALREADY_PRESENT=YES"
    return 0
  fi
  latest=""
  while IFS= read -r cand; do
    case "$cand" in
      "$SD/lgpt/otg/modules") continue ;;
    esac
    if find "$cand" -type f -name 'soundcore.ko' 2>/dev/null | grep -q .; then
      latest="$cand"
      break
    fi
  done < <(find "$SD" -path '*/lgpt/otg/modules' -type d 2>/dev/null | sort -r)
  if [ -n "$latest" ]; then
    echo "AU11Z4_RESTORE_MODULES_FROM=$latest"
    rm -rf "$SD/lgpt/otg/modules" 2>/dev/null || true
    mkdir -p "$SD/lgpt/otg"
    safe_cp_tree "$latest" "$SD/lgpt/otg/modules"
    find "$SD/lgpt/otg/modules" -type f -name '*.ko' | wc -l | sed 's/^/AU11Z4_RESTORED_KO_COUNT=/'
    return 0
  fi
  echo "WARN_AU11Z4_NO_MODULE_BACKUP_FOUND=YES"
  echo "WARN_AU11Z4_USB_AUDIO_WILL_NOT_ENUMERATE_WITHOUT_MODULES=YES"
  return 0
}
restore_au11_modules_from_backup
mkdir -p "$SD/lgpt/otg/bin" "$SD/lgpt/otg/logs" "$SD/lgpt/otg/backups" "$SD/cubegm/cores" "$SD/roms/lgpt"
TS="$(date +%Y%m%d_%H%M%S)"
for p in "$SD/cubegm/cores/lgpt_libretro.so" "$SD/cubegm/lgpt_libretro.so"; do
  if [ -f "$p" ]; then
    safe_cp_file "$p" "$SD/lgpt/otg/backups/$(basename "$p").pre_AU11Z4_$TS" || echo "WARN backup failed: $p"
  fi
done
# Hard-clean stale AU9/AU10 scripts that can hijack setup/mode/capture.
rm -f "$SD/lgpt/otg/bin"/r36s_au10*_usb_audio_io 2>/dev/null || true
rm -f "$SD/lgpt/otg/bin"/r36s_au9*_fifo_to_uac2 2>/dev/null || true
rm -f "$SD/lgpt/otg/bin"/otg_38au10*.sh 2>/dev/null || true
rm -f "$SD/lgpt/otg/bin"/r36s_*_volume_watch 2>/dev/null || true
rm -f "$SD/lgpt/otg"/au10* "$SD/lgpt/otg"/au9* 2>/dev/null || true
rm -f "$SD/lgpt/otg/logs"/u2_38au10*.log "$SD/lgpt/otg/logs"/u2_38au9*.log 2>/dev/null || true
rm -f "$SD/lgpt/otg/U2_38AU10"*"INSTALL_INFO.txt" "$SD/lgpt/otg/U2_38AU10"*"SHA256_INSTALLED.txt" 2>/dev/null || true
rm -rf "$SD/lgpt/otg/logs/runtime_state" 2>/dev/null || true
: > "$SD/lgpt/uac2_bridge_lgpt.log" 2>/dev/null || true
rm -f "$SD/lgpt/otg/logs"/u2_38au11_*.log 2>/dev/null || true

safe_cp_file "$CORE" "$SD/cubegm/cores/lgpt_libretro.so"
safe_cp_file "$CORE" "$SD/cubegm/lgpt_libretro.so"
safe_cp_file "$DAEMON" "$SD/lgpt/otg/bin/r36s_au11_usb_audio_io"
safe_cp_file "$PKG_DIR/device/otg_38au11_common.sh" "$SD/lgpt/otg/bin/otg_38au11_common.sh"
safe_cp_file "$PKG_DIR/device/otg_38au11_mode_manager.sh" "$SD/lgpt/otg/bin/otg_38au11_mode_manager.sh"
safe_cp_file "$PKG_DIR/device/otg_38au11_apply_profile_once.sh" "$SD/lgpt/otg/bin/otg_38au11_apply_profile_once.sh"
safe_cp_file "$PKG_DIR/device/otg_38au11_lgpt_sync_setup_once.sh" "$SD/lgpt/otg/bin/otg_38au11_lgpt_sync_setup_once.sh"
chmod +x "$SD/lgpt/otg/bin/r36s_au11_usb_audio_io" "$SD/lgpt/otg/bin/otg_38au11_"*.sh 2>/dev/null || true
safe_write_text_required "" "$SD/lgpt/otg/enable_lgpt_uac2_bridge"
safe_write_text_required "USB_IN_OUT" "$SD/lgpt/otg/audio_driver_mode"
safe_write_text_required "DUPLEX_STABLE_ALWAYS_OPEN_AU11Z6_AU10Y_DESCRIPTOR" "$SD/lgpt/otg/au11_usb_policy"
safe_write_text_optional "duplex_stable_always_open" "$SD/lgpt/otg/au11_active_usb_profile"
rm -f "$SD/lgpt/otg/disable_mute_local" "$SD/lgpt/otg/lowlat_240" || true
cat > "$SD/lgpt/otg/U2_38AU11_INSTALL_INFO.txt" <<INFO
U2_38AU11Z6_CAMILO_INSTALLED=YES
U2_38AU11U_STABLE_BASE=YES
AU10Y_DESCRIPTOR_REUSED=YES
CAMILO_PENA_CHAT_TITLE=desarrollo_LGPT_CAMILO_PENA
PRETEST_CLEAN_WINDOWS_SD_WSL=YES
COPY_POLICY=NO_PRESERVE_TIMES_MODES_OWNERSHIP_FOR_WSL_SD_MARKERWRITEFIX
DATE=$(date --iso-8601=seconds)
CORE_DEST_1=/cubegm/cores/lgpt_libretro.so
CORE_DEST_2=/cubegm/lgpt_libretro.so
DAEMON=/lgpt/otg/bin/r36s_au11_usb_audio_io
SETUP=/lgpt/otg/bin/otg_38au11_lgpt_sync_setup_once.sh
APPLY=/lgpt/otg/bin/otg_38au11_apply_profile_once.sh
MODE_FILE=/lgpt/otg/audio_driver_mode
DEFAULT_MODE=USB_IN_OUT
VISIBLE_AUDIO_DRIVER_MODES=CONSOLE_AUDIO,USB_IN_OUT,EXTERNAL_RECORDING
FULL_DUPLEX=REMOVED_FROM_UI_ALIAS_TO_USB_OUT_AUTO_MUTE
USB_INPUT_CAPTURE=REMOVED_FROM_AUDIO_DRIVER_MENU_USE_USB_C_RECORD_SCPI_R
SCPI_R=Instrument_R1_RIGHT
CHOPPER_L2_A_USB_REC=DISABLED_USE_INSTRUMENT_R1_RIGHT
FIRST_MODAL_COLD_SAFE=YES
FIRST_MODAL_STOP_ONLY_BARRIER=YES
FIRST_MODAL_TRAMPOLINE_NO_SAMPLEPOOL=YES
FIRST_SAMPLE_A_BROAD_PRE_FIELDVIEW_GATE=YES
WINDOWS_COMPANION_HELPER_OPTIONAL=YES
USB_HOST_PLAYBACK_ALWAYS_DRAIN=YES
USB_RECORD_EXIT_MONITOR_OFF=YES
FIRST_SAMPLE_A_ARM_ONLY=YES
USB_OUT_RECOVERY_AFTER_MONITOR_OFF=NO_KEEP_ENDPOINTS_OPEN
USB_PROFILE_POLICY=DUPLEX_STABLE_ALWAYS_OPEN_AU11Z6_AU10Y_DESCRIPTOR
NORMAL_PROFILE=duplex_stable_always_open p_chmask=1 c_chmask=1 monitor=0 output_allowed=1 input_drained_to_null=1 playback_endpoint_kept_open=1
USB_C_RECORD_PROFILE=duplex_stable_always_open p_chmask=1 c_chmask=1 monitor=1 output_silence_to_windows_input=1 input_monitor_open=1
WINDOWS_ENDPOINTS=playback_and_recording_visible_all_time
ALLOW_INSTRUMENT_VIEW_WHILE_PLAYING=YES
FIRST_MODAL_TRAMPOLINE=YES
FIRST_MODAL_TRAMPOLINE_NO_SAMPLEPOOL=YES
SHORTCUT_ENTER=Instrument_R1_RIGHT
SHORTCUT_EXIT=USB_REC_R1_LEFT
CAPTURE_RUNTIME=/tmp/r36sx_lgpt_usb
PCM_PLAY=/dev/snd/pcmC0D0p
PCM_CAPTURE=/dev/snd/pcmC0D0c
FIFO=/tmp/r36sx_uac2_bridge_fifo
LOG=/mnt/sdcard/lgpt/uac2_bridge_lgpt.log
BACKUP_TS=$TS
MODULE_RESTORE=BACKUP_AWARE_OR_PACKAGE_CACHE
CLEANROOM_PREINSTALL=AU11Z4_CAMILO_REQUIRED
INFO
strings -a "$SD/cubegm/cores/lgpt_libretro.so" | grep -E 'U2_38AU11|AU11_|AU11U_|SCPI-R|USB-C RECORD|LOCAL_ONLY|USB_OUT_AUTO_MUTE|FULL_DUPLEX' > "$SD/lgpt/otg/U2_38AU11_CORE_STRINGS_INSTALLED.txt" || true
sha256sum "$SD/cubegm/cores/lgpt_libretro.so" "$SD/cubegm/lgpt_libretro.so" "$SD/lgpt/otg/bin/r36s_au11_usb_audio_io" > "$SD/lgpt/otg/U2_38AU11_SHA256_INSTALLED.txt"
echo "INSTALLED_CORE_SIZE=$(wc -c < "$SD/cubegm/cores/lgpt_libretro.so")"
echo "INSTALLED_DAEMON_SIZE=$(wc -c < "$SD/lgpt/otg/bin/r36s_au11_usb_audio_io")"
echo "SUMMARY=PASS_AU11Z6_SD_INSTALL_COPYFIX_MARKERWRITEFIX"
