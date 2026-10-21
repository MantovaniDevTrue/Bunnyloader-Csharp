#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-gore-contracts"
mkdir -p "$output"
flags=(-std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -pthread -DBL_HAVE_QUICKJS=0 -Itests/basic_items/include -Iapp/src/main/cpp)
"${CXX:-g++}" "${flags[@]}" tests/csharp_gore_discovery_test.cpp app/src/main/cpp/experiments/csharp/ManagedGoreDiscovery.cpp -o "$output/discovery"
"$output/discovery"
"${CXX:-g++}" "${flags[@]}" tests/csharp_gore_catalog_test.cpp -o "$output/catalog"
"$output/catalog"
"${CXX:-g++}" "${flags[@]}" tests/gore/native_gore_runtime_test.cpp app/src/main/cpp/experiments/csharp/ManagedGoreDiscovery.cpp -o "$output/runtime"
"$output/runtime"
"${CXX:-g++}" "${flags[@]}" -fPIC -shared -DBUNNY_GORE_LIBRARY_ONLY tests/gore/native_gore_runtime_test.cpp app/src/main/cpp/experiments/csharp/ManagedGoreDiscovery.cpp -o "$output/libbunny.so"
LD_LIBRARY_PATH="$output${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "${DOTNET:-dotnet}" run --project tests/gore/Gore.HostTests.csproj -c Release -- samples/ExampleModCSharp/Playable
node tests/gore/mixed_js_gore_test.cjs
