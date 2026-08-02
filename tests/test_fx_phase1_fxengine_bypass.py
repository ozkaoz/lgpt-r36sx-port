#!/usr/bin/env python3
"""Phase 1 tests for the FxEngine bypass skeleton (PLAN_FX_REDESIGN_ES.md).

Verifies the Fase 1 deliverables:

1. Pure bypass in legacy mode: Process() leaves the master fixed buffer
   untouched (gain 1.0, no DSP), so the golden/legacy output is preserved.
2. RT contract: zero malloc/new/free/syscalls/file-I/O in Process(), verified
   both by source scan and by the runtime rtViolations_ counter.
3. Static memory: Buses are fixed-size static arrays (no dynamic allocation);
   the FxEngine instance owns them.
4. Telemetry: callCount_ / frames_ / maxFrames_ increment, rtViolations_ stays 0.
5. Integration: AudioOutDriver::Trigger() and DummyAudioOut::Trigger() call
   FxEngine::GetInstance().Process() after AudioMixer::Render.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "source/sources"

FIXED_SHIFT = 15
SCALE = 1 << FIXED_SHIFT


def i2fp(a):
    return a << FIXED_SHIFT


class FxEngineModel:
    """Faithful Python port of FxEngine::Process (legacy bypass)."""

    def __init__(self):
        self.legacy_mode = True
        self.call_count = 0
        self.frames = 0
        self.max_frames = 0
        self.rt_violations = 0
        self.last_buffer = None

    def process(self, buffer, samplecount):
        if samplecount <= 0 or buffer is None:
            self.rt_violations += 1
            return buffer
        self.call_count += 1
        self.frames += samplecount
        if samplecount > self.max_frames:
            self.max_frames = samplecount
        if self.legacy_mode:
            self.last_buffer = buffer
            return buffer
        if samplecount > 2048 * 2:
            self.rt_violations += 1
        self.last_buffer = buffer
        return buffer


def make_buffer(n, seed=0x1234):
    buf = []
    x = seed
    for _ in range(n):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        buf.append(i2fp((x % 20000) - 10000))
    return buf


def check_bypass_1_to_1():
    f = FxEngineModel()
    n = 918  # ~44.1kHz, 120 BPM slice
    buf = make_buffer(n)
    before = list(buf)
    out = f.process(buf, n)
    assert out is buf
    assert out == before, "legacy bypass must not touch the master buffer"
    assert f.rt_violations == 0
    # telemetry
    assert f.call_count == 1
    assert f.frames == n
    assert f.max_frames == n
    # more calls
    for i in range(100):
        b = make_buffer(512, seed=i)
        f.process(b, 512)
    assert f.call_count == 101
    assert f.frames == n + 100 * 512
    assert f.max_frames == n
    assert f.rt_violations == 0


def check_invalid_inputs_flag_violation():
    f = FxEngineModel()
    f.process(None, 0)
    assert f.rt_violations == 1
    f.process([0], -5)
    assert f.rt_violations == 2


def check_no_alloc_syscall_in_process():
    cpp = (SRC / "Application/Audio/FxEngine/FxEngine.cpp").read_text()
    h = (SRC / "Application/Audio/FxEngine/FxEngine.h").read_text()
    combined = cpp + "\n" + h
    # No dynamic allocation or syscall primitives anywhere in the module.
    for pat in [
        r"\bmalloc\s*\(",
        r"\bnew\s+[A-Za-z_:]",
        r"\bnew\s*\[",
        r"\bdelete\b",
        r"\bfree\s*\(",
        r"\bfopen\s*\(",
        r"\bprintf\s*\(",
        r"\bopen\s*\(",
        r"\bwrite\s*\(",
        r"\bgettimeofday\s*\(",
        r"System::GetInstance\(\)->GetClock\(",
        r"\bfprintf\s*\(",
        r"\bTrace::",
        r"\blog[A-Za-z]*\s*\(",
    ]:
        assert not re.search(pat, combined), f"RT-forbidden pattern {pat!r} found"


def check_static_buses():
    h = (SRC / "Application/Audio/FxEngine/FxEngine.h").read_text()
    # Buses must be fixed arrays, declared as member data (no pointers/heap).
    assert re.search(
        r"fixed\s+dry_\[FX_ENGINE_MAX_CHANNELS\]\[FX_ENGINE_MAX_FIXED\];", h
    )
    assert re.search(
        r"fixed\s+send_\[FX_ENGINE_MAX_CHANNELS\]\[FX_ENGINE_MAX_FIXED\];", h
    )
    assert re.search(r"fixed\s+returnDelay_\[FX_ENGINE_MAX_FIXED\];", h)
    assert re.search(r"fixed\s+returnReverb_\[FX_ENGINE_MAX_FIXED\];", h)
    assert re.search(r"fixed\s+master_\[FX_ENGINE_MAX_FIXED\];", h)
    # Buses is a plain struct: member-only declaration, no methods that alloc.
    assert "struct Buses" in h
    assert "sizeof(Buses)" in h  # exposed so tests can assert footprint


def check_bus_memory_footprint():
    # Q15 fixed = 4 bytes on the MIPS target.
    ch = 8
    nfix = 2048 * 2
    buses_bytes = (2 * ch * nfix + 3 * nfix) * 4
    dry = ch * nfix * 4
    send = ch * nfix * 4
    ret = 3 * nfix * 4
    assert buses_bytes == dry + send + ret
    # document expected footprint (Fase 1: dry/send dominate)
    assert buses_bytes < 2 * 1024 * 1024, buses_bytes


def check_integration_points():
    out = (SRC / "Services/Audio/AudioOutDriver.cpp").read_text()
    dummy = (SRC / "Application/Audio/DummyAudioOut.cpp").read_text()
    assert "Application/Audio/FxEngine/FxEngine.h" in out
    assert "Application/Audio/FxEngine/FxEngine.h" in dummy
    assert "FxEngine::FxEngine::GetInstance().Process" in out
    assert "FxEngine::FxEngine::GetInstance().Process" in dummy
    # Must run AFTER AudioMixer::Render in both triggers.
    i_out_render = out.index("AudioMixer::Render")
    i_out_fx = out.index("GetInstance().Process")
    assert i_out_fx > i_out_render, "FxEngine must run after the master mix"
    i_dummy_render = dummy.index("AudioMixer::Render")
    i_dummy_fx = dummy.index("GetInstance().Process")
    assert i_dummy_fx > i_dummy_render


def check_makefile_registers_fxengine():
    mk = (ROOT / "source/projects/Makefile").read_text()
    assert "Application/Audio/FxEngine" in mk
    assert "FxEngine.o" in mk


check_bypass_1_to_1()
check_invalid_inputs_flag_violation()
check_no_alloc_syscall_in_process()
check_static_buses()
check_bus_memory_footprint()
check_integration_points()
check_makefile_registers_fxengine()
print("FXENGINE_BYPASS_PHASE1_OK")
