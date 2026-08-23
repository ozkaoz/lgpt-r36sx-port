# LGPT for R36SX

Little Piggy Tracker port for the R36SX V2.6 handheld, integrated with TreeFrogUI and bidirectional USB-C OTG audio.

**Current release: Bacon 1.5 — Latest**

Physically validated on R36SX. Single LGPT TreeFrogUI integration, Local / Windows / Android audio, SP404 USB Audio Host, Synths, EQ8 / Analyzer, Pitch / Chopper, Startup project actions via SELECT (Rename / Duplicate / Export / Delete), New: A random / START confirm, Save Song As under projects/
See [CHANGELOG](CHANGELOG.md).

## Main features

- Three selectable USB audio driver modes: Local, Windows (UAC2) and Android (H38 bridge, LGPTUsbAudioBridge-H38-debug.apk).
- LGPT audio output to Windows through USB Audio at 48 kHz.
- Windows audio capture from the LGPT Record screen.
- Android bidirectional audio bridge with the LGPTUsbAudioBridge APK.
- Input monitoring only while Record is open.
- Mixer view with live VU meters and per-instrument FX menu (`R2 + A`, `R1 + A` toggles Solo).
- Sidebar pattern table at bottom-left.
- Phrase FX commands reduced to beatmaking essentials.
- Transactional recording with Preview, Save and Discard.
- Safe sample rename and deletion from the sample browser.
- Chopper global Undo/Redo with `L1+X` and `R1+X`.
- Chord-aware input handling for the R36SX controls.
- Contextual help overlay (`SELECT+R1`) with per-view controls.
- TreeFrogUI integration for kernel `4.4.186-release`.

## How it works — navigation

| Combination | Action |
|---|---|
| `R1 + LEFT/RIGHT` | Switch main view (Song / Chain / Phrase / Instrument / Table / Groove / Mixer) |
| `SELECT + R1` | Contextual help overlay (latched; release to close) |
| `SELECT + R2` | Audio Driver dialog (USB mode) |
| `START` | Play / Stop |

In the **Mixer**, `SELECT` cycles the pages MIX → DELAY → REVERB → EQ → COMP.
On the FX pages, `UP/DN` moves the row, `L/R` edits, `A` is coarse, and
`BYPASS` is always the first row (`ON = effect disabled`). See
[docs/CONTROLS_EN.md](docs/CONTROLS_EN.md) (English) /
[docs/CONTROLS_ES.md](docs/CONTROLS_ES.md) (Español) for the full key map,
sample browser and chopper shortcuts.

## Download

Use the latest GitHub Release **Bacon-1.5** to install the precompiled port. The repository contains source code; release assets contain the compiled core and SD installer.

**Download:**
- `LGPT_R36SX_Bacon-1.5_SD_ROOT.zip` (direct-copy SD root, 7-8M) — extract and copy CONTENTS to SD root
- `LGPT_R36SX_Bacon-1.5_Android.apk` (H38, 298118, validated) — separate asset and also inside ZIP at root

- [Installation guide — English](docs/INSTALL_EN.md)
- [Guía de instalación — Español](docs/INSTALL_ES.md)
- [Build guide — English](docs/BUILD_EN.md)
- [Guía de compilación — Español](docs/BUILD_ES.md)
- [Controls — English](docs/CONTROLS_EN.md)
- [Controles — Español](docs/CONTROLS_ES.md)
- [USB audio architecture](docs/AUDIO_OTG.md)
- [Troubleshooting — English](docs/TROUBLESHOOTING_EN.md)
- [Solución de problemas — Español](docs/TROUBLESHOOTING_ES.md)

## Repository layout

- `source/`: current LGPT/TreeFrog source.
- `device/`: R36SX launcher, USB Audio daemon and OTG scripts.
- `deployment/`: files installed on the SD card.
- `recovery/`: validated UAC2 kernel module.
- `kernel_module_tools/`: module rebuild and verification tools.
- `scripts/`: current build, install, verification and recovery commands.
- `tests/`: current regression tests.
- `docs/`: consolidated user and developer documentation.

## License

See [LICENSE](LICENSE). The repository may not contain the complete TreeFrogUI vendor base, but the Bacon-1.5 direct-copy release ZIP (`LGPT_R36SX_Bacon-1.5_SD_ROOT.zip`) contains the required integration/vendor runtime files deliberately packaged for this validated configuration, including `cubegm/picoarch` and `cubegm/lgpt.elf` as applicable.
