#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-world-tile-flags"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -I app/src/main/cpp tests/world_loaders/tile_flags_test.cpp -o "$output"
"$output" build/basic-items/full-source-runtime-values.txt
anim="${RUNNER_TEMP:-/tmp}/bunny-world-tile-animation"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -DBL_HAVE_QUICKJS=0 -I app/src/main/cpp tests/world_loaders/tile_animation_test.cpp -o "$anim"
"$anim"
adj="${RUNNER_TEMP:-/tmp}/bunny-world-tile-adjacencies"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -DBL_HAVE_QUICKJS=0 -I app/src/main/cpp tests/world_loaders/tile_adjacency_test.cpp -o "$adj"
"$adj"
