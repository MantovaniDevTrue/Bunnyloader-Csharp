#include "experiments/csharp/ManagedAchievementMetadata.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace bl::csharp;

int main() {
    const std::string row =
        "6578616D706C656D6F642F4578616D706C65|"
        "456E656D79204465666561746564|"
        "44656665617420616E20656E656D79|"
        "436F6E74656E742F416368696576656D656E74732F4578616D706C652E706E67|"
        "0|0|1|0|0.5\n";
    const std::string input = "BACH_META_V1\n" + row;
    std::vector<AchievementMetadata> records;
    std::string error;
    assert(parseAchievementMetadata(input, "examplemod", &records, &error));
    assert(records.size() == 1);
    assert(records[0].name == "examplemod/Example");
    assert(records[0].title == "Enemy Defeated");
    assert(records[0].texture == "Content/Achievements/Example.png");
    assert(records[0].hidden && !records[0].completed &&
           records[0].progress == 0.5 && records[0].category == 0);
    assert(!parseAchievementMetadata(input, "anothermod", &records, &error));
    assert(records.empty());

    const auto invalid = [&](const std::string& wire) {
        assert(!parseAchievementMetadata(wire, "examplemod", &records, &error));
        assert(!error.empty() && records.empty());
    };
    invalid("BACH_META_V2\n" + row);
    invalid("BACH_META_V1\n" + row + row);
    invalid("BACH_META_V1\n" + row.substr(0, row.size() - 1));
    const std::string completed = "0|0|1|0|0.5\n";
    const std::string completeBad = "0|0|0|1|0.5\n";
    auto updated = row;
    updated.replace(updated.size() - completed.size(),
                    completed.size(), completeBad);
    invalid("BACH_META_V1\n" + updated);
    auto large = input;
    for (int i = 0; i < 200; ++i) large += row;
    invalid(large);

    auto done = row;
    done.replace(done.size() - completed.size(), completed.size(),
                 "0|0|0|1|1\n");
    assert(parseAchievementMetadata("BACH_META_V1\n" + done,
                                     "examplemod", &records, &error));
    assert(records[0].completed && records[0].progress == 1);
    std::cout << "CSHARP_ACHIEVEMENT_METADATA_PARSER_PASS=YES\n";
}
