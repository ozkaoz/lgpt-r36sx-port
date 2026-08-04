#!/bin/sh
# H38.4 host-side runtime supervisor for the unified audio driver.
# Owns the SP404MKII (host UAC2) and USB-MIDI daemons. It only runs when the
# musb controller is in host role and a host-side policy (SP404/USB OUT/MIDI)
# is selected. Selection and role switching are owned by the H38.2 apply script.
set -u
ROOT="${LGPT_SD_ROOT:-/mnt/sdcard}"
BASE="$ROOT/lgpt/otg"
BIN="$BASE/bin"
LOGROOT="${LGPT_LOGROOT:-/mnt/sdcard/LGPT_OTG_LOGS}"
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
# v14.2: USB_DUPLEX_OTG (WINDOWS) is NOT a host policy. In WINDOWS the musb
# controller is bound as a UAC2 gadget, so the host supervisor must exit and
# never spawn the SP404/MIDI daemons there. Keeping USB_DUPLEX_OTG here let a
# leftover supervisor respawn the SP404 daemon during a WINDOWS apply, which
# held the host PCM and produced the HW_PARAMS EIO storms and role races seen
# in the RC9.2 field test.
policy_host(){ case "$(cat "$POLICY_FILE" 2>/dev/null || true)" in SP404_OTG|USB_OUT_OTG|MIDI_OTG) return 0;; *) return 1;; esac; }
sp404_wanted(){ case "$(cat "$POLICY_FILE" 2>/dev/null || true)" in SP404_OTG|USB_OUT_OTG) return 0;; *) return 1;; esac; }
midi_wanted(){ [ "$(cat "$POLICY_FILE" 2>/dev/null || true)" = "MIDI_OTG" ]; }
# v14.1: RO-proof daemon launch. A dirty SD FAT mounted read-only would make
# `daemon >> SD.log &` fail the redirect and the daemon would never start.
pick_log_path(){ p="$1"; if ( : >> "$p" ) 2>/dev/null; then printf '%s' "$p"; else printf '/tmp/%s' "$(basename "$p")"; fi; }
# v14.3: stop daemons gracefully - SIGUSR1 (drain + clean PCM close) before
# SIGTERM/SIGKILL. Killing a live musb URB stream abruptly wedges the
# controller on the Sampler bounce.
terminate_pid(){ p="$1"; name="$2"; [ -n "$p" ] && [ "$p" != "0" ] || return 0; pid_alive "$p" || return 0; log "STOP name=$name pid=$p"; kill -USR1 "$p" 2>/dev/null || true; n=0; while pid_alive "$p" && [ "$n" -lt 40 ]; do sleep 0.05; n=$((n+1)); done; pid_alive "$p" || { wait "$p" 2>/dev/null || true; return 0; }; kill "$p" 2>/dev/null || true; n=0; while pid_alive "$p" && [ "$n" -lt 20 ]; do sleep 0.05; n=$((n+1)); done; pid_alive "$p" && kill -9 "$p" 2>/dev/null || true; wait "$p" 2>/dev/null || true; }
cleanup(){
  STOP=1
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
# Run the host-side device probe so SP404/MIDI markers are current.
detect_now(){ /bin/sh "$BIN/otg_h37_host_device_detect.sh" >>"$LOG" 2>&1 || true; }
[ -r "$BIN/otg_h37_host_device_detect.sh" ] && detect_now
sp_pid=0
mi_pid=0
backoff=1
# Re-probe periodically: the USB host role switch and the SP404/MIDI
# enumeration may lag the first probe, and the device can be hot-plugged after
# the supervisor starts. The daemons read these markers live, so refreshing
# them unblocks a late-arriving card without restarting anything.
detect_tick=0
while [ "$STOP" -eq 0 ] && policy_host; do
  run_sp404=0; run_midi=0
  sp404_wanted && run_sp404=1
  midi_wanted && run_midi=1
  detect_tick=$((detect_tick + 1))
  if [ "$detect_tick" -ge 3 ]; then
    detect_tick=0
    [ -r "$BIN/otg_h37_host_device_detect.sh" ] && detect_now
  fi

  if [ "$run_sp404" -eq 1 ]; then
    if ! pid_alive "$sp_pid"; then
      [ -x "$SP404_DAEMON" ] || { log "ERROR missing_sp404_daemon=$SP404_DAEMON"; exit 31; }
      [ -p "$SP404_FIFO" ] || { rm -f "$SP404_FIFO" 2>/dev/null || true; mkfifo "$SP404_FIFO" 2>/dev/null || true; }
      # Prefer full PCM paths written by the improved detector; fall back to
      # the old card-index marker for backward compatibility with ABI1.
      sp_play="$(cat "$RUNTIME/sp404_playback_pcm" 2>/dev/null || echo none)"
      sp_cap="$(cat "$RUNTIME/sp404_capture_pcm" 2>/dev/null || echo none)"
      case "$sp_play" in none|''|FAILED|/dev/snd/pcmC0D0p)
        card="$(cat "$RUNTIME/sp404_card" 2>/dev/null || echo 1)"
        sp_play="/dev/snd/pcmC${card}D0p"
        sp_cap="/dev/snd/pcmC${card}D0c"
        ;;
      esac
      if [ "$sp_play" = "none" ] || [ "$sp_cap" = "none" ] || [ ! -e "$sp_play" ] || [ ! -e "$sp_cap" ]; then
        log "SP404_NODE_MISSING play=$sp_play cap=$sp_cap usb=$(cat "$RUNTIME/sp404_usb_id" 2>/dev/null || echo none) wait=$backoff"
        sleep "$backoff"
        [ "$backoff" -lt 4 ] && backoff=$((backoff+1))
        continue
      fi
      # U2.52.5 NO_PROBE_ALL: the first-start --probe-all sequence played
      # ~20-30 s of 1 kHz square waves on the SP404 ("pitido inicial"). The
      # daemon configures the PCM itself (constraint picking), so the probe is
      # no longer needed; start streaming directly.
      "$SP404_DAEMON" "$SP404_FIFO" "$sp_play" "$sp_cap" >>"$(pick_log_path "$LOGROOT/H38_SP404_HOST_AUDIO_DAEMON.log")" 2>&1 &
      sp_pid=$!
      atomic_write "$RUNTIME/sp404_daemon_pid" "$sp_pid" || true
      atomic_write "$RUNTIME/daemon_pid" "$sp_pid" || true
      log "SP404_DAEMON_STARTED pid=$sp_pid play=$sp_play cap=$sp_cap"
      sleep 0.3
    fi
  else
    if pid_alive "$sp_pid"; then terminate_pid "$sp_pid" sp404; sp_pid=0; fi
  fi

  if [ "$run_midi" -eq 1 ]; then
    if ! pid_alive "$mi_pid"; then
      [ -x "$MIDI_DAEMON" ] || { log "ERROR missing_midi_daemon=$MIDI_DAEMON"; exit 32; }
      midi_node="$(cat "$RUNTIME/midi_rawmidi" 2>/dev/null || echo none)"
      case "$midi_node" in none|''|FAILED) log "MIDI_DEVICE_NONE wait=$backoff"; sleep "$backoff"; [ "$backoff" -lt 4 ] && backoff=$((backoff+1)); continue;; esac
      [ -p "$MIDI_FIFO" ] || { rm -f "$MIDI_FIFO" 2>/dev/null || true; mkfifo "$MIDI_FIFO" 2>/dev/null || true; }
      "$MIDI_DAEMON" "/dev/snd/$midi_node" "$MIDI_FIFO" >>"$(pick_log_path "$LOGROOT/H38_MIDI_HOST_DAEMON.log")" 2>&1 &
      mi_pid=$!
      atomic_write "$RUNTIME/midi_daemon_pid" "$mi_pid" || true
      atomic_write "$RUNTIME/daemon_pid" "$mi_pid" || true
      log "MIDI_DAEMON_STARTED pid=$mi_pid node=$midi_node"
      sleep 0.3
    fi
  else
    if pid_alive "$mi_pid"; then terminate_pid "$mi_pid" midi; mi_pid=0; fi
  fi

  sleep 1
  backoff=1
done
log "POLICY_EXIT policy=$(cat "$POLICY_FILE" 2>/dev/null || echo missing)"
