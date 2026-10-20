#pragma once
#include <array>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace bl::csharp {

struct ManagedPrefixEntry {
    std::string modId;
    int callbackMask = 0;
    std::string name;
    std::string displayName;
    int type = -1;
    int category = -1;
    float rollWeight = 0.f;
    std::map<int, int> itemWeights;
    std::map<std::string, int> itemNames;
    std::map<int, bool> itemCanRoll;
    std::map<std::string, bool> itemNamesCanRoll;
    std::set<int> preferredTypes;
    std::set<std::string> preferredNames;
    std::array<float, 6> multipliers{1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
    int crit = 0;
    int tagDamage = 0;
    int armorPenetration = 0;
    float valueMultiplier = 1.f;
    int accessoryDefense = 0;
    int applyDamageBonus = 0;
    int applyValueBonus = 0;
    int minItemDamage = 0;
    int maxItemDamage = 100000;
    int minItemMana = 0;
    int maxItemMana = 10000;
    int minItemStack = 0;
    int maxItemStack = 999999;
    bool hasStackRollWeight = false;
    int minWeightStack = 0;
    int maxWeightStack = 0;
    int weightInsideStackRange = 0;
    bool hasAttributeRollWeight = false;
    char attributeWeightKind = 'D';
    int minWeightAttribute = 0;
    int maxWeightAttribute = 0;
    int weightInsideAttributeRange = 0;
    bool requireEffectiveStats = true;
    std::string extraTooltip;
    std::vector<std::string> tooltipLines;
    std::vector<std::string> tooltipItemLines;
};

struct ManagedItemPrefixCallbackBinding {
    std::string modId;
    std::string owner;
};

bool applyManagedItemPrefixCallbackBindings(
    const std::string& modId, const std::vector<std::pair<int, std::string>>& items);
std::shared_ptr<const std::map<int, ManagedItemPrefixCallbackBinding>>
managedItemPrefixCallbackView();
bool bindManagedItemPrefixCallbackAssembly(const std::string& modId,
                                           const std::string& assembly);
std::shared_ptr<const std::map<std::string, std::string>>
managedItemPrefixCallbackAssemblies();

struct ManagedItemPrefixApply {
    int damageBonus = 0;
    int valueBonus = 0;
};

bool applyManagedItemPrefixEffects(const std::vector<std::pair<int, ManagedItemPrefixApply>>& effects);
std::shared_ptr<const std::map<int, ManagedItemPrefixApply>> managedItemPrefixApplyView();

bool applyManagedItemPrefixDenials(
    const std::string& modId,
    const std::vector<std::pair<int, std::vector<std::string>>>& policies);
std::shared_ptr<const std::map<int, std::set<int>>> managedItemPrefixDenyView();

struct ManagedPrefixStackDenyRange {
    int minStack = 0;
    int maxStack = 0;
};

struct ManagedPrefixStackDenyToken {
    std::string prefix;
    ManagedPrefixStackDenyRange range;
};

using ManagedPrefixStackDenyTable =
    std::map<int, std::map<int, ManagedPrefixStackDenyRange>>;

bool applyManagedItemPrefixStackDenials(
    const std::string& modId,
    const std::vector<std::pair<int, std::vector<ManagedPrefixStackDenyToken>>>& policies);
std::shared_ptr<const ManagedPrefixStackDenyTable> managedItemPrefixStackDenyView();

struct ManagedPrefixAttributeDenyToken {
    std::string prefix;
    char attribute = 'D';
    int minimum = 0;
    int maximum = 0;
};

struct ManagedPrefixAttributeDenyRange {
    char attribute = 'D';
    int minimum = 0;
    int maximum = 0;
};

using ManagedPrefixAttributeDenyTable =
    std::map<int, std::map<int, std::vector<ManagedPrefixAttributeDenyRange>>>;

bool applyManagedItemPrefixAttributeDenials(
    const std::string& modId,
    const std::vector<std::pair<int, std::vector<ManagedPrefixAttributeDenyToken>>>& policies);
std::shared_ptr<const ManagedPrefixAttributeDenyTable> managedItemPrefixAttributeDenyView();

bool applyManagedItemPrefixChances(const std::vector<std::pair<int, bool>>& policies);
std::shared_ptr<const std::map<int, bool>> managedItemPrefixChanceView();

struct ManagedPrefixStackChance {
    int minStack = 0;
    int maxStack = 0;
    bool allowed = false;
};

bool applyManagedItemPrefixStackChances(
    const std::vector<std::pair<int, ManagedPrefixStackChance>>& policies);
std::shared_ptr<const std::map<int, ManagedPrefixStackChance>> managedItemPrefixStackChanceView();

struct ManagedPrefixChoiceToken {
    int itemType = -1;
    std::string prefix;
    int minStack = 0;
    int maxStack = 0;
};

struct ManagedPrefixChoice {
    int prefix = 0;
    int minStack = 0;
    int maxStack = 0;
};

bool applyManagedItemPrefixChoices(
    const std::string& modId, const std::vector<ManagedPrefixChoiceToken>& policies);
std::shared_ptr<const std::map<int, ManagedPrefixChoice>> managedItemPrefixChoiceView();

bool applyManagedPrefixDescriptors(const std::string& modId,
                                   const std::vector<ManagedPrefixEntry>& prefixes);
void reserveManagedPrefixDescriptor(int type);
bool managedPrefixDescriptorPending(int type);
bool managedPrefixDescriptorReady(int type);
int registeredManagedPrefixCount();
std::vector<ManagedPrefixEntry> snapshotManagedPrefixes();
std::shared_ptr<const std::vector<ManagedPrefixEntry>> managedPrefixView();

bool bindManagedPrefixCallbackAssembly(const std::string& modId,const std::string& assembly);
std::shared_ptr<const std::map<std::string,std::string>> managedPrefixCallbackAssemblies();
}
