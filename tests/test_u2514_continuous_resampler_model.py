#!/usr/bin/env python3
"""Validate the streaming 44.1 kHz -> 48 kHz U2.51.4 resampler."""
from pathlib import Path

DENOMINATOR = 160
INCREMENT = 147
CALLBACK_FRAMES = 735


def produce(fill_frames: int, phase: int) -> tuple[int, int, int]:
    out_frames = 0
    while phase // DENOMINATOR + 1 < fill_frames:
        out_frames += 1
        phase += INCREMENT
    consumed = phase // DENOMINATOR
    assert consumed < fill_frames
    fill_frames -= consumed
    phase -= consumed * DENOMINATOR
    return out_frames, fill_frames, phase


fill = 0
phase = 0
counts = []
callbacks = 3600
for _ in range(callbacks):
    fill += CALLBACK_FRAMES
    out_frames, fill, phase = produce(fill, phase)
    counts.append(out_frames)

assert counts[0] == 799, counts[:3]
assert all(value == 800 for value in counts[1:]), set(counts[1:])
assert fill == 1
assert phase == 13
expected = callbacks * CALLBACK_FRAMES * 160 / 147
assert abs(sum(counts) - expected) < 2.0, (sum(counts), expected)

root = Path(__file__).resolve().parents[1]
source = (root / "source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.cpp").read_text()
for marker in (
    "U2514_CONTINUOUS_FIXED_RATIO_160_147",
    "U2514_RESAMPLE_INPUT_CAPACITY_FRAMES",
    "g_resample_input_fill_frames",
    "idx + 1U >= g_resample_input_fill_frames",
    "memmove(",
):
    assert marker in source, marker
assert "if (next >= frames) next = frames - 1" not in source
print("U2514_CONTINUOUS_RESAMPLER_MODEL_OK")
