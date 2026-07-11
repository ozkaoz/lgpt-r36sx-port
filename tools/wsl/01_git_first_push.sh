#!/usr/bin/env bash
set -euo pipefail
REMOTE_URL="${1:-}"
if [[ -z "$REMOTE_URL" ]]; then
  echo "Uso: bash tools/wsl/01_git_first_push.sh https://github.com/ozkaoz/lgpt-r36sx-port.git" >&2
  exit 2
fi
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
if [[ ! -d .git ]]; then
  git init
fi
git checkout -B main
git add .
if ! git diff --cached --quiet; then
  git commit -m "Initial R36SX v2.6 TreeFrogUI LGPT port source"
fi
if git remote get-url origin >/dev/null 2>&1; then
  git remote set-url origin "$REMOTE_URL"
else
  git remote add origin "$REMOTE_URL"
fi
git push -u origin main
git checkout -B r36sx-v2.6-treefrog-au11z6
git push -u origin r36sx-v2.6-treefrog-au11z6
cat <<MSG

Repositorio publicado.
Ramas creadas:
- main
- r36sx-v2.6-treefrog-au11z6

Siguiente paso recomendado:
  bash tools/wsl/02_verify_treefrog_sd.sh LETRA_SD
MSG
