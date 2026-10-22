#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-scene-effects"
mkdir -p "$output"
flags=(-std=c++20 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -pthread -Iapp/src/main/cpp)
"${CXX:-g++}" "${flags[@]}" tests/scene_effects/scene_effects_test.cpp app/src/main/cpp/content/scene/SceneEffects.cpp -o "$output/scene_effects"
"$output/scene_effects"
