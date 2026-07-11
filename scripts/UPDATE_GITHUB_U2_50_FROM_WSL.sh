#!/usr/bin/env bash
set -euo pipefail

REPO="${1:-/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx-port}"
BRANCH="${2:-u2.50-final-mixer-master}"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ ! -d "$REPO/.git" ]; then
  echo "ERROR: repo git no encontrado: $REPO" >&2
  exit 1
fi

cd "$REPO"

if [ -n "$(git status --porcelain)" ]; then
  echo "ERROR: el repo tiene cambios locales sin commit/stash." >&2
  git status -sb
  exit 1
fi

git fetch --all --prune || true
BACKUP_BRANCH="backup-before-u2.50-$(date +%Y%m%d-%H%M%S)"
git branch "$BACKUP_BRANCH"
echo "Backup branch: $BACKUP_BRANCH"

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

echo
 echo "Revisa el diff. Luego ejecuta:"
echo "git add -A"
echo "git commit -m 'U2.50: final Mixer and Master workflow for R36SX'"
echo "git push -u origin $BRANCH"
