#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-background-contracts"
mkdir -p "$output"
flags=(-std=c++20 -Wall -Wextra -Werror -pthread -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -Iapp/src/main/cpp)
"${CXX:-g++}" "${flags[@]}" tests/backgrounds/native_background_test.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp -o "$output/native"
"$output/native"
"${CXX:-g++}" "${flags[@]}" -fPIC -shared tests/backgrounds/native_background_test.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp -o "$output/libbunny.so"
LD_LIBRARY_PATH="$output${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "${DOTNET:-dotnet}" run --project tests/backgrounds/Backgrounds.csproj -c Release -- samples/ExampleModCSharp/Playable/content/Assets/Textures/Backgrounds/ExampleBiomeSurfaceFar.png

node tests/backgrounds/shared_background_js_test.cjs
