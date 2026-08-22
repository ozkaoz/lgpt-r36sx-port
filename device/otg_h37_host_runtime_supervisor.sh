#!/bin/sh
# H38.4 host-side runtime supervisor for the unified audio driver.
# Owns the SP404MKII (host UAC2) and USB-MIDI daemons. It only runs when the
# musb controller is in host role and a host-side policy (SP404/USB OUT/MIDI)
# is selected. Selection and role switching are owned by the H38.2 apply script.
# OPTIMIZED SP404 STABLE: State machine HOST_SEARCHING / HOST_STABLE / HOST_RECOVERING
#  - SEARCHING: full detect permitted every ~0.6s
#  - STABLE: no full detect, only light watchdog every 1500ms
#  - RECOVERING: full detect until recovery
set -u
ROOT="${LGPT_SD_ROOT:-/mnt/sdcard}"
BASE="$ROOT/lgpt/otg"
BIN="$BASE/bin"
LOGROOT="${LGPT_LOGROOT:-/tmp/r36sx_lgpt_logs}"
RUNTIME="${LGPT_RUNTIME_DIR:-/tmp/r36sx_lgpt_usb}"
POLICY_FILE="$RUNTIME/audio_driver_policy"
LOG="$LOGROOT/H38_HOST_RUNTIME_SUPERVISOR.log"
LOCK="${LGPT_H38_HOST_RUNTIME_LOCK:-/tmp/r36sx_h38_host_runtime.lock}"
SUP_PID="$RUNTIME/h38_host_supervisor_pid"
SP404_DAEMON="$BIN/r36s_sp404_host_audio_io"
MIDI_DAEMON="$BIN/r36s_midi_host_io"
SP404_FIFO="/tmp/r36sx_sp404_pcm_fifo"
MIDI_FIFO="/tmp/r36sx_midi_pcm_fifo"
STOP=0
mkdir -p "$LOGROOT" "$RUNTIME" 2>/dev/null || exit 20
log(){ printf '%s H38_HOST_SUPERVISOR %s\n' "$(date 2>/dev/null || echo no-date)" "$*" >>"$LOG" 2>/dev/null || true; }
atomic_write(){ p="$1"; v="$2"; d="$(dirname "$p")"; mkdir -p "$d" 2>/dev/null || true; t="${p}.h38tmp.$$"; rm -f "$t" 2>/dev/null || true; printf '%s\n' "$v" >"$t" 2>/dev/null && mv -f "$t" "$p" 2>/dev/null; }
pid_alive(){ p="$1"; [ -n "$p" ] && [ "$p" != "0" ] && kill -0 "$p" 2>/dev/null; }
policy_host(){ case "$(cat "$POLICY_FILE" 2>/dev/null || true)" in SP404_OTG|USB_OUT_OTG|MIDI_OTG) return 0;; *) return 1;; esac; }
sp404_wanted(){ case "$(cat "$POLICY_FILE" 2>/dev/null || true)" in SP404_OTG|USB_OUT_OTG) return 0;; *) return 1;; esac; }
midi_wanted(){ [ "$(cat "$POLICY_FILE" 2>/dev/null || true)" = "MIDI_OTG" ]; }
pick_log_path(){ p="$1"; if ( : >> "$p" ) 2>/dev/null; then printf '%s' "$p"; else printf '/tmp/%s' "$(basename "$p")"; fi; }
LIVE_FLUSH="${LGPT_LIVE_FLUSH:-0}"
flush_tick=0
live_flush(){
  SD_LOGS="/mnt/sdcard/LGPT_OTG_LOGS"
  mkdir -p "$SD_LOGS" 2>/dev/null || return 0
  if ! ( : >> "$SD_LOGS/.live_flush_probe" ) 2>/dev/null; then return 0; fi
  for f in "$LOGROOT"/* "$LOGROOT"/mirror/*; do
    [ -f "$f" ] || continue
    cp -f "$f" "$SD_LOGS/" 2>/dev/null || true
  done
}
terminate_pid(){ p="$1"; name="$2"; [ -n "$p" ] && [ "$p" != "0" ] || return 0; pid_alive "$p" || return 0; log "STOP name=$name pid=$p"; kill -USR1 "$p" 2>/dev/null || true; n=0; while pid_alive "$p" && [ "$n" -lt 40 ]; do sleep 0.05; n=$((n+1)); done; pid_alive "$p" || { wait "$p" 2>/dev/null || true; return 0; }; kill "$p" 2>/dev/null || true; n=0; while pid_alive "$p" && [ "$n" -lt 20 ]; do sleep 0.05; n=$((n+1)); done; pid_alive "$p" && kill -9 "$p" 2>/dev/null || true; wait "$p" 2>/dev/null || true; }
SP404_GUARD_PID=""
guard_start(){
  if [ -n "$SP404_GUARD_PID" ] && pid_alive "$SP404_GUARD_PID"; then return 0; fi
  [ -p "$SP404_FIFO" ] || { rm -f "$SP404_FIFO" 2>/dev/null || true; mkfifo "$SP404_FIFO" 2>/dev/null || true; }
  ( while :; do cat "$SP404_FIFO" >/dev/null 2>/dev/null || { sleep 0.1; continue; }; sleep 0.1; done ) >/dev/null 2>&1 &
  SP404_GUARD_PID=$!
  log "GUARD_START fifo=$SP404_FIFO pid=$SP404_GUARD_PID"
}
guard_stop(){
  if [ -n "$SP404_GUARD_PID" ]; then
    if pid_alive "$SP404_GUARD_PID"; then kill "$SP404_GUARD_PID" 2>/dev/null || true; wait "$SP404_GUARD_PID" 2>/dev/null || true; fi
    log "GUARD_STOP pid=$SP404_GUARD_PID"
  fi
  SP404_GUARD_PID=""
}
cleanup(){
  STOP=1
  guard_stop
  for p in "$RUNTIME/sp404_daemon_pid" "$RUNTIME/midi_daemon_pid"; do
    dp="$(cat "$p" 2>/dev/null || true)"
    terminate_pid "$dp" "$(basename "$p")"
  done
  rm -f "$SUP_PID" "$RUNTIME/sp404_daemon_pid" "$RUNTIME/midi_daemon_pid" "$RUNTIME/daemon_pid" 2>/dev/null || true
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
atomic_write "$SUP_PID" "$$" || true
log "START supervisor=$$"
if [ -r "$BASE/sp404_gain" ]; then
  cp -f "$BASE/sp404_gain" "$RUNTIME/sp404_gain" 2>/dev/null \
    && log "SP404_GAIN_SEED src=$BASE/sp404_gain val=$(cat "$RUNTIME/sp404_gain" 2>/dev/null || echo unknown)"
fi
# ---- HOST STABLE STATE MACHINE ----
HOST_STATE="SEARCHING"
full_detector_calls=0
stable_watchdog_checks=0
detect_tick=0
stable_tick=0
# cached PCM paths for stable cheap checks
CACHED_SP_PLAY=""
CACHED_SP_CAP=""
CACHED_MIDI_NODE=""
CACHED_GENERATION=""
# persisted counters for diagnose script
persist_counters(){
  atomic_write "$RUNTIME/h38_host_full_detector_calls" "$full_detector_calls" 2>/dev/null || true
  atomic_write "$RUNTIME/h38_host_stable_watchdog_checks" "$stable_watchdog_checks" 2>/dev/null || true
  atomic_write "$RUNTIME/h38_host_state" "$HOST_STATE" 2>/dev/null || true
}
detect_now(){
  full_detector_calls=$((full_detector_calls+1))
  persist_counters
  /bin/sh "$BIN/otg_h37_host_device_detect.sh" >>"$LOG" 2>&1 || true
  log "DETECT_NOW count=$full_detector_calls state=$HOST_STATE"
}
# cheap stable checks: 0=pass 1=fail->needs RECOVERING
stable_checks_pass(){
  # policy must still be host
  policy_host || return 1
  # generation check if file exists (introduced by apply script)
  if [ -e "$RUNTIME/h38_host_generation" ]; then
    cur_gen="$(cat "$RUNTIME/h38_host_generation" 2>/dev/null || echo none)"
    if [ "$CACHED_GENERATION" != "" ] && [ "$cur_gen" != "$CACHED_GENERATION" ]; then
      log "STABLE_CHECK_FAIL generation changed cached=$CACHED_GENERATION cur=$cur_gen"
      return 1
    fi
  fi
  if sp404_wanted; then
    # daemon alive
    pid_alive "$sp_pid" || { log "STABLE_CHECK_FAIL sp404 daemon dead pid=$sp_pid"; return 1; }
    # playback PCM node exists (use cached path if available, else runtime file)
    pp="${CACHED_SP_PLAY:-$(cat "$RUNTIME/sp404_playback_pcm" 2>/dev/null || echo none)}"
    cc="${CACHED_SP_CAP:-$(cat "$RUNTIME/sp404_capture_pcm" 2>/dev/null || echo none)}"
    case "$pp" in none|''|FAILED|/dev/snd/pcmCnoneD*) log "STABLE_CHECK_FAIL sp404 play invalid pp=$pp"; return 1;; esac
    case "$cc" in none|''|FAILED|/dev/snd/pcmCnoneD*) log "STABLE_CHECK_FAIL sp404 cap invalid cc=$cc"; return 1;; esac
    [ -e "$pp" ] || { log "STABLE_CHECK_FAIL sp404 play missing $pp"; return 1; }
    [ -e "$cc" ] || { log "STABLE_CHECK_FAIL sp404 cap missing $cc"; return 1; }
    # FIFO exists (daemon owns it, but node must exist)
    [ -p "$SP404_FIFO" ] || [ -e "$SP404_FIFO" ] || { log "STABLE_CHECK_FAIL sp404 fifo missing"; return 1; }
  fi
  if midi_wanted; then
    pid_alive "$mi_pid" || { log "STABLE_CHECK_FAIL midi daemon dead pid=$mi_pid"; return 1; }
    mn="${CACHED_MIDI_NODE:-$(cat "$RUNTIME/midi_rawmidi" 2>/dev/null || echo none)}"
    case "$mn" in none|''|FAILED) log "STABLE_CHECK_FAIL midi node invalid $mn"; return 1;; esac
    [ -e "/dev/snd/$mn" ] || { log "STABLE_CHECK_FAIL midi rawmidi missing /dev/snd/$mn"; return 1; }
    [ -p "$MIDI_FIFO" ] || [ -e "$MIDI_FIFO" ] || { log "STABLE_CHECK_FAIL midi fifo missing"; return 1; }
  fi
  return 0
}
# full stable conditions met -> transition to STABLE
stable_conditions_met(){
  policy_host || return 1
  if sp404_wanted; then
    pid_alive "$sp_pid" || return 1
    pp="$(cat "$RUNTIME/sp404_playback_pcm" 2>/dev/null || echo none)"
    cc="$(cat "$RUNTIME/sp404_capture_pcm" 2>/dev/null || echo none)"
    case "$pp" in none|''|FAILED|/dev/snd/pcmCnoneD*) return 1;; esac
    case "$cc" in none|''|FAILED|/dev/snd/pcmCnoneD*) return 1;; esac
    [ -e "$pp" ] || return 1
    [ -e "$cc" ] || return 1
    [ -p "$SP404_FIFO" ] || [ -e "$SP404_FIFO" ] || return 1
    # cache validated paths and generation
    CACHED_SP_PLAY="$pp"
    CACHED_SP_CAP="$cc"
    if [ -e "$RUNTIME/h38_host_generation" ]; then
      CACHED_GENERATION="$(cat "$RUNTIME/h38_host_generation" 2>/dev/null || echo 0)"
    else
      CACHED_GENERATION=""
    fi
    return 0
  fi
  if midi_wanted; then
    pid_alive "$mi_pid" || return 1
    mn="$(cat "$RUNTIME/midi_rawmidi" 2>/dev/null || echo none)"
    case "$mn" in none|''|FAILED) return 1;; esac
    [ -e "/dev/snd/$mn" ] || return 1
    [ -p "$MIDI_FIFO" ] || [ -e "$MIDI_FIFO" ] || return 1
    CACHED_MIDI_NODE="$mn"
    if [ -e "$RUNTIME/h38_host_generation" ]; then
      CACHED_GENERATION="$(cat "$RUNTIME/h38_host_generation" 2>/dev/null || echo 0)"
    else
      CACHED_GENERATION=""
    fi
    return 0
  fi
  return 1
}
# Initial detect and gadget teardown (preserved)
[ -r "$BIN/otg_h37_host_device_detect.sh" ] && detect_now
for g in /sys/kernel/config/usb_gadget/r36sx_lgpt_* \
         /sys/kernel/config/usb_gadget/r36sx_uac2_*; do
  [ -d "$g" ] || continue
  if [ -n "$(cat "$g/UDC" 2>/dev/null || true)" ]; then
    log "GADGET_TEARDOWN $g udc=$(cat "$g/UDC" 2>/dev/null || echo none)"
    echo "" > "$g/UDC" 2>/dev/null || true
    sleep 1
    for p in r36s_u241_usb_audio_io r36s_u240_usb_audio_io r36s_au11_usb_audio_io; do
      pidof "$p" >/dev/null 2>&1 && { log "GADGET_DAEMON_STOP name=$p"; pkill -USR1 -x "$p" 2>/dev/null || pkill -USR1 "$p" 2>/dev/null || killall "$p" 2>/dev/null || true; sleep 0.5; }
    done
    detect_now
  fi
done
sp_pid=0
mi_pid=0
backoff=1
persist_counters
while [ "$STOP" -eq 0 ] && policy_host; do
  run_sp404=0; run_midi=0
  sp404_wanted && run_sp404=1
  midi_wanted && run_midi=1

  # ---- STATE-DEPENDENT DETECTION ----
  case "$HOST_STATE" in
    SEARCHING|RECOVERING)
      detect_tick=$((detect_tick + 1))
      if [ "$detect_tick" -ge 2 ]; then
        detect_tick=0
        [ -r "$BIN/otg_h37_host_device_detect.sh" ] && detect_now
      fi
      ;;
    STABLE)
      # In STABLE, no full detector. Lightweight watchdog every 1500ms
      stable_tick=$((stable_tick + 1))
      if [ "$stable_tick" -ge 5 ]; then
        stable_tick=0
        stable_watchdog_checks=$((stable_watchdog_checks+1))
        persist_counters
        if ! stable_checks_pass; then
          log "STABLE_LOST -> RECOVERING"
          HOST_STATE="RECOVERING"
          detect_tick=0
          stable_tick=0
          # ensure guard protects FIFO if daemon dead
          persist_counters
        else
          log "STABLE_WATCHDOG_OK checks=$stable_watchdog_checks"
        fi
      fi
      # immediate daemon death detection each loop (without full scan)
      if [ "$run_sp404" -eq 1 ] && ! pid_alive "$sp_pid"; then
        log "STABLE_DAEMON_DEAD -> RECOVERING pid=$sp_pid"
        HOST_STATE="RECOVERING"
        detect_tick=0
        persist_counters
      fi
      if [ "$run_midi" -eq 1 ] && ! pid_alive "$mi_pid"; then
        log "STABLE_MIDI_DEAD -> RECOVERING pid=$mi_pid"
        HOST_STATE="RECOVERING"
        detect_tick=0
        persist_counters
      fi
      ;;
  esac

  if [ "$run_sp404" -eq 1 ]; then
    if ! pid_alive "$sp_pid"; then
      [ -x "$SP404_DAEMON" ] || { log "ERROR missing_sp404_daemon=$SP404_DAEMON"; exit 31; }
      [ -p "$SP404_FIFO" ] || { rm -f "$SP404_FIFO" 2>/dev/null || true; mkfifo "$SP404_FIFO" 2>/dev/null || true; }
      sp_play="$(cat "$RUNTIME/sp404_playback_pcm" 2>/dev/null || echo none)"
      sp_cap="$(cat "$RUNTIME/sp404_capture_pcm" 2>/dev/null || echo none)"
      case "$sp_play" in none|''|FAILED|/dev/snd/pcmC0D0p)
        card="$(cat "$RUNTIME/sp404_card" 2>/dev/null || echo 1)"
        sp_play="/dev/snd/pcmC${card}D0p"
        sp_cap="/dev/snd/pcmC${card}D0c"
        ;;
      esac
      if [ "$sp_play" = "none" ] || [ "$sp_cap" = "none" ] || [ ! -e "$sp_play" ] || [ ! -e "$sp_cap" ]; then
        log "SP404_NODE_MISSING play=$sp_play cap=$sp_cap usb=$(cat "$RUNTIME/sp404_usb_id" 2>/dev/null || echo none) wait=$backoff state=$HOST_STATE"
        sleep "$backoff"
        [ "$backoff" -lt 4 ] && backoff=$((backoff+1))
        # In RECOVERING, keep probing; in STABLE this path shouldn't happen because stable_checks would have triggered RECOVERING
        continue
      fi
      g_idx="$(echo "$sp_play" | sed -n 's#^/dev/snd/pcmC\([0-9]*\)D.*#\1#p')"
      if [ -n "$g_idx" ] && grep -q "^[[:space:]]*$g_idx \[UAC2Gadget" /proc/asound/cards 2>/dev/null; then
        log "SP404_GADGET_GUARD idx=$g_idx play=$sp_play wait=$backoff"
        sleep "$backoff"
        [ "$backoff" -lt 4 ] && backoff=$((backoff+1))
        continue
      fi
      "$SP404_DAEMON" "$SP404_FIFO" "$sp_play" "$sp_cap" 2 >>"$(pick_log_path "$LOGROOT/H38_SP404_HOST_AUDIO_DAEMON.log")" 2>&1 &
      sp_pid=$!
      atomic_write "$RUNTIME/sp404_daemon_pid" "$sp_pid" || true
      atomic_write "$RUNTIME/daemon_pid" "$sp_pid" || true
      log "SP404_DAEMON_STARTED pid=$sp_pid play=$sp_play cap=$sp_cap state=$HOST_STATE"
      CACHED_SP_PLAY="$sp_play"
      CACHED_SP_CAP="$sp_cap"
      if [ -e "$RUNTIME/h38_host_generation" ]; then
        CACHED_GENERATION="$(cat "$RUNTIME/h38_host_generation" 2>/dev/null || echo 0)"
      fi
      sleep 0.3
    fi
  else
    if pid_alive "$sp_pid"; then terminate_pid "$sp_pid" sp404; sp_pid=0; CACHED_SP_PLAY=""; fi
  fi
  # FIFO-GUARDIAN contract: daemon alive -> OFF, daemon dead -> ON
  if [ "$run_sp404" -eq 1 ] && ! pid_alive "$sp_pid"; then
    guard_start
  else
    guard_stop
  fi

  if [ "$run_midi" -eq 1 ]; then
    if ! pid_alive "$mi_pid"; then
      [ -x "$MIDI_DAEMON" ] || { log "ERROR missing_midi_daemon=$MIDI_DAEMON"; exit 32; }
      midi_node="$(cat "$RUNTIME/midi_rawmidi" 2>/dev/null || echo none)"
      case "$midi_node" in none|''|FAILED) log "MIDI_DEVICE_NONE wait=$backoff state=$HOST_STATE"; sleep "$backoff"; [ "$backoff" -lt 4 ] && backoff=$((backoff+1)); continue;; esac
      [ -p "$MIDI_FIFO" ] || { rm -f "$MIDI_FIFO" 2>/dev/null || true; mkfifo "$MIDI_FIFO" 2>/dev/null || true; }
      "$MIDI_DAEMON" "/dev/snd/$midi_node" "$MIDI_FIFO" >>"$(pick_log_path "$LOGROOT/H38_MIDI_HOST_DAEMON.log")" 2>&1 &
      mi_pid=$!
      atomic_write "$RUNTIME/midi_daemon_pid" "$mi_pid" || true
      atomic_write "$RUNTIME/daemon_pid" "$mi_pid" || true
      log "MIDI_DAEMON_STARTED pid=$mi_pid node=$midi_node state=$HOST_STATE"
      CACHED_MIDI_NODE="$midi_node"
      sleep 0.3
    fi
  else
    if pid_alive "$mi_pid"; then terminate_pid "$mi_pid" midi; mi_pid=0; CACHED_MIDI_NODE=""; fi
  fi

  # ---- STATE TRANSITION TO STABLE ----
  if [ "$HOST_STATE" != "STABLE" ]; then
    if stable_conditions_met; then
      HOST_STATE="STABLE"
      stable_tick=0
      detect_tick=0
      stable_watchdog_checks=0
      persist_counters
      log "HOST_STABLE entered calls=$full_detector_calls"
    fi
  fi

  flush_tick=$((flush_tick+1))
  if [ "$flush_tick" -ge 5 ]; then
    flush_tick=0
    [ "$LIVE_FLUSH" = "1" ] && live_flush
  fi
  sleep 0.3
  backoff=1
done
log "POLICY_EXIT policy=$(cat "$POLICY_FILE" 2>/dev/null || echo missing) state=$HOST_STATE calls=$full_detector_calls stable_checks=$stable_watchdog_checks"
