#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-recipe-callbacks"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I tests/basic_items/include -I app/src/main/cpp tests/recipe_callbacks/native_callbacks_test.cpp -o "$output"
"$output"
"$output" --wrong-type
"$output" --wrong-offset
"$output" --ushort-sink
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I app/src/main/cpp tests/minion_buff/minion_buff_test.cpp app/src/main/cpp/experiments/csharp/SourceParser.cpp -o "$output-buffs"
"$output-buffs"
