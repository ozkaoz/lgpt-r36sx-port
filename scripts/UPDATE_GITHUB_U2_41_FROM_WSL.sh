#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  bash scripts/UPDATE_GITHUB_U2_41_FROM_WSL.sh <github_repo_path> [branch]

Example:
  bash scripts/UPDATE_GITHUB_U2_41_FROM_WSL.sh "/mnt/d/R36S/PORT LPTRACKER/GITHUB/lgpt-r36sx" u2.41-wav-export

This script copies the current source tree into an existing local Git repository,
excluding .git, object files, build folders, and built cores. It does not push.
USAGE
}

if [ $# -lt 1 ]; then
  usage
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_DIR="$1"
BRANCH="${2:-u2.41-wav-export}"

if [ ! -d "$REPO_DIR/.git" ]; then
  echo "ERROR: target is not a Git repository: $REPO_DIR" >&2
  exit 2
fi

cd "$REPO_DIR"
git checkout -B "$BRANCH"

rsync -a --delete \
  --exclude='.git/' \
  --exclude='projects/buildTREEFROG/' \
  --exclude='*.o' \
  --exclude='*.d' \
  --exclude='*.so' \
  --exclude='dist/lgpt_libretro.so' \
  "$SRC_DIR/" "$REPO_DIR/"

cd "$REPO_DIR"

echo "Repository updated on branch: $BRANCH"
echo
git status --short

echo
echo "Next commands, after reviewing the diff:"
echo "  git diff --stat"
echo "  git add -A"
echo "  git commit -m 'U2.41: project-scoped WAV export and stem naming'"
echo "  git push -u origin $BRANCH"
