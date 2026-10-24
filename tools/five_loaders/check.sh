#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root"
mkdir -p build/five-checks
python3 tests/five_loaders/package_import_test.py
export MSBUILDDISABLENODEREUSE=1
build() { dotnet build "$1" -c Release --nologo -m:1 -nr:false -p:BuildInParallel=false -p:UseSharedCompilation=false; }
run() { dotnet run --project "$1" -c Release --no-build -- "${@:2}"; }
for project in experiments/csharp/BunnySourceSdk/BunnySourceSdk.csproj tests/StatModifierParity/StatModifierParity.csproj tests/world_update_loader/WorldUpdateLoader.csproj tests/keybind_loader/KeybindLoaderTests.csproj tests/csharp_gore_examples/GoreExamples.csproj tests/world_save/BunnyWorldData.Smoke.csproj tests/world_save/WorldSaveExampleMod.csproj; do build "$project"; done
run tests/csharp_gore_examples/GoreExamples.csproj
run tests/StatModifierParity/StatModifierParity.csproj
run tests/world_update_loader/WorldUpdateLoader.csproj
run tests/keybind_loader/KeybindLoaderTests.csproj
run tests/world_save/BunnyWorldData.Smoke.csproj
run tests/world_save/BunnyWorldData.Smoke.csproj --mono-host-tests "$root/tests/world_save/bin/Release/net8.0/WorldSaveExampleMod.dll"
run tests/world_save/BunnyWorldData.Smoke.csproj --emit-fixture "$root/build/five-checks/world.bwc"
run tests/world_save/BunnyWorldData.Smoke.csproj --emit-network-fixture "$root/build/five-checks/network.bwn"
python3 tests/world_update_loader/check_contract.py
python3 tests/world_update_loader/check_arm64_assembly.py
python3 tests/keybind_loader/check_native_touch_signatures.py
flags=(-std=c++20 -O1 -g -Wall -Wextra -Werror -Wno-deprecated-declarations -pedantic -pthread -Iapp/src/main/cpp)
cpp=app/src/main/cpp/experiments/csharp
native() { local name="$1"; shift; g++ "${flags[@]}" "$@" -o "build/five-checks/$name"; }
g++ "${flags[@]}" -ffunction-sections -fdata-sections -Itests/basic_items/include tests/five_loaders/gore_startup_stage_test.cpp -Wl,--gc-sections -o build/five-checks/gore-startup
build/five-checks/gore-startup
g++ "${flags[@]}" -ffunction-sections -fdata-sections -Itests/basic_items/include tests/five_loaders/mobile_chat_test.cpp -Wl,--gc-sections -o build/five-checks/mobile-chat
build/five-checks/mobile-chat
native csharp-log -Itests/five_loaders/include tests/five_loaders/csharp_log_test.cpp app/src/main/cpp/core/Log.cpp
build/five-checks/csharp-log
native gore-catalog tests/csharp_gore_catalog_test.cpp
native gore-discovery "$cpp/ManagedGoreDiscovery.cpp" tests/csharp_gore_discovery_test.cpp
for name in action_ids input layout_json; do
 case "$name" in action_ids) src=NativeTouchActionIds;; input) src=NativeTouchInput;; layout_json) src=NativeTouchLayoutJson;; esac
 native "touch-$name" "tests/keybind_loader/native_touch_${name}_test.cpp" "$cpp/$src.cpp"
done
for name in group_selection layout_identity layout_refresh; do native "touch-$name" "tests/keybind_loader/native_touch_${name}_test.cpp"; done
for name in stable_world_uid world_net_async mono_world_mailbox; do native "$name" "tests/world_save/${name}_test.cpp"; done
native world-parser "$cpp/SourceParser.cpp" tests/world_save/world_source_parser_test.cpp
native world-store "$cpp/NativeWorldStore.cpp" tests/world_save/native_world_store_test.cpp
native world-coordinator "$cpp/NativeWorldStore.cpp" "$cpp/NativeWorldCoordinator.cpp" tests/world_save/native_world_coordinator_test.cpp
native world-network "$cpp/NativeWorldStore.cpp" "$cpp/NativeWorldCoordinator.cpp" "$cpp/NativeWorldNetwork.cpp" tests/world_save/native_world_network_test.cpp
for name in gore-catalog gore-discovery touch-action_ids touch-input touch-layout_json touch-group_selection touch-layout_identity touch-layout_refresh stable_world_uid world_net_async mono_world_mailbox world-parser; do "build/five-checks/$name"; done
build/five-checks/world-store build/five-checks/world.bwc
build/five-checks/world-coordinator build/five-checks/world.bwc
build/five-checks/world-network build/five-checks/network.bwn
g++ "${flags[@]}" -ffunction-sections -fdata-sections -Itests/basic_items/include tests/five_loaders/mobile_netpacket_test.cpp "$cpp/NativeWorldNetwork.cpp" -Wl,--gc-sections -o build/five-checks/mobile-netpacket
build/five-checks/mobile-netpacket build/five-checks/network.bwn
native world-quit "$cpp/NativeWorldStore.cpp" "$cpp/NativeWorldCoordinator.cpp" tests/five_loaders/world_quit_save_test.cpp
build/five-checks/world-quit build/five-checks/world.bwc
native qa-discovery "$cpp/SourceParser.cpp" tests/five_loaders/source_discovery_test.cpp
build/five-checks/qa-discovery samples/FiveLoadersCSharp/Playable/content
printf 'FIVE_LOADERS_CONTRACTS_PASS\n'
