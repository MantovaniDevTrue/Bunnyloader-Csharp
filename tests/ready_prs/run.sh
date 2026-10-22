#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
g++ -std=c++20 -Wall -Wextra -Werror -pthread -Iapp/src/main/cpp -Itests/basic_items/include tests/ready_prs/dust_emission_test.cpp app/src/main/cpp/il2cpp/Signature.cpp -o "${RUNNER_TEMP:-/tmp}/bunny-dust-emission"
"${RUNNER_TEMP:-/tmp}/bunny-dust-emission"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -ffunction-sections -fdata-sections -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -I app/src/main/cpp tests/ready_prs/full_source_bootstrap_test.cpp app/src/main/cpp/experiments/csharp/SourceParser.cpp -Wl,--gc-sections -o "${RUNNER_TEMP:-/tmp}/bunny-full-source-bootstrap"
"${RUNNER_TEMP:-/tmp}/bunny-full-source-bootstrap" samples/ExampleModCSharp/Playable/content
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -I app/src/main/cpp tests/ready_prs/managed_source_class_count_test.cpp app/src/main/cpp/experiments/csharp/SourceParser.cpp -o "${RUNNER_TEMP:-/tmp}/bunny-managed-source-count"
"${RUNNER_TEMP:-/tmp}/bunny-managed-source-count" samples/ExampleModCSharp/Playable/content
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -pthread -Iapp/src/main/cpp -Itests/basic_items/include tests/ready_prs/invasion_hud_test.cpp -o "${RUNNER_TEMP:-/tmp}/bunny-invasion-hud"
"${RUNNER_TEMP:-/tmp}/bunny-invasion-hud"
