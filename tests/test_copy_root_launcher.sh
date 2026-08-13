#!/usr/bin/env bash
set -Eeuo pipefail

TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "$TEST_DIR/../../cubegm/lgpt" ]]; then
    PAYLOAD_ROOT="$(cd "$TEST_DIR/../.." && pwd)"
elif [[ -f "$TEST_DIR/../sd_root/cubegm/lgpt" ]]; then
    PAYLOAD_ROOT="$(cd "$TEST_DIR/../sd_root" && pwd)"
else
    echo 'TEST_FAIL: payload copy-root no encontrado.' >&2
    exit 2
fi
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
SD="$TMP/mnt/sdcard"

mkdir -p "$SD"
cp -a "$PAYLOAD_ROOT/cubegm" "$PAYLOAD_ROOT/lgpt" "$PAYLOAD_ROOT/roms" "$SD/"
mkdir -p "$SD/LGPT_OTG_LOGS"

# Simulate a ZIP/Explorer copy that lost empty runtime folders.
rm -rf \
    "$SD/lgpt/samples" \
    "$SD/lgpt/instruments" \
    "$SD/lgpt/projects" \
    "$SD/lgpt/tmp" \
    "$SD/lgpt/usbrecs"

mkdir -p "$SD/cubegm/cores"
cat > "$SD/cubegm/picoarch" <<'MOCK'
#!/bin/sh
printf 'MOCK_PICOARCH core=%s rom=%s\n' "$1" "$2"
[ -s "$1" ]
[ -r "$2" ]
MOCK
chmod +x "$SD/cubegm/picoarch"
printf 'mock arm core for host layout test\n' > "$SD/cubegm/cores/lgpt_r36sx_port_libretro.so"

sh -n "$SD/cubegm/lgpt"
LGPT_SD_ROOT="$SD" LGPT_LOGROOT="$SD/LGPT_OTG_LOGS" "$SD/cubegm/lgpt"

for rel in \
    lgpt/samples \
    lgpt/samples/records \
    lgpt/instruments \
    lgpt/projects \
    lgpt/tmp/record \
    lgpt/usbrecs; do
    [[ -d "$SD/$rel" ]] || {
        echo "TEST_FAIL missing $rel" >&2
        exit 20
    }
done

LOG="$SD/LGPT_OTG_LOGS/LGPT_U2524_COPYROOT_UAC2_LAUNCHER.log"
grep -F 'SAMPLELIB_OK=1' "$LOG" >/dev/null
grep -F "SAMPLELIB_RESOLVED=$SD/lgpt/samples" "$LOG" >/dev/null
grep -F 'INSTRUMENTFOLDER_OK=1' "$LOG" >/dev/null
grep -F 'MOCK_PICOARCH' "$LOG" >/dev/null

echo 'TEST_LAUNCHER_AUTOCREATES_SAMPLELIB_OK'
