#include "experiments/csharp/NativeAnglerQuestRegistry.h"
#include <cassert>
#include <atomic>
#include <thread>
#include <iostream>
#include <string>
#include <vector>

using namespace bl::csharp::angler;

int main() {
    Registry registry;
    std::string error;
    assert(registry.generation() == 0);
    assert(registry.publish("noquest", {}, &error));
    assert(registry.generation() == 0);
    Quest fish{"examplemodcsharp", "ExampleQuestFish", 850, false, true,
               "An odd upside-down fish.", "Underground"};
    assert(registry.publish("examplemodcsharp", {fish}, &error));
    assert(registry.generation() == 1);
    assert(registry.publish("examplemodcsharp", {fish}, &error));
    assert(registry.generation() == 1);
    assert(registry.snapshot().size() == 1);
    auto [capturedRevision, capturedQuests] = registry.snapshotVersion();
    assert(capturedRevision == registry.generation());
    assert(capturedQuests == registry.snapshot());
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
           "Um peixe de ponta-cabeça\n\n(Pescado ao contrário)");
    assert(Registry::chat(translated, "fallback", "pt-PT") ==
           "Um peixe de ponta-cabeça\n\n(Pescado ao contrário)");
    assert(Registry::chat(translated, "fallback", "en-US") ==
           "An odd upside-down fish.\n\n(Underground)");
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
    const std::vector<int> moddedIds{1, 2, 850, 7001, 851, 2};
    auto same = Registry::reconcileIds(moddedIds, {850}, {fish}, 3);
    assert(same.changed && same.selected == 2);
    assert((same.ids == std::vector<int>{1, 2, 7001, 851, 2, 850}));
    const auto stable = Registry::reconcileIds(same.ids, {850}, {fish}, same.selected);
    assert(!stable.changed && stable.selected == same.selected && stable.ids == same.ids);
    auto newQuest = Quest{"examplemodcsharp", "OtherQuest", 860, true, true, "", ""};
    auto updated = Registry::reconcileIds(moddedIds, {850}, {newQuest}, 4);
    assert(updated.changed);
    assert((updated.ids == std::vector<int>{1, 2, 7001, 851, 2, 860}));
    assert(updated.selected == 3);
    auto unloaded = Registry::reconcileIds(updated.ids, {860}, {}, 5);
    assert(unloaded.changed);
    assert((unloaded.ids == std::vector<int>{1, 2, 7001, 851, 2}));
    assert(unloaded.selected == 0);
    assert(Registry::reconcileIds(unloaded.ids, {}, {}, 2).ids == unloaded.ids);


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

    Quest first{"alpha", "First", 950, true, true, "", ""};
    Quest second{"zeta", "Second", 900, true, true, "", ""};
    const std::vector<int> base{1, 2, 7001, 2};
    auto inOrder = Registry::reconcileIds(base, {}, {first, second}, 2);
    auto reverseOrder = Registry::reconcileIds(base, {}, {second, first}, 2);
    assert(inOrder.ids == reverseOrder.ids);
    const auto manifest=Registry::manifestFingerprint({first,second});
    assert(manifest==Registry::manifestFingerprint({second,first}));
    assert(manifest!=Registry::manifestFingerprint({first}));
    Quest differentType=second;
    differentType.type=901;
    assert(manifest!=Registry::manifestFingerprint({first,differentType}));
    differentType=second;
    differentType.callbackMask=1;
    assert(manifest!=Registry::manifestFingerprint({first,differentType}));
    differentType=second;
    differentType.description="Different quest";
    assert(manifest!=Registry::manifestFingerprint({first,differentType}));
    differentType=second;
    differentType.location="Jungle";
    assert(manifest!=Registry::manifestFingerprint({first,differentType}));
    differentType=second;
    differentType.locales.emplace("pt-BR",std::make_pair("Peixe","Selva"));
    assert(manifest!=Registry::manifestFingerprint({first,differentType}));
    Quest reversedLocales=differentType;
    reversedLocales.locales.emplace("en-US",std::make_pair("Fish","Jungle"));
    Quest swappedLocales=differentType;
    swappedLocales.locales.clear();
    swappedLocales.locales.emplace("en-US",std::make_pair("Fish","Jungle"));
    swappedLocales.locales.emplace("pt-BR",std::make_pair("Peixe","Selva"));
    assert(Registry::manifestFingerprint({first,reversedLocales})==
           Registry::manifestFingerprint({swappedLocales,first}));
    const auto effective=Registry::listFingerprint(inOrder.ids,{first,second});
    assert(effective==Registry::listFingerprint(reverseOrder.ids,{second,first}));
    assert(effective!=Registry::listFingerprint({1,2,7001,2,900,950},
                                               {first,second}));
    assert(effective!=Registry::listFingerprint({1,2,7002,2,950,900},
                                               {first,second}));
    assert(effective!=Registry::listFingerprint(inOrder.ids,{first,differentType}));
    assert((inOrder.ids == std::vector<int>{1, 2, 7001, 2, 950, 900}));
    auto incremental = Registry::reconcileIds(base, {}, {second}, 2);
    incremental = Registry::reconcileIds(incremental.ids, {900}, {second, first},
                                         incremental.selected);
    assert(incremental.ids == inOrder.ids && incremental.selected == 2);
    assert(registry.publish("zeta", {second}, &error));
    assert(registry.publish("alpha", {first}, &error));
    const auto sorted = registry.snapshot();
    assert(sorted[0].mod == "alpha" && sorted[1].mod == "examplemodcsharp" &&
           sorted[2].mod == "zeta");
    registry.unload("zeta");
    registry.unload("alpha");


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
    auto [clearedRevision, clearedQuests] = registry.snapshotVersion();
    assert(clearedRevision == registry.generation());
    assert(clearedRevision > capturedRevision && clearedQuests.empty());

    Registry concurrent;
    std::atomic<bool> published{false};
    std::thread writer([&] {
        for (int i=1;i<=1000;++i) {
            const std::string name="Q"+std::to_string(i);
            std::string message;
            assert(concurrent.publish("parallel",
                {Quest{"parallel",name,910,true,true,"",""}},&message));
        }
        published.store(true,std::memory_order_release);
    });
    do {
        const auto [generation, values]=concurrent.snapshotVersion();
        if (!generation) {
            assert(values.empty());
        } else {
            assert(values.size()==1);
            assert(values.front().name=="Q"+std::to_string(generation));
        }
    } while (!published.load(std::memory_order_acquire));
    writer.join();
    const auto [finalGeneration, finalValues]=concurrent.snapshotVersion();
    assert(finalGeneration==1000 && finalValues.front().name=="Q1000");

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
