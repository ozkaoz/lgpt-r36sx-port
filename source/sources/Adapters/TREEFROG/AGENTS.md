# TREEFROG — Scoped Instructions

> Adds constraints for R36SX / TreeFrog / audio-input pipeline.
> Must not contradict root `AGENTS.md`; always read root first.

---

## Scope

`source/sources/Adapters/TREEFROG/` — TreeFrog audio, bridge, input, 48 kHz stereo pipeline.

## Invariants (R36SX)

```
RATE = 48000 Hz  (TreeFrogAudio.cpp: return 48000; TreeFrogLibretro.cpp timing.sample_rate=48000.0)
CHANNELS = 2 Stereo (SubmitStereo48000 / MixUsbCaptureMonitorStereo48000)
TARGET = R36SX TreeFrogUI, kernel 4.4.186-release
```

- **No accidental mono fallback:** verify `SubmitStereo48000` / `MixUsbCaptureMonitorStereo48000` paths, not `MONO_48K`. No `MONO_48K` as current expected profile — that profile is not Bacon-1.5.
- **No unapproved resampling:** do not add 44100 paths or fallback sample rates without explicit `DECISION` and measurement. Search `44100, mono, fallback, resampling` before approving audio changes.
- **No silent resampling in bridge:** `TreeFrogUac2Bridge` rate must remain 48000; `g_usb_rate`/`g_engine_rate` must agree.

## Validation

Any runtime change here → **physical R36SX PASS required** (boot + 48kHz Stereo + affected feature). `PHYSICAL PASS` is the gate for checkpoint, not `HOST PASS`.

## References

- Contract: `docs/ai/RELEASE_CONTRACT.md`
- Audio map: `docs/AUDIO_OTG.md`
- Decisions: `DEC-2026-08-21-01`/`03`/`30`/`31`/`32`, `DEC-2026-08-23-01..06`
