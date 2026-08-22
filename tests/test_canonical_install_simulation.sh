#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PAYLOAD="$ROOT/sd_root"
TMP="$(mktemp -d /tmp/canonical_install_sim.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
SD="$TMP/fake_sd"
mkdir -p "$SD/cubegm/cores" "$SD/cubegm/bios" "$SD/frogui" "$SD/roms/lgpt" "$SD/lgpt/projects" "$SD/lgpt/samples" "$SD/lgpt/otg/bin" "$SD/cubegm/saves" "$SD/PS" "$SD/music"

echo "=== Creating fake TreeFrog SD (stock) ==="
printf '#!/bin/sh\nexec /mnt/sdcard/cubegm/lgpt.elf\n' > "$SD/cubegm/lgpt"
chmod +x "$SD/cubegm/lgpt"
printf 'stock_lgpt_elf_placeholder_528k' > "$SD/cubegm/lgpt.elf"
printf 'fake core' > "$SD/cubegm/cores/snes9x_fake.so"
printf 'fake saves' > "$SD/cubegm/saves/fake.sav"
printf 'user project' > "$SD/lgpt/projects/user.lgptsav"
printf 'user sample' > "$SD/lgpt/samples/user.wav"
printf 'media' > "$SD/music/song.mp3"
cat > "$SD/frogui/core_overrides.txt" <<'OV'
/mnt/sdcard/roms/snes|/mnt/sdcard/cubegm/cores/snes_fake.so
OV
mkdir -p "$SD/roms/snes"
printf 'rom' > "$SD/roms/snes/game.sfc"
echo "FAKE_SD_CREATED $SD"
# Install canonical lgpt_core.so (non-enumerated)
cp -f "$PAYLOAD/cubegm/cores/lgpt_core.so" "$SD/cubegm/cores/lgpt_core.so"
# Launcher
cp -f "$PAYLOAD/cubegm/lgpt" "$SD/cubegm/lgpt"
cp -f "$PAYLOAD/lgpt/otg/bin/otg_u241_common.sh" "$SD/lgpt/otg/bin/otg_u241_common.sh" 2>/dev/null || true
cp -f "$PAYLOAD/lgpt/otg/bin/otg_h37_apply_driver_mode.sh" "$SD/lgpt/otg/bin/otg_h37_apply_driver_mode.sh" 2>/dev/null || true
cp -f "$PAYLOAD/lgpt/otg/bin/otg_h37_host_runtime_supervisor.sh" "$SD/lgpt/otg/bin/otg_h37_host_runtime_supervisor.sh" 2>/dev/null || true
cp -f "$PAYLOAD/lgpt/otg/bin/r36s_sp404_host_audio_io" "$SD/lgpt/otg/bin/r36s_sp404_host_audio_io" 2>/dev/null || true
cp -f "$PAYLOAD/lgpt/otg/bin/r36s_midi_host_io" "$SD/lgpt/otg/bin/r36s_midi_host_io" 2>/dev/null || true
cp -f "$PAYLOAD/lgpt/otg/bin/r36s_u241_usb_audio_io" "$SD/lgpt/otg/bin/r36s_u241_usb_audio_io" 2>/dev/null || true
OVERRIDE="$SD/frogui/core_overrides.txt"
TMP_OV="$OVERRIDE.tmp.$$"
grep -v "cubegm/cores/lgpt" "$OVERRIDE" 2>/dev/null > "$TMP_OV" 2>/dev/null || true
cat "$PAYLOAD/frogui/core_overrides.txt" >> "$TMP_OV"
mv -f "$TMP_OV" "$OVERRIDE"
if ! grep -q "stock_lgpt_elf_placeholder" "$SD/cubegm/lgpt.elf"; then
  echo "FAIL stock lgpt.elf was overwritten" >&2
  exit 1
fi
echo "=== Verification ==="
errors=0
check(){ if eval "$1"; then echo "PASS $2"; else echo "FAIL $2" >&2; errors=$((errors+1)); fi; }
check "[ -s $SD/cubegm/cores/lgpt_core.so ]" "canonical exists"
check "cmp -s $PAYLOAD/cubegm/cores/lgpt_core.so $SD/cubegm/cores/lgpt_core.so" "canonical SHA matches payload"
check "[ ! -e $SD/cubegm/cores/lgpt_libretro.so ] && [ ! -e $SD/cubegm/cores/lgpt_r36sx_port_libretro.so ]" "no enumerated lgpt in active cores (single entry)"
check "! ls $SD/cubegm/cores/*_libretro.so 2>/dev/null | grep -q lgpt" "no lgpt libretro enumerated (picker single)"
check "grep -q \"lgpt_core.so\" $SD/cubegm/lgpt" "launcher canonical"
check "! grep -q \"lgpt_r36sx_port\" $SD/cubegm/lgpt" "launcher no legacy"
check "grep -q \"lgpt_core.so\" $SD/frogui/core_overrides.txt" "override canonical"
check "! grep -q \"lgpt_r36sx_port\" $SD/frogui/core_overrides.txt" "override no legacy"
check "grep -q \"snes_fake\" $SD/frogui/core_overrides.txt" "other overrides preserved"
check "[ -s $SD/cubegm/cores/snes9x_fake.so ]" "other cores intact"
check "[ -s $SD/lgpt/projects/user.lgptsav ]" "user projects intact"
check "[ -s $SD/lgpt/samples/user.wav ]" "user samples intact"
check "[ -s $SD/cubegm/saves/fake.sav ]" "saves intact"
check "[ -s $SD/music/song.mp3 ]" "media intact"
check "grep -q stock_lgpt_elf_placeholder $SD/cubegm/lgpt.elf" "stock elf unchanged"
check "[ -s $SD/lgpt/otg/bin/r36s_sp404_host_audio_io ]" "P3 daemon installed"
count=$(ls $SD/cubegm/cores/*_libretro.so 2>/dev/null | wc -l)
echo "ENUMERATED_LIBRETRO_COUNT=$count (lgpt_core not counted)"
check "[ $count -eq 1 ]" "enumerated count sanity (only snes_fake, no lgpt)"
echo "EXPECTED_LGPT_PICKER_ENTRY_COUNT=1 (standalone cubegm/lgpt only)"
if (( errors == 0 )); then
  echo "INSTALL_SIMULATION_OK"
  exit 0
else
  echo "INSTALL_SIMULATION_FAIL errors=$errors" >&2
  exit 1
fi