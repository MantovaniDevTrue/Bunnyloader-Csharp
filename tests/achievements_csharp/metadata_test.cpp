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
    const std::string name = "6578616D706C656D6F642F4578616D706C65";
    const std::string condA = "C|" + name + "|416E79|1|1|1\n";
    const std::string condB = "C|" + name + "|50726F6772657373|2|3|1.5\n";
    const std::string v2 = "BACH_META_V2\nA|" + row + condA + condB;
    assert(parseAchievementMetadata(v2, "examplemod", &records, &error));
    assert(records.size() == 1 && records[0].conditions.size() == 2);
    assert(records[0].conditions[0].key == "Any");
    assert(records[0].conditions[0].kind == 1 &&
           records[0].conditions[0].maximum == 1 &&
           records[0].conditions[0].value == 1);
    assert(records[0].conditions[1].kind == 2 &&
           records[0].conditions[1].maximum == 3 &&
           records[0].conditions[1].value == 1.5);
    invalid("BACH_META_V2\nA|" + row);
    invalid("BACH_META_V2\n" + condA);
    invalid("BACH_META_V2\nA|" + row + condA + condA);
    invalid("BACH_META_V2\nA|" + row +
            "C|" + name + "|416E79|1|1|0.5\n");
    invalid("BACH_META_V2\nA|" + row +
            "C|" + name + "|416E79|6|2|1\n");
    invalid("BACH_META_V2\nA|" + row +
            "C|" + name + "|416E79|2|3|nan\n");
    invalid("BACH_META_V2\nA|" + row +
            "C|" + name + "|416E79|2|3|9\n");
    invalid("BACH_META_V2\nA|" + row + condA +
            "C|6F74686572|50726F6772657373|2|3|1.5\n");

    std::cout << "CSHARP_ACHIEVEMENT_METADATA_PARSER_PASS=YES\n";
}
