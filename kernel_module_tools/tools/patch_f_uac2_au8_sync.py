#!/usr/bin/env python3
"""
Patch Linux 4.4.186 f_uac2.c into the historical R36SX AU8-SYNC variant.

This deliberately changes only:
- four UAC2 isochronous endpoint synchronization attributes:
  ASYNC -> SYNC;
- user-visible USB Audio interface strings;
- a build marker.

It does not alter the ALSA engine, clock request handler, packet sizes,
channel masks, or ConfigFS attributes.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path
from typing import NoReturn


ASYNC_LINE = (
    ".bmAttributes = USB_ENDPOINT_XFER_ISOC | "
    "USB_ENDPOINT_SYNC_ASYNC,"
)
SYNC_LINE = (
    ".bmAttributes = USB_ENDPOINT_XFER_ISOC | "
    "USB_ENDPOINT_SYNC_SYNC,"
)

STRING_REPLACEMENTS = {
    '"Source/Sink"': '"R36SX USB AUDIO"',
    '"Topology Control"': '"R36SX USB AUDIO"',
    '"USBH Out"': '"R36SX USB AUDIO"',
    '"USBD Out"': '"R36SX USB AUDIO"',
    '"USBH In"': '"R36SX USB AUDIO"',
    '"USBD In"': '"R36SX USB AUDIO"',
    '"Playback Inactive"': '"R36SX USB AUDIO"',
    '"Playback Active"': '"R36SX USB AUDIO"',
    '"Capture Inactive"': '"R36SX USB AUDIO"',
    '"Capture Active"': '"R36SX USB AUDIO"',
}

MARKER = "R36SX_U2414_AU8_SYNC_REPLICA"


def fail(message: str) -> NoReturn:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(2)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_file", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--backup", type=Path)
    args = parser.parse_args()

    source = args.source_file
    if not source.is_file():
        fail(f"Source file not found: {source}")

    original = source.read_text(encoding="utf-8")
    original_hash = hashlib.sha256(original.encode("utf-8")).hexdigest()

    if "f_uac2.c -- USB Audio Class 2.0 Function" not in original:
        fail("The input does not look like Linux f_uac2.c.")
    if "USB_OUT_CLK_ID" not in original or "USB_IN_CLK_ID" not in original:
        fail("Expected Linux 4.4 UAC2 clock topology is absent.")
    if "struct cntrl_range_lay3" not in original:
        fail("Expected fixed-range control structure is absent.")
    if MARKER in original:
        fail("Source is already patched with U2.41.4 marker.")

    async_count = original.count(ASYNC_LINE)
    if async_count != 4:
        fail(
            "Expected exactly four asynchronous endpoint descriptors; "
            f"found {async_count}."
        )
    if original.count(SYNC_LINE) != 0:
        fail("Source already contains synchronous endpoint descriptors.")

    patched = original.replace(ASYNC_LINE, SYNC_LINE)

    string_changes = {}
    for before, after in STRING_REPLACEMENTS.items():
        count = patched.count(before)
        if count != 1:
            fail(
                f"Expected exactly one occurrence of {before}; found {count}."
            )
        patched = patched.replace(before, after, 1)
        string_changes[before] = after

    marker_anchor = 'static const char *uac2_name = "snd_uac2";'
    if marker_anchor not in patched:
        # Some vendor trees omit static.
        marker_anchor = 'const char *uac2_name = "snd_uac2";'
    if marker_anchor not in patched:
        fail("Could not locate uac2_name marker anchor.")

    marker_block = (
        marker_anchor
        + "\n"
        + 'MODULE_INFO(r36sx_build, "'
        + MARKER
        + '");'
    )
    patched = patched.replace(marker_anchor, marker_block, 1)

    if patched.count(SYNC_LINE) != 4:
        fail("Patched source does not contain four synchronous endpoints.")
    if patched.count(ASYNC_LINE) != 0:
        fail("An asynchronous endpoint descriptor remains.")
    if patched.count(MARKER) != 1:
        fail("Build marker validation failed.")

    if args.backup:
        args.backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, args.backup)

    source.write_text(patched, encoding="utf-8")
    patched_hash = sha256(source)

    report = {
        "patch": "U2414_AU8_SYNC_REPLICA",
        "source": str(source),
        "original_sha256": original_hash,
        "patched_sha256": patched_hash,
        "endpoint_changes": {
            "async_before": async_count,
            "sync_after": patched.count(SYNC_LINE),
            "mode": "USB_ENDPOINT_SYNC_SYNC",
            "applied_to": [
                "Full-Speed OUT",
                "High-Speed OUT",
                "Full-Speed IN",
                "High-Speed IN",
            ],
        },
        "string_changes": string_changes,
        "marker": MARKER,
        "clock_handler_modified": False,
        "clock_topology_modified": False,
        "packet_sizes_modified": False,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(
        json.dumps(report, indent=2) + "\n",
        encoding="utf-8",
    )

    print("PATCH_U2414_AU8_SYNC_SOURCE_OK")
    print(f"ORIGINAL_SHA256={original_hash}")
    print(f"PATCHED_SHA256={patched_hash}")
    print("SYNCHRONOUS_ENDPOINTS=4")
    print("WINDOWS_NAME=R36SX USB AUDIO")
    print(f"MARKER={MARKER}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
