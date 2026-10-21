#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
compiler="${CXX:-c++}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
for file in "$root/tests/cloud_loader/"*_test.cpp; do
  extras=()
  if [[ "${file##*/}" == "source_parser_cloud_test.cpp" ]]; then
    extras+=("$root/app/src/main/cpp/experiments/csharp/SourceParser.cpp")
  fi
  if [[ "${file##*/}" == "native_cloud_autoload_test.cpp" ]]; then
    extras+=("$root/app/src/main/cpp/experiments/csharp/NativeCloudAutoload.cpp")
  fi
  "$compiler" -std=c++20 -O2 -Wall -Wextra -Werror -pedantic \
    -I"$root/app/src/main/cpp" "$file" "${extras[@]}" -o "$tmp/test"
  "$tmp/test"
done
