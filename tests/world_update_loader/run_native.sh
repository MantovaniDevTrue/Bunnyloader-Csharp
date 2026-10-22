#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-system-update-contracts"
mkdir -p "$output"
flags=(-std=c++20 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -pthread -Itests/world_update_loader/include -Itests/basic_items/include -Iapp/src/main/cpp)
for test in native_contract_test time_rate_mailbox_test shared_time_rates_test; do
 "${CXX:-g++}" "${flags[@]}" "tests/world_update_loader/$test.cpp" -o "$output/$test"
 "$output/$test"
done
"${CXX:-g++}" "${flags[@]}" tests/world_update_loader/verified_hook_chain_test.cpp app/src/main/cpp/hook/HookManager.cpp -o "$output/verified_hook_chain_test"
"$output/verified_hook_chain_test"
