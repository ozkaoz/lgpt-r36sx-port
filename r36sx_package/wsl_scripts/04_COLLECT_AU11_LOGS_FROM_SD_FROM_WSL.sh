#!/usr/bin/env bash
set -euo pipefail
DRIVE="${1:-F}"; DRIVE="${DRIVE%:}"
OUTZIP="${2:-/mnt/d/R36S/PORT LPTRACKER/U2_38AU11_TEST_LOGS_$(date +%Y%m%d_%H%M%S).zip}"
SD="/mnt/${DRIVE,,}"
[ -d "$SD" ] || { echo "ERROR SD mount not found: $SD"; exit 4; }
TMP="$(mktemp -d /tmp/au11_logs_XXXXXX)"
mkdir -p "$TMP/logs" "$TMP/state" "$TMP/samples_probe"
cp -a "$SD/lgpt/otg/logs"/* "$TMP/logs/" 2>/dev/null || true
cp -a "$SD/lgpt/uac2_bridge_lgpt.log" "$TMP/uac2_bridge_lgpt.log" 2>/dev/null || true
cp -a "$SD/lgpt/otg/U2_38AU11_INSTALL_INFO.txt" "$TMP/state/" 2>/dev/null || true
cp -a "$SD/lgpt/otg/U2_38AU11_SHA256_INSTALLED.txt" "$TMP/state/" 2>/dev/null || true
cp -a "$SD/lgpt/otg/U2_38AU11_CORE_STRINGS_INSTALLED.txt" "$TMP/state/" 2>/dev/null || true
cp -a "$SD/lgpt/otg/audio_driver_mode" "$TMP/audio_driver_mode" 2>/dev/null || true
cp -a "$SD/lgpt/otg/au11_usb_policy" "$TMP/state/" 2>/dev/null || true
cp -a "$SD/lgpt/otg/au11_active_usb_profile" "$TMP/state/" 2>/dev/null || true
cp -a "$SD/lgpt/otg/au11_keep_duplex_experimental" "$TMP/state/" 2>/dev/null || true
for f in usb_capture_status usb_capture_level usb_capture_level_l usb_capture_level_r usb_capture_elapsed usb_capture_monitor usb_capture_cmd usb_capture_last_name usb_capture_last_path; do
  cp -a "$SD/lgpt/otg/logs/runtime_state/$f" "$TMP/state/$f" 2>/dev/null || true
done
find "$SD/lgpt/otg/bin" -maxdepth 1 -type f -printf '%f %s bytes\n' | sort > "$TMP/state/otg_bin_ls.txt" 2>/dev/null || true
find "$SD/lgpt/otg/bin" -maxdepth 1 \( -name 'otg_38au10*.sh' -o -name 'r36s_au10*_usb_audio_io' -o -name 'r36s_au9*_fifo_to_uac2' \) -printf '%f\n' | sort > "$TMP/state/stale_au9_au10_files.txt" 2>/dev/null || true
strings -a "$SD/cubegm/cores/lgpt_libretro.so" | grep -E 'U2_38AU11|AU11_|AU11U_|SCPI-R|USB-C RECORD|LOCAL_ONLY|USB_OUT_AUTO_MUTE|FULL_DUPLEX|L2\+A USB REC' > "$TMP/state/core_strings_probe.txt" 2>/dev/null || true
find "$SD/roms/lgpt" "$SD/lgpt" -type f -iname '*usb*rec*.wav' -o -iname 'Sample_*.wav' 2>/dev/null | sort > "$TMP/samples_probe/usbrecs_all_lgpt.txt" || true

# AU11Y extended probes
find "$SD/lgpt/otg/modules" -type f -name '*.ko' -printf '%P %s bytes\n' 2>/dev/null | sort > "$TMP/state/modules_ko_list.txt" || true
find "$SD" -maxdepth 2 -type d -name '_AU11Y_CLEANROOM_BACKUP_*' -printf '%f\n' 2>/dev/null | sort > "$TMP/state/au11y_cleanroom_backups.txt" || true
find "$SD" -maxdepth 2 -type d -name '_AU11Z_CLEANROOM_BACKUP_*' -printf '%f\n' 2>/dev/null | sort > "$TMP/state/au11z_cleanroom_backups.txt" || true
cp -a "$SD/lgpt/otg/AU11Z_CAMILO_CLEANROOM_PREINSTALL.txt" "$TMP/state/" 2>/dev/null || true
latest_backup="$(find "$SD" -maxdepth 1 -type d -name '_AU11Y_CLEANROOM_BACKUP_*' 2>/dev/null | sort | tail -1 || true)"
if [ -n "$latest_backup" ] && [ -f "$latest_backup/cleanroom_report.txt" ]; then
  cp -a "$latest_backup/cleanroom_report.txt" "$TMP/state/latest_cleanroom_report.txt" 2>/dev/null || true
fi
latest_backup_z="$(find "$SD" -maxdepth 1 -type d -name '_AU11Z_CLEANROOM_BACKUP_*' 2>/dev/null | sort | tail -1 || true)"
if [ -n "$latest_backup_z" ] && [ -f "$latest_backup_z/cleanroom_report.txt" ]; then
  cp -a "$latest_backup_z/cleanroom_report.txt" "$TMP/state/latest_cleanroom_report_au11z.txt" 2>/dev/null || true
fi
find "$SD/lgpt" "$SD/roms" -type f -iname '*.wav' -printf '%p %s bytes\n' 2>/dev/null | sort > "$TMP/samples_probe/wav_files_with_sizes.txt" || true
find "$SD/lgpt" "$SD/roms" -type f -iname '*.wav' -size -65c -printf '%p %s bytes\n' 2>/dev/null | sort > "$TMP/samples_probe/invalid_small_wavs_remaining.txt" || true
find "$SD/lgpt" "$SD/roms" -maxdepth 5 -type f \( -iname '*retroarch*.log' -o -iname '*treefrog*.log' -o -iname 'lgpt*.log' -o -iname '*uac*.log' \) -printf '%p %s bytes\n' 2>/dev/null | sort > "$TMP/state/possible_runtime_logs_index.txt" || true

(
  cd "$TMP"
  zip -qr "$OUTZIP" .
)
echo "OUTZIP=$OUTZIP"
echo "SUMMARY=PASS_AU11_LOG_COLLECTION"
