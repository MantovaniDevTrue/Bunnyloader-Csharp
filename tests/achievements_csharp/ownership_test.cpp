#include "experiments/csharp/NativeAchievementOwnership.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    using bl::csharp::achievementOwnedByMod;
    using bl::csharp::validAchievementModUid;
    assert(validAchievementModUid("split"));
    assert(validAchievementModUid("Example.Mod-1"));
    assert(!validAchievementModUid(""));
    assert(!validAchievementModUid("../split"));
    assert(!validAchievementModUid("split/other"));
    assert(!validAchievementModUid(std::string(97, 'a')));

    assert(achievementOwnedByMod("split/ParaffinKilled", "split"));
    assert(achievementOwnedByMod("split/ManyWormsKilled", "split"));
    assert(!achievementOwnedByMod("splitplus/ParaffinKilled", "split"));
    assert(!achievementOwnedByMod("split", "split"));
    assert(!achievementOwnedByMod("split/", "split"));
    assert(!achievementOwnedByMod("Split/Boss", "split"));
    assert(!achievementOwnedByMod("Terraria/KingSlime", "split"));
    assert(!achievementOwnedByMod("other/split/Boss", "split"));
    assert(!achievementOwnedByMod("split/Boss", "split/other"));
    std::cout << "CSHARP_ACHIEVEMENT_NATIVE_OWNERSHIP_PASS=YES\n";
}
