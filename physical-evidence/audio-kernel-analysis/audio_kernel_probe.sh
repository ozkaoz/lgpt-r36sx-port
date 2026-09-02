#!/bin/sh
LOG=/mnt/sdcard/audio_kernel_probe.log
echo "=== audio_kernel_probe start" > "$LOG"
date >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- uname -a ---" >> "$LOG"; uname -a >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- cat /proc/version ---" >> "$LOG"; cat /proc/version >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- cat /proc/cmdline ---" >> "$LOG"; cat /proc/cmdline >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- cat /proc/modules ---" >> "$LOG"; cat /proc/modules >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- cat /proc/devices ---" >> "$LOG"; cat /proc/devices >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- ls -l /dev/auddec ---" >> "$LOG"; ls -l /dev/auddec >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- ls -l /dev/sndC0i2so ---" >> "$LOG"; ls -l /dev/sndC0i2so >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- ls -l /dev/snd ---" >> "$LOG"; ls -l /dev/snd >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- cat /proc/asound/cards ---" >> "$LOG"; cat /proc/asound/cards >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- ls -l /sys/class/sound ---" >> "$LOG"; ls -l /sys/class/sound >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- ps ---" >> "$LOG"; ps >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- mount ---" >> "$LOG"; mount >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "--- ls /lib/modules ---" >> "$LOG"; ls -l /lib/modules >> "$LOG" 2>&1 || echo "NOT_AVAILABLE" >> "$LOG"
echo "=== audio_kernel_probe end ===" >> "$LOG"
sync
