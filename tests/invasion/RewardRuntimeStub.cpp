#include "../../app/src/main/cpp/experiments/csharp/NativeInvasionRewards.h"

#include <atomic>
#include <string>

namespace {
std::atomic<int> rewardType{0};
std::atomic<int> rewardStack{0};
std::atomic<int> rewardPlayer{-1};
}

namespace bl::runtime {
int netMode() { return 0; }
bool modItemsSettled() { return true; }
int modItemTypeByKey(const std::string& key) {
    return key == "mod-a/ExampleItem" ? 6215 : -1;
}
std::string modItemKey(int type) {
    return type == 6215 ? "mod-a/ExampleItem" : std::string();
}
int itemTypeCount() { return 6400; }
void giveItem(int type, int stack, int player) {
    rewardType.store(type);
    rewardStack.store(stack);
    rewardPlayer.store(player);
}
}

extern "C" void bunny_invasion_test_reward_world(int ready, long session) {
    bl::csharp::setNativeInvasionRewardWorldState(
        ready != 0, static_cast<uint64_t>(session));
}

extern "C" void bunny_invasion_test_reward_pump() {
    bl::csharp::pumpNativeInvasionRewardsOnGameThread();
}

extern "C" int bunny_invasion_test_reward_type() { return rewardType.load(); }
extern "C" int bunny_invasion_test_reward_stack() { return rewardStack.load(); }
extern "C" int bunny_invasion_test_reward_player() { return rewardPlayer.load(); }
