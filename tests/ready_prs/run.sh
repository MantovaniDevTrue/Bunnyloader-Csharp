#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
g++ -std=c++20 -Wall -Wextra -Werror -pthread -Iapp/src/main/cpp -Itests/basic_items/include tests/ready_prs/dust_emission_test.cpp -o "${RUNNER_TEMP:-/tmp}/bunny-dust-emission"
"${RUNNER_TEMP:-/tmp}/bunny-dust-emission"
