#include "experiments/csharp/ManagedPrefixBridge.h"
#include "experiments/csharp/ManagedPrefixGameplay.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/ManagedPrefixTooltipTemplate.h"
#include "content/items/ModPrefixes.h"
#include "content/items/ModItems.h"
#include "core/Log.h"

#include <cmath>
#include <algorithm>
#include <charconv>
#include <map>
#include <mutex>
#include <memory>
#include <atomic>
#include <set>

namespace bl::csharp {
namespace {

std::mutex g_prefixMutex;
std::map<int, ManagedPrefixEntry> g_descriptors;
std::set<int> g_reservedManagedPrefixTypes;
std::shared_ptr<const std::vector<ManagedPrefixEntry>> g_prefixView =
    std::make_shared<const std::vector<ManagedPrefixEntry>>();
std::shared_ptr<const std::map<std::string,std::string>> g_callbackAssemblies =
    std::make_shared<const std::map<std::string,std::string>>();
std::shared_ptr<const std::map<std::string,std::string>> g_itemCallbackAssemblies =
    std::make_shared<const std::map<std::string,std::string>>();
std::shared_ptr<const std::vector<ManagedGlobalPrefixCallbackBinding>> g_globalCallbackView =
    std::make_shared<const std::vector<ManagedGlobalPrefixCallbackBinding>>();
std::shared_ptr<const std::map<int, ManagedItemPrefixCallbackBinding>> g_itemCallbackView =
    std::make_shared<const std::map<int, ManagedItemPrefixCallbackBinding>>();
std::shared_ptr<const std::map<int, bool>> g_itemChanceView =
    std::make_shared<const std::map<int, bool>>();
std::shared_ptr<const std::map<int, ManagedPrefixStackChance>> g_itemStackChanceView =
    std::make_shared<const std::map<int, ManagedPrefixStackChance>>();
std::shared_ptr<const std::map<int, ManagedPrefixChoice>> g_itemChoiceView =
    std::make_shared<const std::map<int, ManagedPrefixChoice>>();
std::shared_ptr<const std::map<int, ManagedItemPrefixApply>> g_itemApplyView =
    std::make_shared<const std::map<int, ManagedItemPrefixApply>>();
std::shared_ptr<const std::map<int, std::set<int>>> g_itemDenyView =
    std::make_shared<const std::map<int, std::set<int>>>();
std::shared_ptr<const ManagedPrefixStackDenyTable> g_itemStackDenyView =
    std::make_shared<const ManagedPrefixStackDenyTable>();
std::shared_ptr<const ManagedPrefixAttributeDenyTable> g_itemAttributeDenyView =
    std::make_shared<const ManagedPrefixAttributeDenyTable>();

bool valid(const ManagedPrefixEntry& p) {
    if (p.callbackMask<0 || p.callbackMask>63 ||
        (p.callbackMask && (p.modId.empty() || p.modId.size()>128)) ||
        p.type < 0 || p.type > 255 || p.category < 0 || p.category > 6 ||
        p.displayName.size() > 80 ||
        std::any_of(p.displayName.begin(), p.displayName.end(),
                    [](unsigned char ch) { return ch < 32; }) ||
        !std::isfinite(p.rollWeight) || p.rollWeight <= 0.f || p.rollWeight > 20.f || std::floor(p.rollWeight) != p.rollWeight ||
        !std::isfinite(p.valueMultiplier) || p.valueMultiplier < 0.05f ||
        p.valueMultiplier > 100.f || p.crit < -100 || p.crit > 100 ||
        p.tagDamage < -100 || p.tagDamage > 100 ||
        p.armorPenetration < -100 || p.armorPenetration > 100 ||
        p.accessoryDefense < -100 || p.accessoryDefense > 100 ||
        (p.category != 5 && p.accessoryDefense != 0) ||
        p.minItemDamage < 0 || p.maxItemDamage < p.minItemDamage ||
        p.maxItemDamage > 100000 || p.minItemMana < 0 ||
        p.maxItemMana < p.minItemMana || p.maxItemMana > 10000 ||
        p.minItemStack < 0 || p.maxItemStack < p.minItemStack ||
        p.maxItemStack > 999999 ||
        (p.hasStackRollWeight &&
            (p.minWeightStack < 0 || p.maxWeightStack < p.minWeightStack ||
             p.maxWeightStack > 999999 || p.weightInsideStackRange < 0 ||
             p.weightInsideStackRange > 20)) ||
        (p.hasAttributeRollWeight &&
            (p.hasStackRollWeight ||
             (p.attributeWeightKind != 'D' && p.attributeWeightKind != 'M') ||
             p.minWeightAttribute < 0 ||
             p.maxWeightAttribute < p.minWeightAttribute ||
             p.maxWeightAttribute > (p.attributeWeightKind == 'D' ? 100000 : 10000) ||
             p.weightInsideAttributeRange < 0 ||
             p.weightInsideAttributeRange > 20)) ||
        p.applyDamageBonus < 0 || p.applyDamageBonus > 1000 ||
        p.applyValueBonus < 0 || p.applyValueBonus > 100000 ||
        p.extraTooltip.size() > 120 ||
        std::any_of(p.extraTooltip.begin(), p.extraTooltip.end(),
                    [](unsigned char ch) { return ch < 32 || ch == 127; }) ||
        p.tooltipLines.size() + p.tooltipItemLines.size() > 4) return false;
    for (const auto& line : p.tooltipLines)
        if (line.empty() || line.size() > 120 ||
            std::any_of(line.begin(), line.end(),
                [](unsigned char ch) { return ch < 32 || ch == 127; }))
            return false;
    for (const auto& line : p.tooltipItemLines)
        if (!validPrefixItemTooltip(line)) return false;
    for (float value : p.multipliers)
        if (!std::isfinite(value) || value < 0.1f || value > 10.f) return false;
    if (p.itemWeights.size() + p.itemNames.size() > 32) return false;
    for (const auto& [itemType, weight] : p.itemWeights)
        if (itemType < 1 || itemType > 32767 || weight < 0 || weight > 20) return false;
    for (const auto& [type, allowed] : p.itemCanRoll)
        if (!p.itemWeights.contains(type)) return false;
    for (const auto& [name, allowed] : p.itemNamesCanRoll)
        if (!p.itemNames.contains(name)) return false;
    for (int type : p.preferredTypes) {
        const auto weight = p.itemWeights.find(type);
        const auto decision = p.itemCanRoll.find(type);
        if (weight == p.itemWeights.end() ||
            !(decision == p.itemCanRoll.end() ? weight->second > 0 : decision->second))
            return false;
    }
    for (const auto& name : p.preferredNames) {
        const auto weight = p.itemNames.find(name);
        const auto decision = p.itemNamesCanRoll.find(name);
        if (weight == p.itemNames.end() ||
            !(decision == p.itemNamesCanRoll.end() ? weight->second > 0 : decision->second))
            return false;
    }
    if (p.category == 6) {
        bool anyAllowed = false;
        for (const auto& [type, weight] : p.itemWeights) {
            const auto override = p.itemCanRoll.find(type);
            anyAllowed |= override == p.itemCanRoll.end() ? weight > 0 : override->second;
        }
        for (const auto& [name, weight] : p.itemNames) {
            const auto override = p.itemNamesCanRoll.find(name);
            anyAllowed |= override == p.itemNamesCanRoll.end() ? weight > 0 : override->second;
        }
        if (!anyAllowed) return false;
    }
    for (const auto& [name, weight] : p.itemNames) {
        if (name.empty() || name.size() > 64 || weight < 0 || weight > 20 ||
            !((name[0] >= 'A' && name[0] <= 'Z') ||
              (name[0] >= 'a' && name[0] <= 'z') || name[0] == '_')) return false;
        for (unsigned char ch : name)
            if (!((ch >= 'A' && ch <= 'Z') ||
                  (ch >= 'a' && ch <= 'z') ||
                  (ch >= '0' && ch <= '9') || ch == '_')) return false;
    }
    return true;
}

}

bool applyManagedItemPrefixCallbackBindings(
    const std::string& modId, const std::vector<std::pair<int, std::string>>& items,
    const std::map<int,int>& masks) {
    if (modId.empty() || modId.size() > 128 || items.size() > 4096 ||
        masks.size()>items.size()) return false;
    std::map<int, ManagedItemPrefixCallbackBinding> staged;
    for (const auto& [type, owner] : items) {
        const auto mask=masks.find(type);
        const int bits=mask==masks.end()?1:mask->second;
        if (bits<1 || bits>15 || type < 1 || type > 32767 ||
            owner.empty() || owner.size() > 64 ||
            !((owner[0] >= 'A' && owner[0] <= 'Z') ||
              (owner[0] >= 'a' && owner[0] <= 'z') || owner[0] == '_') ||
            !std::all_of(owner.begin(), owner.end(), [](unsigned char ch) {
                return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                       (ch >= '0' && ch <= '9') || ch == '_';
            }) || bl::runtime::modItemTypeByKey(modId + "/" + owner) != type ||
            !staged.emplace(type, ManagedItemPrefixCallbackBinding{modId, owner, bits}).second)
            return false;
    }
    if (staged.empty()) return true;
    for (const auto& [type,mask] : masks)
        if (!staged.contains(type) || mask<1 || mask>15) return false;
    if (!installManagedPrefixGameplayHooks() || !installManagedPrefixChanceHook())
        return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    const auto previous = std::atomic_load_explicit(&g_itemCallbackView,
                                                    std::memory_order_acquire);
    for (const auto& entry : staged)
        if (previous->contains(entry.first)) return false;
    auto updated = std::make_shared<std::map<int, ManagedItemPrefixCallbackBinding>>(*previous);
    updated->insert(staged.begin(), staged.end());
    std::shared_ptr<const std::map<int, ManagedItemPrefixCallbackBinding>> published = updated;
    std::atomic_store_explicit(&g_itemCallbackView, std::move(published),
                               std::memory_order_release);
    return true;
}

std::shared_ptr<const std::map<int, ManagedItemPrefixCallbackBinding>>
managedItemPrefixCallbackView() {
    return std::atomic_load_explicit(&g_itemCallbackView, std::memory_order_acquire);
}

bool bindManagedItemPrefixCallbackAssembly(
    const std::string& modId,const std::string& assembly) {
    if (modId.empty() || modId.size()>128 || assembly.empty() || assembly.size()>4096)
        return false;
    const auto owners=managedItemPrefixCallbackView();
    for (const auto& [type, owner]:*owners)
        if (owner.modId==modId && !queueManagedItemPrefixWarmup(modId,assembly,owner.owner))
            return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    const auto previous=std::atomic_load_explicit(&g_itemCallbackAssemblies,
                                                   std::memory_order_acquire);
    if (previous->contains(modId)) return previous->at(modId)==assembly;
    auto updated=std::make_shared<std::map<std::string,std::string>>(*previous);
    updated->emplace(modId,assembly);
    std::shared_ptr<const std::map<std::string,std::string>> published=updated;
    std::atomic_store_explicit(&g_itemCallbackAssemblies,std::move(published),
                               std::memory_order_release);
    return true;
}

std::shared_ptr<const std::map<std::string,std::string>>
managedItemPrefixCallbackAssemblies() {
    return std::atomic_load_explicit(&g_itemCallbackAssemblies,std::memory_order_acquire);
}

bool applyManagedGlobalPrefixCallbackBindings(
    const std::string& modId,
    const std::vector<std::pair<std::string,int>>& policies) {
    if (modId.empty() || modId.size()>128 || policies.size()>128) return false;
    std::set<std::string> seen;
    for (const auto& [owner, mask]:policies) {
        if (owner.empty() || owner.size()>64 || mask<1 || mask>15 ||
            !((owner[0]>='A'&&owner[0]<='Z') ||
              (owner[0]>='a'&&owner[0]<='z') || owner[0]=='_') ||
            !std::all_of(owner.begin(),owner.end(),[](unsigned char c) {
                return (c>='A'&&c<='Z') || (c>='a'&&c<='z') ||
                       (c>='0'&&c<='9') || c=='_';
            }) || !seen.insert(owner).second) return false;
    }
    if(policies.empty())return true;
    if(!installManagedPrefixGameplayHooks() || !installManagedPrefixChanceHook())
        return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    const auto previous=std::atomic_load_explicit(&g_globalCallbackView,
                                                   std::memory_order_acquire);
    if(previous->size()+policies.size()>128)return false;
    for(const auto& [owner,mask]:policies)
        for(const auto& prior:*previous)
            if(prior.modId==modId && prior.owner==owner)return false;
    auto updated=std::make_shared<std::vector<ManagedGlobalPrefixCallbackBinding>>(*previous);
    for(const auto& [owner,mask]:policies)
        updated->push_back({modId,owner,mask});
    std::shared_ptr<const std::vector<ManagedGlobalPrefixCallbackBinding>> published=updated;
    std::atomic_store_explicit(&g_globalCallbackView,std::move(published),
                               std::memory_order_release);
    return true;
}

std::shared_ptr<const std::vector<ManagedGlobalPrefixCallbackBinding>>
managedGlobalPrefixCallbackView() {
    return std::atomic_load_explicit(&g_globalCallbackView,std::memory_order_acquire);
}

bool bindManagedGlobalPrefixCallbackAssembly(const std::string& modId,
                                              const std::string& assembly) {
    if(modId.empty() || modId.size()>128 || assembly.empty() || assembly.size()>4096)
        return false;
    const auto globals=managedGlobalPrefixCallbackView();
    for(const auto& owner:*globals)
        if(owner.modId==modId &&
           !queueManagedItemPrefixWarmup(modId,assembly,owner.owner))
            return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    const auto previous=std::atomic_load_explicit(&g_itemCallbackAssemblies,
                                                   std::memory_order_acquire);
    if(previous->contains(modId))return previous->at(modId)==assembly;
    auto updated=std::make_shared<std::map<std::string,std::string>>(*previous);
    updated->emplace(modId,assembly);
    std::shared_ptr<const std::map<std::string,std::string>> published=updated;
    std::atomic_store_explicit(&g_itemCallbackAssemblies,std::move(published),
                               std::memory_order_release);
    return true;
}

bool applyManagedItemPrefixDenials(
    const std::string& modId,
    const std::vector<std::pair<int, std::vector<std::string>>>& policies) {
    if (policies.size() > 4096) return false;
    std::map<int, std::set<int>> staged;
    for (const auto& [itemType, entries] : policies) {
        if (itemType < 1 || itemType > 32767 ||
            entries.empty() || entries.size() > 32) return false;
        std::set<int> ids;
        for (const auto& token : entries) {
            int prefix = -1;
            const bool numeric = !token.empty() &&
                std::all_of(token.begin(), token.end(),
                    [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
            if (numeric) {
                const char* begin = token.data();
                const char* end = begin + token.size();
                const auto result = std::from_chars(begin, end, prefix);
                if (result.ec != std::errc{} || result.ptr != end) return false;
            } else {
                if (token.empty() || token.size() > 64 ||
                    !((token[0] >= 'A' && token[0] <= 'Z') ||
                      (token[0] >= 'a' && token[0] <= 'z') || token[0] == '_'))
                    return false;
                for (unsigned char ch : token)
                    if (!((ch >= 'A' && ch <= 'Z') ||
                          (ch >= 'a' && ch <= 'z') ||
                          (ch >= '0' && ch <= '9') || ch == '_'))
                        return false;
                prefix = bl::runtime::modPrefixByKey(modId + "/" + token);
            }
            if (prefix < 1 || prefix > 255 || !ids.insert(prefix).second)
                return false;
        }
        if (!staged.emplace(itemType, std::move(ids)).second)
            return false;
    }
    if (staged.empty()) return true;
    if (!installManagedPrefixGameplayHooks() || !installManagedPrefixChanceHook())
        return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    auto previous = std::atomic_load_explicit(&g_itemDenyView, std::memory_order_acquire);
    for (const auto& [type, denied] : staged)
        if (previous->contains(type)) return false;
    auto updated = std::make_shared<std::map<int, std::set<int>>>(*previous);
    updated->insert(staged.begin(), staged.end());
    std::shared_ptr<const std::map<int, std::set<int>>> published = updated;
    std::atomic_store_explicit(&g_itemDenyView, std::move(published),
                               std::memory_order_release);
    return true;
}

std::shared_ptr<const std::map<int, std::set<int>>> managedItemPrefixDenyView() {
    return std::atomic_load_explicit(&g_itemDenyView, std::memory_order_acquire);
}

bool applyManagedItemPrefixStackDenials(
    const std::string& modId,
    const std::vector<std::pair<int, std::vector<ManagedPrefixStackDenyToken>>>& policies) {
    if (policies.size() > 4096) return false;
    ManagedPrefixStackDenyTable staged;
    for (const auto& [itemType, entries] : policies) {
        if (itemType < 1 || itemType > 32767 ||
            entries.empty() || entries.size() > 32) return false;
        std::map<int, ManagedPrefixStackDenyRange> rules;
        for (const auto& entry : entries) {
            const auto& token = entry.prefix;
            if (entry.range.minStack < 0 ||
                entry.range.maxStack < entry.range.minStack ||
                entry.range.maxStack > 999999) return false;
            int prefix = -1;
            const bool numeric = !token.empty() &&
                std::all_of(token.begin(), token.end(),
                    [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
            if (numeric) {
                const char* begin = token.data();
                const char* end = begin + token.size();
                const auto result = std::from_chars(begin, end, prefix);
                if (result.ec != std::errc{} || result.ptr != end) return false;
            } else {
                if (token.empty() || token.size() > 64 ||
                    !((token[0] >= 'A' && token[0] <= 'Z') ||
                      (token[0] >= 'a' && token[0] <= 'z') || token[0] == '_'))
                    return false;
                for (unsigned char ch : token)
                    if (!((ch >= 'A' && ch <= 'Z') ||
                          (ch >= 'a' && ch <= 'z') ||
                          (ch >= '0' && ch <= '9') || ch == '_'))
                        return false;
                prefix = bl::runtime::modPrefixByKey(modId + "/" + token);
            }
            if (prefix < 1 || prefix > 255 ||
                !rules.emplace(prefix, entry.range).second) return false;
        }
        if (!staged.emplace(itemType, std::move(rules)).second) return false;
    }
    if (staged.empty()) return true;
    if (!installManagedPrefixGameplayHooks() || !installManagedPrefixChanceHook())
        return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    auto previous = std::atomic_load_explicit(&g_itemStackDenyView, std::memory_order_acquire);
    for (const auto& [type, rules] : staged)
        if (previous->contains(type)) return false;
    auto updated = std::make_shared<ManagedPrefixStackDenyTable>(*previous);
    updated->insert(staged.begin(), staged.end());
    std::shared_ptr<const ManagedPrefixStackDenyTable> published = updated;
    std::atomic_store_explicit(&g_itemStackDenyView, std::move(published),
                               std::memory_order_release);
    return true;
}

std::shared_ptr<const ManagedPrefixStackDenyTable> managedItemPrefixStackDenyView() {
    return std::atomic_load_explicit(&g_itemStackDenyView, std::memory_order_acquire);
}

bool applyManagedItemPrefixAttributeDenials(
    const std::string& modId,
    const std::vector<std::pair<int, std::vector<ManagedPrefixAttributeDenyToken>>>& policies) {
    if (policies.size() > 4096) return false;
    ManagedPrefixAttributeDenyTable staged;
    for (const auto& [itemType, entries] : policies) {
        if (itemType < 1 || itemType > 32767 ||
            entries.empty() || entries.size() > 32) return false;
        std::map<int, std::vector<ManagedPrefixAttributeDenyRange>> rules;
        std::set<std::pair<int, char>> seen;
        for (const auto& entry : entries) {
            const auto& token = entry.prefix;
            const int limit = entry.attribute == 'D' ? 100000 :
                              entry.attribute == 'M' ? 10000 : -1;
            if (limit < 0 || entry.minimum < 0 ||
                entry.maximum < entry.minimum || entry.maximum > limit)
                return false;
            int prefix = -1;
            const bool numeric = !token.empty() &&
                std::all_of(token.begin(), token.end(),
                    [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
            if (numeric) {
                const char* begin = token.data();
                const char* end = begin + token.size();
                const auto result = std::from_chars(begin, end, prefix);
                if (result.ec != std::errc{} || result.ptr != end) return false;
            } else {
                if (token.empty() || token.size() > 64 ||
                    !((token[0] >= 'A' && token[0] <= 'Z') ||
                      (token[0] >= 'a' && token[0] <= 'z') || token[0] == '_'))
                    return false;
                for (unsigned char ch : token)
                    if (!((ch >= 'A' && ch <= 'Z') ||
                          (ch >= 'a' && ch <= 'z') ||
                          (ch >= '0' && ch <= '9') || ch == '_'))
                        return false;
                prefix = bl::runtime::modPrefixByKey(modId + "/" + token);
            }
            if (prefix < 1 || prefix > 255 ||
                !seen.emplace(prefix, entry.attribute).second) return false;
            rules[prefix].push_back({entry.attribute, entry.minimum, entry.maximum});
        }
        if (!staged.emplace(itemType, std::move(rules)).second) return false;
    }
    if (staged.empty()) return true;
    if (!installManagedPrefixGameplayHooks() || !installManagedPrefixChanceHook())
        return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    auto previous = std::atomic_load_explicit(&g_itemAttributeDenyView,
                                               std::memory_order_acquire);
    for (const auto& [type, rules] : staged)
        if (previous->contains(type)) return false;
    auto updated = std::make_shared<ManagedPrefixAttributeDenyTable>(*previous);
    updated->insert(staged.begin(), staged.end());
    std::shared_ptr<const ManagedPrefixAttributeDenyTable> published = updated;
    std::atomic_store_explicit(&g_itemAttributeDenyView, std::move(published),
                               std::memory_order_release);
    return true;
}

std::shared_ptr<const ManagedPrefixAttributeDenyTable> managedItemPrefixAttributeDenyView() {
    return std::atomic_load_explicit(&g_itemAttributeDenyView, std::memory_order_acquire);
}

bool applyManagedItemPrefixEffects(
    const std::vector<std::pair<int, ManagedItemPrefixApply>>& effects) {
    if (effects.size() > 4096) return false;
    std::map<int, ManagedItemPrefixApply> staged;
    for (const auto& [type, effect] : effects)
        if (type < 1 || type > 32767 ||
            effect.damageBonus < 0 || effect.damageBonus > 1000 ||
            effect.valueBonus < 0 || effect.valueBonus > 100000 ||
            (effect.damageBonus == 0 && effect.valueBonus == 0) ||
            !staged.emplace(type, effect).second) return false;
    if (staged.empty()) return true;
    if (!installManagedPrefixChanceHook()) return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    auto previous = std::atomic_load_explicit(&g_itemApplyView, std::memory_order_acquire);
    for (const auto& [type, effect] : staged)
        if (previous->contains(type)) return false;
    auto updated = std::make_shared<std::map<int, ManagedItemPrefixApply>>(*previous);
    updated->insert(staged.begin(), staged.end());
    std::shared_ptr<const std::map<int, ManagedItemPrefixApply>> published = updated;
    std::atomic_store_explicit(&g_itemApplyView, std::move(published), std::memory_order_release);
    return true;
}

std::shared_ptr<const std::map<int, ManagedItemPrefixApply>> managedItemPrefixApplyView() {
    return std::atomic_load_explicit(&g_itemApplyView, std::memory_order_acquire);
}

bool applyManagedItemPrefixChances(const std::vector<std::pair<int, bool>>& policies) {
    if (policies.size() > 4096) return false;
    std::map<int, bool> staged;
    for (const auto& [type, allowed] : policies)
        if (type < 1 || type > 32767 || !staged.emplace(type, allowed).second)
            return false;
    if (staged.empty()) return true;
    if (!installManagedPrefixChanceHook()) return false;
    {
        std::lock_guard<std::mutex> guard(g_prefixMutex);
        auto previous = std::atomic_load_explicit(&g_itemChanceView, std::memory_order_acquire);
        for (const auto& [type, allowed] : staged)
            if (previous->contains(type)) return false;
        auto updated = std::make_shared<std::map<int, bool>>(*previous);
        updated->insert(staged.begin(), staged.end());
        std::shared_ptr<const std::map<int, bool>> published = updated;
        std::atomic_store_explicit(&g_itemChanceView, std::move(published), std::memory_order_release);
    }
    return true;
}

std::shared_ptr<const std::map<int, bool>> managedItemPrefixChanceView() {
    return std::atomic_load_explicit(&g_itemChanceView, std::memory_order_acquire);
}

bool applyManagedItemPrefixStackChances(
    const std::vector<std::pair<int, ManagedPrefixStackChance>>& policies) {
    if (policies.size() > 4096) return false;
    std::map<int, ManagedPrefixStackChance> staged;
    for (const auto& [type, rule] : policies)
        if (type < 1 || type > 32767 ||
            rule.minStack < 0 || rule.maxStack < rule.minStack ||
            rule.maxStack > 999999 || !staged.emplace(type, rule).second)
            return false;
    if (staged.empty()) return true;
    if (!installManagedPrefixGameplayHooks() || !installManagedPrefixChanceHook())
        return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    auto previous = std::atomic_load_explicit(&g_itemStackChanceView,
                                               std::memory_order_acquire);
    for (const auto& [type, rule] : staged)
        if (previous->contains(type)) return false;
    auto updated = std::make_shared<std::map<int, ManagedPrefixStackChance>>(*previous);
    updated->insert(staged.begin(), staged.end());
    std::shared_ptr<const std::map<int, ManagedPrefixStackChance>> published = updated;
    std::atomic_store_explicit(&g_itemStackChanceView, std::move(published),
                               std::memory_order_release);
    return true;
}

std::shared_ptr<const std::map<int, ManagedPrefixStackChance>> managedItemPrefixStackChanceView() {
    return std::atomic_load_explicit(&g_itemStackChanceView, std::memory_order_acquire);
}

bool applyManagedItemPrefixChoices(
    const std::string& modId, const std::vector<ManagedPrefixChoiceToken>& policies) {
    if (policies.size() > 4096) return false;
    std::map<int, ManagedPrefixChoice> staged;
    for (const auto& policy : policies) {
        if (policy.itemType < 1 || policy.itemType > 32767 ||
            policy.minStack < 0 || policy.maxStack < policy.minStack ||
            policy.maxStack > 999999) return false;
        const auto& token = policy.prefix;
        int prefix = -1;
        const bool numeric = !token.empty() &&
            std::all_of(token.begin(), token.end(),
                [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
        if (numeric) {
            const char* begin = token.data();
            const auto result = std::from_chars(begin, begin + token.size(), prefix);
            if (result.ec != std::errc{} || result.ptr != begin + token.size())
                return false;
        } else {
            if (token.empty() || token.size() > 64 ||
                !((token[0] >= 'A' && token[0] <= 'Z') ||
                  (token[0] >= 'a' && token[0] <= 'z') || token[0] == '_'))
                return false;
            for (unsigned char ch : token)
                if (!((ch >= 'A' && ch <= 'Z') ||
                      (ch >= 'a' && ch <= 'z') ||
                      (ch >= '0' && ch <= '9') || ch == '_'))
                    return false;
            prefix = bl::runtime::modPrefixByKey(modId + "/" + token);
        }
        if (prefix < 1 || prefix > 255 ||
            !staged.emplace(policy.itemType,
                ManagedPrefixChoice{prefix, policy.minStack, policy.maxStack}).second)
            return false;
    }
    if (staged.empty()) return true;
    if (!installManagedPrefixGameplayHooks()) return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    auto previous = std::atomic_load_explicit(&g_itemChoiceView, std::memory_order_acquire);
    for (const auto& [type, choice] : staged)
        if (previous->contains(type)) return false;
    auto updated = std::make_shared<std::map<int, ManagedPrefixChoice>>(*previous);
    updated->insert(staged.begin(), staged.end());
    std::shared_ptr<const std::map<int, ManagedPrefixChoice>> published = updated;
    std::atomic_store_explicit(&g_itemChoiceView, std::move(published),
                               std::memory_order_release);
    return true;
}

std::shared_ptr<const std::map<int, ManagedPrefixChoice>> managedItemPrefixChoiceView() {
    return std::atomic_load_explicit(&g_itemChoiceView, std::memory_order_acquire);
}

void reserveManagedPrefixDescriptor(int type) {
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    if (type > 0 && type <= 255) g_reservedManagedPrefixTypes.insert(type);
}

bool managedPrefixDescriptorPending(int type) {
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    return g_reservedManagedPrefixTypes.contains(type) && !g_descriptors.contains(type);
}

bool managedPrefixDescriptorReady(int type) {
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    return g_descriptors.contains(type);
}

bool applyManagedPrefixDescriptors(
    const std::string& modId, const std::vector<ManagedPrefixEntry>& prefixes) {
    if (prefixes.size() > 16) return false;
    std::map<int, ManagedPrefixEntry> staged;
    for (const auto& entry : prefixes) {
        if (!valid(entry) ||
            bl::runtime::modPrefixByKey(modId + "/" + entry.name) != entry.type)
            return false;
        ManagedPrefixEntry resolved = entry;
        for (const auto& [name, weight] : entry.itemNames) {
            const int type = bl::runtime::modItemTypeByKey(modId + "/" + name);
            if (type < 1 || type > 32767 ||
                !resolved.itemWeights.emplace(type, weight).second) return false;
            const auto decision = entry.itemNamesCanRoll.find(name);
            if (decision != entry.itemNamesCanRoll.end() &&
                !resolved.itemCanRoll.emplace(type, decision->second).second) return false;
            if (entry.preferredNames.contains(name) && !resolved.preferredTypes.insert(type).second)
                return false;
        }
        resolved.itemNames.clear();
        resolved.itemNamesCanRoll.clear();
        resolved.preferredNames.clear();
        if (!valid(resolved) || !staged.emplace(entry.type, std::move(resolved)).second)
            return false;
    }
    if (!staged.empty() && !installManagedPrefixGameplayHooks()) return false;
    {
        std::lock_guard<std::mutex> guard(g_prefixMutex);
        for (const auto& entry : staged)
            if (g_descriptors.contains(entry.first)) return false;
        std::set<int> chosenItems;
        for (const auto& entry : g_descriptors)
            for (int item : entry.second.preferredTypes)
                chosenItems.insert(item);
        for (const auto& entry : staged)
            for (int item : entry.second.preferredTypes)
                if (!chosenItems.insert(item).second) return false;
        auto next = std::make_shared<std::vector<ManagedPrefixEntry>>();
        next->reserve(g_descriptors.size() + staged.size());
        for (const auto& entry : g_descriptors) next->push_back(entry.second);
        for (const auto& entry : staged) next->push_back(entry.second);
        std::sort(next->begin(), next->end(),
                  [](const auto& a, const auto& b) { return a.type < b.type; });
        std::shared_ptr<const std::vector<ManagedPrefixEntry>> published = next;
        g_descriptors.insert(staged.begin(), staged.end());
        std::atomic_store_explicit(&g_prefixView, std::move(published), std::memory_order_release);
    }
    BL_INFO("csharp prefix: %zu descritores gerenciados registrados para %s; hooks de gameplay instalados",
            staged.size(), modId.c_str());
    return true;
}

bool bindManagedPrefixCallbackAssembly(
    const std::string& modId,const std::string& assembly) {
    if (modId.empty() || modId.size()>128 || assembly.empty() || assembly.size()>4096)
        return false;
    const auto prefixes=managedPrefixView();
    for (const auto& prefix:*prefixes)
        if (prefix.modId==modId && prefix.callbackMask!=0 &&
            !queueManagedPrefixWarmup(modId,assembly,prefix.name)) return false;
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    const auto old=std::atomic_load_explicit(&g_callbackAssemblies,std::memory_order_acquire);
    if (old->contains(modId)) return old->at(modId)==assembly;
    auto updated=std::make_shared<std::map<std::string,std::string>>(*old);
    updated->emplace(modId,assembly);
    std::shared_ptr<const std::map<std::string,std::string>> published=updated;
    std::atomic_store_explicit(&g_callbackAssemblies,std::move(published),
                               std::memory_order_release);
    return true;
}

std::shared_ptr<const std::map<std::string,std::string>> managedPrefixCallbackAssemblies() {
    return std::atomic_load_explicit(&g_callbackAssemblies,std::memory_order_acquire);
}

std::shared_ptr<const std::vector<ManagedPrefixEntry>> managedPrefixView() {
    return std::atomic_load_explicit(&g_prefixView, std::memory_order_acquire);
}

std::vector<ManagedPrefixEntry> snapshotManagedPrefixes() {
    return *managedPrefixView();
}

int registeredManagedPrefixCount() {
    std::lock_guard<std::mutex> guard(g_prefixMutex);
    return static_cast<int>(g_descriptors.size());
}

}
