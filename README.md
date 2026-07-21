# LGPT for R36SX

Little Piggy Tracker port for the R36SX handheld, integrated with TreeFrogUI and bidirectional USB-C OTG audio.

**Current stable version: U2.52.3**

## Main features

- LGPT audio output to Windows through USB Audio at 48 kHz.
- Windows audio capture from the LGPT Record screen.
- Input monitoring only while Record is open.
- Transactional recording with Preview, Save and Discard.
- Safe sample rename and deletion from the sample browser.
- Chopper global Undo/Redo with `L1+X` and `R1+X`.
- Chord-aware input handling for the R36SX controls.
- TreeFrogUI integration for kernel `4.4.186-release`.

## Download

Use the latest GitHub Release to install the precompiled port. The repository contains source code; release assets contain the compiled core and SD installer.

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

See [LICENSE](LICENSE). TreeFrogUI and `picoarch` are not redistributed in this repository or in the binary release.
