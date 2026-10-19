#pragma once
#include <array>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace bl::csharp {

struct ManagedPrefixEntry {
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
};

bool applyManagedItemPrefixChances(const std::vector<std::pair<int, bool>>& policies);
std::shared_ptr<const std::map<int, bool>> managedItemPrefixChanceView();

bool applyManagedPrefixDescriptors(const std::string& modId,
                                   const std::vector<ManagedPrefixEntry>& prefixes);
void reserveManagedPrefixDescriptor(int type);
bool managedPrefixDescriptorPending(int type);
bool managedPrefixDescriptorReady(int type);
int registeredManagedPrefixCount();
std::vector<ManagedPrefixEntry> snapshotManagedPrefixes();
std::shared_ptr<const std::vector<ManagedPrefixEntry>> managedPrefixView();
}
