#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/screenfx
CXX="${CXX:-g++}"
"${CXX}" -std=c++20 -O2 -Wall -Wextra -Werror \
  -Iapp/src/main/cpp \
  app/src/main/cpp/graphics/FrameComposer.cpp \
  tests/frame_composer_multi_mod_test.cpp \
  -o build/screenfx/frame_composer_test
build/screenfx/frame_composer_test

"${CXX}" -std=c++20 -O2 -Wall -Wextra -Werror \
  -Itests/screenfx_stubs -Iapp/src/main/cpp \
  app/src/main/cpp/graphics/CSharpScreenEffects.cpp \
  tests/screenfx_registry_test.cpp \
  -o build/screenfx/screenfx_registry_test
build/screenfx/screenfx_registry_test
echo "SCREENFX_NATIVE_TESTS_OK"
