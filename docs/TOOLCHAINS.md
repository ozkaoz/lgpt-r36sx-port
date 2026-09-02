# Toolchains y entorno de compilación — Mapa canónico

**Fecha:** 2026-09-01
**Alcance:** dónde vive cada pieza del entorno de compilación R36SX/LGPT/TreeFrogUI y qué script la usa.

## Mapa general (disco D: + WSL)

| Qué | Dónde | Notas |
|-----|-------|-------|
| **Repos git** (código evolucionado) | `D:\GitHub\<repo>\` (= `/mnt/d/Github/<repo>` en WSL) | lgpt-r36sx-port, treefrog-ui-r36sx, TreeFrogUI_picoarch, treefrog-content-manager |
| **Bases de compilación** (kernel, LGPT vanilla, iteraciones audio) | `D:\Toolchains\R36SX\` (= `/mnt/d/Toolchains/R36SX`) | ver tabla abajo |
| **Toolchain MIPS** (compilador cruzado) | WSL: `~/sf3000-work/sf3000toolchain/` | Codescape GNU MIPS MTI 6.3.0 — **NO mover** (los scripts referencian `/home/...`) |
| **Builds activos WSL** (TreeFrogUI, FrogUI) | WSL: `~/sf3000-work/treefrog-ui-r36sx-build`, `~/sf3000-work/FrogUI` | entorno activo de compilación |
| **Backups SD + git bundles** | `D:\R36S\PORT LPTRACKER\BACKUPS\` | snapshots restauradores por día; **siguen en su sitio** |
| **Evidencia forense** | `D:\R36S\PORT LPTRACKER\PAPELERA_20260811\` | irrepetible; sigue en su sitio |
| **Evidencia física/binaria en repo** | `physical-evidence/` (este repo) | golden físico, core iterations, stock kernel |

## D:\Toolchains\R36SX (bases de compilación)

| Carpeta | Contenido | La usan |
|---------|-----------|---------|
| `kernel-linux-4.4.186\` | Kernel vanilla 4.4.186 (árbol 52k archivos + tarball + manifests) | `kernel_module_tools/scripts/00_PREPARAR_FUENTE_KERNEL_44186.sh` y toda la serie 01–06 (compilación de `.ko` UAC2/USB audio) |
| `lgpt-u2523-source\` | Source LGPT U2523 vanilla (bitmaps, libs, installers) | `scripts/build.sh` (compila `lgpt_core.so` + daemons) via `SOURCE` |
| `audio-build-iterations\` | Salidas de build por versión (U2414…U2534) + `LOGS\` | `scripts/build.sh` (`OUT_DIR`), `collect_logs.sh` |

## Variables de entorno de los scripts

Todos los scripts de este repo aceptan override por entorno; defaults desde 2026-09-01:

```bash
PROJECT_ROOT   # default: /mnt/d/Toolchains/R36SX  (antes: /mnt/d/R36S/PORT LPTRACKER)
SOURCE         # default: $PROJECT_ROOT/lgpt-u2523-source
TC             # default: ~/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
KERNEL_SRC_DEST# default: $PROJECT_ROOT/kernel-linux-4.4.186 (kernel_module_tools)
```

Ejemplo de build completo del core:

```bash
cd /mnt/d/Github/lgpt-r36sx-port
PROJECT_ROOT=/mnt/d/Toolchains/R36SX bash scripts/build.sh
# (o simplemente: bash scripts/build.sh — los defaults ya apuntan a Toolchains)
```

## Compilación de módulos del kernel (UAC2)

```bash
cd /mnt/d/Github/lgpt-r36sx-port/kernel_module_tools/scripts
bash 00_PREPARAR_FUENTE_KERNEL_44186.sh   # extrae kernel -> /mnt/d/Toolchains/R36SX/kernel-linux-4.4.186
bash 01_COMPILAR_U2414_AU8_SYNC.sh       # aplica patches (repo) y compila .ko con el toolchain WSL
```

Los patches viven en este repo: `kernel_module_tools/tools/patch_f_uac2_48k_stereo.py`, `patch_f_uac2_au8_sync.py`. El kernel vanilla es GPL (kernel.org 4.4.186); el tarball en Toolchains es la copia de trabajo.

## treefrog-ui-r36sx (FrogUI/picoarch)

Los scripts de build están en `treefrog-ui-r36sx/local-build-scripts/` (copiados del workspace antiguo). Usan:
- Toolchain: `~/sf3000-work/sf3000toolchain/...` (WSL)
- Fuentes: los propios repos (`D:\GitHub\treefrog-ui-r36sx`, submodule `frogui`)

## Historial de rutas (para docs antiguos)

Antes de 2026-09-01 las bases de compilación vivían en `/mnt/d/R36S/PORT LPTRACKER/{KERNEL,BUILD,WORK,LOGS}`. Los docs históricos (COPY_ROOT_INSTALL_ES, RELEASE_BACON_*) conservan esas rutas como referencia de la época, con nota. Si un doc antiguo dice `PROJECT_ROOT=...PORT LPTRACKER`, hoy equivale a `/mnt/d/Toolchains/R36SX`.
