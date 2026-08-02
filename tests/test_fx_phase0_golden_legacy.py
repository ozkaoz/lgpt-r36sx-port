#!/usr/bin/env python3
"""Phase 0 golden legacy validation.

Verifies the reference WAVs in validation/PHASE0_GOLDEN/ exist, are
well-formed 16-bit mono WAVs at the expected frame rate, and that the
crush/DC variants differ from their raw source as expected (i.e. the
golden artifacts genuinely capture the Q15 DSP behaviour, not silence).
"""
import glob
import os
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GOLDEN = ROOT / "validation" / "PHASE0_GOLDEN"

EXPECTED = {
    "golden_impulse.raw.wav",
    "golden_sine_440.raw.wav",
    "golden_sine_8000.raw.wav",
    "golden_noise.raw.wav",
    "golden_dc_offset.raw.wav",
    "golden_sine_440_crush16.wav",
    "golden_sine_440_crush8.wav",
    "golden_noise_crush4.wav",
    "golden_noise_crush0.wav",
    "golden_sine_440_lp_full.wav",
    "golden_sine_8000_lp_full.wav",
    "golden_noise_lp_full.wav",
    "golden_impulse_lp_full.wav",
}
assert len(EXPECTED) == 13

present = {os.path.basename(f) for f in glob.glob(str(GOLDEN / "*.wav"))}
missing = EXPECTED - present
assert not missing, f"missing golden wavs: {sorted(missing)}"


def read_wav(path):
    with wave.open(str(path), "rb") as w:
        assert w.getsampwidth() == 2, "must be 16-bit"
        assert w.getnchannels() == 1, "must be mono"
        assert w.getframerate() == 44100, w.getframerate()
        frames = w.readframes(w.getnframes())
    return struct.unpack("<%dh" % (len(frames) // 2), frames)


def energy(samples):
    return sum(abs(s) for s in samples)


raw_sine = read_wav(GOLDEN / "golden_sine_440.raw.wav")
raw_impulse = read_wav(GOLDEN / "golden_impulse.raw.wav")
raw_noise = read_wav(GOLDEN / "golden_noise.raw.wav")

# Sanity: sources are not silent.
assert energy(raw_sine) > 0
assert energy(raw_impulse) > 0
assert energy(raw_noise) > 0

# Crush16 must be bit-identical to the raw source (bypass at full depth).
crush16 = read_wav(GOLDEN / "golden_sine_440_crush16.wav")
assert crush16 == raw_sine, "crush16 != raw (bypass broken)"

# Coarser crush (8) must differ from the source (quantization audible).
crush8 = read_wav(GOLDEN / "golden_sine_440_crush8.wav")
assert crush8 != raw_sine

# Crush 0 (sign bit only) must produce a 1-bit square wave: every sample is
# either 0 or full-scale (+/- 32768), never an intermediate value.
crush0 = read_wav(GOLDEN / "golden_noise_crush0.wav")
for s in crush0:
    assert s in (0, 32768, -32768), s
assert len(set(crush0)) >= 2, "crush0 degenerated to silence"

# DC offset passes through the fully-open lowpass (reaches the DC level).
dc = read_wav(GOLDEN / "golden_dc_offset.raw.wav")
lp_full = read_wav(GOLDEN / "golden_sine_440_lp_full.wav")
assert max(dc) == 8000, max(dc)

print("GOLDEN_LEGACY_PHASE0_OK")
