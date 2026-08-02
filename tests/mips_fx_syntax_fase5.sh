#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="$ROOT/source/sources"
BASE=(
  -std=gnu++03 -DPLATFORM_TREEFROG -DCPP_MEMORY -DHAVE_STDINT_H
  -D_NDEBUG -D_NO_JACK_ -DDUMMYMIDI -include stdint.h
  -mips32r2 -march=mips32r2 -mtune=74kc -mdspr2 -mfp32 -mhard-float -EL
  -I"$SOURCE"
)
FILES=(
  "$SOURCE/Application/Audio/FxEngine/DelayLine.cpp"
  "$SOURCE/Application/Audio/FxEngine/Reverb.cpp"
  "$SOURCE/Application/Audio/FxEngine/ParametricEQ.cpp"
  "$SOURCE/Application/Audio/FxEngine/Compressor.cpp"
  "$SOURCE/Application/Audio/FxEngine/FxEngine.cpp"
)
for f in "${FILES[@]}"; do
  mipsel-linux-gnu-g++ "${BASE[@]}" -fsyntax-only "$f"
done
echo MIPS_FX_SYNTAX_FASE5_OK
