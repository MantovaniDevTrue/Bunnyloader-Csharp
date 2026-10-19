#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
"${DOTNET:-dotnet}" run --project tests/shooting/Shooting.csproj -c Release
output="${RUNNER_TEMP:-/tmp}/bunny-native-shooting"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -I app/src/main/cpp tests/shooting/native_test.cpp -pthread -o "$output"
"$output" build/shooting/shotgun-output.txt build/basic-items/full-source-runtime-values.txt
