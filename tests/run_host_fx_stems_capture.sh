#!/usr/bin/env bash
# MULTITRACK_EXPORT (bacon-1.5, item 8): host test of the FxEngine stems
# capture (delay return / reverb return / master WAV) and the
# UnixFileSystem::GetFreeSpace free-space probe under ASAN/UBSAN.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/source/sources"
TMP="$(mktemp -d /tmp/fx_stems_capture_host.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=gnu++03 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wall -Wextra \
  -I"$SRC" \
  "$ROOT/tests/host/fx_stems_capture_host_test.cpp" \
  "$SRC/Application/Audio/FxEngine/FxEngine.cpp" \
  "$SRC/Application/Audio/FxEngine/DelayLine.cpp" \
  "$SRC/Application/Audio/FxEngine/Reverb.cpp" \
  "$SRC/Application/Audio/FxEngine/ParametricEQ.cpp" \
  "$SRC/Application/Audio/FxEngine/Compressor.cpp" \
  "$SRC/Application/Instruments/WavFileWriter.cpp" \
  "$SRC/Adapters/Unix/FileSystem/UnixFileSystem.cpp" \
  "$SRC/Application/Utils/wildcard.cpp" \
  "$SRC/System/FileSystem/FileSystem.cpp" \
  "$SRC/System/Errors/Result.cpp" \
  "$SRC/Foundation/Observable.cpp" \
  -lm \
  -o "$TMP/fx_stems_capture_host_test"
(cd "$ROOT" && "$TMP/fx_stems_capture_host_test")
