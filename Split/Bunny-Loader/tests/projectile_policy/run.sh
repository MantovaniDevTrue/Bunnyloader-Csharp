#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-projectile-policy"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I app/src/main/cpp tests/projectile_policy/native_policy_test.cpp -o "$output"
"$output" build/basic-items/projectile-policy-values.txt
