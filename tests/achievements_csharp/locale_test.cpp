#include "experiments/csharp/NativeAchievementLocaleJson.h"

#include <cassert>
#include <iostream>
#include <map>
#include <string>

using namespace bl::csharp;

int main() {
    std::map<std::string, AchievementTranslation> translated;
    const std::string en = R"({
      "ItemName": {"Example": "Sword"},
      "World": {"array": [true, false, null, {"x": -1.1e+3}]},
      "Achievements": {
        "ExampleBossKilled": {
          "FriendlyName": "Boss defeated",
          "Description": "Kill the boss"
        },
        "ManyWormsKilled": {
          "Name": "Worms",
          "Description": "Defeat five worms"
        }
      }
    })";
    assert(parseAchievementTranslations(en, &translated));
    assert(translated.size() == 2);
    assert(translated["ExampleBossKilled"].title == "Boss defeated");
    assert(translated["ExampleBossKilled"].description == "Kill the boss");
    assert(translated["ManyWormsKilled"].title == "Worms");

    const std::string pt =
        "{\"Achievements\":{\"ManyWormsKilled\":{\"FriendlyName\":"
        "\"Muitos Vermes\",\"Description\":\"Voc\\u00ea venceu!\"}}}";
    assert(parseAchievementTranslations(pt, &translated));
    assert(translated.size() == 1);
    assert(translated["ManyWormsKilled"].description == "Você venceu!");

    const auto invalid = [&](std::string_view json) {
        translated["Old"] = {"Old", "Old"};
        assert(!parseAchievementTranslations(json, &translated));
    };
    invalid("{\"Achievements\":{\"A\":{\"FriendlyName\":\"\\uDC00\"}}}");
    invalid("{\"Achievements\":{\"A\":{\"Description\":\"\\uD800\"}}}");
    invalid("{\"Achievements\":{\"A\":{\"Description\":\"bad\\q\"}}}");
    invalid("{\"Achievements\":{\"A\":{\"Description\":\"bad\"},\"A\":{}}}");
    invalid("{\"Achievements\":{\"A\":{\"Description\":}}}");
    invalid("{\"Achievements\":[]}");
    invalid(std::string(512 * 1024 + 1, 'x'));
    assert(parseAchievementTranslations("{}", &translated));
    assert(translated.empty());
    std::cout << "CSHARP_ACHIEVEMENT_LOCALE_JSON_PASS=YES\n";
}
