#include <atomic>
#include <cassert>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "experiments/csharp/ManagedPrefixBridge.h"

namespace bl::log {
void write(int, const char*, ...) {}
}

namespace bl::runtime {
int modPrefixByKey(const std::string& key) {
    if (key == "test/Guarded") return 98;
    if (key == "test/Mighty") return 99;
    if (key == "test/Swift") return 100;
    if (key == "test/Custom") return 101;
    if (key == "test/Alternate") return 102;
    return -1;
}
int modItemTypeByKey(const std::string& key) {
    if (key == "test/ExampleBlade") return 6207;
    if (key == "test/ExampleCharm") return 6208;
    return -1;
}
}

namespace bl::csharp {
bool installManagedPrefixGameplayHooks() { return true; }
bool installManagedPrefixChanceHook() { return true; }
bool queueManagedPrefixWarmup(const std::string&,const std::string&,const std::string&) {
    return true;
}
bool queueManagedItemPrefixWarmup(const std::string&,const std::string&,const std::string&) {
    return true;
}
}

#include "../../app/src/main/cpp/experiments/csharp/ManagedPrefixBridge.cpp"

int main() {
    using namespace bl::csharp;
    auto first = managedPrefixView();
    assert(first && first->empty());
    assert(registeredManagedPrefixCount() == 0);
    assert(managedItemPrefixCallbackView()->empty());
    assert(!applyManagedItemPrefixCallbackBindings("", {{6207, "ExampleBlade"}}));
    assert(!applyManagedItemPrefixCallbackBindings("test", {{6207, "Missing"}}));
    assert(!applyManagedItemPrefixCallbackBindings("test",
        {{6207, "ExampleBlade"}, {6207, "ExampleBlade"}}));
    assert(managedItemPrefixCallbackView()->empty());
    assert(applyManagedItemPrefixCallbackBindings("test", {{6207, "ExampleBlade"}}));
    auto firstCallbacks = managedItemPrefixCallbackView();
    assert(firstCallbacks->at(6207).modId == "test");
    assert(firstCallbacks->at(6207).owner == "ExampleBlade");
    assert(!applyManagedItemPrefixCallbackBindings("test", {{6207, "ExampleBlade"}}));
    assert(managedItemPrefixCallbackView() == firstCallbacks);
    assert(applyManagedItemPrefixCallbackBindings("test", {{6208, "ExampleCharm"}}));
    assert(managedItemPrefixCallbackView()->at(6208).owner == "ExampleCharm");
    assert(firstCallbacks->size() == 1);
    assert(managedItemPrefixChanceView()->empty());
    assert(!applyManagedItemPrefixChances({{6207, false}, {6207, true}}));
    assert(managedItemPrefixChanceView()->empty());
    assert(applyManagedItemPrefixChances({{6207, false}}));
    auto firstChances = managedItemPrefixChanceView();
    assert(firstChances->at(6207) == false);
    assert(!applyManagedItemPrefixChances({{6207, true}}));
    assert(managedItemPrefixChanceView() == firstChances);
    assert(applyManagedItemPrefixChances({{6208, true}}));
    assert(managedItemPrefixChanceView()->at(6208));
    assert(firstChances->size() == 1);
    assert(managedItemPrefixStackChanceView()->empty());
    assert(!applyManagedItemPrefixStackChances({{6207, {5, 3, true}}}));
    assert(managedItemPrefixStackChanceView()->empty());
    assert(!applyManagedItemPrefixStackChances({{6207, {0, 1000000, true}}}));
    assert(managedItemPrefixStackChanceView()->empty());
    assert(!applyManagedItemPrefixStackChances({{6207, {3, 5, true}},
                                                {6207, {4, 6, false}}}));
    assert(managedItemPrefixStackChanceView()->empty());
    assert(applyManagedItemPrefixStackChances({{6207, {3, 5, true}}}));
    auto firstStackChances = managedItemPrefixStackChanceView();
    assert(firstStackChances->at(6207).minStack == 3);
    assert(firstStackChances->at(6207).maxStack == 5);
    assert(firstStackChances->at(6207).allowed);
    assert(!applyManagedItemPrefixStackChances({{6207, {1, 2, false}}}));
    assert(managedItemPrefixStackChanceView() == firstStackChances);
    assert(applyManagedItemPrefixStackChances({{6208, {3, 5, false}}}));
    assert(!managedItemPrefixStackChanceView()->at(6208).allowed);
    assert(firstStackChances->size() == 1);
    assert(managedItemPrefixChoiceView()->empty());
    assert(!applyManagedItemPrefixChoices("test",
        {{6207, "MissingPrefix", 1, 2}}));
    assert(managedItemPrefixChoiceView()->empty());
    assert(!applyManagedItemPrefixChoices("test",
        {{6207, "71", 5, 3}}));
    assert(managedItemPrefixChoiceView()->empty());
    assert(!applyManagedItemPrefixChoices("test",
        {{6207, "256", 1, 2}}));
    assert(managedItemPrefixChoiceView()->empty());
    assert(!applyManagedItemPrefixChoices("test",
        {{6207, "Guarded", 1, 2}, {6207, "71", 1, 2}}));
    assert(managedItemPrefixChoiceView()->empty());
    assert(applyManagedItemPrefixChoices("test",
        {{6207, "Guarded", 1, 2}}));
    auto firstChoices = managedItemPrefixChoiceView();
    assert(firstChoices->at(6207).prefix == 98);
    assert(firstChoices->at(6207).minStack == 1);
    assert(firstChoices->at(6207).maxStack == 2);
    assert(!applyManagedItemPrefixChoices("test",
        {{6207, "71", 1, 3}}));
    assert(managedItemPrefixChoiceView() == firstChoices);
    assert(applyManagedItemPrefixChoices("test",
        {{6208, "71", 2, 4}}));
    assert(managedItemPrefixChoiceView()->at(6208).prefix == 71);
    assert(firstChoices->size() == 1);
    assert(managedItemPrefixApplyView()->empty());
    assert(!applyManagedItemPrefixEffects({{6207, {0, 0}}}));
    assert(!applyManagedItemPrefixEffects({{6207, {1001, 0}}}));
    assert(!applyManagedItemPrefixEffects({{6207, {4, 125}}, {6207, {5, 200}}}));
    assert(managedItemPrefixApplyView()->empty());
    assert(applyManagedItemPrefixEffects({{6207, {4, 125}}}));
    auto firstEffects = managedItemPrefixApplyView();
    assert(firstEffects->at(6207).damageBonus == 4);
    assert(firstEffects->at(6207).valueBonus == 125);
    assert(!applyManagedItemPrefixEffects({{6207, {1, 0}}}));
    assert(managedItemPrefixApplyView() == firstEffects);
    assert(applyManagedItemPrefixEffects({{6208, {2, 0}}}));
    assert(managedItemPrefixApplyView()->at(6208).damageBonus == 2);
    assert(firstEffects->size() == 1);
    assert(managedItemPrefixDenyView()->empty());
    assert(!applyManagedItemPrefixDenials("test", {{6207, {"71", "71"}}}));
    assert(managedItemPrefixDenyView()->empty());
    assert(!applyManagedItemPrefixDenials("test", {{6207, {"MissingPrefix"}}}));
    assert(managedItemPrefixDenyView()->empty());
    assert(!applyManagedItemPrefixDenials("test", {{6207, {"256"}}}));
    assert(managedItemPrefixDenyView()->empty());
    assert(applyManagedItemPrefixDenials("test", {{6207, {"71", "Mighty"}}}));
    auto firstBans = managedItemPrefixDenyView();
    assert(firstBans->at(6207).contains(71));
    assert(firstBans->at(6207).contains(99));
    assert(!applyManagedItemPrefixDenials("test", {{6207, {"72"}}}));
    assert(managedItemPrefixDenyView() == firstBans);
    assert(applyManagedItemPrefixDenials("test", {{6208, {"72"}}}));
    assert(managedItemPrefixDenyView()->at(6208).contains(72));
    assert(firstBans->size() == 1);
    assert(managedItemPrefixStackDenyView()->empty());
    assert(!applyManagedItemPrefixStackDenials("test",
        {{6207, {{"MissingPrefix", {3, 5}}}}}));
    assert(managedItemPrefixStackDenyView()->empty());
    assert(!applyManagedItemPrefixStackDenials("test",
        {{6207, {{"72", {6, 5}}}}}));
    assert(managedItemPrefixStackDenyView()->empty());
    assert(!applyManagedItemPrefixStackDenials("test",
        {{6207, {{"72", {3, 5}}, {"72", {4, 6}}}}}));
    assert(managedItemPrefixStackDenyView()->empty());
    assert(applyManagedItemPrefixStackDenials("test",
        {{6207, {{"72", {3, 5}}, {"Guarded", {4, 6}}}}}));
    auto firstStackBans = managedItemPrefixStackDenyView();
    assert(firstStackBans->at(6207).at(72).minStack == 3);
    assert(firstStackBans->at(6207).at(98).maxStack == 6);
    assert(!applyManagedItemPrefixStackDenials("test",
        {{6207, {{"73", {1, 2}}}}}));
    assert(managedItemPrefixStackDenyView() == firstStackBans);
    assert(applyManagedItemPrefixStackDenials("test",
        {{6208, {{"74", {1, 2}}}}}));
    assert(managedItemPrefixStackDenyView()->at(6208).contains(74));
    assert(firstStackBans->size() == 1);
    assert(managedItemPrefixAttributeDenyView()->empty());
    assert(!applyManagedItemPrefixAttributeDenials("test",
        {{6207, {{"Unknown", 'D', 10, 25}}}}));
    assert(managedItemPrefixAttributeDenyView()->empty());
    assert(!applyManagedItemPrefixAttributeDenials("test",
        {{6207, {{"71", 'M', 0, 10001}}}}));
    assert(managedItemPrefixAttributeDenyView()->empty());
    assert(!applyManagedItemPrefixAttributeDenials("test",
        {{6207, {{"71", 'X', 0, 8}}}}));
    assert(managedItemPrefixAttributeDenyView()->empty());
    assert(!applyManagedItemPrefixAttributeDenials("test",
        {{6207, {{"71", 'D', 10, 25}, {"71", 'D', 11, 24}}}}));
    assert(managedItemPrefixAttributeDenyView()->empty());
    assert(applyManagedItemPrefixAttributeDenials("test",
        {{6207, {{"71", 'D', 10, 25},
                  {"71", 'M', 2, 8},
                  {"Guarded", 'M', 0, 4}}}}));
    auto firstAttributes = managedItemPrefixAttributeDenyView();
    assert(firstAttributes->at(6207).at(71).size() == 2);
    assert(firstAttributes->at(6207).at(98).front().attribute == 'M');
    assert(!applyManagedItemPrefixAttributeDenials("test",
        {{6207, {{"72", 'D', 1, 2}}}}));
    assert(managedItemPrefixAttributeDenyView() == firstAttributes);
    assert(applyManagedItemPrefixAttributeDenials("test",
        {{6208, {{"72", 'D', 1, 2}}}}));
    assert(managedItemPrefixAttributeDenyView()->at(6208).contains(72));
    assert(firstAttributes->size() == 1);

    reserveManagedPrefixDescriptor(98);
    assert(managedPrefixDescriptorPending(98));
    assert(!managedPrefixDescriptorReady(98));

    ManagedPrefixEntry guarded;
    guarded.type = 98;
    guarded.name = "Guarded";
    guarded.displayName = "Guarded";
    guarded.category = 5;
    guarded.rollWeight = 1.f;
    guarded.accessoryDefense = 4;
    guarded.hasAttributeRollWeight = true;
    guarded.attributeWeightKind = 'M';
    guarded.minWeightAttribute = 0;
    guarded.maxWeightAttribute = 8;
    guarded.weightInsideAttributeRange = 4;
    auto invalidAttribute = guarded;
    invalidAttribute.attributeWeightKind = 'X';
    assert(!applyManagedPrefixDescriptors("test", {invalidAttribute}));
    assert(managedPrefixView() == first);
    invalidAttribute = guarded;
    invalidAttribute.maxWeightAttribute = 10001;
    assert(!applyManagedPrefixDescriptors("test", {invalidAttribute}));
    assert(managedPrefixView() == first);
    invalidAttribute = guarded;
    invalidAttribute.weightInsideAttributeRange = 21;
    assert(!applyManagedPrefixDescriptors("test", {invalidAttribute}));
    assert(managedPrefixView() == first);
    invalidAttribute = guarded;
    invalidAttribute.hasStackRollWeight = true;
    assert(!applyManagedPrefixDescriptors("test", {invalidAttribute}));
    assert(managedPrefixView() == first);

    assert(applyManagedPrefixDescriptors("test", {guarded}));
    auto second = managedPrefixView();
    assert(second && second != first && first->empty());
    assert(second->size() == 1 && second->front().type == 98);
    assert(second->front().hasAttributeRollWeight);
    assert(second->front().attributeWeightKind == 'M');
    assert(second->front().weightInsideAttributeRange == 4);
    assert(!managedPrefixDescriptorPending(98));
    assert(managedPrefixDescriptorReady(98));

    ManagedPrefixEntry mighty;
    mighty.type = 99;
    mighty.name = "Mighty";
    mighty.category = 4;
    mighty.rollWeight = 5.f;
    mighty.multipliers[0] = 1.2f;
    mighty.minItemDamage = 10;
    mighty.maxItemDamage = 25;
    mighty.minItemMana = 0;
    mighty.maxItemMana = 8;
    mighty.minItemStack = 1;
    mighty.maxItemStack = 10;
    mighty.hasStackRollWeight = true;
    mighty.minWeightStack = 3;
    mighty.maxWeightStack = 5;
    mighty.weightInsideStackRange = 6;
    mighty.applyDamageBonus = 3;
    mighty.applyValueBonus = 40;
    mighty.requireEffectiveStats = false;
    mighty.extraTooltip = "Bonus adicional do prefixo";
    mighty.tooltipLines = {"First line", "Second line"};
    mighty.tooltipItemLines = {"Dano {damage}, mana {mana}, pilha {stack}"};
    auto invalidPost = mighty;
    invalidPost.applyDamageBonus = 1001;
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.maxItemDamage = 5;
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.maxItemMana = 10001;
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.minWeightStack = 6;
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.weightInsideStackRange = 21;
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.maxItemStack = 0;
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.maxItemStack = 1000000;
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.tooltipItemLines = {"bad {unknown}"};
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.tooltipItemLines = {"{stack}", "{mana}", "{damage}"};
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.tooltipLines = std::vector<std::string>(5, "overflow");
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.tooltipLines = {"valid", std::string(121, 'A')};
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    invalidPost = mighty;
    invalidPost.extraTooltip = std::string(121, 'A');
    assert(!applyManagedPrefixDescriptors("test", {invalidPost}));
    assert(managedPrefixView() == second);
    assert(applyManagedPrefixDescriptors("test", {mighty}));
    auto third = managedPrefixView();
    assert(third && third->size() == 2 && second->size() == 1);
    assert(third->at(0).name == "Guarded");
    assert(third->at(1).name == "Mighty");
    assert(third->at(1).applyDamageBonus == 3);
    assert(third->at(1).minItemDamage == 10);
    assert(third->at(1).maxItemDamage == 25);
    assert(third->at(1).maxItemMana == 8);
    assert(third->at(1).minItemStack == 1);
    assert(third->at(1).maxItemStack == 10);
    assert(third->at(1).hasStackRollWeight);
    assert(third->at(1).minWeightStack == 3);
    assert(third->at(1).maxWeightStack == 5);
    assert(third->at(1).weightInsideStackRange == 6);
    assert(third->at(1).applyValueBonus == 40);
    assert(!third->at(1).requireEffectiveStats);
    assert(third->at(1).extraTooltip == "Bonus adicional do prefixo");
    assert(third->at(1).tooltipLines.size() == 2);
    assert(third->at(1).tooltipLines[0] == "First line");
    assert(third->at(1).tooltipItemLines.size() == 1);
    assert(third->at(1).tooltipItemLines[0] == "Dano {damage}, mana {mana}, pilha {stack}");
    assert(snapshotManagedPrefixes().size() == 2);
    assert(registeredManagedPrefixCount() == 2);
    assert(managedPrefixView() == third);

    assert(!applyManagedPrefixDescriptors("test", {mighty}));
    assert(managedPrefixView() == third);

    ManagedPrefixEntry invalid = mighty;
    invalid.type = 100;
    invalid.name = "Swift";
    invalid.category = 7;
    assert(!applyManagedPrefixDescriptors("test", {invalid}));
    assert(managedPrefixView() == third);

    ManagedPrefixEntry swift = invalid;
    swift.category = 1;
    swift.rollWeight = 2.f;
    swift.itemWeights[1] = -1;
    assert(!applyManagedPrefixDescriptors("test", {swift}));
    assert(managedPrefixView() == third);
    swift.itemWeights[1] = 21;
    assert(!applyManagedPrefixDescriptors("test", {swift}));
    assert(managedPrefixView() == third);
    swift.itemWeights[1] = 0;
    swift.itemWeights[512] = 7;
    swift.itemCanRoll[1001] = true;
    assert(!applyManagedPrefixDescriptors("test", {swift}));
    assert(managedPrefixView() == third);
    swift.itemCanRoll.erase(1001);
    swift.itemCanRoll[1] = true;
    swift.itemNames["UnknownItem"] = 3;
    assert(!applyManagedPrefixDescriptors("test", {swift}));
    assert(managedPrefixView() == third);
    swift.itemNames.clear();
    swift.itemNames["ExampleBlade"] = 11;
    swift.itemNames["ExampleCharm"] = 0;
    swift.itemNamesCanRoll["ExampleCharm"] = true;
    auto duplicate = swift;
    duplicate.itemWeights[6207] = 9;
    assert(!applyManagedPrefixDescriptors("test", {duplicate}));
    assert(managedPrefixView() == third);

    std::atomic<bool> go{false};
    std::atomic<int> snapshots{0};
    std::thread reader([&] {
        while (!go.load(std::memory_order_acquire)) {
            auto current = managedPrefixView();
            assert(current);
            assert(current->size() == 2 || current->size() == 3);
            assert(current->at(0).type == 98);
            assert(current->at(1).type == 99);
            if (current->size() == 3) assert(current->at(2).type == 100);
            snapshots.fetch_add(1, std::memory_order_relaxed);
        }
    });
    while (snapshots.load(std::memory_order_acquire) == 0)
        std::this_thread::yield();
    assert(applyManagedPrefixDescriptors("test", {swift}));
    go.store(true, std::memory_order_release);
    reader.join();

    auto final = managedPrefixView();
    assert(final->size() == 3 && final->at(2).name == "Swift");
    assert(final->at(2).itemWeights.at(1) == 0);
    assert(final->at(2).itemCanRoll.at(1));
    assert(final->at(2).itemWeights.at(512) == 7);
    assert(final->at(2).itemWeights.at(6207) == 11);
    assert(final->at(2).itemWeights.at(6208) == 0);
    assert(final->at(2).itemCanRoll.at(6208));
    assert(final->at(2).itemNamesCanRoll.empty());
    assert(final->at(2).itemNames.empty());
    assert(third->size() == 2);
    assert(second->size() == 1);
    assert(first->empty());
    assert(registeredManagedPrefixCount() == 3);
    assert(snapshots.load(std::memory_order_relaxed) > 0);

    ManagedPrefixEntry custom;
    custom.type = 101;
    custom.name = "Custom";
    custom.category = 6;
    custom.rollWeight = 1.f;
    custom.itemNames["ExampleBlade"] = 0;
    custom.itemNamesCanRoll["ExampleBlade"] = true;
    custom.preferredNames.insert("ExampleBlade");
    custom.itemNames["ExampleCharm"] = 0;
    auto denied = custom;
    denied.itemNamesCanRoll["ExampleBlade"] = false;
    assert(!applyManagedPrefixDescriptors("test", {denied}));
    assert(managedPrefixView() == final);
    assert(applyManagedPrefixDescriptors("test", {custom}));
    auto withCustom = managedPrefixView();
    assert(withCustom->size() == 4 && withCustom->at(3).category == 6);
    assert(withCustom->at(3).itemNames.empty());
    assert(withCustom->at(3).itemWeights.at(6207) == 0);
    assert(withCustom->at(3).itemCanRoll.at(6207));
    assert(withCustom->at(3).preferredTypes.contains(6207));
    assert(withCustom->at(3).preferredNames.empty());
    assert(withCustom->at(3).itemWeights.at(6208) == 0);
    assert(final->size() == 3);
    assert(registeredManagedPrefixCount() == 4);
    ManagedPrefixEntry alternate;
    alternate.type = 102;
    alternate.name = "Alternate";
    alternate.category = 6;
    alternate.rollWeight = 1.f;
    alternate.itemNames["ExampleBlade"] = 1;
    alternate.preferredNames.insert("ExampleBlade");
    assert(!applyManagedPrefixDescriptors("test", {alternate}));
    assert(managedPrefixView() == withCustom);
    assert(registeredManagedPrefixCount() == 4);
    alternate.itemNames.clear();
    alternate.preferredNames.clear();
    alternate.itemNames["ExampleCharm"] = 1;
    alternate.preferredNames.insert("ExampleCharm");
    assert(applyManagedPrefixDescriptors("test", {alternate}));
    auto separateChoice = managedPrefixView();
    assert(separateChoice->size() == 5);
    assert(separateChoice->at(4).preferredTypes.contains(6208));
    assert(withCustom->size() == 4);
}
