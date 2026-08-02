#!/usr/bin/env bash
# Phase 0 golden legacy generator.
# Produces deterministic reference WAVs from the verified Q15 DSP models
# (bit crusher + inline lowpass) into validation/PHASE0_GOLDEN/. These are
# the "legacy reference" artifacts: when the FxEngine lands (fases 1-5),
# its output for the same inputs must match these within tolerance.
#
# Usage: bash scripts/generate_golden_legacy.sh
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/validation/PHASE0_GOLDEN"
mkdir -p "$OUT"

python3 - <<PY
import struct, wave, math
from pathlib import Path

OUT = Path("$OUT")
SR = 44100

def write_wav(path, samples):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = b"".join(struct.pack("<h", max(-32768, min(32767, int(round(s))))) for s in samples)
        w.writeframes(frames)

def sine(freq, n, amp=0.5):
    return [amp * 32767 * math.sin(2 * math.pi * freq * i / SR) for i in range(n)]

def impulse(n):
    s = [0.0] * n
    s[0] = 30000.0
    return s

def noise(n, seed=12345):
    r = seed
    out = []
    for _ in range(n):
        r = (r * 1103515245 + 12345) & 0x7FFFFFFF
        out.append((r / 0x7FFFFFFF) * 2.0 - 1.0)
    return out

# --- Bit crusher model (tests/test_fx_phase0_bitcrusher_model.py) ---
def apply_mask(value, mask):
    u = value & 0xFFFFFFFF
    r = u & mask
    if r >= 0x80000000:
        r -= 0x100000000
    return r

def crush_mask(bits):
    if bits > 16: bits = 16
    if bits < 0: bits = 0
    shift = 16 - bits
    mask = 0xFFFFFFFF
    if shift != 0:
        mask = (mask << (15 + shift)) & 0xFFFFFFFF
    return mask

def bitcrush_wav(label, bits, source):
    mask = crush_mask(bits)
    out = []
    for s in source:
        q = int(round(s)) << 15
        out.append(apply_mask(q, mask) >> 15)
    write_wav(OUT / label, out)

# --- Inline lowpass model (tests/test_fx_phase0_inline_lowpass_model.py) ---
def fl2fp(f): return int(f * (1 << 15))
def i2fp(a): return a << 15
def fp2fl(a): return a / (1 << 15)
def fp_mul(x, y): return (x * y) >> 15

def set_filter(param1, param2, bassy=True):
    f = {}
    f["dirt"] = fp_mul(i2fp(100), i2fp(1) - param1) + fp_mul(i2fp(5000), param1)
    if bassy:
        power = fl2fp(0.6) + fp_mul(param1, fl2fp(3.1))
        freq = fp_mul(fl2fp(pow(10.0, fp2fl(power))), fl2fp(1 / 22050.0))
    else:
        freq = fp_mul(param1, param1)
    f["freq"] = freq
    reso = i2fp(1) - param2
    f["reso"] = (1 << 15) - fp_mul(reso, fp_mul(reso, reso))
    f["mix"] = fp_mul(i2fp(0), fl2fp(1 / 255.0))
    f["mix_inv"] = (1 << 15) - f["mix"]
    f["speed"] = 0
    f["height"] = 0
    f["hipdelay"] = 0
    return f

def lowpass_process(f, s2):
    lpin = fp_mul(s2, f["mix_inv"])
    hpin = -fp_mul(s2, f["mix"])
    difr = lpin - f["height"]
    f["speed"] = fp_mul(f["speed"], f["reso"])
    f["speed"] += fp_mul(difr, f["freq"])
    f["height"] += f["speed"]
    f["height"] += f["hipdelay"] - hpin
    out = f["height"]
    f["hipdelay"] = hpin
    return out

def lowpass_wav(label, param1f, param2f, source):
    f = set_filter(fl2fp(param1f), fl2fp(param2f))
    out = []
    for s in source:
        out.append(lowpass_process(f, int(round(s)) << 15) >> 15)
    write_wav(OUT / label, out)

N = SR * 2  # 2 seconds

write_wav(OUT / "golden_impulse.raw.wav", impulse(N))
write_wav(OUT / "golden_sine_440.raw.wav", sine(440, N))
write_wav(OUT / "golden_sine_8000.raw.wav", sine(8000, N))
write_wav(OUT / "golden_noise.raw.wav", noise(N))
write_wav(OUT / "golden_dc_offset.raw.wav", [8000.0] * N)

bitcrush_wav("golden_sine_440_crush16.wav", 16, sine(440, N))
bitcrush_wav("golden_sine_440_crush8.wav", 8, sine(440, N))
bitcrush_wav("golden_noise_crush4.wav", 4, noise(N))
bitcrush_wav("golden_noise_crush0.wav", 0, noise(N))

lowpass_wav("golden_sine_440_lp_full.wav", 1.0, 0.0, sine(440, N))
lowpass_wav("golden_sine_8000_lp_full.wav", 1.0, 0.0, sine(8000, N))
lowpass_wav("golden_noise_lp_full.wav", 1.0, 0.0, noise(N))
lowpass_wav("golden_impulse_lp_full.wav", 1.0, 0.0, impulse(N))

print("GOLDEN_WAVS_WRITTEN=" + str(len(list(OUT.glob("*.wav")))))
PY
