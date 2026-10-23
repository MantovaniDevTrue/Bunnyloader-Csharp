#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
# Roslyn executa o SetDefaults e o SetStaticDefaults reais dos cem fontes.
"${DOTNET:-dotnet}" run --project tests/basic_items/ItemBatch.csproj -c Release
"${DOTNET:-dotnet}" run --project tests/golf_ball/GolfBall.HostTests.csproj -c Release
output="${RUNNER_TEMP:-/tmp}/bunny-golf-ball-native"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations \
  -O1 -ffunction-sections -fdata-sections -DBL_HAVE_QUICKJS=0 \
  -I tests/basic_items/include -I app/src/main/cpp tests/golf_ball/native_test.cpp \
  -Wl,--gc-sections -pthread -o "$output"
"$output" build/basic-items/full-source-runtime-values.txt
