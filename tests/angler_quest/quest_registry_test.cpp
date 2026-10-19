#include "experiments/csharp/NativeAnglerQuestRegistry.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace bl::csharp::angler;

int main() {
    Registry registry;
    std::string error;
    Quest fish{"examplemodcsharp", "ExampleQuestFish", 850, false, true,
               "An odd upside-down fish.", "Underground"};
    assert(registry.publish("examplemodcsharp", {fish}, &error));
    assert(registry.generation() == 1);
    assert(registry.snapshot().size() == 1);
    assert(!Registry::available(fish, false));
    assert(Registry::available(fish, true));
    Quest catchFish = fish;
    catchFish.catchRules = 3;
    assert(!Registry::canCatch(catchFish, false, false));
    assert(!Registry::canCatch(catchFish, false, true));
    assert(!Registry::canCatch(catchFish, true, false));
    assert(Registry::canCatch(catchFish, true, true));
    catchFish.catchRules = 0;
    assert(Registry::canCatch(catchFish, false, false));
    assert(Registry::chat(fish, "fallback") ==
           "An odd upside-down fish.\n\n(Underground)");
    assert(Registry::chat(Quest{"x","Fallback",852,true,true,"",""}, "Fallback Name")
           == "Fallback Name");
    Quest translated = fish;
    translated.locales.emplace("pt-BR",
        std::make_pair("Um peixe de ponta-cabeça", "Pescado ao contrário"));
    assert(Registry::chat(translated, "fallback", "pt-BR") ==
           "Um peixe de ponta-cabeça\\n\\n(Pescado ao contrário)");
    assert(Registry::chat(translated, "fallback", "pt-PT") ==
           "Um peixe de ponta-cabeça\\n\\n(Pescado ao contrário)");
    assert(Registry::chat(translated, "fallback", "en-US") ==
           "An odd upside-down fish.\\n\\n(Underground)");
    assert(registry.publish("examplemodcsharp", {translated}, &error));
    assert(!registry.publish("examplemodcsharp",
        {Quest{"examplemodcsharp","BadLocale",860,true,true,"","",0,
               {{"!!!",{"x","y"}}}}}, &error));
    assert(error == "InvalidQuestTranslation");
    assert(registry.snapshot().front().locales.size() == 1);
    assert(registry.publish("examplemodcsharp", {fish}, &error));

    auto merged = Registry::mergeIds({1, 2, 850, 2}, {fish,
        Quest{"secondmod", "ExtraQuest", 851, true, true, "", ""}});
    assert((merged == std::vector<int>{1, 2, 850, 2, 851}));
    assert(Registry::mergeIds(merged, {fish}).size() == merged.size());

    assert(!registry.publish("secondmod",
        {Quest{"secondmod", "OtherFish", 850, true, true, "", ""}}, &error));
    assert(error == "QuestTypeOwnedByAnotherMod");
    assert(registry.snapshot().size() == 1);
    assert(!registry.publish("examplemodcsharp", {fish, fish}, &error));
    assert(error == "DuplicateQuestInMod");
    assert(registry.snapshot().size() == 1);
    assert(!registry.publish("examplemodcsharp",
        {Quest{"examplemodcsharp", "NoPhases", 860, false, false, "", ""}}, &error));
    assert(error == "InvalidQuestDescriptor");
    assert(registry.snapshot().size() == 1);
    assert(!registry.publish("examplemodcsharp",
        {Quest{"examplemodcsharp", "BadCatchRule", 860, true, true, "", "", 4}}, &error));
    assert(error == "InvalidQuestDescriptor");
    assert(!registry.publish("examplemodcsharp",
        {Quest{"examplemodcsharp", "TooLargeId", 65536, true, true, "", ""}}, &error));
    assert(error == "InvalidQuestDescriptor");
    assert(!registry.publish("examplemodcsharp",
        {Quest{"examplemodcsharp", "InvalidUtf8", 860, true, true,
               std::string({char(0xC0), char(0xAF)}), ""}}, &error));
    assert(error == "InvalidQuestDescriptor");
    assert(registry.generation() == 3);
    assert(!registry.publish("bad",
        {Quest{"different", "WrongOwner", 860, true, true, "", ""}}, &error));
    assert(error == "InvalidQuestDescriptor");
    assert(!registry.publish("bad",
        {Quest{"bad", "Invalid-Name", 860, true, true, "", ""}}, &error));
    assert(error == "InvalidQuestDescriptor");
    assert(!registry.publish("bad",
        {Quest{"bad", "LongText", 860, true, true, std::string(513,'x'), ""}}, &error));
    assert(error == "InvalidQuestDescriptor");

    assert(registry.publish("secondmod",
        {Quest{"secondmod", "SecondFish", 851, true, true, "", ""}}, &error));
    assert(registry.snapshot().size() == 2);
    assert(registry.publish("examplemodcsharp", {fish}, &error));
    assert(registry.snapshot().size() == 2);
    const auto version = registry.generation();
    registry.unload("examplemodcsharp");
    assert(registry.generation() == version + 1);
    assert(registry.snapshot().size() == 1);
    assert(registry.snapshot().front().mod == "secondmod");
    registry.unload("secondmod");
    assert(registry.snapshot().empty());

    std::string decoded = "unchanged";
    assert(decodeHex("48656C6C6F20C3A9", &decoded));
    assert(decoded == "Hello é");
    assert(decodeHex("-", &decoded) && decoded.empty());
    for (const auto& bad : {"ABC", "GG", "01BBFFZ0", "00", "0D", "09"}) {
        assert(!decodeHex(bad, &decoded));
    }
    assert(!decodeHex(std::string(1026, 'A'), &decoded));
    assert(!decodeHex("C0AF", &decoded));
    assert(!decodeHex("EDA080", &decoded));

    int current = 850;
    int rollCount = 0;
    auto swap = [&] { ++rollCount; current = rollCount == 3 ? 851 : 850; };
    auto read = [&] { return current; };
    auto unfinished = [] { return false; };
    auto candidates = std::vector<Quest>{fish};
    Registry::reroll(swap, read, unfinished, candidates, false);
    assert(rollCount == 3);
    Registry::reroll(swap, read, unfinished, candidates, false);
    assert(rollCount == 3);

    current = 850;
    rollCount = 0;
    Registry::reroll([&] { ++rollCount; }, read, unfinished, candidates, false);
    assert(rollCount == 50);
    current = 850;
    rollCount = 0;
    Registry::reroll([&] { ++rollCount; }, read, [] { return true; }, candidates, false);
    assert(rollCount == 0);

    std::cout << "ANGLER_QUEST_REGISTRY_OK\n";
    return 0;
}
