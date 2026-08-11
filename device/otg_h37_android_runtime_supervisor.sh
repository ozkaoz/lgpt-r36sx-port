#!/bin/sh
# H36 Android AOA runtime supervisor (h36 daemons). It owns userspace
# daemon/receiver only. USB backend selection remains owned by the H35
# launcher/selector or the H38.2 apply script.
set -u
ROOT="${LGPT_SD_ROOT:-/mnt/sdcard}"
BASE="$ROOT/lgpt/otg"
BIN="$BASE/bin"
LOGROOT="${LGPT_LOGROOT:-/tmp/r36sx_lgpt_logs}"
RUNTIME="${LGPT_RUNTIME_DIR:-/tmp/r36sx_lgpt_usb}"
POLICY_FILE="$RUNTIME/audio_driver_policy"
LOG="$LOGROOT/H35_ANDROID_RUNTIME_SUPERVISOR.log"
# SD lifecycle U2.54: the duplicated supervisor copy also lives on tmpfs; the
# clean-shutdown flush persists it to LGPT_OTG_LOGS/android_runtime on the SD.
SUPLOG="${LGPT_ANDROID_SUP_LOG:-/tmp/r36sx_lgpt_logs/android_runtime/H35_ANDROID_RUNTIME_SUPERVISOR.log}"
LOCK="${LGPT_H35_ANDROID_RUNTIME_LOCK:-/tmp/r36sx_h35_android_runtime.lock}"
DAEMON="$BIN/r36s_aoa_bulk_audio_io_h36"
RECEIVER="$BIN/r36s_aoa_bulk_receiver_h36"
PROJECT_FIFO="/tmp/r36sx_android_project_fifo"
PCM_FIFO="/tmp/r36sx_aoa_bulk_pcm_fifo"
SUP_PID="$RUNTIME/h35_android_supervisor_pid"
DAEMON_PID="$RUNTIME/daemon_pid"
RECEIVER_PID="$RUNTIME/h35_android_receiver_pid"
STOP=0
GENERATION="${LGPT_H35_GENERATION:-$(cat "$RUNTIME/h35_android_generation" 2>/dev/null || echo 0)}"
mkdir -p "$LOGROOT" "$RUNTIME" 2>/dev/null || exit 20
log(){ printf '%s H35_ANDROID_SUPERVISOR %s\n' "$(date 2>/dev/null || echo no-date)" "$*" >>"$LOG" 2>/dev/null || true; mkdir -p "$(dirname "$SUPLOG")" 2>/dev/null || true; printf '%s H35_ANDROID_SUPERVISOR %s\n' "$(date 2>/dev/null || echo no-date)" "$*" >>"$SUPLOG" 2>/dev/null || true; }
atomic_write(){ p="$1"; v="$2"; d="$(dirname "$p")"; mkdir -p "$d" 2>/dev/null || true; t="${p}.h35tmp.$$"; rm -f "$t" 2>/dev/null || true; printf '%s\n' "$v" >"$t" 2>/dev/null && mv -f "$t" "$p" 2>/dev/null; }
pid_alive(){ p="$1"; [ -n "$p" ] && [ "$p" != "0" ] && kill -0 "$p" 2>/dev/null; }
policy_android(){ case "$(cat "$POLICY_FILE" 2>/dev/null || true)" in ANDROID|ANDROID_OTG|ANDROID_AOA|USB_IN_OTG|USB_IN) return 0;; *) return 1;; esac; }
# v14.1: RO-proof launch - a dirty SD FAT mounted read-only would make
# `daemon >> SD.log &` fail the redirect and the daemon/receiver never start.
pick_log_path(){ p="$1"; if ( : >> "$p" ) 2>/dev/null; then printf '%s' "$p"; else printf '/tmp/%s' "$(basename "$p")"; fi; }
terminate_pid(){ p="$1"; name="$2"; [ -n "$p" ] && [ "$p" != "0" ] || return 0; pid_alive "$p" || return 0; log "STOP name=$name pid=$p"; kill -USR1 "$p" 2>/dev/null || true; n=0; while pid_alive "$p" && [ "$n" -lt 40 ]; do sleep 0.05; n=$((n+1)); done; pid_alive "$p" || { wait "$p" 2>/dev/null || true; return 0; }; kill "$p" 2>/dev/null || true; n=0; while pid_alive "$p" && [ "$n" -lt 20 ]; do sleep 0.05; n=$((n+1)); done; pid_alive "$p" && kill -9 "$p" 2>/dev/null || true; wait "$p" 2>/dev/null || true; }
cleanup(){
  STOP=1
  rp="$(cat "$RECEIVER_PID" 2>/dev/null || true)"
  dp="$(cat "$DAEMON_PID" 2>/dev/null || true)"
  terminate_pid "$rp" receiver
  terminate_pid "$dp" daemon
  rm -f "$SUP_PID" "$RECEIVER_PID" "$DAEMON_PID" 2>/dev/null || true
  rm -rf "$LOCK" 2>/dev/null || true
  log "EXIT supervisor=$$"
}
trap 'STOP=1' INT TERM HUP
trap cleanup EXIT
if ! mkdir "$LOCK" 2>/dev/null; then
  old="$(cat "$SUP_PID" 2>/dev/null || true)"
  if pid_alive "$old"; then log "ALREADY_RUNNING pid=$old"; exit 0; fi
  rm -rf "$LOCK" 2>/dev/null || true
  mkdir "$LOCK" 2>/dev/null || { log "LOCK_BUSY"; exit 40; }
fi
[ -x "$DAEMON" ] || { log "ERROR missing_daemon=$DAEMON"; exit 31; }
[ -x "$RECEIVER" ] || { log "ERROR missing_receiver=$RECEIVER"; exit 32; }
atomic_write "$SUP_PID" "$$" || true
log "START supervisor=$$ generation=$GENERATION daemon=$DAEMON receiver=$RECEIVER"
# Remove only volatile Android markers. Never touch the selected policy or Windows files.
rm -f "$RUNTIME/aoa_host_configured" "$RUNTIME/aoa_bulk_accessory_present" \
      "$RUNTIME/aoa_bulk_stream_ready" "$RUNTIME/aoa_state" "$RUNTIME/aoa_result" \
      "$RUNTIME/daemon_version" "$RUNTIME/capture_abi" "$RUNTIME/export_request" "$RUNTIME/export_state" "$RUNTIME/export_transport" "$RUNTIME"/export_ready.* "$RUNTIME"/export_error.* 2>/dev/null || true
[ -p "$PROJECT_FIFO" ] || { rm -f "$PROJECT_FIFO" 2>/dev/null || true; mkfifo "$PROJECT_FIFO" 2>/dev/null || true; }
# v12.1: always pre-create the PCM fifo (same pattern as PROJECT_FIFO). A
# stale regular file or a missing node made the core's readiness check fail
# with ENOENT ("fifo open pending", recording blocked) until the AOA daemon
# happened to recreate it on its own schedule.
[ -p "$PCM_FIFO" ] || { rm -f "$PCM_FIFO" 2>/dev/null || true; mkfifo "$PCM_FIFO" 2>/dev/null || true; }
"$DAEMON" "$PROJECT_FIFO" - "$PCM_FIFO" 2 48000 >>"$(pick_log_path "$LOGROOT/H35_AOA_BULK_AUDIO_DAEMON.log")" 2>&1 &
dp=$!
atomic_write "$DAEMON_PID" "$dp" || true
sleep 0.15
pid_alive "$dp" || { log "ERROR daemon_exited_early pid=$dp"; exit 33; }
log "DAEMON_STARTED pid=$dp"
if [ -p "$PCM_FIFO" ]; then log "PCM_FIFO_PRESENT $PCM_FIFO"; else log "PCM_FIFO_MISSING $PCM_FIFO"; fi
backoff=1
daemon_restarts=0
while [ "$STOP" -eq 0 ] && policy_android; do
  if ! pid_alive "$dp"; then
    daemon_restarts=$((daemon_restarts+1))
    rm -f "$RUNTIME/aoa_host_configured" "$RUNTIME/aoa_bulk_accessory_present" \
          "$RUNTIME/aoa_bulk_stream_ready" "$RUNTIME/aoa_state" "$RUNTIME/aoa_result" \
          "$RUNTIME/daemon_version" "$RUNTIME/capture_abi" 2>/dev/null || true
    log "DAEMON_EXITED pid=$dp restarts=$daemon_restarts restarting"
    [ -p "$PCM_FIFO" ] || { rm -f "$PCM_FIFO" 2>/dev/null || true; mkfifo "$PCM_FIFO" 2>/dev/null || true; }
"$DAEMON" "$PROJECT_FIFO" - "$PCM_FIFO" 2 48000 >>"$(pick_log_path "$LOGROOT/H35_AOA_BULK_AUDIO_DAEMON.log")" 2>&1 &
    dp=$!
    atomic_write "$DAEMON_PID" "$dp" || true
    sleep 0.25
    if ! pid_alive "$dp"; then
      log "ERROR daemon_restart_failed pid=$dp"
      sleep 1
      continue
    fi
    log "DAEMON_RESTARTED pid=$dp"
    if [ -p "$PCM_FIFO" ]; then log "PCM_FIFO_PRESENT"; else log "PCM_FIFO_MISSING"; fi
    rp="$(cat "$RECEIVER_PID" 2>/dev/null || true)"
    terminate_pid "$rp" receiver
    continue
  fi
  "$RECEIVER" >>"$(pick_log_path "$LOGROOT/H35_AOA_BULK_RECEIVER_WRAPPER.log")" 2>&1 &
  rp=$!
  atomic_write "$RECEIVER_PID" "$rp" || true
  log "RECEIVER_STARTED pid=$rp backoff=$backoff"
  wait "$rp" 2>/dev/null
  rc=$?
  rm -f "$RECEIVER_PID" 2>/dev/null || true
  [ "$STOP" -eq 0 ] || break
  policy_android || break
  log "RECEIVER_EXIT rc=$rc reconnect_in=${backoff}s"
  sleep "$backoff"
  [ "$backoff" -lt 4 ] && backoff=$((backoff+1))
done
log "POLICY_EXIT policy=$(cat "$POLICY_FILE" 2>/dev/null || echo missing)"
