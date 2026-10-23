#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Iapp/src/main/cpp \
  tests/basic_items/basic_source_parser_test.cpp \
  app/src/main/cpp/experiments/csharp/SourceParser.cpp \
  -o "${RUNNER_TEMP:-/tmp}/bunny-armor-source-parser"
"${RUNNER_TEMP:-/tmp}/bunny-armor-source-parser"
python3 -m unittest discover -s tests/examplemod -p 'test_*.py'
"${DOTNET:-dotnet}" run --project tests/basic_items/ItemBatch.csproj -c Release
# Fixtures de ABI e os três itens reais compilados pelo Roslyn são distintos.
python3 tests/run_native_armor_policies.py \
  build/basic-items/armor-source-runtime-values.txt \
  build/basic-items/full-source-runtime-values.txt
python3 tests/run_native_item_policies.py \
  build/basic-items/full-source-runtime-values.txt samples/ExampleModCSharp/Playable
