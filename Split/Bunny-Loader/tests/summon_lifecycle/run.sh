#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-summon-runtime"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -I app/src/main/cpp tests/summon_lifecycle/runtime_test.cpp -Wl,--gc-sections -o "$output"
"$output"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -ffunction-sections -fdata-sections -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -I app/src/main/cpp tests/summon_lifecycle/source_damage_test.cpp -Wl,--gc-sections -o "${output}-source"
"${output}-source"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I app/src/main/cpp tests/summon_lifecycle/contracts_test.cpp -o "${output}-contracts"
"${output}-contracts" build/basic-items/full-source-runtime-values.txt
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -ffunction-sections -fdata-sections -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -I app/src/main/cpp tests/summon_lifecycle/source_registration_test.cpp app/src/main/cpp/experiments/csharp/SourceParser.cpp -Wl,--gc-sections -o "${output}-registration"
"${output}-registration" samples/ExampleModCSharp/Playable/content/Content/Projectiles/ExampleMinion.cs
