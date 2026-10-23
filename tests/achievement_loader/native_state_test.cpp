#include "experiments/csharp/NativeAchievementState.h"

#include <cassert>
#include <string>
#include <vector>

using namespace bl::csharp::achievement;

int main() {
    Descriptor worm;
    worm.name = "test/ManyWormsKilled";
    worm.conditions.push_back({"Condition", 1, 5.f, {}});
    Descriptor boss;
    boss.name = "test/Boss";
    boss.conditions.push_back({"NPCKilled", 3, 1.f, {900}});
    const std::vector<Descriptor> declarations{worm, boss};
    const std::string valid =
        "ACHSTATE1\n"
        "S|746573742F4D616E79576F726D734B696C6C6564|0|0\n"
        "V|436F6E646974696F6E|1|3\n"
        "S|746573742F426F7373|0|1\n"
        "V|4E50434B696C6C6564|3|1";
    std::vector<AchievementState> parsed;
    assert(parseState("test", valid, declarations, &parsed));
    assert(parsed.size() == 2);
    assert(parsed[0].conditions[0].value == 3);
    assert(!parsed[0].completed && parsed[1].completed);
    auto saved = parsed;
    assert(!parseState("test", valid + "\nV|4E50434B696C6C6564|3|1", declarations, &parsed));
    assert(parsed.size() == saved.size());
    auto over = valid;
    over.replace(over.find("|1|3\n"), 5, "|1|6\n");
    assert(!parseState("test", over, declarations, &parsed));
    auto scientific = valid;
    scientific.replace(scientific.find("|1|3\n"), 5, "|1|3E+0\n");
    assert(parseState("test", scientific, declarations, &parsed));
    auto badExponent = valid;
    badExponent.replace(badExponent.find("|1|3\n"), 5, "|1|3E+999\n");
    assert(!parseState("test", badExponent, declarations, &parsed));
    auto fractional = valid;
    fractional.replace(fractional.find("|1|3\n"), 5, "|1|2.5\n");
    assert(!parseState("test", fractional, declarations, &parsed));
    auto forged = valid;
    forged.replace(forged.find("|0|1\n"), 5, "|0|0\n");
    assert(parseState("test", forged, declarations, &parsed));
    assert(!parsed[1].completed);
    std::vector<Descriptor> wrong{boss, worm};
    assert(!parseState("test", valid, wrong, &parsed));
    assert(!parseState("test", "ACHSTATE1", declarations, &parsed));
    assert(parseState("test", "ACHSTATE1", {}, &parsed));
    assert(parsed.empty());
    return 0;
}
