#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-water-contracts"
mkdir -p "$output"
flags=(-std=c++20 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -Wno-unused-function -pthread -Itests/basic_items/include -Iapp/src/main/cpp)
"${CXX:-g++}" "${flags[@]}" tests/water_styles/native_code_patch_test.cpp -o "$output/code-patch"
"$output/code-patch"
sources=(tests/water_styles/native_water_count_test.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp)
"${CXX:-g++}" "${flags[@]}" "${sources[@]}" -o "$output/water-count"
"$output/water-count"
node tests/water_styles/js_water_install_test.cjs
if [[ "${BUNNY_WATER_SANITIZE:-0}" == 1 ]]; then
  "${CXX:-g++}" "${flags[@]}" -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer tests/water_styles/native_code_patch_test.cpp -o "$output/code-patch-sanitized"
  ASAN_OPTIONS=detect_leaks=1 "$output/code-patch-sanitized"
  "${CXX:-g++}" "${flags[@]}" -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer "${sources[@]}" -o "$output/water-count-sanitized"
  ASAN_OPTIONS=detect_leaks=1 "$output/water-count-sanitized"
fi
