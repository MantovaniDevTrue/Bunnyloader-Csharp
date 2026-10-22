#include "experiments/csharp/NativeAchievementMetadataSync.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace bl::csharp;

int main() {
    AchievementMetadata source{};
    source.name = "m/Worms";
    source.title = "Worms";
    source.description = "Defeat worms";
    source.texture = "Content/Achievements/Worms.png";
    source.conditions = {{"Kill", 1, 5, 0}, {"Craft", 5, 1, 0}};
    std::vector<AchievementMetadata> current{source};
    std::string error;

    auto next = current;
    next[0].conditions[0].value = 3;
    next[0].progress = 0.5;
    next[0].hidden = true;
    assert(updateAchievementMetadataSnapshot(&current, next, &error));
    assert(current[0].conditions[0].value == 3);
    assert(current[0].conditions[1].value == 0);
    assert(current[0].progress == 0.5 && current[0].hidden);

    // Reject all-or-nothing, even if first condition was valid.
    auto corrupt = current;
    corrupt[0].conditions[0].value = 4;
    corrupt[0].conditions[1].key = "Changed";
    assert(!updateAchievementMetadataSnapshot(&current, corrupt, &error));
    assert(current[0].conditions[0].value == 3);
    assert(current[0].conditions[1].key == "Craft");

    auto progress = current;
    progress[0].conditions[0].value = 5;
    progress[0].conditions[1].value = 1;
    progress[0].completed = true;
    progress[0].progress = 1;
    progress[0].hidden = false;
    assert(updateAchievementMetadataSnapshot(&current, progress, &error));
    assert(current[0].completed && current[0].conditions[0].value == 5);
    assert(current[0].conditions[1].value == 1);

    auto regressed = current;
    regressed[0].completed = false;
    regressed[0].conditions[0].value = 0;
    assert(!updateAchievementMetadataSnapshot(&current, regressed, &error));
    assert(current[0].completed && current[0].conditions[0].value == 5);

    AchievementMetadata flag{};
    flag.name = "m/Flag";
    flag.conditions = {{"ManualFlag", 0, 1, 1}};
    std::vector<AchievementMetadata> flags{flag};
    auto lost = flags;
    lost[0].conditions[0].value = 0;
    assert(!updateAchievementMetadataSnapshot(&flags, lost, &error));
    assert(flags[0].conditions[0].value == 1);

    auto metadataChanged = current;
    metadataChanged[0].texture = "Different.png";
    assert(!updateAchievementMetadataSnapshot(&current, metadataChanged, &error));
    assert(current[0].texture == "Content/Achievements/Worms.png");

    std::cout << "CSHARP_ACHIEVEMENT_NATIVE_SYNC_PASS=YES\n";
}
