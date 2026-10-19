#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="${RUNNER_TEMP:-/tmp}/bunny-summon-runtime"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -I app/src/main/cpp tests/summon_lifecycle/runtime_test.cpp -pthread -Wl,--gc-sections -o "$output"
"$output"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -ffunction-sections -fdata-sections -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -I app/src/main/cpp tests/summon_lifecycle/source_damage_test.cpp -pthread -Wl,--gc-sections -o "${output}-source"
"${output}-source"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I app/src/main/cpp tests/summon_lifecycle/contracts_test.cpp -o "${output}-contracts"
"${output}-contracts" build/basic-items/full-source-runtime-values.txt
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I tests/basic_items/include -I app/src/main/cpp tests/summon_lifecycle/native_binding_test.cpp -o "${output}-binding"
"${output}-binding"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -I app/src/main/cpp tests/summon_lifecycle/native_defaults_test.cpp -pthread -o "${output}-defaults"
"${output}-defaults" build/basic-items/full-source-runtime-values.txt
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -I app/src/main/cpp tests/summon_lifecycle/holdout_test.cpp -pthread -o "${output}-holdout"
"${output}-holdout" build/basic-items/full-source-runtime-values.txt
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -ffunction-sections -fdata-sections -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -I app/src/main/cpp tests/summon_lifecycle/source_registration_test.cpp app/src/main/cpp/experiments/csharp/SourceParser.cpp -Wl,--gc-sections -o "${output}-registration"
"${output}-registration" samples/ExampleModCSharp/Playable/content/Content/Projectiles/ExampleMinion.cs samples/ExampleModCSharp/Playable/content/Content/Items/Weapons/Ranged/ExampleGun.cs samples/ExampleModCSharp/Playable/content/Content/Items/Weapons/Ranged/ExampleShotgun.cs
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -ffunction-sections -fdata-sections -DBL_HAVE_QUICKJS=0 -I tests/basic_items/include -I app/src/main/cpp tests/summon_lifecycle/scheduler_test.cpp -Wl,--gc-sections -pthread -ldl -o "${output}-scheduler"
"${output}-scheduler"

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -Wno-deprecated-declarations -I app/src/main/cpp tests/summon_lifecycle/use_sound_test.cpp -pthread -o "${output}-sound"
"${output}-sound" samples/ExampleModCSharp/Playable build/basic-items/full-source-runtime-values.txt

"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I tests/basic_items/include -I app/src/main/cpp tests/summon_lifecycle/sound_router_test.cpp -pthread -o "${output}-router"
"${output}-router"
