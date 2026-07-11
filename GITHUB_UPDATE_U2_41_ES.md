# Actualizar repositorio GitHub con U2.41

No se debe copiar el build temporal completo de WSL al repositorio. Usa este paquete fuente como origen limpio.

## Estructura recomendada local

```text
D:\R36S\PORT LPTRACKER\
  LGPT_PORT_U2_41_FINAL_WAV_EXPORT_SOURCE.zip
  GITHUB\
    <tu_repo_lgpt>\
```

En WSL:

```text
/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_41_FINAL_WAV_EXPORT_SOURCE.zip
/mnt/d/R36S/PORT LPTRACKER/GITHUB/<tu_repo_lgpt>/
```

## Opción A: actualizar con script incluido

Desde la fuente extraída:

```bash
cd ~/lgpt_u241_dev/LGPT_PORT_U2_41_FINAL_WAV_EXPORT_SOURCE
bash scripts/UPDATE_GITHUB_U2_41_FROM_WSL.sh "/mnt/d/R36S/PORT LPTRACKER/GITHUB/<tu_repo_lgpt>" u2.41-wav-export
```

Revisa el diff:

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/<tu_repo_lgpt>"
git status --short
git diff --stat
git diff -- sources/Application/Mixer/MixerService.cpp sources/Application/Mixer/MixerService.h sources/Application/Player/Player.cpp
```

Commit y push, si el diff es correcto:

```bash
git add -A
git commit -m "U2.41: project-scoped WAV export and stem naming"
git push -u origin u2.41-wav-export
```

Crear tag opcional:

```bash
git tag -a u2.41-wav-export -m "U2.41 WAV export for R36SX"
git push origin u2.41-wav-export
```

## Opción B: actualización manual con rsync

```bash
SRC="$HOME/lgpt_u241_dev/LGPT_PORT_U2_41_FINAL_WAV_EXPORT_SOURCE"
DST="/mnt/d/R36S/PORT LPTRACKER/GITHUB/<tu_repo_lgpt>"

rsync -a --delete \
  --exclude='.git/' \
  --exclude='projects/buildTREEFROG/' \
  --exclude='*.o' \
  --exclude='*.d' \
  --exclude='dist/lgpt_libretro.so' \
  "$SRC/" "$DST/"

cd "$DST"
git checkout -B u2.41-wav-export
git status --short
git add -A
git commit -m "U2.41: project-scoped WAV export and stem naming"
git push -u origin u2.41-wav-export
```

## Revisión mínima antes de push

```bash
git diff --stat HEAD~1..HEAD
find . -maxdepth 3 -type f -name "*.lgpt" -print
```

No debe agregarse ningún launcher alternativo al diseño final. El launcher esperado sigue siendo:

```text
roms/lgpt/start.lgpt
```
