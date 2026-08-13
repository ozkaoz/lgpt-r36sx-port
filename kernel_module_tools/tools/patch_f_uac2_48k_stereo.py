#!/usr/bin/env python3
"""
Patch Linux 4.4.186 f_uac2.c into the R36SX U2-48K-STEREO variant.

Keeps the four synchronous (SYNC) isochronous endpoints and the INT_FIXED
clock sources (the config that removed Windows Code 10), and adds:

1. Clock-rate mapping fix: USB_OUT_CLK_ID (5) -> playback rate (p_srate),
   USB_IN_CLK_ID (6) -> capture rate (c_srate). Upstream had them swapped.
2. Diagnostic logging of every host control request (class setup phase)
   and every afunc_set_alt() invocation, so the console dmesg shows exactly
   what the host asks before opening a stream.
3. Build marker MODULE_INFO(r36sx_build, "R36SX_U2_48K_STEREO_2026").
4. The endpoint SYNC attributes are asserted present (4x) and preserved.

No other descriptor or ALSA engine changes.
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

MARKER = "R36SX_U2_48K_STEREO_2026"

ASYNC_LINE = (
    ".bmAttributes = USB_ENDPOINT_XFER_ISOC | "
    "USB_ENDPOINT_SYNC_ASYNC,"
)
SYNC_LINE = (
    ".bmAttributes = USB_ENDPOINT_XFER_ISOC | "
    "USB_ENDPOINT_SYNC_SYNC,"
)

# Same user-facing interface names as the historical AU8-SYNC variant.
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


def fail(message: str) -> NoReturn:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(2)


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
    if MARKER in original:
        fail("Source is already patched with the 48k-stereo marker.")

    patched = original

    # ---- 1. flip the four endpoints ASYNC -> SYNC ------------------------
    # Pristine 4.4.186 declares all four isoch endpoints asynchronously,
    # which Windows rejects with Code 10. The historical AU8-SYNC variant
    # flips them to SYNC; we do the same here.
    async_count = original.count(ASYNC_LINE)
    if async_count != 4:
        fail(f"Expected exactly four asynchronous endpoints; found "
             f"{async_count}.")
    if original.count(SYNC_LINE) != 0:
        fail("Source already contains synchronous endpoint descriptors.")

    patched = original.replace(ASYNC_LINE, SYNC_LINE)

    # 1b. user-visible USB Audio interface strings (historical variant kept
    #     the "R36SX USB AUDIO" names; Windows shows these in the sound
    #     device picker and Device Manager).
    string_changes = {}
    for before, after in STRING_REPLACEMENTS.items():
        count = patched.count(before)
        if count != 1:
            fail(
                f"Expected exactly one occurrence of {before}; found {count}."
            )
        patched = patched.replace(before, after, 1)
        string_changes[before] = after

    # ---- 2. fix the clock-rate mapping inversion -------------------------
    # in_rq_cur(): CLK5 (USB_OUT_CLK_ID) feeds the USB-OUT/ALSA-CAPTURE side,
    #              CLK6 (USB_IN_CLK_ID) feeds the USB-IN/ALSA-PLAYBACK side.
    cur_block_old = (
        "		if (entity_id == USB_IN_CLK_ID)\n"
        "			c.dCUR = cpu_to_le32(p_srate);\n"
        "		else if (entity_id == USB_OUT_CLK_ID)\n"
        "			c.dCUR = cpu_to_le32(c_srate);\n"
    )
    cur_block_new = (
        "		if (entity_id == USB_OUT_CLK_ID)\n"
        "			c.dCUR = cpu_to_le32(p_srate);\n"
        "		else if (entity_id == USB_IN_CLK_ID)\n"
        "			c.dCUR = cpu_to_le32(c_srate);\n"
    )
    if cur_block_old not in patched:
        fail("in_rq_cur clock mapping block not found (already fixed?).")
    patched = patched.replace(cur_block_old, cur_block_new, 1)

    range_block_old = (
        "		if (entity_id == USB_IN_CLK_ID)\n"
        "			r.dMIN = cpu_to_le32(p_srate);\n"
        "		else if (entity_id == USB_OUT_CLK_ID)\n"
        "			r.dMIN = cpu_to_le32(c_srate);\n"
    )
    range_block_new = (
        "		if (entity_id == USB_OUT_CLK_ID)\n"
        "			r.dMIN = cpu_to_le32(p_srate);\n"
        "		else if (entity_id == USB_IN_CLK_ID)\n"
        "			r.dMIN = cpu_to_le32(c_srate);\n"
    )
    if range_block_old not in patched:
        fail("in_rq_range clock mapping block not found (already fixed?).")
    patched = patched.replace(range_block_old, range_block_new, 1)

    # ---- 3. diagnostic logging -------------------------------------------
    # 3a. afunc_set_alt(): log every invocation with interface and alt.
    set_alt_anchor = (
        "	/* No i/f has more than 2 alt settings */\n"
        "	if (alt > 1) {"
    )
    if set_alt_anchor not in patched:
        fail("afunc_set_alt anchor not found.")
    patched = patched.replace(
        set_alt_anchor,
        "	dev_info(&uac2->pdev.dev, \"set_alt intf=%u alt=%u\\n\",\n"
        "		 intf, alt);\n"
        "\n"
        + set_alt_anchor,
        1,
    )

    # 3b. afunc_setup(): log every class request reaching the function.
    setup_anchor = (
        "	/* Only Class specific requests are supposed to reach here */\n"
        "	if ((cr->bRequestType & USB_TYPE_MASK) != USB_TYPE_CLASS)"
    )
    if setup_anchor not in patched:
        fail("afunc_setup anchor not found.")
    patched = patched.replace(
        setup_anchor,
        "	dev_info(&uac2->pdev.dev,\n"
        "		 \"class req bt=0x%02x bReq=0x%02x wVal=0x%04x \"\n"
        "		 \"wIdx=0x%04x wLen=%u\\n\",\n"
        "		 cr->bRequestType, cr->bRequest, le16_to_cpu(cr->wValue),\n"
        "		 le16_to_cpu(cr->wIndex), le16_to_cpu(cr->wLength));\n"
        "\n"
        + setup_anchor,
        1,
    )

    # 3c. out_rq_cur(): log SET_CUR sampling-frequency attempts.
    out_cur_old = (
        "	if (control_selector == UAC2_CS_CONTROL_SAM_FREQ)\n"
        "		return w_length;\n"
        "\n"
        "	return -EOPNOTSUPP;\n"
    )
    out_cur_new = (
        "	struct audio_dev *agdev;\n"
        "	if (control_selector == UAC2_CS_CONTROL_SAM_FREQ) {\n"
        "		u16 w_index = le16_to_cpu(cr->wIndex);\n"
        "		agdev = func_to_agdev(fn);\n"
        "		dev_info(&agdev->uac2.pdev.dev,\n"
        "			 \"set_cur sam_freq entity=%u len=%u\\n\",\n"
        "			 (w_index >> 8) & 0xff, w_length);\n"
        "		return w_length;\n"
        "	}\n"
        "\n"
        "	return -EOPNOTSUPP;\n"
    )
    if out_cur_old not in patched:
        fail("out_rq_cur block not found.")
    patched = patched.replace(out_cur_old, out_cur_new, 1)

    # ---- 4. marker --------------------------------------------------------
    marker_anchor = 'static const char *uac2_name = "snd_uac2";'
    if marker_anchor not in patched:
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

    # ---- assertions on the result -----------------------------------------
    if patched.count(SYNC_LINE) != 4:
        fail("Patched source lost a synchronous endpoint.")
    if patched.count(ASYNC_LINE) != 0:
        fail("An asynchronous endpoint descriptor remains after patch.")
    if "set_alt intf=%u alt=%u" not in patched:
        fail("set_alt logging missing.")
    if "class req bt=0x%02x" not in patched:
        fail("afunc_setup logging missing.")
    if MARKER not in patched:
        fail("Marker missing.")

    if args.backup:
        args.backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, args.backup)

    source.write_text(patched, encoding="utf-8")
    patched_hash = hashlib.sha256(patched.encode("utf-8")).hexdigest()

    report = {
        "patch": "U2_48K_STEREO",
        "source": str(source),
        "original_sha256": original_hash,
        "patched_sha256": patched_hash,
        "endpoints": "SYNC x4 (preserved)",
        "clock_mapping": {
            "USB_OUT_CLK_ID": "p_srate (playback) [was c_srate]",
            "USB_IN_CLK_ID": "c_srate (capture) [was p_srate]",
        },
        "diagnostics": [
            "afunc_set_alt logging",
            "afunc_setup class-request logging",
            "out_rq_cur SET_CUR sam_freq logging",
        ],
        "marker": MARKER,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print("PATCH_U2_48K_STEREO_OK")
    print(f"ORIGINAL_SHA256={original_hash}")
    print(f"PATCHED_SHA256={patched_hash}")
    print("SYNCHRONOUS_ENDPOINTS=4")
    print(f"MARKER={MARKER}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
