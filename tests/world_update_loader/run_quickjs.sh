#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-system-update-quickjs"
mkdir -p "$output"
qjs=app/src/main/cpp/third_party/quickjs
# Build the same engine sources used by the APK, with host platform support.
for file in quickjs libregexp libunicode dtoa; do
 "${CC:-gcc}" -std=c11 -O1 -D_GNU_SOURCE -I"$qjs" -c "$qjs/$file.c" -o "$output/$file.o"
done
"${CXX:-g++}" -std=c++20 -O1 -fno-exceptions -fno-rtti -DBL_HAVE_QUICKJS=1 -Wall -Wextra -Werror -pthread -I"$qjs" -Itests/basic_items/include -Iapp/src/main/cpp tests/world_update_loader/quickjs_shared_time_rates_test.cpp app/src/main/cpp/script/api/SystemTimeRates.cpp "$output/quickjs.o" "$output/libregexp.o" "$output/libunicode.o" "$output/dtoa.o" -ldl -lm -o "$output/real-quickjs-shared-rates"
"$output/real-quickjs-shared-rates"
