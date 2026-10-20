#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionRewards.h"

#include <cassert>
#include <cstdint>

extern "C" int bunny_invasion_reward_submit(
    int64_t, int, const char*, const char*, const char*, int, int);
extern "C" void bunny_invasion_test_reward_world(int, long);
extern "C" void bunny_invasion_test_reward_pump();
extern "C" int bunny_invasion_test_reward_type();
extern "C" int bunny_invasion_test_reward_stack();
extern "C" int bunny_invasion_test_reward_player();

int main() {
    bunny_invasion_test_reward_world(0, 0);
    assert(!bunny_invasion_reward_submit(11, 1, "mod-a",
        "example-invasion", "ExampleItem", 5, 0));
    bunny_invasion_test_reward_world(1, 11);
    assert(!bunny_invasion_reward_submit(11, 1, "mod-a",
        "../bad", "ExampleItem", 5, 0));
    assert(!bunny_invasion_reward_submit(10, 1, "mod-a",
        "example-invasion", "ExampleItem", 5, 0));
    assert(bunny_invasion_reward_submit(11, 1, "mod-a",
        "example-invasion", "ExampleItem", 5, 4));
    assert(bunny_invasion_reward_submit(11, 1, "mod-a",
        "example-invasion", "ExampleItem", 5, 4));
    bunny_invasion_test_reward_pump();
    assert(bunny_invasion_test_reward_type() == 6215);
    assert(bunny_invasion_test_reward_stack() == 5);
    assert(bunny_invasion_test_reward_player() == 4);
    assert(bunny_invasion_reward_submit(11, 1, "mod-a",
        "example-invasion", "ExampleItem", 5, 8));
    bunny_invasion_test_reward_pump();
    assert(bunny_invasion_test_reward_player() == 4);
    assert(bunny_invasion_reward_submit(11, 2, "mod-a",
        "example-invasion", "ExampleItem", 3, 7));
    bunny_invasion_test_reward_pump();
    assert(bunny_invasion_test_reward_stack() == 3);
    assert(bunny_invasion_test_reward_player() == 7);
    bunny_invasion_test_reward_world(0, 12);
    assert(!bunny_invasion_reward_submit(11, 3, "mod-a",
        "example-invasion", "ExampleItem", 5, 0));
    bunny_invasion_test_reward_world(1, 12);
    assert(bunny_invasion_reward_submit(12, 1, "mod-a",
        "example-invasion", "ExampleItem", 5, 1));
    bunny_invasion_test_reward_pump();
    assert(bunny_invasion_test_reward_stack() == 5);
    assert(bunny_invasion_test_reward_player() == 1);
    return 0;
}
