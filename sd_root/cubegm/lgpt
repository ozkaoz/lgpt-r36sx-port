#!/bin/sh
# LGPT R36SX U2.52.4 copy-root + ALSA/UAC2 validated
# Default device root remains /mnt/sdcard. LGPT_SD_ROOT is only for host tests.
set -u

ROOT="${LGPT_SD_ROOT:-/mnt/sdcard}"
PICO="${LGPT_PICO:-$ROOT/cubegm/picoarch}"
CORE="${LGPT_CORE:-$ROOT/cubegm/cores/lgpt_r36sx_port_libretro.so}"
ROM="${1:-${LGPT_ROM:-$ROOT/roms/lgpt/start.lgpt}}"
DATA="${LGPT_DATA:-$ROOT/lgpt}"
LOGROOT="${LGPT_LOGROOT:-$ROOT/LGPT_OTG_LOGS}"
LOG="$LOGROOT/LGPT_U2524_COPYROOT_UAC2_LAUNCHER.log"
CONFIG="$DATA/config.xml"
CONFIG_TEMPLATE="$DATA/config.stock.xml"

mkdir -p "$LOGROOT" 2>/dev/null || exit 20

log() {
    printf '%s\n' "$*" >>"$LOG"
}

fail() {
    code="$1"
    shift
    log "ERROR[$code]: $*"
    exit "$code"
}

config_value() {
    key="$1"
    [ -r "$CONFIG" ] || return 1
    sed -n "s/.*<$key value=\"\([^\"]*\)\".*/\1/p" "$CONFIG" | head -n 1
}

map_sd_path() {
    path="$1"
    case "$path" in
        /mnt/sdcard) printf '%s\n' "$ROOT" ;;
        /mnt/sdcard/*) printf '%s/%s\n' "$ROOT" "${path#/mnt/sdcard/}" ;;
        *) return 1 ;;
    esac
}

log "=== LGPT U2.52.4 copy-root + ALSA/UAC2 validated $(date) ==="
log "ROOT=$ROOT"
log "PICO=$PICO"
log "CORE=$CORE"
log "ROM=$ROM"
log "DATA=$DATA"

# Restore a known-good configuration only when config.xml is absent or empty.
if [ ! -s "$CONFIG" ]; then
    [ -s "$CONFIG_TEMPLATE" ] || fail 21 "Missing $CONFIG and fallback $CONFIG_TEMPLATE"
    cp "$CONFIG_TEMPLATE" "$CONFIG" 2>>"$LOG" || fail 22 "Cannot restore $CONFIG"
    log "CONFIG_RESTORED=$CONFIG_TEMPLATE"
fi

SAMPLELIB_CONFIG="$(config_value SAMPLELIB 2>/dev/null || true)"
INSTRUMENT_CONFIG="$(config_value INSTRUMENTFOLDER 2>/dev/null || true)"

[ -n "$SAMPLELIB_CONFIG" ] || SAMPLELIB_CONFIG="/mnt/sdcard/lgpt/samples"
[ -n "$INSTRUMENT_CONFIG" ] || INSTRUMENT_CONFIG="/mnt/sdcard/lgpt/instruments"

SAMPLELIB="$(map_sd_path "$SAMPLELIB_CONFIG" 2>/dev/null || true)"
INSTRUMENTS="$(map_sd_path "$INSTRUMENT_CONFIG" 2>/dev/null || true)"

[ -n "$SAMPLELIB" ] || fail 23 "SAMPLELIB must be under /mnt/sdcard: $SAMPLELIB_CONFIG"
[ -n "$INSTRUMENTS" ] || fail 24 "INSTRUMENTFOLDER must be under /mnt/sdcard: $INSTRUMENT_CONFIG"

# These paths are runtime requirements. Git and ZIP do not retain empty folders,
# so the launcher also provisions them on every start. U2.52.6: legacy
# single-plural $DATA/samplelib and $DATA/project are intentionally NOT
# provisioned anymore; the sample browser uses SAMPLELIB (lgpt/samples) and
# projects live in $DATA/projects.
mkdir -p \
    "$DATA" \
    "$SAMPLELIB" \
    "$SAMPLELIB/records" \
    "$INSTRUMENTS" \
    "$DATA/projects" \
    "$DATA/tmp/record" \
    "$DATA/usbrecs" \
    "$DATA/otg/logs" \
    "$DATA/otg/logs/runtime_state" \
    2>>"$LOG" || fail 25 "Cannot create LGPT runtime directories"

[ -d "$SAMPLELIB" ] || fail 26 "SAMPLELIB is not a directory: $SAMPLELIB"
[ -d "$INSTRUMENTS" ] || fail 27 "INSTRUMENTFOLDER is not a directory: $INSTRUMENTS"
[ -w "$SAMPLELIB" ] || fail 28 "SAMPLELIB is not writable: $SAMPLELIB"

log "SAMPLELIB_CONFIG=$SAMPLELIB_CONFIG"
log "SAMPLELIB_RESOLVED=$SAMPLELIB"
log "SAMPLELIB_OK=1"
log "INSTRUMENTFOLDER_CONFIG=$INSTRUMENT_CONFIG"
log "INSTRUMENTFOLDER_RESOLVED=$INSTRUMENTS"
log "INSTRUMENTFOLDER_OK=1"

[ -x "$PICO" ] || fail 30 "TreeFrogUI picoarch missing or not executable: $PICO"
[ -s "$CORE" ] || fail 31 "LGPT core missing or empty: $CORE"
[ -r "$ROM" ] || fail 32 "TreeFrogUI launcher entry missing: $ROM"
[ -r "$CONFIG" ] || fail 33 "LGPT config not readable: $CONFIG"

if command -v file >/dev/null 2>&1; then
    file "$CORE" >>"$LOG" 2>&1 || true
fi

# U2.52.5 ALWAYS_START_LOCAL_CONSOLE:
# The audio driver must always boot in Local Console, regardless of the mode
# persisted by a previous session (the Audio Driver modal can still switch
# modes in-session; the next boot returns to Local Console).
printf 'LOCAL_CONSOLE\n' >"$DATA/otg/audio_driver_mode" 2>>"$LOG" || true
printf 'LOCAL_CONSOLE\n' >"$DATA/otg/audio_driver_policy" 2>>"$LOG" || true
log "AUDIO_DRIVER_MODE_FORCED=LOCAL_CONSOLE"

# OTG setup remains nonblocking; USB failure must never block local console use.
if [ -x "$DATA/otg/bin/otg_u241_setup_once.sh" ]; then
    "$DATA/otg/bin/otg_u241_setup_once.sh" >>"$LOG" 2>&1 &
fi

cd "$DATA" 2>>"$LOG" || fail 34 "Cannot enter LGPT data directory: $DATA"
log "EXEC=$PICO $CORE $ROM"
exec "$PICO" "$CORE" "$ROM" >>"$LOG" 2>&1
