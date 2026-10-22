#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-surface-quickjs-contracts"
mkdir -p "$output"
qjs=app/src/main/cpp/third_party/quickjs
for file in quickjs libregexp libunicode dtoa; do
 "${CC:-gcc}" -std=c11 -O1 -fPIC -D_GNU_SOURCE -I"$qjs" -c "$qjs/$file.c" -o "$output/$file.o"
done
flags=(-std=c++20 -O1 -fno-exceptions -fno-rtti -DBL_HAVE_QUICKJS=1 -Wall -Wextra -Werror -Wno-unused-function -pthread -I"$qjs" -Itests/basic_items/include -Iapp/src/main/cpp)
sources=(tests/surface_backgrounds/quickjs_surface_test.cpp app/src/main/cpp/script/api/SurfaceBackgrounds.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp app/src/main/cpp/content/backgrounds/SurfaceGeometry.cpp app/src/main/cpp/content/backgrounds/SurfaceTables.cpp app/src/main/cpp/content/backgrounds/SurfaceSpriteBatch.cpp "$output/quickjs.o" "$output/libregexp.o" "$output/libunicode.o" "$output/dtoa.o")
"${CXX:-g++}" "${flags[@]}" "${sources[@]}" -ldl -lm -o "$output/quickjs-test"
"$output/quickjs-test"
"${CXX:-g++}" "${flags[@]}" -fPIC -shared -DBUNNY_SURFACE_QJS_LIBRARY_ONLY "${sources[@]}" -ldl -lm -o "$output/libbunny.so"
LD_LIBRARY_PATH="$output${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "${DOTNET:-dotnet}" run --project tests/surface_backgrounds/Mixed.HostTests.csproj -c Release
