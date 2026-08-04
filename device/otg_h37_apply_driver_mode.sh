#!/bin/sh
# H38.2 backend selector (ABI7). H35 runtime mailbox names are retained for
# exact frontend compatibility. It may only be invoked by the H35 supervisor
# while the core is unloaded. WINDOWS uses the GitHub ABI7 payload
# (otg_u241_setup_once.sh + r36s_u241_usb_audio_io), ANDROID uses the h36 AOA
# supervisor, LOCAL_CONSOLE tears everything down.
set -u
ROOT="${LGPT_SD_ROOT:-/mnt/sdcard}"
BASE="$ROOT/lgpt/otg"
BIN="$BASE/bin"
LOGROOT="${LGPT_LOGROOT:-/tmp/r36sx_lgpt_logs}"
RUNTIME="${LGPT_RUNTIME_DIR:-/tmp/r36sx_lgpt_usb}"
LOCK="${LGPT_H35_APPLY_LOCK:-/tmp/r36sx_h35_audio_mode.lock}"
LOG="$LOGROOT/H35_AUDIO_DRIVER_MODE.log"
STATUS="$RUNTIME/h35_mode_apply_status"
MODE_RAW="${1:-LOCAL_CONSOLE}"
mkdir -p "$BASE" "$BIN" "$LOGROOT" "$RUNTIME" 2>/dev/null || true
normalize_mode(){ case "$1" in
  ANDROID|ANDROID_OTG|ANDROID_AOA) echo ANDROID;;
  USB_IN|USB_IN_OTG) echo ANDROID;;
  USB_OUT|USB_OUT_OTG|SP404|SP404_OTG) echo USB_OUT;;
  MIDI|MIDI_OTG) echo MIDI;;
  WINDOWS|WINDOWS_OTG|USB_DUPLEX|USB_DUPLEX_OTG|USB_IN_OUT|FULL_DUPLEX) echo WINDOWS;;
  *) echo LOCAL_CONSOLE;; esac; }
policy_for_mode(){ case "$1" in ANDROID) echo USB_IN_OTG;; WINDOWS) echo USB_DUPLEX_OTG;; USB_OUT) echo USB_OUT_OTG;; MIDI) echo MIDI_OTG;; *) echo LOCAL_CONSOLE;; esac; }
atomic_write(){ p="$1"; v="$2"; d="$(dirname "$p")"; mkdir -p "$d" 2>/dev/null || true; t="${p}.h35tmp.$$"; rm -f "$t" 2>/dev/null || true; printf '%s\n' "$v" >"$t" 2>/dev/null && mv -f "$t" "$p" 2>/dev/null; }
log(){ printf '%s H35 mode=%s supervisor=%s %s\n' "$(date 2>/dev/null || echo no-date)" "$MODE" "${LGPT_H35_SUPERVISOR_PID:-none}" "$*" >>"$LOG" 2>/dev/null || true; }
pid_alive(){ p="$1"; [ -n "$p" ] && [ "$p" != "0" ] && kill -0 "$p" 2>/dev/null; }
terminate_pid(){ p="$1"; [ -n "$p" ] && [ "$p" != "0" ] || return 0; pid_alive "$p" || return 0; kill "$p" 2>/dev/null || true; n=0; while pid_alive "$p" && [ "$n" -lt 20 ]; do sleep 0.05; n=$((n+1)); done; pid_alive "$p" && kill -9 "$p" 2>/dev/null || true; }
kill_name(){ name="$1"; for p in $(pidof "$name" 2>/dev/null || true); do terminate_pid "$p"; done; }
stop_android_runtime(){
  sp="$(cat "$RUNTIME/h35_android_supervisor_pid" 2>/dev/null || true)"; terminate_pid "$sp"
  kill_name r36s_aoa_bulk_receiver_h36; kill_name r36s_aoa_bulk_audio_io_h36
  kill_name r36s_aoa_bulk_receiver_h35; kill_name r36s_aoa_bulk_audio_io_h35
  kill_name r36s_aoa_bulk_receiver_h16; kill_name r36s_aoa_bulk_audio_io_h16
  rm -f "$RUNTIME/h35_android_supervisor_pid" "$RUNTIME/h35_android_receiver_pid" \
    "$RUNTIME/aoa_host_configured" "$RUNTIME/aoa_bulk_accessory_present" \
    "$RUNTIME/aoa_bulk_stream_ready" "$RUNTIME/aoa_state" "$RUNTIME/aoa_result" \
    "$RUNTIME/daemon_pid" /tmp/r36sx_aoa_bulk_pcm_fifo /tmp/r36sx_android_project_fifo 2>/dev/null || true
}
stop_windows_runtime(){
  if [ -r "$BIN/otg_u241_shutdown.sh" ]; then /bin/sh "$BIN/otg_u241_shutdown.sh" >>"$LOG" 2>&1 || true; fi
  kill_name r36s_u241_usb_audio_io; kill_name r36s_u240_usb_audio_io; kill_name r36s_au11_usb_audio_io
  kill_name r36s_h37_usb_audio_io
}
android_runtime_ready(){
  sp="$(cat "$RUNTIME/h35_android_supervisor_pid" 2>/dev/null || true)"
  dp="$(cat "$RUNTIME/daemon_pid" 2>/dev/null || true)"
  pid_alive "$sp" && pid_alive "$dp" && \
    grep -q 'R36SX_AOA_BULK_AUDIO_DAEMON_ABI=4' "$RUNTIME/daemon_version" 2>/dev/null && \
    grep -q 'R36SX_CAPTURE_ABI=4' "$RUNTIME/capture_abi" 2>/dev/null
}
MODE="$(normalize_mode "$MODE_RAW")"; POLICY="$(policy_for_mode "$MODE")"
PREVIOUS_POLICY="$(cat "$RUNTIME/audio_driver_policy" 2>/dev/null || echo LOCAL_CONSOLE)"
if [ "${LGPT_H35_SUPERVISOR_APPLY:-0}" != 1 ] || [ -z "${LGPT_H35_SUPERVISOR_PID:-}" ]; then
  atomic_write "$STATUS" "REFUSED reason=SUPERVISOR_ONLY mode=$MODE" || true
  log "MODE_APPLY_REFUSED supervisor_only=1"
  exit 45
fi
if ! kill -0 "$LGPT_H35_SUPERVISOR_PID" 2>/dev/null; then
  atomic_write "$STATUS" "REFUSED reason=SUPERVISOR_PID_NOT_ALIVE mode=$MODE" || true
  log "MODE_APPLY_REFUSED supervisor_pid_not_alive=1"
  exit 46
fi
if ! mkdir "$LOCK" 2>/dev/null; then atomic_write "$STATUS" "BUSY mode=$MODE" || true; log MODE_APPLY_BUSY; exit 40; fi
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT INT TERM
atomic_write "$RUNTIME/audio_driver_mode" "$MODE" || true
atomic_write "$RUNTIME/audio_driver_policy" "$POLICY" || true
echo "$MODE" > "$BASE/audio_driver_mode" 2>/dev/null || true
echo "$POLICY" > "$BASE/audio_driver_policy" 2>/dev/null || true
H35_ROLE_PATH="/sys/devices/platform/soc/18844000.usb/musb-hdrc.0.auto/mode"
h35_clear_transient_state(){
  rm -f "$RUNTIME"/export_request "$RUNTIME"/export_ready.* "$RUNTIME"/export_error.* \
    "$RUNTIME"/export_state "$RUNTIME"/export_transport "$RUNTIME"/aoa_* \
    "$RUNTIME"/bulk_receiver_pid "$RUNTIME"/receiver_pid "$RUNTIME"/daemon_pid 2>/dev/null || true
}
h35_switch_host_role(){
  for g in /sys/kernel/config/usb_gadget/r36sx_lgpt_* /sys/kernel/config/usb_gadget/r36sx_uac2_*; do
    [ -d "$g" ] || continue
    printf '\n' >"$g/UDC" 2>/dev/null || true
  done
  [ -e "$H35_ROLE_PATH" ] || H35_ROLE_PATH="$(find /sys/devices -path '*musb-hdrc.0.auto/mode' -print -quit 2>/dev/null)"
  [ -n "$H35_ROLE_PATH" ] && [ -e "$H35_ROLE_PATH" ] || return 2
  echo host >"$H35_ROLE_PATH" 2>/dev/null || echo b_host >"$H35_ROLE_PATH" 2>/dev/null || return 3
  role="$(cat "$H35_ROLE_PATH" 2>/dev/null || true)"
  echo "H35_ANDROID_ROLE_AFTER=$role"
  echo "$role" | grep -q host
}

# Load the ALSA core stack plus the host-side USB audio/MIDI modules so the
# musb controller (now in host role) can enumerate the SP404/UAC2 or a USB-MIDI
# device as an ALSA card/rawmidi node. Idempotent; searches the SD deployment
# dir and the built-in module tree. Without these the SP404 never appears.
# All eight modules are mandatory for the sampler profile: a failure of
# snd-pcm/snd-usbmidi-lib/snd-usb-audio (or any of the core stack) aborts the
# mode with a full diagnostic instead of silently continuing to host role.
H35_HOST_MODBASE=$BASE/modules/4.4.186-release/host_usb_audio
H35_UAC2_MODBASE=$BASE/modules/4.4.186-release/u2_38au8_sync_uac2
h35_loaded(){ grep -q "^$1 " /proc/modules 2>/dev/null; }
h35_module_vermagic(){
  f="$1"; [ -r "$f" ] || return 1
  strings "$f" 2>/dev/null | grep -m1 '^[0-9]\+\.[0-9]\+\.[0-9]\+' || true
}
h35_insmod_with_diag(){
  p="$1"
  err="$(insmod "$p" 2>&1 || true)"
  rc=$?
  [ "$rc" -eq 0 ] && return 0
  {
    echo "insmod FAILED rc=$rc path=$p"
    echo "  stderr: $err"
    echo "  vermagic=$(h35_module_vermagic "$p")"
    echo "  uname=$(uname -r 2>/dev/null || echo unknown)"
    echo "  dmesg tail:"
    dmesg 2>/dev/null | tail -n 60
  } >>"$LOGROOT/H38_HOST_MODULE_LOAD.err" 2>/dev/null || true
  return 1
}
h35_load_host_module(){
  filename="$1"; module="$(echo "${filename%.ko}" | tr - _)"
  h35_loaded "$module" && echo "HOST_LOAD_${filename}_ALREADY=YES" && return 0
  # Preferred: modprobe with modules.dep when the on-disk module tree is usable.
  if command -v modprobe >/dev/null 2>&1 && [ -d "/lib/modules/$(uname -r 2>/dev/null)" ]; then
    if modprobe "$module" 2>>"$LOGROOT/H38_HOST_MODULE_LOAD.err"; then
      echo "HOST_LOAD_${filename}_MODPROBE=YES"
      return 0
    fi
  fi
  # Fallback: manual insmod respecting the on-disk search order.
  for p in \
    /lib/modules/4.4.186-release/kernel/sound/core/"$filename" \
    /lib/modules/4.4.186-release/kernel/sound/usb/"$filename" \
    /lib32/modules/4.4.186-release/kernel/sound/core/"$filename" \
    /lib32/modules/4.4.186-release/kernel/sound/usb/"$filename" \
    "$H35_UAC2_MODBASE"/"$filename" \
    $(find "$H35_HOST_MODBASE" -type f -name "$filename" 2>/dev/null); do
    [ -f "$p" ] || continue
    h35_insmod_with_diag "$p" && { echo "HOST_LOAD_${filename}_INSMOD=YES FROM=$p"; return 0; }
  done
  echo "HOST_LOAD_${filename}_FAILED=YES"
  return 1
}
H35_CORE_STACK="soundcore.ko snd.ko snd-timer.ko snd-pcm.ko snd-hwdep.ko snd-seq-device.ko snd-rawmidi.ko"
H35_USB_STACK="snd-usbmidi-lib.ko snd-usb-audio.ko"
H35_HOST_MODULE_ERRLOG="$LOGROOT/H38_HOST_MODULE_LOAD.err"
h35_load_host_stack(){
  local failed=""
  for m in $H35_CORE_STACK; do
    h35_load_host_module "$m" || failed="$failed $m"
  done
  for m in $H35_USB_STACK; do
    h35_load_host_module "$m" || failed="$failed $m"
  done
  if [ -n "$failed" ]; then
    {
      echo "H38_HOST_STACK_ABORT failed=[$failed]"
      echo "  modules.dep=$(ls /lib/modules/$(uname -r 2>/dev/null)/modules.dep 2>/dev/null || echo missing)"
      echo "  uname=$(uname -r 2>/dev/null || echo unknown)"
      echo "  /proc/modules tail:"
      grep -E '^(snd|soundcore|usbcore|usb)' /proc/modules 2>/dev/null || true
      echo "  dmesg tail:"
      dmesg 2>/dev/null | tail -n 60
    } >>"$H35_HOST_MODULE_ERRLOG" 2>/dev/null || true
    echo "HOST_STACK_ABORT failed=[$failed]"
    return 1
  fi
  echo "HOST_STACK_OK"
  return 0
}

case "$MODE" in
  WINDOWS) atomic_write "$RUNTIME/audio_usb_profile" "MONO_48K" ;;
  ANDROID) atomic_write "$RUNTIME/audio_usb_profile" "STEREO_44K1_AOA_BULK";;
  USB_OUT) atomic_write "$RUNTIME/audio_usb_profile" "MONO_48K";;
  MIDI) atomic_write "$RUNTIME/audio_usb_profile" "MIDI_48K";;
  *) atomic_write "$RUNTIME/audio_usb_profile" "LOCAL";;
esac
atomic_write "$STATUS" "STARTING mode=$MODE policy=$POLICY" || true
log "MODE_APPLY_BEGIN policy=$POLICY core_unloaded=1"
case "$MODE" in
  LOCAL_CONSOLE)
    stop_android_runtime; stop_windows_runtime
    atomic_write "$STATUS" "READY mode=LOCAL_CONSOLE" || true
    log MODE_APPLY_READY
    ;;
  WINDOWS)
    stop_android_runtime
    [ -r "$BIN/otg_u241_setup_once.sh" ] || { atomic_write "$STATUS" "ERROR mode=WINDOWS missing=setup" || true; exit 31; }
    /bin/sh "$BIN/otg_u241_setup_once.sh" >>"$LOG" 2>&1 & p=$!
    atomic_write "$STATUS" "STARTED mode=WINDOWS pid=$p" || true
    log "MODE_APPLY_STARTED windows_pid=$p"
    ;;
  USB_OUT)
    # Sampler / host UAC2 duplex mode. The ALSA host stack is MANDATORY: if the
    # essential modules fail to load, abort with ERROR instead of switching to
    # host role and reporting a phantom "STARTED" without a card.
    stop_windows_runtime
    stop_android_runtime
    h35_clear_transient_state
    if ! h35_load_host_stack; then
      atomic_write "$STATUS" "ERROR mode=USB_OUT reason=host_stack_load_failed" || true
      log MODE_APPLY_ERROR_HOST_STACK
      exit 38
    fi
    h35_switch_host_role || exit $?
    [ -r "$BIN/otg_h37_host_runtime_supervisor.sh" ] || { atomic_write "$STATUS" "ERROR mode=USB_OUT missing=host_supervisor" || true; exit 33; }
    LGPT_H38_POLICY=USB_OUT_OTG /bin/sh "$BIN/otg_h37_host_runtime_supervisor.sh" >>"$LOG" 2>&1 & p=$!
    atomic_write "$STATUS" "STARTED mode=USB_OUT supervisor_pid=$p" || true
    log "MODE_APPLY_STARTED usb_out_supervisor_pid=$p"
    ;;
  MIDI)
    # USB-MIDI piano/controller. The ALSA host stack is mandatory.
    stop_windows_runtime
    stop_android_runtime
    h35_clear_transient_state
    if ! h35_load_host_stack; then
      atomic_write "$STATUS" "ERROR mode=MIDI reason=host_stack_load_failed" || true
      log MODE_APPLY_ERROR_HOST_STACK
      exit 39
    fi
    h35_switch_host_role || exit $?
    [ -r "$BIN/otg_h37_host_runtime_supervisor.sh" ] || { atomic_write "$STATUS" "ERROR mode=MIDI missing=host_supervisor" || true; exit 34; }
    LGPT_H38_POLICY=MIDI_OTG /bin/sh "$BIN/otg_h37_host_runtime_supervisor.sh" >>"$LOG" 2>&1 & p=$!
    atomic_write "$STATUS" "STARTED mode=MIDI supervisor_pid=$p" || true
    log "MODE_APPLY_STARTED midi_supervisor_pid=$p"
    ;;
  ANDROID)
    # A normal re-entry reuses a healthy Android runtime and does not disturb
    # the active accessory. A real Windows/Local -> Android transition performs
    # a complete, ordered gadget teardown and host-role reset.
    if echo "$PREVIOUS_POLICY" | grep -Eq "^(ANDROID|ANDROID_OTG|ANDROID_AOA|USB_IN_OTG|USB_IN)$" && android_runtime_ready; then
      atomic_write "$STATUS" "READY mode=ANDROID reused=1" || true
      log MODE_APPLY_READY_ANDROID_REUSED
      exit 0
    fi
    stop_windows_runtime
    stop_android_runtime
    h35_clear_transient_state
    h35_switch_host_role || exit $?
    gen_file="$RUNTIME/h35_android_generation"
    old_gen="$(cat "$gen_file" 2>/dev/null || echo 0)"
    case "$old_gen" in ''|*[!0-9]*) old_gen=0;; esac
    new_gen=$((old_gen + 1))
    atomic_write "$gen_file" "$new_gen" || true
    [ -r "$BIN/otg_h37_android_runtime_supervisor.sh" ] || { atomic_write "$STATUS" "ERROR mode=ANDROID missing=supervisor" || true; exit 32; }
    LGPT_H35_GENERATION="$new_gen" /bin/sh "$BIN/otg_h37_android_runtime_supervisor.sh" >>"$LOG" 2>&1 & p=$!
    atomic_write "$STATUS" "STARTED mode=ANDROID supervisor_pid=$p" || true
    log "MODE_APPLY_STARTED android_supervisor_pid=$p"
    ;;
esac
exit 0
