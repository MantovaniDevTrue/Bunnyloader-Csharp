#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-underground-contracts"
mkdir -p "$output"
flags=(-std=c++20 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -pthread -Itests/basic_items/include -Iapp/src/main/cpp)
"${CXX:-g++}" "${flags[@]}" tests/underground_backgrounds/native_underground_test.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp -o "$output/native"
"$output/native"
"${CXX:-g++}" "${flags[@]}" -Wno-unused-function tests/underground_backgrounds/mailbox_underground_test.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp -o "$output/mailbox"
"$output/mailbox"
"${CXX:-g++}" "${flags[@]}" -Wno-unused-function -fPIC -shared -DBUNNY_UNDERGROUND_LIBRARY_ONLY tests/underground_backgrounds/native_underground_test.cpp app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp -o "$output/libbunny.so"
LD_LIBRARY_PATH="$output${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "${DOTNET:-dotnet}" run --project tests/underground_backgrounds/Underground.HostTests.csproj -c Release
