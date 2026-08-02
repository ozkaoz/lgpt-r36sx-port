#!/usr/bin/env python3
"""Phase 0 DSP model tests for the fixed-point bit crusher (CRSH).

Validates the corrected Q15 crush logic in SampleInstrument::Render:
- crush value is clamped to [0,16] (retained bits); 16 = bypass.
- the mask shift is never negative and never exceeds the integer width
  (old code `mask <<= FIXED_SHIFT + shift` on a signed mask was UB).
- the downsample shift is clamped to [0,31].
"""
from pathlib import Path

FIXED_SHIFT = 15
MASK32 = 0xFFFFFFFF


def crush_mask_old(crush_bits):
    shift = 16 - crush_bits
    mask = MASK32
    if shift != 0:
        mask = (mask << (FIXED_SHIFT + shift)) & MASK32
    return mask


def crush_mask_new(crush_bits):
    bits = crush_bits
    if bits > 16:
        bits = 16
    if bits < 0:
        bits = 0
    shift = 16 - bits
    mask = MASK32
    if shift != 0:
        mask = (mask << (FIXED_SHIFT + shift)) & MASK32
    return mask


def downsample_mask(down):
    if down > 31:
        down = 31
    if down < 0:
        down = 0
    return (MASK32 << down) & MASK32


def apply_crush(value_q15, crush_bits, drive_frac=1.0):
    """value_q15: signed int32 in Q15. Returns crushed Q15 (sign-preserving)."""
    masked = apply_mask(value_q15, crush_mask_new(crush_bits))
    return round(masked * drive_frac)


def apply_mask(value_q15, mask):
    """Bitwise AND preserving the bit pattern of a signed 32-bit int."""
    u = value_q15 & MASK32
    r = u & mask
    if r >= 0x80000000:
        r -= 0x100000000
    return r


def check_no_ub():
    for bits in range(0, 256):
        m = crush_mask_new(bits)
        assert 0 <= m <= MASK32, (bits, m)
        dm = downsample_mask(bits)
        assert 0 <= dm <= MASK32, (bits, dm)
    # Old code: negative shift for crush_bits > 16 (undefined behaviour).
    for bits in range(17, 256):
        shift = 16 - bits
        assert shift < 0, bits
    # New code never reaches a negative shift.
    assert crush_mask_new(255) == crush_mask_new(16) == MASK32  # bypass
    assert crush_mask_new(0) == 0x80000000  # sign bit only (hardest crush)


def check_bypass():
    for value in (0, 1, -1, 0x3FFF, -0x4000, 32767, -32768, 0x1FFFFFFF, -0x20000000):
        v = apply_crush(value << FIXED_SHIFT, 16)
        expected = apply_mask(value << FIXED_SHIFT, MASK32)
        assert v == expected, (value, v, expected)


def check_monotonic_bit_depth():
    # mask(bits) = 0xFFFFFFFF << (31 - bits) (with bypass at 16), so a larger
    # bits value clears fewer low bits: in the unsigned bit-pattern domain,
    # retained magnitude is monotonically non-decreasing as bits grow, and
    # full resolution (16) must reproduce the source exactly (bypass).
    for value in (-20000, -100, 0, 100, 20000):
        q = value << FIXED_SHIFT
        u = q & MASK32
        mags = [apply_mask(q, crush_mask_new(bits)) & MASK32 for bits in range(0, 17)]
        assert mags[16] == u, (value, hex(mags[16]), hex(u))  # full res = source
        for lo, hi in zip(mags, mags[1:]):
            assert hi >= lo, (value, hex(lo), hex(hi))
        if value == 0:
            assert all(m == 0 for m in mags)


def check_downsample_no_ub():
    assert downsample_mask(0) == MASK32
    assert downsample_mask(1) == 0xFFFFFFFE
    assert downsample_mask(31) == 0x80000000
    assert downsample_mask(32) == 0x80000000  # clamped
    assert downsample_mask(255) == 0x80000000  # clamped


def check_source_markers():
    root = Path(__file__).resolve().parents[1]
    src = (root / "source/sources/Application/Instruments/SampleInstrument.cpp").read_text()
    for marker in (
        "unsigned int mask=0xFFFFFFFFu",
        "if (crushBits>16) crushBits=16",
        "if (crushBits<0) crushBits=0",
        "unsigned int dsMask=0xFFFFFFFFu<<downsmpl",
        "s2=(fixed)((unsigned int)s2 & mask)",
    ):
        assert marker in src, marker
    assert "fixed mask=0xFFFFFFFF" not in src


check_no_ub()
check_bypass()
check_monotonic_bit_depth()
check_downsample_no_ub()
check_source_markers()
print("DSP_BITCRUSHER_PHASE0_OK")
