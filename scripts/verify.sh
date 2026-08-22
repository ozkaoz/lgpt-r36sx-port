#!/usr/bin/env bash
set -Eeuo pipefail
PROJECT_ROOT="${PROJECT_ROOT:-/mnt/d/R36S/PORT LPTRACKER}"
SD="${SD_MOUNT:-/mnt/f}"
BUILD="$PROJECT_ROOT/BUILD/U2523"
CORE_BUILD="$BUILD/lgpt_r36sx_u2523.so"
DAEMON_BUILD="$BUILD/r36s_u2523_usb_audio_io"
SP404_BUILD="$BUILD/r36s_sp404_host_audio_io"
MIDI_BUILD="$BUILD/r36s_midi_host_io"
HOST_AUDIO_BUILD="$PROJECT_ROOT/BUILD/HOST_USB_AUDIO/snd-usb-audio.ko"
HOST_USBMIDI_BUILD="$PROJECT_ROOT/BUILD/HOST_USB_AUDIO/snd-usbmidi-lib.ko"
CORE="$SD/cubegm/cores/lgpt_core.so"
LEGACY_CORE="$SD/cubegm/cores/lgpt_r36sx_port_libretro.so"
LEGACY_CANONICAL_OLD="$SD/cubegm/cores/lgpt_libretro.so"
DAEMON="$SD/lgpt/otg/bin/r36s_u241_usb_audio_io"
SP404="$SD/lgpt/otg/bin/r36s_sp404_host_audio_io"
MIDI="$SD/lgpt/otg/bin/r36s_midi_host_io"
MODULE="$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/usb_f_uac2.ko"
HOST_AUDIO="$SD/lgpt/otg/modules/4.4.186-release/host_usb_audio/snd-usb-audio.ko"
HOST_USBMIDI="$SD/lgpt/otg/modules/4.4.186-release/host_usb_audio/snd-usbmidi-lib.ko"
ERRORS=0
for f in "$CORE_BUILD" "$DAEMON_BUILD" "$CORE" "$DAEMON" "$MODULE" "$SD/cubegm/lgpt"; do [[ -s "$f" ]] || ERRORS=$((ERRORS+1)); done
for m in soundcore.ko snd.ko snd-timer.ko snd-pcm.ko; do
  [[ -s "$SD/lgpt/otg/modules/4.4.186-release/u2_38au8_sync_uac2/$m" ]] || ERRORS=$((ERRORS+1))
done
for marker in U2523_RENAME_CARET_ALIGNMENT_GITHUB_FINAL U2520_PENDING_TAKE_RECORD_EDITOR_PHYSICAL_EDGE_READY R36SX_CAPTURE_ABI=2; do grep -aFq "$marker" "$CORE" || ERRORS=$((ERRORS+1)); done
grep -aFq 'R36SX_USB_AUDIO_DAEMON_ABI=7' "$DAEMON" || ERRORS=$((ERRORS+1))
grep -aFq R36SX_U2414_AU8_SYNC_REPLICA "$MODULE" || ERRORS=$((ERRORS+1))
[[ "$(sha256sum "$CORE_BUILD"|awk '{print $1}')" == "$(sha256sum "$CORE"|awk '{print $1}')" ]] || ERRORS=$((ERRORS+1))
[[ "$(sha256sum "$DAEMON_BUILD"|awk '{print $1}')" == "$(sha256sum "$DAEMON"|awk '{print $1}')" ]] || ERRORS=$((ERRORS+1))
if [[ -s "$SP404_BUILD" && -s "$MIDI_BUILD" && -s "$SP404" && -s "$MIDI" ]]; then
  grep -aFq 'R36SX_SP404_AUDIO_DAEMON_ABI=1' "$SP404" || ERRORS=$((ERRORS+1))
  grep -aFq 'R36SX_MIDI_DAEMON_ABI=1' "$MIDI" || ERRORS=$((ERRORS+1))
else
  echo "WARN: host-side backends not built/installed"
fi
if [[ -s "$HOST_AUDIO_BUILD" && -s "$HOST_USBMIDI_BUILD" && -s "$HOST_AUDIO" && -s "$HOST_USBMIDI" ]]; then
  for pair in "$HOST_AUDIO_BUILD:$HOST_AUDIO" "$HOST_USBMIDI_BUILD:$HOST_USBMIDI"; do
    [[ "$(sha256sum "${pair%%:*}"|awk '{print $1}')" == "$(sha256sum "${pair##*:}"|awk '{print $1}')" ]] || ERRORS=$((ERRORS+1))
  done
else
  echo "WARN: host USB audio modules not built/installed"
fi
[[ "$(cat "$SD/lgpt/otg/audio_usb_profile" 2>/dev/null)" == STEREO_48K ]] || ERRORS=$((ERRORS+1))
grep -aFq 'value="3F5FBF"' "$SD/lgpt/config.xml" || ERRORS=$((ERRORS+1))
grep -aFq 'FF008C' "$SD/lgpt/config.xml" && ERRORS=$((ERRORS+1))
[[ -s "$SD/lgpt/config.stock.xml" ]] || ERRORS=$((ERRORS+1))
[[ -s "$SD/lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36" ]] || ERRORS=$((ERRORS+1))
[[ -x "$SD/lgpt/otg/bin/r36s_aoa_bulk_audio_io_h36" ]] || ERRORS=$((ERRORS+1))
[[ -s "$SD/lgpt/otg/bin/r36s_aoa_bulk_receiver_h36" ]] || ERRORS=$((ERRORS+1))
[[ -x "$SD/lgpt/otg/bin/r36s_aoa_bulk_receiver_h36" ]] || ERRORS=$((ERRORS+1))
[[ -s "$SD/ANDROID/LGPTUsbAudioBridge-H36-debug.apk" ]] || ERRORS=$((ERRORS+1))
[[ -s "$SD/ANDROID/LGPTUsbAudioBridge-H38-debug.apk" ]] || ERRORS=$((ERRORS+1))
[[ -s "$CORE" ]] || ERRORS=$((ERRORS+1))
if [[ -s "$LEGACY_CORE" ]]; then
  [[ "$(sha256sum "$CORE" | awk '{print $1}')" == "$(sha256sum "$LEGACY_CORE" | awk '{print $1}')" ]] || ERRORS=$((ERRORS+1))
fi
if [[ -s "$LEGACY_CANONICAL_OLD" ]]; then
  [[ "$(sha256sum "$CORE" | awk '{print $1}')" == "$(sha256sum "$LEGACY_CANONICAL_OLD" | awk '{print $1}')" ]] || ERRORS=$((ERRORS+1))
fi
grep -q "cubegm/cores/lgpt_core.so" "$SD/frogui/core_overrides.txt" 2>/dev/null || ERRORS=$((ERRORS+1))
[[ -s "$SD/cubegm/lgpt.elf" ]] || echo "WARN: stock lgpt.elf missing (expected on TreeFrog cards)"
echo "ERRORS=$ERRORS"
[[ "$ERRORS" -eq 0 ]]
echo VERIFY_U2523_OK