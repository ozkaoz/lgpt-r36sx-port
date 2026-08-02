#!/usr/bin/env bash
set -Eeuo pipefail
cd "$(dirname "$0")/.."
BASE="-std=gnu++03 -DPLATFORM_TREEFROG -DCPP_MEMORY -DHAVE_STDINT_H -D_NDEBUG -D_NO_JACK_ -DDUMMYMIDI -include stdint.h -Isource/sources"
for f in DelayLine Reverb ParametricEQ Compressor; do
  echo "== $f =="
  g++ $BASE -Wall -Wextra -Werror -fsyntax-only "source/sources/Application/Audio/FxEngine/$f.cpp"
done
echo HARDEN_SYNTAX_WERROR_OK
