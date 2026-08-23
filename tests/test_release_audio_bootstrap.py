#!/usr/bin/env python3
"""
tests/test_release_audio_bootstrap.py
Bacon-1.5 golden bootstrap recovery — package baseline must exist after ZIP extraction.

Checks:
- sentinel, profile, mode, policy exist with exact values
- sentinel is empty regular file
- profile STEREO_48K, mode/policy LOCAL_CONSOLE
- active branch clean baseline if present
- no volatile runtime state packaged
"""

import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parents[1]
ROOT = REPO / "sd_root"

MANDATORY = {
    "lgpt/otg/enable_lgpt_uac2_bridge": b"",
    "lgpt/otg/audio_usb_profile": b"STEREO_48K\n",
    "lgpt/otg/audio_driver_mode": b"LOCAL_CONSOLE\n",
    "lgpt/otg/audio_driver_policy": b"LOCAL_CONSOLE\n",
}

# Optional but if present must be clean LOCAL_CONSOLE baseline
OPTIONAL_CLEAN = {
    "lgpt/otg/active_audio_branch": b"audio_driver_local_console\n",
    "lgpt/otg/branches/audio_driver_local_console/MODE": b"LOCAL_CONSOLE\n",
}

VOLATILE_PATTERNS = [
    "daemon_pid",
    "daemon_version",
    "capture_abi",
    "audio_channels",
    "audio_rate",
    "setup_result",
    "aoa_state",
    "aoa_result",
    "sp404_card",
    "sp404_playback_pcm",
    "sp404_capture_pcm",
    "/tmp/",
    ".fifo",
]

def main():
    errors = []
    # Check mandatory
    for rel, expected in MANDATORY.items():
        p = ROOT / rel
        if not p.exists():
            errors.append(f"missing mandatory {rel}")
            continue
        if not p.is_file():
            errors.append(f"{rel} not a regular file")
            continue
        data = p.read_bytes()
        if data != expected:
            errors.append(f"{rel} content mismatch: got {data!r} expected {expected!r}")
        if rel == "lgpt/otg/enable_lgpt_uac2_bridge" and len(data) != 0:
            errors.append(f"{rel} must be empty, size {len(data)}")
    # Check optional clean baseline if present
    for rel, expected in OPTIONAL_CLEAN.items():
        p = ROOT / rel
        if p.exists():
            data = p.read_bytes()
            if data != expected:
                errors.append(f"optional {rel} must be {expected!r}, got {data!r}")
            # Must not be other branch state like USB_OUT etc.
            if rel == "lgpt/otg/active_audio_branch" and data not in (b"audio_driver_local_console\n",):
                errors.append(f"{rel} must be clean LOCAL_CONSOLE baseline, got {data!r}")
    # Ensure no volatile files packaged
    for pattern in VOLATILE_PATTERNS:
        for p in ROOT.rglob("*"):
            if pattern in str(p):
                # Allow the binary names containing those strings? e.g., r36s_aoa_bulk... contains aoa but is binary, not runtime state
                # So only check under lgpt/otg, not bin
                rel = p.relative_to(ROOT).as_posix()
                if rel.startswith("lgpt/otg/") and not rel.startswith("lgpt/otg/bin/") and pattern in rel:
                    errors.append(f"volatile file packaged: {rel}")
                # Also check file content for PID/FIFO?
                if rel.endswith(".fifo") or rel.endswith("_pid"):
                    errors.append(f"volatile file packaged: {rel}")
    # Ensure no other branch MODE files contain non-LOCAL state (if present they must be clean)
    branches_root = ROOT / "lgpt/otg/branches"
    if branches_root.exists():
        for mode_file in branches_root.rglob("MODE"):
            rel = mode_file.relative_to(ROOT).as_posix()
            data = mode_file.read_bytes()
            # Only local_console should exist as baseline; others if present must not be WINDOWS etc. as initial
            if "audio_driver_local_console" not in str(mode_file):
                # If other branches exist, they should still be valid MODE values but not required
                # For strict baseline, warn if they exist with non-LOCAL values that would auto-start
                valid = [b"LOCAL_CONSOLE\n", b"LOCAL_CONSOLE", b"WINDOWS\n", b"USB_OUT\n", b"USB_IN\n", b"MIDI\n"]
                # But spec says do not package state that starts Windows etc. automatically
                # Active branch determines initial, so other branches existence is okay as long as active is local
                pass
            if data not in (b"LOCAL_CONSOLE\n", b"LOCAL_CONSOLE", b"WINDOWS\n", b"USB_OUT\n", b"USB_IN\n", b"MIDI\n", b"USB_DUPLEX\n", b"USB_OUT_OTG\n", b"USB_IN_OTG\n"):
                errors.append(f"{rel} has unexpected MODE content {data!r}")
    if errors:
        print("RELEASE_AUDIO_BOOTSTRAP: FAIL")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("RELEASE_AUDIO_BOOTSTRAP: PASS")
    print("  sentinel empty PASS")
    print("  profile STEREO_48K PASS")
    print("  mode LOCAL_CONSOLE PASS")
    print("  policy LOCAL_CONSOLE PASS")
    print("  no volatile packaged PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
