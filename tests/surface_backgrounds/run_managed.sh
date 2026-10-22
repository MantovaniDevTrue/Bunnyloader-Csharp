#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-surface-managed-contracts"
mkdir -p "$output"
flags=(-std=c++20 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -Wno-unused-function -pthread -Itests/basic_items/include -Iapp/src/main/cpp)
sources=(app/src/main/cpp/il2cpp/Signature.cpp app/src/main/cpp/content/backgrounds/BackgroundAssetLifetime.cpp app/src/main/cpp/content/backgrounds/SurfaceGeometry.cpp app/src/main/cpp/content/backgrounds/SurfaceTables.cpp app/src/main/cpp/content/backgrounds/SurfaceSpriteBatch.cpp)
"${CXX:-g++}" "${flags[@]}" -fPIC -shared tests/surface_backgrounds/managed_surface_fixture.cpp "${sources[@]}" -o "$output/libbunny.so"
LD_LIBRARY_PATH="$output${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "${DOTNET:-dotnet}" run --project tests/surface_backgrounds/Surface.HostTests.csproj -c Release
LD_LIBRARY_PATH="$output${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "${DOTNET:-dotnet}" run --project tests/surface_backgrounds/Session.HostTests.csproj -c Release -- tests/surface_backgrounds/session_fixture/bin/Release/net8.0/Fixture.dll
