#include "experiments/csharp/NativeAchievementAtlasPlan.h"

#include <cassert>
#include <vector>

using namespace bl::csharp::achievement;

int main() {
    Descriptor one;
    one.name = "example/ManyWormsKilled";
    one.texture = "Content/Achievements/ManyWormsKilled";
    std::vector<AtlasEntry> a;
    assert(planAchievementAtlases("example", {one}, &a));
    assert(a.size() == 1);
    assert(a[0].filename == "content/Content/Achievements/ManyWormsKilled.png");
    assert(planAchievementAtlases("example", {one, one}, &a));
    assert(a.size() == 1);
    assert(!planAchievementAtlases("other", {one}, &a));
    assert(a.size() == 1);
    one.texture = "../outside";
    assert(!planAchievementAtlases("example", {one}, &a));
    one.texture = "Content/Achievements/../../escape";
    assert(!planAchievementAtlases("example", {one}, &a));
    one.texture = "Content/Achievements\\Escape";
    assert(!planAchievementAtlases("example", {one}, &a));
    one.texture = "Content/Achievements/ManyWormsKilled";
    one.index = 8;
    assert(!planAchievementAtlases("example", {one}, &a));
    assert(a.size() == 1);
    return 0;
}
