#!/usr/bin/env bash
set -euo pipefail
REMOTE="${1:-https://github.com/ozkaoz/lgpt-r36sx-port.git}"
BRANCH="${2:-r36sx-v2.6-treefrog-au11z6}"

if [[ ! -d .git ]]; then
  echo "ERROR: no estás dentro de un repositorio git. Entra primero a la carpeta lgpt-r36sx-port." >&2
  exit 1
fi

git branch -M main
if git remote get-url origin >/dev/null 2>&1; then
  git remote set-url origin "$REMOTE"
else
  git remote add origin "$REMOTE"
fi

if ! git rev-parse --verify HEAD >/dev/null 2>&1; then
  git add -A
  git commit -m "Initial R36SX v2.6 TreeFrogUI LGPT port source"
fi

echo "Probando conexión con GitHub/remoto..."
if ! git ls-remote "$REMOTE" >/tmp/lgpt_git_lsremote.txt 2>/tmp/lgpt_git_lsremote.err; then
  echo "ERROR: no pude contactar el remoto o no tienes autenticación." >&2
  echo "Remoto: $REMOTE" >&2
  echo "Detalle:" >&2
  cat /tmp/lgpt_git_lsremote.err >&2 || true
  echo "" >&2
  echo "Si aparece 'Could not resolve host', es DNS/conexión en WSL. Prueba:" >&2
  echo "  ping -c 1 github.com" >&2
  echo "  git remote -v" >&2
  echo "  git push -u origin main" >&2
  exit 2
fi

git push -u origin main

git checkout -B "$BRANCH"
git push -u origin "$BRANCH"

git checkout main

echo "OK: ramas publicadas en GitHub: main y $BRANCH"
