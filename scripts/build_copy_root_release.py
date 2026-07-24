#!/usr/bin/env python3
"""Build a deterministic ZIP that contains both SD payload and all hotfix source."""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import stat
import zipfile

FIXED_TIME = (2026, 7, 24, 0, 0, 0)
EXCLUDED_PARTS = {".git", "BACKUPS", "COLLECTED_LOGS", "dist", "__pycache__"}


def should_include(path: Path, root: Path) -> bool:
    rel = path.relative_to(root)
    if any(part in EXCLUDED_PARTS for part in rel.parts):
        return False
    return True


def file_mode(path: Path) -> int:
    mode = path.stat().st_mode
    return 0o755 if mode & stat.S_IXUSR else 0o644


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    files = sorted(p for p in root.rglob("*") if p.is_file() and should_include(p, root))
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in files:
            rel = path.relative_to(root).as_posix()
            info = zipfile.ZipInfo(rel, FIXED_TIME)
            info.create_system = 3
            info.external_attr = (file_mode(path) & 0xFFFF) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, path.read_bytes())

    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    print(f"ZIP_OK {output}")
    print(f"SHA256 {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
