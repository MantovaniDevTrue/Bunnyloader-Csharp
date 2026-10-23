#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
"${DOTNET:-dotnet}" run --project tests/three_loaders/ThreeLoaders.csproj -c Release
"${DOTNET:-dotnet}" run --project tests/basic_items/ItemBatch.csproj -c Release
for fixture in callback_retirement rarity_retirement localization_retirement; do
  "${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations \
    -O1 -ffunction-sections -fdata-sections -DBL_HAVE_QUICKJS=0 \
    -Itests/basic_items/include -Iapp/src/main/cpp \
    "tests/three_loaders/${fixture}_test.cpp" -Wl,--gc-sections -pthread -ldl \
    -o "${RUNNER_TEMP:-/tmp}/bunny-three-${fixture}"
  "${RUNNER_TEMP:-/tmp}/bunny-three-${fixture}"
done
# Primitive questItem binding: real production NativeDefaults parser/writer,
# including the adjacent-byte guard. The fixture also covers summon defaults.
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations \
  -Iapp/src/main/cpp tests/summon_lifecycle/native_defaults_test.cpp -pthread \
  -o "${RUNNER_TEMP:-/tmp}/bunny-three-quest-defaults"
"${RUNNER_TEMP:-/tmp}/bunny-three-quest-defaults" build/basic-items/full-source-runtime-values.txt
