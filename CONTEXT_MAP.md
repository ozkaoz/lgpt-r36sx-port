# CONTEXT MAP — Mapa de navegación del repositorio

**Última actualización:** 2026-08-21
**Commit:** e27c741441cffbff56144813323b56759ef2dc58 (feature/bacon-1.5-fx)
**Rama:** feature/bacon-1.5-fx
**Canonical WSL repository:** `/home/dafunknoise/lgpt-repo`
**Remote:** `origin` → https://github.com/ozkaoz/lgpt-r36sx-port.git

> Verificado contra el filesystem real. Todos los paths listados existen en el checkout canónico. Si un path no existe, el mapa está desactualizado y debe corregirse.

---

## Audio (DSP, Mezclador, Drivers)

| Área | Documentos | Código (existencia verificada) | Scripts | Decisiones |
|------|------------|--------------------------------|---------|------------|
| Mezclador / AudioMixer | — | `source/sources/Services/Audio/AudioMixer.cpp/.h` ✅ | — | DECISIONS.md: DEC-2026-08-21-03 |
| Ecualizador (InstrumentEq / ParametricEQ) | — | `source/sources/Application/Audio/InstrumentEq.cpp/.h` ✅<br>`source/sources/Application/Audio/FxEngine/ParametricEQ.cpp/.h` ✅<br>`source/sources/Application/Audio/EqBiquad.h` ✅ | — | DEC-2026-08-21-01, DEC-2026-08-21-03 |
| Compresor | — | `source/sources/Application/Audio/FxEngine/Compressor.cpp/.h` ✅ | — | — |
| Spectrum Analyzer | — | `source/sources/Application/Audio/SpectrumAnalyzer.cpp/.h` ✅ | — | DEC-2026-08-21-04 |
| Delay / Reverb / FxEngine | — | `source/sources/Application/Audio/FxEngine/FxEngine.cpp/.h` ✅<br>`source/sources/Application/Audio/FxEngine/DelayLine.cpp/.h` ✅<br>`source/sources/Application/Audio/FxEngine/Reverb.cpp/.h` ✅ | — | — |
| Drivers SDL / SDL2 | — | `source/sources/Adapters/SDL/Audio/SDLAudioDriver.cpp/.h` ✅<br>`source/sources/Adapters/SDL2/Audio/SDLAudioDriver.cpp/.h` ✅ | — | DEC-2026-08-21-02 |
| Drivers TreeFrog / W32 / Dummy / RTAudio | — | `source/sources/Adapters/TREEFROG/Audio/` ✅ (`TreeFrogAudio.cpp`, `TreeFrogUac2Bridge.cpp`, `TreeFrogAudioDriver.cpp`)<br>`source/sources/Adapters/W32/Audio/` ✅<br>`source/sources/Adapters/Dummy/Audio/` ✅<br>`source/sources/Adapters/RTAudio/` ✅ | — | — |
| Instrumentos (SampleInstrument, BassSynth, PianoSynth) | — | `source/sources/Application/Instruments/` ✅ (`SampleInstrument.cpp/.h`, `BassSynth.cpp/.h`, `PianoSynth.cpp/.h`, `SynthMath.h`) | `tests/host/bass_synth_host_test.cpp` | — |
| AudioEngine / Backend / Router | — | `source/sources/Application/Audio/AudioEngine.h` ✅<br>`source/sources/Application/Audio/AudioBackend.h` ✅<br>`source/sources/Application/Audio/AudioRouter.h` ✅<br>`source/sources/Application/Audio/AudioCapabilities.h` ✅ | — | — |

**Invariantes audio verificados:**
- `source/sources/Adapters/TREEFROG/Audio/TreeFrogAudio.cpp: return 48000;`
- `source/sources/Adapters/TREEFROG/Main/TreeFrogLibretro.cpp: timing.sample_rate = 48000.0;`
- `source/sources/Adapters/TREEFROG/Audio/TreeFrogUac2Bridge.*: SubmitStereo48000 / MixUsbCaptureMonitorStereo48000 / g_usb_rate=48000 / g_engine_rate=48000`

---

## Build (Compilación, Build System)

| Área | Documentos | Código / Scripts (verificado) |
|------|------------|--------------------------------|
| Build principal (WSL / Makefile) | `docs/BUILD_ES.md` ✅ / `docs/BUILD_EN.md` ✅ | `source/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh` ✅ (artefacto canónico: `source/dist/lgpt_libretro.so`) |
| Makefile / Toolchain | — | `source/projects/Makefile` ✅<br>`source/projects/Makefile.TREEFROG` ✅<br>Toolchain: `$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot` ✅ |
| Build output (BUILD) | — | `source/projects/buildTREEFROG/` (objects, .d, .so ignorados por .gitignore) |
| Dist (DEPLOY) | — | `source/dist/lgpt_libretro.so` (canónico para SD) |
| Payload SD (SOURCE) | — | `sd_root/cubegm/cores/lgpt_r36sx_port_libretro.so` ✅ (771c384... obsoleto en repo, afcf5ba en HEAD e27c741, nuevo build cc21ab en source/dist pendiente actualizar sd_root) |
| Scripts de build / auditoría | — | `scripts/build.sh` ✅ (usa /mnt/d/R36S/PORT LPTRACKER/WORK/U2523_SOURCE → legacy; canónico actual es BUILD_TREEFROG_R36SX...)<br>`scripts/audit.sh` ✅<br>`scripts/install.sh` ✅<br>`scripts/verify.sh` ✅ |
| Tests host | — | `tests/host/*_host_test.cpp` ✅<br>`tests/run_host_*.sh` ✅<br>`tests/test_*.py` ✅ |
| Deploy a SD (G:) | — | `scripts/install.sh` → `/mnt/g/cubegm/cores/` ✅ (SD FAT32 60G, /mnt/g montada)<br>`scripts/verify.sh` ✅ |

**Comando canónico build (documentado en CURRENT.md):**
```bash
cd /home/dafunknoise/lgpt-repo/source && bash BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh
# → source/dist/lgpt_libretro.so
# sha256sum source/dist/lgpt_libretro.so
# cp source/dist/lgpt_libretro.so /mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so
# sha256sum /mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so  # comparar
```

---

## UI / Vistas / Input

| Área | Código (verificado) |
|------|---------------------|
| Vistas principales | `source/sources/Application/UI/Views/` ✅ (SongView.cpp, ChainView.cpp, PhraseView.cpp, TableView.cpp, InstrumentView.cpp, InstrumentEqView.cpp, ProjectView.cpp, MixerView.cpp, GrooveView.cpp, etc.) |
| Input / Controllers | `source/sources/Adapters/TREEFROG/Input/` ✅<br>`source/sources/Services/Controllers/` ✅<br>`source/sources/Application/UI/Input/` ✅ |
| Input SDL / SDL2 | `source/sources/Adapters/SDL/Input/` ✅<br>`source/sources/Adapters/SDL2/Input/` ✅ |
| ModalDialogs | `source/sources/Application/UI/Views/ModalDialogs/` ✅ (SelectProjectDialog, ImportSampleDialog, SampleChopperModal, UsbRecordModal, etc.) |
| Help / HelpRegistry | `source/sources/Application/UI/Views/BaseClasses/HelpRegistry.cpp` ✅<br>`source/sources/Application/UI/Views/BaseClasses/HelpOverlay.cpp` ✅<br>`source/sources/Application/UI/Views/BaseClasses/ModalView.cpp` ✅ |
| Navigation | `source/sources/Application/UI/Navigation/NavigationController.cpp` ✅<br>`source/sources/Application/UI/Input/ScenarioCatalog.h` ✅ (56 escenarios) |
| AppWindow | `source/sources/Application/AppWindow.cpp/.h` ✅ |

---

## Instrumentos

| Tipo | Archivos clave (verificado) |
|------|-----------------------------|
| SampleInstrument | `source/sources/Application/Instruments/SampleInstrument.cpp/.h` ✅ |
| BassSynth | `source/sources/Application/Instruments/BassSynth.cpp/.h` ✅ |
| PianoSynth | `source/sources/Application/Instruments/PianoSynth.cpp/.h` ✅ |
| InstrumentEq | `source/sources/Application/Audio/InstrumentEq.cpp/.h` ✅ |
| ParametricEQ | `source/sources/Application/Audio/FxEngine/ParametricEQ.cpp/.h` ✅ |
| EqBiquad | `source/sources/Application/Audio/EqBiquad.h` ✅ |
| SpectrumAnalyzer | `source/sources/Application/Audio/SpectrumAnalyzer.cpp/.h` ✅ |

---

## Configuración / Persistencia / Proyecto

| Área | Código (verificado) |
|------|---------------------|
| Proyecto / Song / Chain / Phrase | `source/sources/Application/Model/` ✅ (Project.cpp, Song.cpp, Chain.cpp, Phrase.cpp, Table.cpp, Groove.cpp) |
| InstrumentBank / SamplePool | `source/sources/Application/Instruments/InstrumentBank.cpp` ✅<br>`source/sources/Application/Instruments/SamplePool.cpp` ✅ |
| Persistencia / Config | `source/sources/Application/Persistency/` ✅<br>`source/sources/Application/Model/Project.cpp` ✅ (lgptsav.dat) |
| Config.xml / sd_root | `sd_root/lgpt/config.xml` ✅<br>`sd_root/lgpt/config.stock.xml` ✅<br>`sd_root/VERSION.txt` ✅ (`BACON_1.2_MIXER_U2.72_H43` en HEAD e27c741; `BACON_1.4` en origin/main 449041f)<br>`sd_root/cubegm/lgpt` (launcher) ✅ |

---

## Tests / Audits / Scripts

| Tipo | Ubicación (verificado) |
|------|------------------------|
| Tests host (C++) | `tests/host/*_host_test.cpp` ✅ |
| Tests host (scripts) | `tests/run_host_*.sh` ✅ |
| Tests Python | `tests/test_*.py` ✅ |
| Auditoría | `scripts/audit.sh` ✅ (AUDIT_CLEAN_MAIN_U2523_OK) |
| Build verification | `scripts/verify.sh` ✅ |
| Host syntax check | `tests/host_syntax_check.sh` / `scripts/rc3_base_syntax_check.sh` ✅ |

---

## Documentación

| Archivo | Propósito | Estado |
|---------|-----------|--------|
| `README.md` | Visión general del proyecto | ✅ |
| `CHANGELOG.md` | Historial de cambios detallado | ✅ |
| `docs/BUILD_ES.md` / `docs/BUILD_EN.md` | Guía de compilación | ✅ |
| `docs/AUDIO_ARCHITECTURE_MAP_ES.md` | Arquitectura de audio | ✅ |
| `docs/INSTALL_ES.md` / `docs/INSTALL_EN.md` | Instalación | ✅ |
| `docs/CONTROLS_ES.md` / `docs/CONTROLS_EN.md` | Controles de usuario | ✅ |
| `docs/AUDIO_OTG.md` | Arquitectura USB OTG Audio | ✅ |
| `docs/REFACTOR_ROADMAP_ES.md` | Roadmap refactor | ✅ (actualizado en e27c741) |
| `docs/RELEASE_BACON_1.5_ES.md` | Release Bacon 1.5 | ✅ |
| `AGENTS.md` / `CURRENT.md` / `DECISIONS.md` | Infraestructura multiagente | ✅ (e27c741) |

---

## SD Card (G:) — Estructura en dispositivo

```
/mnt/g/  (60G FAT32, /mnt/g montada, verificada 2026-08-21 12:51)
├── cubegm/
│   └── cores/
│       └── lgpt_r36sx_port_libretro.so   ← Core en SD 2026-08-21 12:51: cc21ab2623a3dfe22e29074047d81da540f6d3d2f4e1404a1ba5820b1b2345b8 (1.4M) == source/dist cc21ab, built from e27c741 (previo 358eff 1.4M obsoleto, sd_root 771c384 1.3M obsoleto)
├── lgpt/
│   ├── config.xml
│   ├── projects/lgpt_KAOZ/lgptsav.dat   ← Proyecto con EQ hipass 40Hz en banda 0 (según CURRENT.md)
│   └── otg/                              ← Configuración USB OTG
└── VERSION.txt (si existe, en sd_root/VERSION.txt → BACON_1.2_MIXER_U2.72_H43 en HEAD e27c741)
```

**Deploy canónico:**
```
Origen:  /home/dafunknoise/lgpt-repo/source/dist/lgpt_libretro.so
Destino: /mnt/g/cubegm/cores/lgpt_r36sx_port_libretro.so
Verify:  sha256sum local vs SD, size, commit SHA en VERSION.txt
```

---

## Git / Branching

| Rama | Estado | Descripción | HEAD |
|------|--------|-------------|------|
| `feature/bacon-1.5-fx` | **ACTIVA** | Desarrollo Bacon 1.5 (U2.71+ + infra IA) | e27c741 (origin/feature/bacon-1.5-fx) |
| `main` | Estable | Bacon 1.4 (U2.54) | 449041f (origin/main, tag golden-bacon-1.4) |
| `refactor/bacon-1.2.1-preserve` | Obsoleta (remota borrada [gone]) | Histórico, HEAD local previo 9b0009d | 9b0009d [gone] - no usar |

- `origin` → https://github.com/ozkaoz/lgpt-r36sx-port.git
- `feature/bacon-1.5-fx` → up to date with `origin/feature/bacon-1.5-fx` (0 ahead, 0 behind)
- `main` → up to date with `origin/main` (449041f)
- Previo HEAD `refactor/bacon-1.2.1-preserve` estaba 39 behind origin/main y 76 behind feature/bacon-1.5-fx; corregido con `git checkout feature/bacon-1.5-fx`

---

## Decisiones técnicas clave (referencia rápida)

| ID | Tema | Resumen | Archivo |
|----|------|---------|---------|
| DEC-2026-08-21-01 | EQ < -80 dB | No hay tabla indexada por dB+80; gain = pow(10, dB/20) | InstrumentEq.cpp, ParametricEQ.cpp |
| DEC-2026-08-21-02 | SDL2 Driver | SDL1.2 API legacy en SDL1 y SDL2 adapters | SDLAudioDriver.cpp (SDL/SDL2) |
| DEC-2026-08-21-03 | EQ < 80 Hz | Q=0.707 para todos los tipos <80 Hz si slope>1 | InstrumentEq.cpp:388 |
| DEC-2026-08-21-04 | Analyzer | Blackman -67dB, hold solo >140 Hz | SpectrumAnalyzer.cpp, InstrumentEqView.cpp |
| DEC-2026-08-21-05 | Versión inicio | NullView → "LGPT R36SX - Bacon 1.5" | NullView |

Ver `DECISIONS.md` para detalle completo (Status ACTIVE).

---

## Próximas actualizaciones

- Actualizar cuando se añadan nuevos subsistemas
- Actualizar cuando cambie la ubicación de archivos clave
- NO actualizar por cambios triviales (typos, comentarios)
- Mantener commit y rama sincronizados con `git rev-parse HEAD` y `git branch --show-current`
