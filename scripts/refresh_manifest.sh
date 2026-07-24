#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "${1:-.}" && pwd)"
cd "$ROOT"
find . -type f \
    ! -path './.git/*' \
    ! -path './dist/*' \
    ! -path './BACKUPS/*' \
    ! -path './COLLECTED_LOGS/*' \
    ! -path '*/__pycache__/*' \
    ! -name MANIFEST.txt \
    ! -name SHA256SUMS.txt \
    -printf '%P\n' | LC_ALL=C sort > MANIFEST.txt

while IFS= read -r rel; do
    sha256sum "$rel"
done < MANIFEST.txt > SHA256SUMS.txt

echo "MANIFEST_REFRESHED files=$(wc -l < MANIFEST.txt)"
