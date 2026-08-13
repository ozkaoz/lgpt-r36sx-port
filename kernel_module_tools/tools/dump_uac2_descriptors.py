#!/usr/bin/env python3
"""
Dump the UAC2 descriptor bundle from the built usb_f_uac2.ko (ET_REL).

Kernel 4.4.186's f_uac2.c keeps every descriptor struct and pointer array in
.data; the pointer slots already contain the in-section byte offsets, so no
relocation pass is needed: each *desc[] array entry is read directly and its
target decoded from .data.

Usage: dump_uac2_descriptors.py usb_f_uac2.ko
"""
import struct
import sys

ko = sys.argv[1]
data = open(ko, "rb").read()
assert data[:4] == b"\x7fELF", "not ELF"

e_shoff = struct.unpack_from("<I", data, 0x20)[0]
e_shentsize = struct.unpack_from("<H", data, 0x2E)[0]
e_shnum = struct.unpack_from("<H", data, 0x30)[0]
shstrndx = struct.unpack_from("<H", data, 0x32)[0]

secs = []
for i in range(e_shnum):
    n, t, f, a, o, s = struct.unpack_from("<IIIIII", data, e_shoff + i * e_shentsize)
    secs.append((n, t, f, a, o, s))
shstr = data[secs[shstrndx][4]:secs[shstrndx][4] + secs[shstrndx][5]]


def sec_bytes(name):
    for n, t, f, a, o, s in secs:
        nex = shstr.index(b"\x00", n)
        if shstr[n:nex].decode("latin-1") == name:
            return data[o:o + s]
    return None


strtab = sec_bytes(".strtab")
data_blob = sec_bytes(".data")
symtab = sec_bytes(".symtab")
assert data_blob is not None and symtab is not None

symbols = {}
for k in range(0, len(symtab) - 15, 16):
    n, v, sz, info, other, sh = struct.unpack_from("<IIIBBH", symtab, k)
    nex = strtab.find(b"\x00", n)
    nm = strtab[n:nex].decode("latin-1")
    symbols[nm] = (v, sz, sh)

data_index = None
for k in range(e_shnum):
    nex = shstr.index(b"\x00", secs[k][0])
    if shstr[secs[k][0]:nex].decode("latin-1") == ".data":
        data_index = k
        break

sync_name = {0: "NO_SYNC", 1: "ASYNC", 2: "ADAPTIVE", 3: "SYNC"}
cs_names = {
    0x01: "AC_HEADER", 0x02: "INPUT_TERM", 0x03: "OUTPUT_TERM", 0x04: "MIXER",
    0x05: "SELECTOR", 0x06: "FEATURE", 0x07: "PROCESSING", 0x08: "EXTENSION",
    0x0A: "CLOCK_SRC", 0x0B: "CLOCK_SEL", 0x0C: "CLOCK_MULT",
    0x0D: "SAMPLE_FREQ",
}


def decode_descriptor(raw):
    if len(raw) < 2:
        return None
    blen = raw[0]
    d = raw[1]
    if blen == 0:
        return None
    if d == 0x04:
        return "IF num=%d alt=%d n_eps=%d" % (raw[2], raw[3], raw[4])
    if d == 0x05:
        bm = raw[3]
        sync = (bm >> 2) & 3
        mps = struct.unpack_from("<H", raw, 4)[0]
        return "EP addr=%02x sync=%-8s bm=%02x mps=%d interval=%d" % (
            raw[2], sync_name.get(sync, "?"), bm, mps, raw[6])
    if d == 0x24:
        st = raw[2]
        name = cs_names.get(st, "T%02x" % st)
        extra = ""
        if st == 0x01:
            extra = " bcd=%04x" % (raw[4] | (raw[5] << 8))
        elif st == 0x02 or st == 0x03:
            extra = " term_id=%d type=%04x" % (raw[3], raw[4] | (raw[5] << 8))
        elif st in (0x0A, 0x0B):
            extra = " clk_id=%d attr=%02x %s" % (
                raw[3], raw[4], "INT_FIXED" if raw[4] == 0 else "EXTERNAL/VAR")
        elif st == 0x0C:
            extra = " clk_id=%d n_pins=%d pins=[%s]" % (
                raw[3], raw[5], ",".join(str(x) for x in raw[6:6 + raw[5]]))
        elif st == 0x0D:
            extra = " clk_id=%d src=%d" % (raw[3], raw[4])
        elif st == 0x05:
            extra = " unit=%d n_in=%d" % (raw[3], raw[4])
        elif st == 0x06:
            extra = " unit=%d src=%d" % (raw[3], raw[4])
        return "CS %s%s" % (name, extra)
    if d == 0x25:
        return "CS_EP sel=%d" % raw[2]
    return "T%02x" % d


totals = {"SYNC": 0, "ASYNC": 0, "ADAPTIVE": 0, "NO_SYNC": 0, "INT_FIXED": 0}
for nm in sorted(symbols):
    v, sz, sh = symbols[nm]
    if not nm.endswith("desc") or sh != data_index or sz == 0 or sz > 4096:
        continue
    if sz % 4 != 0 or v % 4 != 0:
        continue
    nptr = sz // 4
    entries = []
    for k in range(nptr):
        val = struct.unpack_from("<I", data_blob, v + k * 4)[0]
        if val == 0:
            entries.append((val, None))
        elif val < len(data_blob):
            entries.append((val, data_blob[val:val + 64]))
        else:
            entries.append((val, "?"))
    has_invalid = any(e[1] == "?" for e in entries)
    if nptr == 1 and not has_invalid and entries[0][1] is None:
        continue
    print("=== ARRAY %s (%d ptrs) ===" % (nm, nptr))
    for k in range(nptr):
        val, e = entries[k]
        if e is None:
            print("  [%2d] NULL" % k)
            continue
        if e == "?":
            print("  [%2d] ptr out of .data" % k)
            continue
        dec = decode_descriptor(e)
        line = "  [%2d] -> .data+0x%04x : %s" % (k, val, dec)
        print(line)
        if dec:
            for key in totals:
                if ("sync=" + key) in dec:
                    totals[key] += 1
                if key == "INT_FIXED" and "INT_FIXED" in dec:
                    totals[key] += 1

print("DESCRIPTOR_SUMMARY SYNC=%d ASYNC=%d ADAPTIVE=%d NO_SYNC=%d INT_FIXED=%d" % (
    totals["SYNC"], totals["ASYNC"], totals["ADAPTIVE"],
    totals["NO_SYNC"], totals["INT_FIXED"]))