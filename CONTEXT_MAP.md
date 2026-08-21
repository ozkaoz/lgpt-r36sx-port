# CONTEXT MAP — Mapa de navegación del repositorio

**Última actualización:** 2026-08-21
**Commit:** 8cc0a47 (Bacon 1.5 U2.71)
**Rama:** feature/bacon-1.5-fx

---

## Audio (DSP, Mezclador, Drivers)
| Área | Documentos | Código | Scripts | Decisiones |
|------|------------|--------|---------|------------|
| Mezclador / AudioMixer | — | `source/sources/Services/Audio/AudioMixer.cpp/.h` | — | DECISIONS.md#audio-mixer |
| Ecualizador (InstrumentEq / ParametricEQ) | — | `source/sources/Application/Audio/InstrumentEq.cpp/.h`<br>`source/sources/Application/Audio/FxEngine/ParametricEQ.cpp/.h`<br>`source/sources/Application/Audio/EqBiquad.h` | — | DECISIONS.md#eq |
| Compresor | — | `source/sources/Application/Audio/FxEngine/Compressor.cpp/.h` | — | DECISIONS.md#compressor |
| Spectrum Analyzer | — | `source/sources/Application/Audio/SpectrumAnalyzer.cpp/.h` | — | — |
| Drivers SDL / SDL2 | — | `source/sources/Adapters/SDL/Audio/SDLAudioDriver.cpp/.h`<br>`source/sources/Adapters/SDL2/Audio/SDLAudioDriver.cpp/.h` | — | DECISIONS.md#sdl2-driver |
| Drivers TreeFrog / W32 / Dummy / RTAudio | — | `source/sources/Adapters/*/Audio/` | — | — |
| Instrumentos (SampleInstrument, BassSynth, PianoSynth) | — | `source/sources/Application/Instruments/` | tests/host/bass_synth_host_test.cpp | — |

---

## Build (Compilación, Build System)
| Área | Documentos | Código / Scripts |
|------|------------|------------------|
| Build principal (WSL / Makefile) | `docs/BUILD_ES.md` / `docs/BUILD_EN.md` | `source/BUILD_TREEFROG_R36SX_BADGE_OFF_SELECT_OFF.sh` |
| Makefile / CMake | — | `source/projects/Makefile` |
| Scripts de build / auditoría | `docs/audit.sh` | `scripts/audit.sh`, `scripts/install.sh` |
| Tests host | — | `tests/host/*.sh`, `tests/host/*.cpp` |
| Deploy a SD (G:) | — | `scripts/install.sh`, `scripts/verify.sh` |

---

## UI / Vistas / Input
| Área | Código |
|------|--------|
| Vistas principales | `source/sources/Application/UI/Views/` (SongView, ChainView, PhraseView, TableView, InstrumentView, InstrumentEqView, ProjectView, MixerView, etc.) |
| Input / Controllers | `source/sources/Adapters/*/Input/`, `source/sources/Services/Controllers/` |
| Input SDL / SDL2 | `source/sources/Adapters/SDL/Input/`, `source/sources/Adapters/SDL2/Input/` |
| ModalDialogs | `source/sources/Application/UI/Views/ModalDialogs/` |
| Help / HelpRegistry | `source/sources/Application/UI/Views/BaseClasses/HelpRegistry.cpp` |

---

## Instrumentos
| Tipo | Archivos clave |
|------|----------------|
| SampleInstrument | `source/sources/Application/Instruments/SampleInstrument.cpp/.h` |
| BassSynth | `source/sources/Application/Instruments/BassSynth.cpp/.h` |
| PianoSynth | `source/sources/Application/Instruments/PianoSynth.cpp/.h` |
| InstrumentEq | `source/sources/Application/Audio/InstrumentEq.cpp/.h` |
| ParametricEQ | `source/sources/Application/Audio/FxEngine/ParametricEQ.cpp/.h` |

---

## Configuración / Persistencia / Proyecto
| Área | Código |
|------|--------|
| Proyecto / Song / Chain / Phrase | `source/sources/Application/Model/` (Project.cpp, Song.cpp, Chain.cpp, Phrase.cpp) |
| InstrumentBank / SamplePool | `source/sources/Application/Instruments/InstrumentBank.cpp`, `SamplePool.cpp` |
| Persistencia / Config | `source/sources/Application/Persistency/`, `source/sources/Application/Model/Project.cpp` |
| Config.xml / sd_root | `sd_root/lgpt/config.xml`, `sd_root/VERSION.txt` |

---

## Tests / Audits / Scripts
| Tipo | Ubicación |
|------|-----------|
| Tests host (C++) | `tests/host/*_host_test.cpp` |
| Tests host (scripts) | `tests/run_host_*.sh` |
| Tests Python | `tests/test_*.py` |
| Auditoría | `scripts/audit.sh`, `scripts/audit.sh` |
| Build verification | `scripts/verify.sh` |

---

## Documentación
| Archivo | Propósito |
|---------|-----------|
| `README.md` | Visión general del proyecto |
| `CHANGELOG.md` | Historial de cambios detallado |
| `docs/BUILD_ES.md` / `docs/BUILD_EN.md` | Guía de compilación |
| `docs/AUDIO_ARCHITECTURE_MAP_ES.md` | Arquitectura de audio |
| `docs/INSTALL_ES.md` / `docs/INSTALL_EN.md` | Instalación |
| `docs/AUDIO_OTG.md` | Arquitectura USB OTG Audio |
| `docs/CONTROLS_ES.md` / `docs/CONTROLS_EN.md` | Controles de usuario |
| `docs/AUDIO_OTG.md` | Audio OTG / USB |

---

## SD Card (G:) — Estructura en dispositivo
```
/mnt/g/
├── cubegm/
│   └── cores/
│       └── lgpt_r36sx_port_libretro.so   ← Core actual (DBAD57A7)
├── lgpt/
│   ├── config.xml
│   ├── projects/lgpt_KAOZ/lgptsav.dat   ← Proyecto con EQ configurado
│   └── otg/                              ← Configuración USB OTG
└── VERSION.txt (si existe)
```

---

## Git / Branching
| Rama | Estado | Descripción |
|------|--------|-------------|
| `feature/bacon-1.5-fx` | **ACTIVA** | Desarrollo Bacon 1.5 (U2.71+) |
| `main` | Estable | Bacon 1.4 (U2.54) |
| `feature/android-usb-audio-otg` | Experimental | USB OTG Android |

- `origin` → https://github.com/ozkaoz/lgpt-r36sx-port.git
- `feature/bacon-1.5-fx` → up to date with `origin/feature/bacon-1.5-fx`

---

## Decisiones técnicas clave (referencia rápida)
| ID | Tema | Resumen | Archivo |
|----|------|---------|---------|
| DEC-2026-08-21-01 | EQ < -80 dB | No hay tabla indexada por dB+80; gain = pow(10, dB/20) | — |
| DEC-2026-08-21-02 | SDL2 Driver | SDL1.2 API legacy en SDL1 y SDL2 adapters | — |
| DEC-2026-08-21-03 | EQ < 80 Hz | Q=0.707 para todos los tipos <80 Hz si slope>1 | InstrumentEq.cpp:388 |

---

## Próximas actualizaciones
- Actualizar cuando se añadan nuevos subsistemas
- Actualizar cuando cambie la ubicación de archivos clave
- NO actualizar por cambios triviales (typos, comentarios)