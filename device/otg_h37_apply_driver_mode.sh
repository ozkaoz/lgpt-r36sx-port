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
  SP404_IN|SP404_IN_OTG|SP404IN|SP404IN_OTG) echo SP404_IN;;
  USB_OUT|USB_OUT_OTG|SP404|SP404_OTG) echo USB_OUT;;
  MIDI|MIDI_OTG) echo MIDI;;
  WINDOWS|WINDOWS_OTG|USB_DUPLEX|USB_DUPLEX_OTG|USB_IN_OUT|FULL_DUPLEX) echo WINDOWS;;
  *) echo LOCAL_CONSOLE;; esac; }
policy_for_mode(){ case "$1" in ANDROID) echo USB_IN_OTG;; WINDOWS) echo USB_DUPLEX_OTG;; SP404_IN|USB_OUT) echo USB_OUT_OTG;; MIDI) echo MIDI_OTG;; *) echo LOCAL_CONSOLE;; esac; }
atomic_write(){ p="$1"; v="$2"; d="$(dirname "$p")"; mkdir -p "$d" 2>/dev/null || true; t="${p}.h35tmp.$$"; rm -f "$t" 2>/dev/null || true; printf '%s\n' "$v" >"$t" 2>/dev/null && mv -f "$t" "$p" 2>/dev/null; }
log(){ printf '%s H35 mode=%s supervisor=%s %s\n' "$(date 2>/dev/null || echo no-date)" "$MODE" "${LGPT_H35_SUPERVISOR_PID:-none}" "$*" >>"$LOG" 2>/dev/null || true; }
pid_alive(){ p="$1"; [ -n "$p" ] && [ "$p" != "0" ] && kill -0 "$p" 2>/dev/null; }
terminate_pid(){ p="$1"; [ -n "$p" ] && [ "$p" != "0" ] || return 0; pid_alive "$p" || return 0; kill "$p" 2>/dev/null || true; n=0; while pid_alive "$p" && [ "$n" -lt 20 ]; do sleep 0.05; n=$((n+1)); done; pid_alive "$p" && kill -9 "$p" 2>/dev/null || true; }
kill_name(){ name="$1"; for p in $(pidof "$name" 2>/dev/null || true); do terminate_pid "$p"; done; }
stop_host_runtime(){
  hp="$(cat "$RUNTIME/h38_host_supervisor_pid" 2>/dev/null || true)"
  if [ -n "$hp" ] && pid_alive "$hp"; then
    log "STOP_HOST_RUNTIME supervisor_pid=$hp"
    kill "$hp" 2>/dev/null || true
    n=0; while pid_alive "$hp" && [ "$n" -lt 40 ]; do sleep 0.05; n=$((n+1)); done
    pid_alive "$hp" && kill -9 "$hp" 2>/dev/null || true
    n=0; while pid_alive "$hp" && [ "$n" -lt 20 ]; do sleep 0.05; n=$((n+1)); done
  fi
  graceful_kill_sp404
  graceful_kill_midi
  for catpid in $(ps 2>/dev/null | grep -E "cat.*r36sx_(sp404|midi).*fifo" | awk '{print $1}' 2>/dev/null || true); do
    terminate_pid "$catpid" fifo_guardian
  done
  for dp in $(ps 2>/dev/null | grep "otg_h37_host_device_detect" | awk '{print $1}' 2>/dev/null || true); do
    terminate_pid "$dp" detector
  done
  rm -f "$RUNTIME/h38_host_supervisor_pid" "$RUNTIME/sp404_daemon_pid" "$RUNTIME/midi_daemon_pid" "$RUNTIME/daemon_pid" 2>/dev/null || true
  rm -rf "/tmp/r36sx_h38_host_runtime.lock" "/tmp/r36sx_h38_host_detect.lock" 2>/dev/null || true
  rm -f "$RUNTIME/sp404_card" "$RUNTIME/sp404_playback_pcm" "$RUNTIME/sp404_capture_pcm" "$RUNTIME/sp404_stream_caps" "$RUNTIME/sp404_usb_id" "$RUNTIME/sp404_syspath" "$RUNTIME/sp404_last_good" "$RUNTIME/midi_rawmidi" "$RUNTIME/h38_host_state" "$RUNTIME/h38_host_full_detector_calls" "$RUNTIME/h38_host_stable_watchdog_checks" 2>/dev/null || true
  log "STOP_HOST_RUNTIME done"
}
invalidate_host_generation(){
  gen_file="$RUNTIME/h38_host_generation"
  old_gen="$(cat "$gen_file" 2>/dev/null || echo 0)"
  case "$old_gen" in ''|*[!0-9]*) old_gen=0;; esac
  new_gen=$((old_gen + 1))
  atomic_write "$gen_file" "$new_gen" || true
  log "HOST_GENERATION_INVALIDATE old=$old_gen new=$new_gen"
}
verify_local_runtime_clean(){
  fail=0
  for pidfile in "$RUNTIME/h38_host_supervisor_pid" "$RUNTIME/sp404_daemon_pid" "$RUNTIME/midi_daemon_pid" "$RUNTIME/daemon_pid" "$RUNTIME/h35_android_supervisor_pid"; do
    pp="$(cat "$pidfile" 2>/dev/null || true)"
    if pid_alive "$pp"; then
      log "LOCAL_CLEAN_FAIL pidfile=$pidfile pid=$pp still alive"
      fail=1
    fi
  done
  for name in r36s_sp404_host_audio_io r36s_midi_host_io r36s_aoa_bulk_receiver_h36 r36s_aoa_bulk_audio_io_h36 r36s_u241_usb_audio_io r36s_u240_usb_audio_io r36s_au11_usb_audio_io; do
    if pidof "$name" >/dev/null 2>&1; then
      log "LOCAL_CLEAN_FAIL name=$name still running"
      fail=1
    fi
  done
  if ps 2>/dev/null | grep -qE "cat.*r36sx_(sp404|midi).*fifo"; then
    log "LOCAL_CLEAN_FAIL guardian still alive"
    fail=1
  fi
  if ps 2>/dev/null | grep -q "otg_h37_host_device_detect"; then
    log "LOCAL_CLEAN_FAIL detector still alive"
    fail=1
  fi
  if [ -e "/tmp/r36sx_h38_host_runtime.lock" ] || [ -e "/tmp/r36sx_h38_host_detect.lock" ]; then
    log "LOCAL_CLEAN_FAIL stale lock still exists"
  fi
  if [ -p "/tmp/r36sx_sp404_pcm_fifo" ]; then
    if lsof /tmp/r36sx_sp404_pcm_fifo 2>/dev/null | grep -q "r36s_sp404"; then
      log "LOCAL_CLEAN_FAIL fifo still held by daemon"
      fail=1
    fi
  fi
  if [ "$fail" -eq 0 ]; then
    log "LOCAL_RUNTIME_CLEAN verified"
    return 0
  else
    return 1
  fi
}
graceful_kill_sp404(){
  for p in $(pidof r36s_sp404_host_audio_io 2>/dev/null || true); do
    log "GRACEFUL_STOP sp404 pid=$p SIGUSR1"
    kill -USR1 "$p" 2>/dev/null || true
    n=0; while pid_alive "$p" && [ "$n" -lt 40 ]; do sleep 0.05; n=$((n+1)); done
    if pid_alive "$p"; then
      kill "$p" 2>/dev/null || true
      n=0; while pid_alive "$p" && [ "$n" -lt 20 ]; do sleep 0.05; n=$((n+1)); done
      pid_alive "$p" && kill -9 "$p" 2>/dev/null || true
    fi
  done
}
graceful_kill_midi(){
  for p in $(pidof r36s_midi_host_io 2>/dev/null || true); do
    log "GRACEFUL_STOP midi pid=$p SIGUSR1"
    kill -USR1 "$p" 2>/dev/null || true
    n=0; while pid_alive "$p" && [ "$n" -lt 40 ]; do sleep 0.05; n=$((n+1)); done
    if pid_alive "$p"; then
      kill "$p" 2>/dev/null || true
      n=0; while pid_alive "$p" && [ "$n" -lt 20 ]; do sleep 0.05; n=$((n+1)); done
      pid_alive "$p" && kill -9 "$p" 2>/dev/null || true
    fi
  done
}
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
  pid="$(cat /tmp/r36sx_lgpt_usb/u241_setup_pid 2>/dev/null || true)"
  if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    n=0; while kill -0 "$pid" 2>/dev/null && [ "$n" -lt 40 ]; do sleep 0.05; n=$((n+1)); done
    if kill -0 "$pid" 2>/dev/null; then
      kill -9 "$pid" 2>/dev/null || true
      n=0; while kill -0 "$pid" 2>/dev/null && [ "$n" -lt 20 ]; do sleep 0.05; n=$((n+1)); done
    fi
  fi
  rm -f /tmp/r36sx_lgpt_usb/u241_setup_pid 2>/dev/null || true
  n=0; while [ -d /tmp/r36sx_u2414_audio_driver_lock ] && [ "$n" -lt 40 ]; do sleep 0.05; n=$((n+1)); done
  if [ -r "$BIN/otg_u241_shutdown.sh" ]; then /bin/sh "$BIN/otg_u241_shutdown.sh" >>"$LOG" 2>&1; fi
  n=0; while [ "$n" -lt 40 ]; do
    udc="$(cat /sys/kernel/config/usb_gadget/r36sx_lgpt_u2414/UDC 2>/dev/null || echo none)"
    [ -z "$udc" ] || [ "$udc" = "" ] && break
    sleep 0.05; n=$((n+1))
  done
  kill_name r36s_u241_usb_audio_io; kill_name r36s_u240_usb_audio_io; kill_name r36s_au11_usb_audio_io
  kill_name r36s_h37_usb_audio_io
}
verify_all_udc_unbound(){
  for g in /sys/kernel/config/usb_gadget/*/UDC; do
    [ -e "$g" ] || continue
    udc="$(cat "$g" 2>/dev/null || echo none)"
    if [ -n "$udc" ] && [ "$udc" != "" ]; then
      log "UDC_STILL_BOUND gadget=$g udc=$udc"
      return 1
    fi
  done
  log "UDC_ALL_UNBOUND"
  return 0
}
wait_windows_teardown(){
  pid="$(cat /tmp/r36sx_lgpt_usb/u241_setup_pid 2>/dev/null || true)"
  n=0; while [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null && [ "$n" -lt 40 ]; do sleep 0.05; n=$((n+1)); done
  rm -f /tmp/r36sx_lgpt_usb/u241_setup_pid 2>/dev/null || true
  n=0; while [ -d /tmp/r36sx_u2414_audio_driver_lock ] && [ "$n" -lt 40 ]; do sleep 0.05; n=$((n+1)); done
  if [ -r "$BIN/otg_u241_shutdown.sh" ]; then /bin/sh "$BIN/otg_u241_shutdown.sh" >>"$LOG" 2>&1 || true; fi
  n=0; while [ "$n" -lt 40 ]; do
    verify_all_udc_unbound && break
    sleep 0.05; n=$((n+1))
  done
  verify_all_udc_unbound || return 1
  return 0
}
verify_host_stable(){
  H35_ROLE_PATH="/sys/devices/platform/soc/18844000.usb/musb-hdrc.0.auto/mode"
  [ -e "$H35_ROLE_PATH" ] || H35_ROLE_PATH="$(find /sys/devices -path '*musb-hdrc.0.auto/mode' -print -quit 2>/dev/null)"
  [ -n "$H35_ROLE_PATH" ] && [ -e "$H35_ROLE_PATH" ] || return 1
  n=0; while [ "$n" -lt 10 ]; do
    role="$(cat "$H35_ROLE_PATH" 2>/dev/null || echo none)"
    echo "H35_HOST_ROLE_CHECK n=$n role=$role" >>"$LOG" 2>&1 || true
    echo "$role" | grep -q host || { log "HOST_ROLE_FAIL role=$role"; return 1; }
    sleep 0.1; n=$((n+1))
  done
  for d in 0.1 0.15 0.25 0.5; do sleep "$d"; role="$(cat "$H35_ROLE_PATH" 2>/dev/null || echo none)"; echo "$role" | grep -q host || { log "HOST_STABILITY_FAIL role=$role"; return 1; }; done
  log "HOST_STABLE_VERIFIED"
  return 0
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
  if command -v modprobe >/dev/null 2>&1 && [ -d "/lib/modules/$(uname -r 2>/dev/null)" ]; then
    if modprobe "$module" 2>>"$LOGROOT/H38_HOST_MODULE_LOAD.err"; then
      echo "HOST_LOAD_${filename}_MODPROBE=YES"
      return 0
    fi
  fi
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
  WINDOWS) atomic_write "$RUNTIME/audio_usb_profile" "STEREO_48K"; atomic_write "$BASE/audio_usb_profile" "STEREO_48K" ;;
  ANDROID) atomic_write "$RUNTIME/audio_usb_profile" "STEREO_44K1_AOA_BULK";;
  USB_OUT|SP404_IN) atomic_write "$RUNTIME/audio_usb_profile" "STEREO_48K"; atomic_write "$BASE/audio_usb_profile" "STEREO_48K";;
  MIDI) atomic_write "$RUNTIME/audio_usb_profile" "MIDI_48K";;
  *) atomic_write "$RUNTIME/audio_usb_profile" "LOCAL";;
esac
atomic_write "$STATUS" "STARTING mode=$MODE policy=$POLICY" || true
log "MODE_APPLY_BEGIN policy=$POLICY core_unloaded=1 prev=$PREVIOUS_POLICY"
case "$MODE" in
  LOCAL_CONSOLE)
    invalidate_host_generation
    stop_host_runtime
    stop_android_runtime
    stop_windows_runtime
    wait_windows_teardown || true
    verify_all_udc_unbound || log "LOCAL_CONSOLE UDC still bound after teardown (non-fatal)"
    h35_clear_transient_state
    if ! verify_local_runtime_clean; then
      atomic_write "$STATUS" "ERROR mode=LOCAL_CONSOLE reason=LOCAL_RUNTIME_NOT_CLEAN" || true
      log "LOCAL_RUNTIME_NOT_CLEAN"
      exit 37
    fi
    atomic_write "$STATUS" "READY mode=LOCAL_CONSOLE" || true
    log MODE_APPLY_READY
    ;;
  WINDOWS)
    invalidate_host_generation
    stop_host_runtime
    stop_android_runtime
    wait_windows_teardown || true
    verify_all_udc_unbound || true
    h35_clear_transient_state
    [ -r "$BIN/otg_u241_setup_once.sh" ] || { atomic_write "$STATUS" "ERROR mode=WINDOWS missing=setup" || true; exit 31; }
    /bin/sh "$BIN/otg_u241_setup_once.sh" >>"$LOG" 2>&1 & p=$!
    atomic_write "$STATUS" "STARTED mode=WINDOWS pid=$p" || true
    log "MODE_APPLY_STARTED windows_pid=$p"
    ;;
  USB_OUT|SP404_IN)
    invalidate_host_generation
    stop_host_runtime
    stop_windows_runtime
    if ! wait_windows_teardown; then
      atomic_write "$STATUS" "ERROR mode=$MODE reason=UDC_still_bound" || true
      log "SP404_UDC_STILL_BOUND after Windows teardown"
      exit 35
    fi
    stop_android_runtime
    h35_clear_transient_state
    if ! h35_load_host_stack; then
      atomic_write "$STATUS" "ERROR mode=$MODE reason=host_stack_load_failed" || true
      log MODE_APPLY_ERROR_HOST_STACK
      exit 38
    fi
    h35_switch_host_role || exit $?
    if ! verify_host_stable; then
      atomic_write "$STATUS" "ERROR mode=$MODE reason=MUSB_not_host" || true
      log "SP404_HOST_ROLE_FAIL"
      exit 36
    fi
    gen_file="$RUNTIME/h38_host_generation"
    old_gen="$(cat "$gen_file" 2>/dev/null || echo 0)"
    case "$old_gen" in ''|*[!0-9]*) old_gen=0;; esac
    new_gen=$((old_gen + 1))
    atomic_write "$gen_file" "$new_gen" || true
    log "SP404_HOST_GENERATION new=$new_gen"
    [ -r "$BIN/otg_h37_host_runtime_supervisor.sh" ] || { atomic_write "$STATUS" "ERROR mode=$MODE missing=host_supervisor" || true; exit 33; }
    LGPT_H38_POLICY=USB_OUT_OTG /bin/sh "$BIN/otg_h37_host_runtime_supervisor.sh" >>"$LOG" 2>&1 & p=$!
    atomic_write "$STATUS" "STARTED mode=$MODE supervisor_pid=$p generation=$new_gen" || true
    log "MODE_APPLY_STARTED ${MODE}_supervisor_pid=$p generation=$new_gen"
    ;;
  MIDI)
    invalidate_host_generation
    stop_host_runtime
    stop_windows_runtime
    if ! wait_windows_teardown; then
      atomic_write "$STATUS" "ERROR mode=MIDI reason=UDC_still_bound" || true
      log "MIDI_UDC_STILL_BOUND"
      exit 35
    fi
    stop_android_runtime
    h35_clear_transient_state
    if ! h35_load_host_stack; then
      atomic_write "$STATUS" "ERROR mode=MIDI reason=host_stack_load_failed" || true
      log MODE_APPLY_ERROR_HOST_STACK
      exit 39
    fi
    h35_switch_host_role || exit $?
    if ! verify_host_stable; then
      atomic_write "$STATUS" "ERROR mode=MIDI reason=MUSB_not_host" || true
      log "MIDI_HOST_ROLE_FAIL"
      exit 36
    fi
    gen_file="$RUNTIME/h38_host_generation"
    old_gen="$(cat "$gen_file" 2>/dev/null || echo 0)"
    case "$old_gen" in ''|*[!0-9]*) old_gen=0;; esac
    new_gen=$((old_gen + 1))
    atomic_write "$gen_file" "$new_gen" || true
    log "MIDI_HOST_GENERATION new=$new_gen"
    [ -r "$BIN/otg_h37_host_runtime_supervisor.sh" ] || { atomic_write "$STATUS" "ERROR mode=MIDI missing=host_supervisor" || true; exit 34; }
    LGPT_H38_POLICY=MIDI_OTG /bin/sh "$BIN/otg_h37_host_runtime_supervisor.sh" >>"$LOG" 2>&1 & p=$!
    atomic_write "$STATUS" "STARTED mode=MIDI supervisor_pid=$p generation=$new_gen" || true
    log "MODE_APPLY_STARTED midi_supervisor_pid=$p generation=$new_gen"
    ;;
  ANDROID)
    if echo "$PREVIOUS_POLICY" | grep -Eq "^(ANDROID|ANDROID_OTG|ANDROID_AOA|USB_IN_OTG|USB_IN)$" && android_runtime_ready; then
      atomic_write "$STATUS" "READY mode=ANDROID reused=1" || true
      log MODE_APPLY_READY_ANDROID_REUSED
      exit 0
    fi
    invalidate_host_generation
    stop_host_runtime
    stop_windows_runtime
    if ! wait_windows_teardown; then
      atomic_write "$STATUS" "ERROR mode=ANDROID reason=UDC_still_bound" || true
      log "ANDROID_UDC_STILL_BOUND"
      exit 35
    fi
    stop_android_runtime
    h35_clear_transient_state
    h35_switch_host_role || exit $?
    if ! verify_host_stable; then
      atomic_write "$STATUS" "ERROR mode=ANDROID reason=MUSB_not_host" || true
      log "ANDROID_HOST_ROLE_FAIL"
      exit 36
    fi
    gen_file="$RUNTIME/h35_android_generation"
    old_gen="$(cat "$gen_file" 2>/dev/null || echo 0)"
    case "$old_gen" in ''|*[!0-9]*) old_gen=0;; esac
    new_gen=$((old_gen + 1))
    atomic_write "$gen_file" "$new_gen" || true
    [ -r "$BIN/otg_h37_android_runtime_supervisor.sh" ] || { atomic_write "$STATUS" "ERROR mode=ANDROID missing=supervisor" || true; exit 32; }
    LGPT_H35_GENERATION="$new_gen" /bin/sh "$BIN/otg_h37_android_runtime_supervisor.sh" >>"$LOG" 2>&1 & p=$!
    atomic_write "$STATUS" "STARTED mode=ANDROID supervisor_pid=$p" || true
    log "MODE_APPLY_STARTED android_supervisor_pid=$p generation=$new_gen"
    ;;
esac
exit 0
