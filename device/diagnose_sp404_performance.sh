#!/bin/sh
# diagnose_sp404_performance.sh - H39 SP404 performance instrumentation
# Output: /LGPT_OTG_LOGS/SP404_PERF_<timestamp>.txt  (and /tmp/r36sx_lgpt_logs)
set -u
ROOT="${LGPT_SD_ROOT:-/mnt/sdcard}"
RUNTIME="${LGPT_RUNTIME_DIR:-/tmp/r36sx_lgpt_usb}"
LOGROOT="${LGPT_LOGROOT:-/tmp/r36sx_lgpt_logs}"
SD_LOGDIR="/mnt/sdcard/LGPT_OTG_LOGS"
mkdir -p "$LOGROOT" "$SD_LOGDIR" 2>/dev/null || true
TS="$(date +%Y%m%d_%H%M%S 2>/dev/null || echo unknown)"
OUT="$SD_LOGDIR/SP404_PERF_${TS}.txt"
TMP_OUT="$LOGROOT/SP404_PERF_${TS}.txt"
# header
{
echo "===== SP404 PERFORMANCE DIAGNOSTICS $TS ====="
echo "mode=$(cat "$RUNTIME/audio_driver_mode" 2>/dev/null || echo none)"
echo "policy=$(cat "$RUNTIME/audio_driver_policy" 2>/dev/null || echo none)"
echo "generation_host=$(cat "$RUNTIME/h38_host_generation" 2>/dev/null || echo none)"
echo "generation_android=$(cat "$RUNTIME/h35_android_generation" 2>/dev/null || echo none)"
echo "supervisor_pid=$(cat "$RUNTIME/h38_host_supervisor_pid" 2>/dev/null || echo none)"
echo "daemon_pid=$(cat "$RUNTIME/daemon_pid" 2>/dev/null || echo none)"
echo "sp404_daemon_pid=$(cat "$RUNTIME/sp404_daemon_pid" 2>/dev/null || echo none)"
echo "midi_daemon_pid=$(cat "$RUNTIME/midi_daemon_pid" 2>/dev/null || echo none)"
# guardian PID from supervisor log? try pgrep
echo "guardian_pids=$(ps 2>/dev/null | grep -E 'cat.*r36sx_sp404_pcm_fifo' | awk '{print $1}' | tr '\n' ' ' || echo none)"
echo "detector_lock=$(ls -ld /tmp/r36sx_h38_host_detect.lock 2>/dev/null || echo none)"
echo "supervisor_lock=$(ls -ld /tmp/r36sx_h38_host_runtime.lock 2>/dev/null || echo none)"
echo "full_detector_calls=$(cat "$RUNTIME/h38_host_full_detector_calls" 2>/dev/null || echo 0)"
echo "stable_watchdog_checks=$(cat "$RUNTIME/h38_host_stable_watchdog_checks" 2>/dev/null || echo 0)"
echo "host_state=$(cat "$RUNTIME/h38_host_state" 2>/dev/null || echo none)"
echo ""
echo "--- loadavg ---"
cat /proc/loadavg 2>/dev/null || echo "no loadavg"
echo ""
echo "--- /proc/stat (first 20 lines) ---"
head -n 20 /proc/stat 2>/dev/null || cat /proc/stat 2>/dev/null | head -n 20 || echo none
echo ""
echo "--- context switches (ctxt) ---"
grep ctxt /proc/stat 2>/dev/null || echo none
echo ""
echo "--- interrupts ---"
head -n 30 /proc/interrupts 2>/dev/null || echo none
echo ""
echo "--- FIFO diagnostics (via bridge counters if available, else file checks) ---"
echo "pending_samples_file=$(cat /tmp/r36sx_lgpt_usb/sp404_pending 2>/dev/null || echo notrack)"
# Try to get from core if lgpt running: via log?
echo "fifo_exists_sp404=$(ls -l /tmp/r36sx_sp404_pcm_fifo 2>/dev/null || echo missing)"
echo "fifo_exists_midi=$(ls -l /tmp/r36sx_midi_pcm_fifo 2>/dev/null || echo missing)"
echo "fifo_exists_uac2=$(ls -l /tmp/r36sx_uac2_bridge_fifo 2>/dev/null || echo missing)"
echo "pending stage events: see bridge log"
grep -E "fifo backpressure|pending" "$LOGROOT/uac2_bridge_lgpt.log" 2>/dev/null | tail -n 20 || echo "no bridge log"
echo ""
echo "--- PCM paths ---"
echo "sp404_playback_pcm=$(cat "$RUNTIME/sp404_playback_pcm" 2>/dev/null || echo none)"
echo "sp404_capture_pcm=$(cat "$RUNTIME/sp404_capture_pcm" 2>/dev/null || echo none)"
echo "sp404_card=$(cat "$RUNTIME/sp404_card" 2>/dev/null || echo none)"
echo "sp404_usb_id=$(cat "$RUNTIME/sp404_usb_id" 2>/dev/null || echo none)"
echo "sp404_stream_caps=$(cat "$RUNTIME/sp404_stream_caps" 2>/dev/null || echo none)"
echo "midi_rawmidi=$(cat "$RUNTIME/midi_rawmidi" 2>/dev/null || echo none)"
echo "cards:"
cat /proc/asound/cards 2>/dev/null || echo none
echo "snd nodes:"
ls -l /dev/snd 2>/dev/null || echo none
echo ""
echo "--- MUSB role ---"
cat /sys/devices/platform/soc/18844000.usb/musb-hdrc.0.auto/mode 2>/dev/null || find /sys/devices -path '*musb-hdrc.0.auto/mode' -exec cat {} \; 2>/dev/null | head -1 || echo none
echo "UDC bindings:"
for g in /sys/kernel/config/usb_gadget/*/UDC; do [ -e "$g" ] || continue; echo "$g: $(cat "$g" 2>/dev/null || echo none)"; done
echo ""
echo "--- process count ---"
ps 2>/dev/null | wc -l
ps 2>/dev/null | head -n 30
echo ""
echo "--- daemon version ---"
cat "$RUNTIME/daemon_version" 2>/dev/null || echo none
cat "$RUNTIME/capture_abi" 2>/dev/null || echo none
echo "audio_channels=$(cat "$RUNTIME/audio_channels" 2>/dev/null || echo none)"
echo "audio_rate=$(cat "$RUNTIME/audio_rate" 2>/dev/null || echo none)"
echo ""
echo "--- supervisor log tail ---"
tail -n 50 "$LOGROOT/H38_HOST_RUNTIME_SUPERVISOR.log" 2>/dev/null || echo none
echo ""
echo "--- bridge log tail ---"
tail -n 50 "$LOGROOT/uac2_bridge_lgpt.log" 2>/dev/null || echo none
echo "===== END ====="
} > "$TMP_OUT" 2>/dev/null || true
cp -f "$TMP_OUT" "$OUT" 2>/dev/null || true
echo "DIAG written to $TMP_OUT and $OUT" 2>/dev/null || true
cat "$TMP_OUT" 2>/dev/null || true
