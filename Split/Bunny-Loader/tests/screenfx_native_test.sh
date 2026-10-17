#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/screenfx
CXX="${CXX:-g++}"
"${CXX}" -std=c++20 -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
  -Itests/screen_scene_stubs -Iapp/src/main/cpp \
  tests/screen_scene_field_contract_test.cpp -o build/screenfx/screen_scene_field_contract_test
build/screenfx/screen_scene_field_contract_test
"${CXX}" -std=c++20 -O2 -Wall -Wextra -Werror \
  -Iapp/src/main/cpp \
  app/src/main/cpp/graphics/FrameComposer.cpp \
  tests/frame_composer_multi_mod_test.cpp \
  -o build/screenfx/frame_composer_test
build/screenfx/frame_composer_test

"${CXX}" -std=c++20 -O2 -Wall -Wextra -Werror \
  -Iapp/src/main/cpp \
  tests/screenfx_frame_hold_test.cpp \
  -o build/screenfx/screenfx_frame_hold_test

"${CXX}" -std=c++20 -O2 -Wall -Wextra -Werror \
  -Iapp/src/main/cpp \
  tests/screen_scene_transition_test.cpp \
  -o build/screenfx/screen_scene_transition_test
build/screenfx/screen_scene_transition_test
build/screenfx/screenfx_frame_hold_test

"${CXX}" -std=c++20 -O2 -Wall -Wextra -Werror \
  -Itests/screenfx_stubs -Iapp/src/main/cpp \
  app/src/main/cpp/graphics/CSharpScreenEffects.cpp \
  app/src/main/cpp/graphics/CSharpScreenGraph.cpp \
  tests/screenfx_registry_test.cpp \
  -o build/screenfx/screenfx_registry_test
build/screenfx/screenfx_registry_test

"${CXX}" -std=c++20 -O2 -Wall -Wextra -Werror \
  -Iapp/src/main/cpp \
  tests/screenfx_native_identity_test.cpp \
  -o build/screenfx/screenfx_native_identity_test
build/screenfx/screenfx_native_identity_test
"${CXX}" -std=c++20 -O2 -Wall -Wextra -Werror \
  -Iapp/src/main/cpp \
  app/src/main/cpp/experiments/csharp/SourceParser.cpp \
  tests/screenfx_source_parser_test.cpp \
  -o build/screenfx/screenfx_source_parser_test
build/screenfx/screenfx_source_parser_test

echo "SCREENFX_NATIVE_TESTS_OK"
