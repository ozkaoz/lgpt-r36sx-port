# lgpt-r36sx-port

Port de **LittleGPTracker / Little Piggy Tracker** para **R36SX v2.6** sobre **TreeFrogUI**.

Este repositorio parte de la fuente estable `AU11Z6` revisada para TreeFrogUI. La estrategia técnica actual es compilar LGPT como **core libretro** (`lgpt_libretro.so`) para que TreeFrogUI/picoarch pueda cargarlo en la R36SX.

## Estado del port

- Base upstream: LittleGPTracker.
- Plataforma objetivo: R36SX v2.6 con Stock OS + TreeFrogUI.
- Salida esperada: `lgpt_libretro.so`.
- Frontend objetivo: TreeFrogUI.
- Tipo de integración actual: libretro core, no aplicación standalone.
- Rama recomendada de desarrollo: `r36sx-v2.6-treefrog-au11z6`.

## Estructura relevante

```text
projects/Makefile.TREEFROG                 # Makefile principal del port TreeFrog/libretro
sources/Adapters/TREEFROG/                 # Adaptador TreeFrog: audio, GUI, eventos, timer, libretro
r36sx_package/source_overrides/            # Fuentes auxiliares del paquete R36SX
tools/wsl/                                 # Scripts limpios para WSL Ubuntu 24
docs/                                      # Auditoría y guía de desarrollo
```

## Flujo corto en WSL Ubuntu 24

Desde Windows, el desarrollo debe quedar en:

```text
D:\R36S\PORT LPTRACKER\GITHUB\lgpt-r36sx-port
```

En WSL Ubuntu 24 esa ruta se ve así:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"
```

Primeros comandos:

```bash
git status
bash tools/wsl/02_verify_treefrog_sd.sh F
bash tools/wsl/00_build_install_r36sx_v26.sh "/mnt/d/R36S/PORT LPTRACKER" F /tmp/lgpt_r36sx_v26
```

Cambia `F` por la letra real de la SD en Windows.

## Publicación inicial en GitHub

Crea primero un repositorio vacío en GitHub llamado:

```text
lgpt-r36sx-port
```

Luego ejecuta:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"
bash tools/wsl/01_git_first_push.sh https://github.com/ozkaoz/lgpt-r36sx-port.git
```

## Nota técnica importante

Los scripts originales incluidos en el paquete fuente estable tenían referencias internas a carpetas `bin/`, `device/`, `src/` y `source_full/` que no estaban presentes en la versión `SOURCE_ONLY`. Para este repositorio se agregaron scripts nuevos en `tools/wsl/` que usan la estructura real del paquete fuente.

