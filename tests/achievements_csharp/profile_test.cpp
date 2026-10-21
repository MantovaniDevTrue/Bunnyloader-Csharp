#include "experiments/csharp/NativeAchievementProfile.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>

int main() {
    using bl::csharp::achievementProfileKey;
    const std::string alice = "/storage/emulated/0/Android/data/terraria/Players/Alice.plr";
    const std::string bob = "/storage/emulated/0/Android/data/terraria/Players/Bob.plr";
    const auto a = achievementProfileKey(alice, false);
    const auto b = achievementProfileKey(bob, false);
    const auto cloud = achievementProfileKey(alice, true);
    assert(a.size() == 64 && b.size() == 64 && cloud.size() == 64);
    assert(std::all_of(a.begin(), a.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    }));
    assert(a != b && a != cloud && b != cloud);
    assert(a == achievementProfileKey(alice, false));
    assert(achievementProfileKey("", false).empty());
    assert(achievementProfileKey(std::string(2049, 'x'), false).empty());
    assert(achievementProfileKey(std::string("/world/") + bob, false) != b);
    std::cout << "CSHARP_ACHIEVEMENT_PROFILE_ISOLATION_PASS=YES\n";
}
