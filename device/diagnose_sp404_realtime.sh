#!/bin/sh
# diagnose_sp404_realtime.sh - H44 realtime CPU + ALSA + control-plane counters
set -u
RUNTIME="${LGPT_RUNTIME_DIR:-/tmp/r36sx_lgpt_usb}"
LOGROOT="${LGPT_LOGROOT:-/tmp/r36sx_lgpt_logs}"
SD_LOGDIR="/mnt/sdcard/LGPT_OTG_LOGS"
mkdir -p "$LOGROOT" "$SD_LOGDIR" 2>/dev/null || true
TS="$(date +%Y%m%d_%H%M%S 2>/dev/null || echo unknown)"
OUT="$SD_LOGDIR/SP404_REALTIME_${TS}.txt"
TMP_OUT="$LOGROOT/SP404_REALTIME_${TS}.txt"
{
echo "===== SP404 REALTIME DIAG $TS ====="
echo "mode=$(cat "$RUNTIME/audio_driver_mode" 2>/dev/null || echo none)"
echo "policy=$(cat "$RUNTIME/audio_driver_policy" 2>/dev/null || echo none)"
echo "sp404_perf_stats:"
cat "$RUNTIME/sp404_perf_stats" 2>/dev/null || echo "no stats"
echo ""
echo "--- LGPT process CPU ---"
ps 2>/dev/null | grep -E "lgpt|LGPT" | head -n 20 || ps 2>/dev/null | head -n 20
echo ""
echo "--- daemon CPU ---"
ps 2>/dev/null | grep -E "sp404|midi" | head -n 20
echo ""
echo "--- system CPU loadavg ---"
cat /proc/loadavg 2>/dev/null || echo none
echo ""
echo "--- /proc/stat ---"
head -n 20 /proc/stat 2>/dev/null || echo none
echo "ctxt:"
grep ctxt /proc/stat 2>/dev/null || echo none
echo ""
echo "--- interrupts ---"
head -n 30 /proc/interrupts 2>/dev/null || echo none
echo ""
echo "--- softirq ---"
grep softirq /proc/stat 2>/dev/null || echo none
echo ""
echo "--- ALSA poll/write timings (from daemon log) ---"
grep -E "PLAY_WAIT|PLAY_XRUN|ASRC|BRIDGE_PROGRESS" "$LOGROOT/H38_SP404_HOST_AUDIO_DAEMON.log" 2>/dev/null | tail -n 50 || echo "no daemon log"
echo ""
echo "--- ASRC backlog ---"
grep -E "ASRC|backlog" "$LOGROOT/H38_SP404_HOST_AUDIO_DAEMON.log" 2>/dev/null | tail -n 20 || echo none
echo ""
echo "--- XRUN/EIO ---"
grep -E "XRUN|EIO|RECOVER|ERROR" "$LOGROOT/H38_SP404_HOST_AUDIO_DAEMON.log" 2>/dev/null | tail -n 30 || echo none
echo ""
echo "--- control-plane counters ---"
cat "$RUNTIME/sp404_perf_stats" 2>/dev/null || echo "no counters"
echo ""
echo "--- supervisor state ---"
cat "$RUNTIME/h38_host_state" 2>/dev/null || echo none
cat "$RUNTIME/h38_host_full_detector_calls" 2>/dev/null || echo 0
echo ""
echo "--- daemon version ---"
cat "$RUNTIME/daemon_version" 2>/dev/null || echo none
echo "--- FIFO ---"
ls -l /tmp/r36sx_sp404_pcm_fifo 2>/dev/null || echo missing
echo "===== END ====="
} > "$TMP_OUT" 2>/dev/null || true
cp -f "$TMP_OUT" "$OUT" 2>/dev/null || true
cat "$TMP_OUT" 2>/dev/null || true
echo "DIAG written to $TMP_OUT and $OUT"
