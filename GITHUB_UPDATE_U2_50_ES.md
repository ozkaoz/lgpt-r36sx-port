# Actualizar GitHub con U2.50 FINAL

Repositorio local esperado:

```text
D:\R36S\PORT LPTRACKER\GITHUB\lgpt-r36sx-port
```

Ruta equivalente en WSL:

```bash
/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port
```

## Paso 1 — Diagnóstico seguro

Este bloque no modifica nada:

```bash
REPO="/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"
ZIP="/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE.zip"

cd "$REPO"

echo "== Rama actual =="
git branch --show-current

echo
 echo "== Estado =="
git status -sb

echo
 echo "== Últimos commits =="
git log --oneline -5

echo
 echo "== ZIP =="
ls -lh "$ZIP"
```

## Paso 2 — Actualizar repo local

Ejecutar solo si el estado local está limpio o si ya sabes que quieres reemplazar el árbol por U2.50 FINAL.

```bash
REPO="/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"
ZIP="/mnt/d/R36S/PORT LPTRACKER/LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE.zip"
WORK="$HOME/lgpt_u250_final_repo_update"
BRANCH="u2.50-final-mixer-master"

cd "$REPO"

git status -sb

if [ -n "$(git status --porcelain)" ]; then
  echo "El repo tiene cambios locales. Haz commit/stash antes de continuar."
else
  git fetch --all --prune
  BACKUP_BRANCH="backup-before-u2.50-$(date +%Y%m%d-%H%M%S)"
  git branch "$BACKUP_BRANCH"
  echo "Backup branch: $BACKUP_BRANCH"

  rm -rf "$WORK"
  mkdir -p "$WORK"
  cd "$WORK"
  unzip -q "$ZIP"

  SRC="$WORK/LGPT_PORT_U2_50_FINAL_MIXER_MASTER_SOURCE"

  cd "$REPO"
  git checkout -B "$BRANCH"

  rsync -a --delete \
    --exclude ".git/" \
    --exclude "projects/buildTREEFROG/" \
    --exclude "dist/lgpt_libretro.so" \
    --exclude "*.o" \
    --exclude "*.so" \
    "$SRC/" "$REPO/"

  git status -sb
  git diff --stat
fi
```

## Paso 3 — Commit y push

```bash
REPO="/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"
BRANCH="u2.50-final-mixer-master"

cd "$REPO"

git add -A
git commit -m "U2.50: final Mixer and Master workflow for R36SX"
git push -u origin "$BRANCH"
```

## Paso 4 — Tag estable

```bash
cd "/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port"

git tag -a u2.50-final -m "U2.50 final Mixer and Master workflow for R36SX"
git push origin u2.50-final
```
