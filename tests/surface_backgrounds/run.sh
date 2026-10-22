#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-surface-contracts"
mkdir -p "$output"
flags=(-std=c++20 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -Wno-unused-function -pthread -Itests/basic_items/include -Iapp/src/main/cpp)
"${CXX:-g++}" "${flags[@]}" tests/surface_backgrounds/native_surface_test.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp app/src/main/cpp/content/backgrounds/SurfaceGeometry.cpp app/src/main/cpp/content/backgrounds/SurfaceTables.cpp app/src/main/cpp/content/backgrounds/SurfaceSpriteBatch.cpp -o "$output/native"
"$output/native"
"${CXX:-g++}" "${flags[@]}" tests/surface_backgrounds/native_surface_loader_test.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp app/src/main/cpp/content/backgrounds/SurfaceGeometry.cpp app/src/main/cpp/content/backgrounds/SurfaceTables.cpp app/src/main/cpp/content/backgrounds/SurfaceSpriteBatch.cpp -o "$output/loader"
"$output/loader"
"${CXX:-g++}" "${flags[@]}" tests/surface_backgrounds/mailbox_surface_test.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp app/src/main/cpp/content/backgrounds/SurfaceGeometry.cpp app/src/main/cpp/content/backgrounds/SurfaceTables.cpp app/src/main/cpp/content/backgrounds/SurfaceSpriteBatch.cpp -o "$output/mailbox"
"$output/mailbox"
node tests/surface_backgrounds/original_surface_comparison.cjs "$output/native"
if [[ "${BUNNY_SURFACE_SANITIZE:-0}" == 1 ]]; then
  "${CXX:-g++}" "${flags[@]}" -g -O1 -fsanitize=address,undefined,float-divide-by-zero -fno-omit-frame-pointer tests/surface_backgrounds/native_surface_test.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp app/src/main/cpp/content/backgrounds/SurfaceGeometry.cpp app/src/main/cpp/content/backgrounds/SurfaceTables.cpp app/src/main/cpp/content/backgrounds/SurfaceSpriteBatch.cpp -o "$output/sanitized"
  ASAN_OPTIONS=detect_leaks=1 "$output/sanitized"
  "${CXX:-g++}" "${flags[@]}" -g -O1 -fsanitize=address,undefined,float-divide-by-zero -fno-omit-frame-pointer tests/surface_backgrounds/native_surface_loader_test.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp app/src/main/cpp/content/backgrounds/SurfaceGeometry.cpp app/src/main/cpp/content/backgrounds/SurfaceTables.cpp app/src/main/cpp/content/backgrounds/SurfaceSpriteBatch.cpp -o "$output/loader-sanitized"
  ASAN_OPTIONS=detect_leaks=1 "$output/loader-sanitized"
  "${CXX:-g++}" "${flags[@]}" -g -O1 -fsanitize=address,undefined,float-divide-by-zero -fno-omit-frame-pointer tests/surface_backgrounds/mailbox_surface_test.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp app/src/main/cpp/content/backgrounds/SurfaceGeometry.cpp app/src/main/cpp/content/backgrounds/SurfaceTables.cpp app/src/main/cpp/content/backgrounds/SurfaceSpriteBatch.cpp -o "$output/mailbox-sanitized"
  ASAN_OPTIONS=detect_leaks=1 "$output/mailbox-sanitized"
fi
