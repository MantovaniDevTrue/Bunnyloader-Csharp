#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
compiler="${CXX:-c++}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
for file in "$root/tests/cloud_loader/"*_test.cpp; do
  "$compiler" -std=c++17 -O2 -Wall -Wextra -Werror -pedantic \
    -I"$root/app/src/main/cpp" "$file" -o "$tmp/test"
  "$tmp/test"
done
