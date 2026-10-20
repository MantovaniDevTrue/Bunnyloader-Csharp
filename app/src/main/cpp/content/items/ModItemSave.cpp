#include "content/items/ModItemSave.h"
#include "content/items/ModItemSaveFormat.h"
#include "content/items/ModItemSaveSafety.h"
#include "content/items/NativeRarityPending.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"
#include "content/common/GameRefs.h"
#include "content/buffs/ModBuffs.h"
#include "content/items/ModItems.h"
#include "content/items/ModPrefixes.h"
#include "experiments/csharp/ManagedPrefixBridge.h"
#include "experiments/csharp/ManagedPrefixGameplay.h"
#include "experiments/csharp/NativeContent.h"
#include "experiments/csharp/SourceLoader.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "experiments/csharp/NativeRarityRegistry.h"
#include "script/api/Items.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace bl::runtime {

namespace {

constexpr const char* kSuffix = ".bl";
constexpr const char* kBuffContainer = "buff";
// Item do JOGO com prefixo de mod: so o prefixo vai no arquivo, na chave
// "vanilla:<tipo>" (o item em si o .plr/.wld ja guardam).
constexpr const char* kVanillaPrefix = "vanilla:";

/**
 * Um item de mod guardado: onde estava e o que era. Buff de mod vai no mesmo
 * arquivo, com container "buff": slot na lista, `stack` = tempo restante.
 * Prefixo de mod vai pelo nome (`prefixKey`), e o numero fica 0: o numero
 * muda com os mods instalados.
 */
using item_save::SavedItem;

/** O prefixo para o arquivo: o de mod vira nome; numero de mod sem dono vira 0. */
void encodePrefix(SavedItem& it, int prefix) {
    it.prefix = prefix;
    it.prefixKey.clear();
    if (prefix < vanillaPrefixCount()) return;
    it.prefixKey = modPrefixKey(prefix);
    it.prefix = 0;
}

/** O numero do prefixo agora; -1 se era de um mod que nao esta carregado. */
int decodePrefix(const SavedItem& it) {
    return it.prefixKey.empty() ? it.prefix : modPrefixByKey(it.prefixKey);
}

bool isVanillaPrefixEntry(const SavedItem& it) {
    return it.key.compare(0, std::char_traits<char>::length(kVanillaPrefix), kVanillaPrefix) == 0;
}

int vanillaPrefixEntryType(const SavedItem& it) {
    return std::atoi(it.key.c_str() + std::char_traits<char>::length(kVanillaPrefix));
}

SavedItem vanillaPrefixEntry(const std::string& container, int slot, int type, int prefix) {
    SavedItem it;
    it.container = container;
    it.slot = slot;
    it.key = kVanillaPrefix + std::to_string(type);
    encodePrefix(it, prefix);
    return it;
}

/** Numero de prefixo de mod que nenhum mod carregado tem (veio de outra sessao). */
bool isStrayPrefix(int prefix) {
    return prefix >= vanillaPrefixCount() && modPrefixKey(prefix).empty();
}

struct Refs {
    bool ok = false;
    int32_t filePath = -1, fileCloud = -1, filePlayer = -1;   // FileData / PlayerFileData
    int32_t inventory = -1, armor = -1, dye = -1, miscEquips = -1, miscDyes = -1;
    int32_t trash = -1, loadouts = -1;
    int32_t bank[4] = {-1, -1, -1, -1};
    int32_t storageItems = -1;                  // InventoryStorage.item
    int32_t loadoutArmor = -1, loadoutDye = -1;  // EquipmentLoadout
    int32_t type = -1, stack = -1, prefix = -1, favorited = -1, rare = -1;   // Item
    const MethodInfo* setDefaults = nullptr;
    const MethodInfo* applyPrefix = nullptr;
    const MethodInfo* applyPrefixDetailed = nullptr;
    const MethodInfo* resetPrefix = nullptr;
};
Refs g_refs;

std::mutex g_mx;
// Itens de mod que nao deu para repor ao carregar (mod nao carregado agora),
// por caminho do personagem. Voltam para o arquivo no proximo save.
std::map<std::string, std::vector<SavedItem>> g_kept;

/**
 * Item de mod reposto SEM o prefixo dele (o mod do prefixo nao esta; o do item
 * pode estar, ou virou "?"): o nome do prefixo volta para o arquivo enquanto o
 * mesmo item seguir no lugar sem prefixo. Por arquivo, por "<lugar>#<slot>".
 */
struct PendingPrefix {
    int type = 0;
    std::string key;
    std::string origin{};
};
std::map<std::string, std::map<std::string, PendingPrefix>> g_pendingPrefix;
std::map<std::string, std::map<std::string, PendingPrefix>> g_pendingRarity;
std::map<std::string, std::map<Il2CppObject*, PendingPrefix>> g_pendingRarityByItem;
std::atomic<int> g_prefixRestoreFailureLogs{0};

std::string rarityFor(Il2CppObject* item) {
    if (!item || g_refs.rare < 0) return {};
    return bl::csharp::nativeRarities().stableKeyForType(
        field<int32_t>(item, g_refs.rare));
}

bool restoreRarity(Il2CppObject* item, const std::string& key) {
    if (key.empty()) return true;
    if (!item || g_refs.rare < 0) return false;
    const int type = bl::csharp::nativeRarities().typeForStableKey(key);
    if (type < bl::csharp::NativeRarityRegistry::kVanillaCount) return false;
    field<int32_t>(item, g_refs.rare) = type;
    return true;
}

std::string pendingRarityFor(const std::string& path, const std::string& container,
    int slot, int type, Il2CppObject* item) {
    std::lock_guard<std::mutex> l(g_mx);
    const auto file = g_pendingRarity.find(path);
    if (file == g_pendingRarity.end()) return {};
    const auto it = file->second.find(container + "#" + std::to_string(slot));
    if (it == file->second.end() || it->second.type != type) return {};
    const auto tracked = g_pendingRarityByItem.find(path);
    if (tracked != g_pendingRarityByItem.end()) {
        for (const auto& [originalItem, state] : tracked->second)
            if (state.type == type && state.key == it->second.key &&
                originalItem != item) return {};
    }
    return it->second.key;
}

std::string pendingRarityForItem(const std::string& path, Il2CppObject* item,
    int type) {
    if (!item) return {};
    std::lock_guard<std::mutex> guard(g_mx);
    const auto file = g_pendingRarityByItem.find(path);
    if (file == g_pendingRarityByItem.end()) return {};
    const auto entry = file->second.find(item);
    return entry != file->second.end() && entry->second.type == type
        ? entry->second.key : std::string();
}

std::string placeKey(const std::string& container, int slot) {
    return container + "#" + std::to_string(slot);
}

/** O nome do prefixo guardado para o item de `type` nesse lugar, ou "". */
std::string pendingPrefixFor(const std::string& path, const std::string& container, int slot, int type) {
    std::lock_guard<std::mutex> l(g_mx);
    auto file = g_pendingPrefix.find(path);
    if (file == g_pendingPrefix.end()) return {};
    auto it = file->second.find(placeKey(container, slot));
    return it != file->second.end() && it->second.type == type ? it->second.key : std::string();
}

std::string toUtf8(Il2CppString* s) {
    std::string out;
    if (!s) return out;
    for (int i = 0; i < s->length; ++i) {
        uint32_t c = s->chars[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < s->length) {
            const uint32_t lo = s->chars[i + 1];
            if (lo >= 0xDC00 && lo < 0xE000) {
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

Il2CppObject* objectAt(Il2CppObject* obj, int32_t offset) {
    return obj && offset >= 0 ? field<Il2CppObject*>(obj, offset) : nullptr;
}

/** Um lugar onde o jogador guarda itens: um array, ou um item so (a lixeira). */
struct Container {
    std::string name;
    Il2CppArray* items = nullptr;
    Il2CppObject* single = nullptr;

    int size() const { return items ? static_cast<int>(items->length) : (single ? 1 : 0); }
    Il2CppObject* at(int slot) const {
        if (slot < 0 || slot >= size()) return nullptr;
        return items ? static_cast<Il2CppObject**>(arrayData(items))[slot] : single;
    }
};

/** Tudo o que o save do personagem grava: inventario, equipamento, cofres, conjuntos. */
std::vector<Container> containersOf(Il2CppObject* player) {
    const Refs& r = g_refs;
    std::vector<Container> v;
    auto add = [&](const std::string& name, Il2CppObject* arr) {
        if (arr) v.push_back({name, reinterpret_cast<Il2CppArray*>(arr), nullptr});
    };
    add("inventory", objectAt(player, r.inventory));
    add("armor", objectAt(player, r.armor));
    add("dye", objectAt(player, r.dye));
    add("misc", objectAt(player, r.miscEquips));
    add("miscdye", objectAt(player, r.miscDyes));
    // Porquinho, cofre, forja defensora e cofre do vazio.
    const char* bankNames[4] = {"bank", "bank2", "bank3", "bank4"};
    for (int i = 0; i < 4; ++i) {
        add(bankNames[i], objectAt(objectAt(player, r.bank[i]), r.storageItems));
    }
    if (auto* loadouts = reinterpret_cast<Il2CppArray*>(objectAt(player, r.loadouts))) {
        for (uintptr_t i = 0; i < loadouts->length; ++i) {
            Il2CppObject* l = static_cast<Il2CppObject**>(arrayData(loadouts))[i];
            const std::string p = "loadout" + std::to_string(i);
            add(p + ".armor", objectAt(l, r.loadoutArmor));
            add(p + ".dye", objectAt(l, r.loadoutDye));
        }
    }
    return v;
}

// ------------------------------ arquivo ------------------------------

std::vector<SavedItem> readFile(const std::string& path) {
    std::vector<SavedItem> items;
    std::ifstream f(path, std::ios::binary);
    if (!f) return items;
    if (!item_save::read(f, items)) {
        BL_ERROR("save de itens de mod: arquivo invalido em %s", path.c_str());
    }
    return items;
}

/** Grava por arquivo temporario + rename: queda no meio nao deixa arquivo pela metade. */
bool writeFile(const std::string& path, const std::vector<SavedItem>& items) {
    if (!item_save::canReplaceExisting(path)) {
        BL_ERROR("save de itens de mod: arquivo existente invalido ou inacessivel em %s; gravacao cancelada",
            path.c_str());
        return false;
    }
    if (items.empty()) {
        std::error_code error;
        std::filesystem::remove(path, error);
        if (error)
            BL_ERROR("save de itens de mod: nao consegui remover %s", path.c_str());
        return !error;
    }
    if (!item_save::preserveLegacyBeforeRaritySave(path, items)) {
        BL_ERROR("save de raridade: backup v1/v2 falhou em %s; gravacao v3 abortada",
            path.c_str());
        return false;
    }
    const std::string tmp = path + ".tmp";
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) {
        BL_ERROR("save de itens de mod: nao consegui escrever %s", tmp.c_str());
        return false;
    }
    const bool encoded = item_save::write(f, items);
    f.flush();
    f.close();
    const bool ok = encoded && !f.fail();
    if (!ok || std::rename(tmp.c_str(), path.c_str()) != 0) {
        BL_ERROR("save de itens de mod: falha ao gravar %s", path.c_str());
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

/** Caminho do .plr, ou "" para personagem na nuvem (nao ha arquivo local ao lado). */
std::string localPath(Il2CppObject* fileData) {
    if (!fileData || field<uint8_t>(fileData, g_refs.fileCloud)) return {};
    return toUtf8(reinterpret_cast<Il2CppString*>(objectAt(fileData, g_refs.filePath)));
}

/**
 * Prefixo de mod ausente num item do jogo: a linha segue no arquivo enquanto o
 * mesmo item estiver no lugar, sem prefixo (o jogo tira o que nao conhece).
 */
bool csharpPrefixSourcePending(const std::string& key) {
    const auto slash = key.find('/');
    if (slash == std::string::npos) return false;
    const std::string modId = key.substr(0, slash);
    for (const auto& mod : bl::csharp::sourceModStatuses())
        if (mod.id == modId && mod.loaded && mod.prefixes > 0 && !mod.runtimeApplied)
            return true;
    return false;
}

bool restoreSavedPrefix(Il2CppObject* item, const SavedItem& saved) {
    if (!saved.prefixKey.empty() && csharpPrefixSourcePending(saved.prefixKey))
        return false;
    const Refs& r = g_refs;
    const int wanted = decodePrefix(saved);
    const MethodInfo* method = !saved.prefixKey.empty() && r.applyPrefixDetailed
        ? r.applyPrefixDetailed : r.applyPrefix;
    if (!item || wanted <= 0 || !method) {
        if (!saved.prefixKey.empty() &&
            g_prefixRestoreFailureLogs.fetch_add(1, std::memory_order_relaxed) < 12)
            BL_WARN("csharp prefix restore: chave=%s id=%d item=%d metodo=%d",
                    saved.prefixKey.c_str(), wanted, item ? 1 : 0, method ? 1 : 0);
        return wanted == 0;
    }
    const int current = field<uint8_t>(item, r.prefix);
    const bool managed = !saved.prefixKey.empty();
    if (managed && bl::csharp::managedPrefixDescriptorPending(wanted)) return false;
    if (current == wanted && !managed) return true;
    auto& api = il2cpp::api();
    Il2CppObject* error = nullptr;
    if (current != 0 && r.resetPrefix) {
        api.runtime_invoke(r.resetPrefix, item, nullptr, &error);
        if (error || field<uint8_t>(item, r.prefix) != 0) return false;
    } else if (current != 0 && !r.resetPrefix) {
        return current == wanted;
    }
    int prefix = wanted;
    uint8_t topTier = 0;
    void* directArguments[2] = {&prefix, &topTier};
    const bool detailed = method == r.applyPrefixDetailed;
    Il2CppObject* result = api.runtime_invoke(method, item, directArguments, &error);
    const bool returned = result &&
        *reinterpret_cast<const uint8_t*>(reinterpret_cast<const char*>(result) + sizeof(Il2CppObject)) != 0;
    const int directAfter = field<uint8_t>(item, r.prefix);
    const bool directException = error != nullptr;
    bool restored = !error && directAfter == wanted;
    bool forcedRollReached = false;
    int retryAfter = directAfter;
    if (!restored && managed && r.resetPrefix && r.applyPrefixDetailed) {
        error = nullptr;
        if (retryAfter != 0)
            api.runtime_invoke(r.resetPrefix, item, nullptr, &error);
        if (!error && field<uint8_t>(item, r.prefix) == 0) {
            restored = bl::csharp::applyManagedPrefixViaNativeRoll(
                item, wanted, r.applyPrefixDetailed, &forcedRollReached);
        }
        retryAfter = field<uint8_t>(item, r.prefix);
        if (restored)
            BL_INFO("csharp prefix: restaurado pelo RollAPrefix nativo id=%d tipo=%d",
                    wanted, field<int32_t>(item, r.type));
    }
    std::string nativeFallbackError;
    if (!restored && managed && bl::csharp::managedPrefixDescriptorReady(wanted) &&
        r.resetPrefix && bl::csharp::managedPrefixGameplayReady()) {
        error = nullptr;
        if (field<uint8_t>(item, r.prefix) != 0)
            api.runtime_invoke(r.resetPrefix, item, nullptr, &error);
        if (!error && field<uint8_t>(item, r.prefix) == 0)
            restored = bl::csharp::applyManagedPrefixStatFallback(item, wanted, &nativeFallbackError);
    }
    if (!restored && managed &&
        g_prefixRestoreFailureLogs.fetch_add(1, std::memory_order_relaxed) < 12)
        BL_WARN("csharp prefix restore: chave=%s id=%d tipo=%d antes=%d direto=%d Prefix()=%d excecao=%d overload2=%d topTier=%d rollHook=%d rollDepois=%d",
                saved.prefixKey.c_str(), wanted, field<int32_t>(item, r.type),
                current, directAfter, returned ? 1 : 0, directException ? 1 : 0,
                detailed ? 1 : 0, topTier ? 1 : 0, forcedRollReached ? 1 : 0, retryAfter);
    if (!restored && managed && !nativeFallbackError.empty())
        BL_WARN("csharp prefix: restauracao rejeitada chave=%s item=%d motivo=%s",
                saved.prefixKey.c_str(), field<int32_t>(item, r.type), nativeFallbackError.c_str());
    if (!restored && managed && r.resetPrefix &&
        field<uint8_t>(item, r.prefix) != 0) {
        error = nullptr;
        api.runtime_invoke(r.resetPrefix, item, nullptr, &error);
    }
    return restored;
}

using SaveFn = void (*)(Il2CppObject*, const MethodInfo*);
SaveFn g_origSave = nullptr;

/**
 * DEPOIS do original: o que conta e o estado que o jogo acabou de gravar (ele
 * guarda o item do cursor no inventario antes de escrever).
 */
void hkInternalSavePlayerFile(Il2CppObject* fileData, const MethodInfo* m) {
    const Refs& r = g_refs;
    Il2CppObject* beforePlayer = objectAt(fileData, r.filePlayer);
    std::map<Il2CppObject*, std::pair<int, int>> before;
    std::map<Il2CppObject*, std::pair<int, std::string>> beforeRarity;
    if (beforePlayer) {
        for (const Container& container : containersOf(beforePlayer)) {
            for (int slot = 0; slot < container.size(); ++slot) {
                Il2CppObject* item = container.at(slot);
                if (!item) continue;
                const int type = field<int32_t>(item, r.type);
                const int prefix = field<uint8_t>(item, r.prefix);
                const std::string stableRarity = rarityFor(item);
                if (type > 0 && !stableRarity.empty())
                    beforeRarity.emplace(item, std::make_pair(type, stableRarity));
                if (type > 0 && prefix >= vanillaPrefixCount() &&
                    !modPrefixKey(prefix).empty()) {
                    before.emplace(item, std::make_pair(type, prefix));
                }
            }
        }
    }
    g_origSave(fileData, m);
    const std::string path = localPath(fileData);
    Il2CppObject* player = objectAt(fileData, r.filePlayer);
    if (path.empty() || !player) return;
    std::vector<SavedItem> items;
    int recoveredPrefixes = 0;
    for (const Container& c : containersOf(player)) {
        for (int slot = 0; slot < c.size(); ++slot) {
            Il2CppObject* item = c.at(slot);
            if (!item) continue;
            const int type = field<int32_t>(item, r.type);
            const int stack = field<int32_t>(item, r.stack);
            int prefix = field<uint8_t>(item, r.prefix);
            if (stack <= 0 || type <= 0) continue;
            if (prefix == 0) {
                const auto previous = before.find(item);
                if (previous != before.end() && previous->second.first == type) {
                    prefix = previous->second.second;
                    ++recoveredPrefixes;
                }
            }
            std::string stableRarity = rarityFor(item);
            if (stableRarity.empty()) {
                const auto previousRarity = beforeRarity.find(item);
                if (previousRarity != beforeRarity.end() &&
                    previousRarity->second.first == type)
                    stableRarity = previousRarity->second.second;
            }
            if (stableRarity.empty())
                stableRarity = pendingRarityForItem(path, item, type);
            if (stableRarity.empty())
                stableRarity = pendingRarityFor(path, c.name, slot, type, item);
            if (!isModItem(type)) {
                const bool customPrefix = prefix >= vanillaPrefixCount() &&
                    !modPrefixKey(prefix).empty();
                if (customPrefix || !stableRarity.empty()) {
                    SavedItem record = vanillaPrefixEntry(c.name, slot, type,
                        customPrefix ? prefix : 0);
                    record.rarityKey = stableRarity;
                    items.push_back(std::move(record));
                }
                continue;
            }
            SavedItem it{c.name, slot, stack, 0, field<uint8_t>(item, r.favorited) != 0, modItemKey(type), {}};
            const std::string pendingKey = pendingPrefixFor(path, c.name, slot, type);
            if (!pendingKey.empty()) it.prefixKey = pendingKey;
            else encodePrefix(it, prefix);
            it.rarityKey = stableRarity;
            items.push_back(std::move(it));
        }
    }
    for (const SavedBuff& b : collectModBuffs(player)) {
        items.push_back({kBuffContainer, b.slot, b.time, 0, false, b.key, {}});
    }
    size_t kept = 0;
    {
        std::lock_guard<std::mutex> l(g_mx);
        auto it = g_kept.find(path);
        if (it != g_kept.end()) {
            const std::vector<Container> containers = containersOf(player);
            for (const SavedItem& k : it->second) {
                SavedItem remaining = k;
                if (isVanillaPrefixEntry(k)) {
                    Il2CppObject* matching = nullptr;
                    for (const Container& c : containers)
                        if (c.name == k.container) matching = c.at(k.slot);
                    if (!matching ||
                        field<int32_t>(matching, g_refs.type) != vanillaPrefixEntryType(k))
                        continue;
                    remaining = item_save::remainingVanillaMetadata(k,
                        field<uint8_t>(matching, g_refs.prefix), rarityFor(matching));
                    if (!item_save::hasRemainingVanillaMetadata(remaining)) continue;
                }
                if (!item_save::mergePending(items, remaining))
                    items.push_back(std::move(remaining));
                ++kept;
            }
        }
    }
    const bool sidecarSaved = writeFile(path + kSuffix, items);
    bool hasRarityPending = false;
    {
        std::lock_guard<std::mutex> guard(g_mx);
        hasRarityPending = g_pendingRarity.find(path) != g_pendingRarity.end() ||
            g_pendingRarityByItem.find(path) != g_pendingRarityByItem.end();
    }
    if (sidecarSaved && hasRarityPending) {
        std::vector<item_save::LiveRarityItem<Il2CppObject*>> liveRarityItems;
        for (const Container& container : containersOf(player)) {
            for (int slot = 0; slot < container.size(); ++slot) {
                auto* item = container.at(slot);
                if (!item) continue;
                liveRarityItems.push_back({item, placeKey(container.name, slot),
                    field<int32_t>(item, r.type), !rarityFor(item).empty()});
            }
        }
        std::lock_guard<std::mutex> guard(g_mx);
        const auto oldSlots = g_pendingRarity.find(path);
        const auto oldItems = g_pendingRarityByItem.find(path);
        if (oldSlots != g_pendingRarity.end() ||
            oldItems != g_pendingRarityByItem.end()) {
            const std::map<std::string, PendingPrefix> emptySlots;
            const std::map<Il2CppObject*, PendingPrefix> emptyItems;
            std::map<std::string, PendingPrefix> nextSlots;
            std::map<Il2CppObject*, PendingPrefix> nextItems;
            item_save::reconcileTrackedRarities(
                oldItems == g_pendingRarityByItem.end() ? emptyItems : oldItems->second,
                oldSlots == g_pendingRarity.end() ? emptySlots : oldSlots->second,
                liveRarityItems, &nextItems, &nextSlots);
            if (nextSlots.empty()) g_pendingRarity.erase(path);
            else g_pendingRarity[path] = std::move(nextSlots);
            if (nextItems.empty()) g_pendingRarityByItem.erase(path);
            else g_pendingRarityByItem[path] = std::move(nextItems);
        }
    }
    if (recoveredPrefixes) {
        BL_INFO("save de itens de mod: %d prefixo(s) preservados antes do save original", recoveredPrefixes);
    }
    if (!items.empty() && sidecarSaved) {
        // Diz quem mandou salvar: o jogo salva o personagem tambem com a
        // gravacao automatica desligada (ao morrer, no multijogador, ao sair),
        // e sem isto parecia que o Bunny Loader salvava por conta propria.
        BL_INFO("save de itens de mod: o jogo salvou o personagem; %zu item(ns) de mod gravado(s) junto "
                "(%zu de mod nao carregado)", items.size(), kept);
    }
}

// ------------------------------ carregar ------------------------------

/**
 * O tipo de `key` agora: o item do mod, se ele esta carregado; senao o "?"
 * que o representa. -1 se nem isso (o jogo ainda nao conhece os tipos de mod,
 * ou a reserva de "?" acabou): quem chama guarda a entrada no arquivo.
 */
int typeForKey(const std::string& key) {
    const int type = modItemTypeByKey(key);
    if (type >= 0) return type < itemTypeCount() ? type : -1;
    return unloadedTypeFor(key);
}

/**
 * Tipo de mod que NAO veio do nosso arquivo: numero cru do .plr/.wld, que so
 * vale se ainda for um tipo que o jogo conhece e que nao e um "?" sem dono.
 * O resto derrubava o jogo (o bau le ContentSamples[tipo] ao carregar o mundo).
 */
bool isStrayType(int type) {
    if (type < kVanillaItemCount) return false;
    if (itemTypeCount() <= kVanillaItemCount) return false;   // tipos de mod ainda nao instalados
    return type >= itemTypeCount() || (isUnloadedType(type) && modItemKey(type).empty());
}

/** Vazio para o jogo: sem tipo ou sem pilha. */
bool isEmpty(Il2CppObject* item) {
    return item && (field<int32_t>(item, g_refs.type) == 0 || field<int32_t>(item, g_refs.stack) <= 0);
}

void clearItem(Il2CppObject* item) {
    Il2CppObject* exc = nullptr;
    int t = 0;
    void* sd[2] = {&t, nullptr};
    il2cpp::api().runtime_invoke(g_refs.setDefaults, item, sd, &exc);
}

bool restore(Il2CppObject* item, int type, const SavedItem& saved, bool* prefixRestored) {
    auto& a = il2cpp::api();
    const Refs& r = g_refs;
    Il2CppObject* exc = nullptr;
    int t = type;
    void* sd[2] = {&t, nullptr};
    a.runtime_invoke(r.setDefaults, item, sd, &exc);   // passa pelo setDefaults do mod
    if (exc || field<int32_t>(item, r.type) != type) return false;
    field<int32_t>(item, r.stack) = saved.stack > 0 ? saved.stack : 1;
    const bool applied = restoreSavedPrefix(item, saved);
    if (prefixRestored) *prefixRestored = applied;
    field<uint8_t>(item, r.favorited) = saved.favorited ? 1 : 0;
    script::loadModItemData(item, saved.data);
    return true;
}

/**
 * O prefixo de mod de um item do jogo, pelo nome. O .plr trouxe o NUMERO de
 * quando foi salvo, que pode ser hoje outro prefixo (ou nenhum): tira o que
 * veio (ResetPrefix refaz o item) e poe o certo. false: o mod do prefixo nao
 * esta carregado, e a linha fica guardada.
 */
bool restoreVanillaPrefix(Il2CppObject* item, const SavedItem& saved) {
    auto& a = il2cpp::api();
    const Refs& r = g_refs;
    const int want = decodePrefix(saved);
    if (!saved.prefixKey.empty() && csharpPrefixSourcePending(saved.prefixKey))
        return false;
    if (!saved.prefixKey.empty() && bl::csharp::managedPrefixDescriptorReady(want))
        return restoreSavedPrefix(item, saved);
    if (!saved.prefixKey.empty() && bl::csharp::managedPrefixDescriptorPending(want))
        return false;
    const int now = field<uint8_t>(item, r.prefix);
    if (want == now) return true;
    Il2CppObject* exc = nullptr;
    if (now != 0 && r.resetPrefix) a.runtime_invoke(r.resetPrefix, item, nullptr, &exc);
    if (want <= 0) return false;
    if (r.applyPrefix) {
        int p = want;
        void* pa[1] = {&p};
        a.runtime_invoke(r.applyPrefix, item, pa, &exc);
    }
    return !exc && field<uint8_t>(item, r.prefix) == want;
}

using LoadFn = Il2CppObject* (*)(Il2CppString*, bool, const MethodInfo*);
LoadFn g_origLoad = nullptr;

Il2CppObject* hkLoadPlayer(Il2CppString* playerPath, bool cloudSave, const MethodInfo* m) {
    Il2CppObject* fileData = g_origLoad(playerPath, cloudSave, m);
    const std::string path = localPath(fileData);
    Il2CppObject* player = objectAt(fileData, g_refs.filePlayer);
    if (path.empty() || !player) return fileData;

    std::vector<SavedItem> saved = readFile(path + kSuffix);
    std::vector<SavedItem> kept;
    // Os buffs primeiro, e fora da lista de itens (a chave nao e de item).
    std::vector<SavedBuff> buffs;
    for (auto it = saved.begin(); it != saved.end();) {
        if (it->container != kBuffContainer) { ++it; continue; }
        buffs.push_back({it->slot, it->stack, it->key});
        it = saved.erase(it);
    }
    const size_t buffCount = buffs.size();
    for (const SavedBuff& b : restoreModBuffs(player, buffs)) {
        kept.push_back({kBuffContainer, b.slot, b.time, 0, false, b.key, {}});
    }
    const size_t buffsKept = kept.size();
    int restored = 0, moved = 0, unloaded = 0, stray = 0, prefixes = 0;
    int managedPrefixes = 0, pendingManagedPrefixes = 0;
    std::map<std::string, PendingPrefix> pending;
    std::map<std::string, PendingPrefix> pendingRarity;
    std::map<Il2CppObject*, PendingPrefix> pendingRarityByItem;
    if (!saved.empty()) {
        const std::vector<Container> containers = containersOf(player);
        const Container* inventory = nullptr;
        for (const Container& c : containers) if (c.name == "inventory") inventory = &c;

        for (const SavedItem& s : saved) {
            if (isVanillaPrefixEntry(s)) {
                Il2CppObject* slotItem = nullptr;
                for (const Container& c : containers) {
                    if (c.name == s.container) slotItem = c.at(s.slot);
                }
                // O item mudou de lugar (ou saiu) sem o loader: a linha nao vale mais.
                if (!slotItem || field<int32_t>(slotItem, g_refs.type) != vanillaPrefixEntryType(s)) continue;
                const bool prefixOk = !item_save::hasSavedVanillaPrefix(s) ||
                    restoreVanillaPrefix(slotItem, s);
                if (prefixOk && item_save::hasSavedVanillaPrefix(s)) ++prefixes;
                if (!prefixOk) kept.push_back(s);
                if (!s.rarityKey.empty() && !restoreRarity(slotItem, s.rarityKey)) {
                    const PendingPrefix unresolved{
                        field<int32_t>(slotItem, g_refs.type), s.rarityKey};
                    pendingRarity[placeKey(s.container, s.slot)] = unresolved;
                    pendingRarityByItem[slotItem] = {
                        unresolved.type, unresolved.key,
                        placeKey(s.container, s.slot)};
                }
                continue;
            }
            const int type = typeForKey(s.key);
            if (type < 0) { kept.push_back(s); continue; }
            if (isUnloadedType(type)) ++unloaded;
            Il2CppObject* slotItem = nullptr;
            for (const Container& c : containers) {
                if (c.name == s.container) slotItem = c.at(s.slot);
            }
            // O jogo NAO troca tipo desconhecido por nada em todo lugar: os
            // cofres (porquinho etc.) voltam com o numero cru do .plr. Se ja e
            // o tipo certo, nada a fazer; se e outro tipo de mod, o numero e que
            // esta velho (outros mods, outra ordem) e o lugar e nosso. Tratar
            // como ocupado punha uma copia no inventario.
            const int slotType = slotItem ? field<int32_t>(slotItem, g_refs.type) : 0;
            if (slotItem && slotType == type) {
                if (!s.prefixKey.empty()) {
                    if (restoreSavedPrefix(slotItem, s)) {
                        ++managedPrefixes;
                    } else {
                        pending[placeKey(s.container, s.slot)] = {type, s.prefixKey};
                        ++pendingManagedPrefixes;
                    }
                }
                if (!s.rarityKey.empty() && !restoreRarity(slotItem, s.rarityKey)) {
                    pendingRarity[placeKey(s.container, s.slot)] = {type, s.rarityKey};
                    pendingRarityByItem[slotItem] = {type, s.rarityKey,
                        placeKey(s.container, s.slot)};
                }
                ++restored;
                continue;
            }
            Il2CppObject* target =
                isEmpty(slotItem) || slotType >= kVanillaItemCount ? slotItem : nullptr;
            if (!target && inventory) {
                // O lugar foi ocupado (o .plr foi mexido sem o loader): primeira
                // vaga do inventario principal, sem moedas, municao e cursor.
                for (int i = 0; i < 50 && !target; ++i) {
                    if (isEmpty(inventory->at(i))) target = inventory->at(i);
                }
                if (target) ++moved;
            }
            bool prefixRestored = false;
            if (target && restore(target, type, s, &prefixRestored)) {
                if (!s.rarityKey.empty() && !restoreRarity(target, s.rarityKey)) {
                    pendingRarityByItem[target] = {type, s.rarityKey,
                        target == slotItem ? placeKey(s.container, s.slot) : std::string()};
                    bool located = false;
                    for (const Container& candidate : containers) {
                        for (int targetSlot = 0; targetSlot < candidate.size(); ++targetSlot) {
                            if (candidate.at(targetSlot) != target) continue;
                            const std::string destination = placeKey(candidate.name, targetSlot);
                            pendingRarity[destination] = {type, s.rarityKey};
                            pendingRarityByItem[target].origin = destination;
                            located = true;
                            break;
                        }
                        if (located) break;
                    }
                    if (!located) kept.push_back(s);
                }
                ++restored;
                if (!s.prefixKey.empty()) {
                    if (prefixRestored) {
                        ++managedPrefixes;
                    } else if (target == slotItem) {
                        pending[placeKey(s.container, s.slot)] = {type, s.prefixKey};
                        ++pendingManagedPrefixes;
                    }
                }
            } else {
                kept.push_back(s);
            }
        }
    }
    // O que sobrou com numero cru invalido (cofre de antes deste arquivo, mod
    // que mudou) vira nada: o jogo nao sabe desenhar nem empilhar.
    for (const Container& c : containersOf(player)) {
        for (int slot = 0; slot < c.size(); ++slot) {
            Il2CppObject* item = c.at(slot);
            if (item && isStrayType(field<int32_t>(item, g_refs.type))) {
                clearItem(item);
                ++stray;
            }
        }
    }
    {
        std::lock_guard<std::mutex> l(g_mx);
        if (kept.empty()) g_kept.erase(path);
        else g_kept[path] = kept;
        if (pending.empty()) g_pendingPrefix.erase(path);
        else g_pendingPrefix[path] = std::move(pending);
        if (pendingRarity.empty()) g_pendingRarity.erase(path);
        else g_pendingRarity[path] = std::move(pendingRarity);
        if (pendingRarityByItem.empty()) g_pendingRarityByItem.erase(path);
        else g_pendingRarityByItem[path] = std::move(pendingRarityByItem);
    }
    if (buffCount) {
        BL_INFO("save de buffs de mod: %zu reposto(s), %zu so no arquivo", buffCount - buffsKept, buffsKept);
    }
    if (restored > 0)
        bl::csharp::requestBasicItemInstanceRefresh();
    if (!saved.empty() || stray) {
        BL_INFO("save de itens de mod: personagem: %d reposto(s) (%d como \"?\")%s, %d prefixo(s) "
                "de mod em item do jogo, %zu so no arquivo, %d numero(s) invalido(s) limpo(s); "
                "prefixos de item C# repostos=%d pendentes=%d",
                restored, unloaded, moved ? ", algum fora do lugar" : "", prefixes, kept.size(), stray,
                managedPrefixes, pendingManagedPrefixes);
    }
    return fileData;
}

bool rehydrateSavedManagedItem(Il2CppObject* item, int type, int wanted) {
    const int kind = bl::csharp::managedNativePrefixItemKind(type);
    if (!kind) return true;
    if (!item || !g_refs.setDefaults) return false;
    auto& api = il2cpp::api();
    Il2CppClass* itemClass = il2cpp::findClass({"Terraria", "Item", {}});
    if (!itemClass) return false;
    static const int damage = il2cpp::fieldOffset(itemClass, "damage");
    static const int maxStack = il2cpp::fieldOffset(itemClass, "maxStack");
    static const int accessory = il2cpp::fieldOffset(itemClass, "accessory");
    if (damage < 0 || maxStack < 0 || accessory < 0) return false;
    const int beforeDamage = field<int32_t>(item, damage);
    const int beforeMaxStack = field<int32_t>(item, maxStack);
    const bool beforeAccessory = field<uint8_t>(item, accessory) != 0;
    const int beforePrefix = field<uint8_t>(item, g_refs.prefix);
    const int keepStack = field<int32_t>(item, g_refs.stack);
    const bool keepFavorited = field<uint8_t>(item, g_refs.favorited) != 0;
    int desiredType = type;
    void* args[2] = {&desiredType, nullptr};
    Il2CppObject* exception = nullptr;
    api.runtime_invoke(g_refs.setDefaults, item, args, &exception);
    const bool ok = !exception && field<int32_t>(item, g_refs.type) == type;
    if (!ok) {
        BL_WARN("csharp prefix: reidratar tipo=%d falhou excecao=%d", type,
                exception ? 1 : 0);
        return false;
    }
    field<int32_t>(item, g_refs.stack) = keepStack > 0 ? keepStack : 1;
    field<uint8_t>(item, g_refs.favorited) = keepFavorited ? 1 : 0;
    bool hasWanted = false;
    const MethodInfo* rollable = il2cpp::findMethodBySignature(
        itemClass, il2cpp::parseSignature("int[] GetRollablePrefixes()"));
    if (rollable) {
        exception = nullptr;
        auto* candidates = reinterpret_cast<Il2CppArray*>(
            api.runtime_invoke(rollable, item, nullptr, &exception));
        if (!exception && candidates && candidates->length < 512) {
            const auto* ids = static_cast<const int32_t*>(arrayData(candidates));
            for (uintptr_t i = 0; i < candidates->length; ++i)
                if (ids[i] == wanted) {
                    hasWanted = true;
                    break;
                }
        }
    }
    BL_INFO("csharp prefix: reidratacao tipo=%d especie=%d dano=%d->%d pilhaMax=%d->%d acessorio=%d->%d prefixoAntes=%d candidato=%d",
            type, kind, beforeDamage, field<int32_t>(item, damage),
            beforeMaxStack, field<int32_t>(item, maxStack),
            beforeAccessory ? 1 : 0, field<uint8_t>(item, accessory) ? 1 : 0,
            beforePrefix, hasWanted ? 1 : 0);
    return true;
}

void retryPendingManagedPrefixesInWorld() {
    if (!g_refs.ok) return;
    auto& api = il2cpp::api();
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    const MethodInfo* getter = main
        ? api.class_get_method_from_name(main, "get_ActivePlayerFileData", 0) : nullptr;
    if (!getter) return;
    Il2CppObject* error = nullptr;
    Il2CppObject* fileData = api.runtime_invoke(getter, nullptr, nullptr, &error);
    if (error || !fileData) return;
    const std::string path = localPath(fileData);
    Il2CppObject* player = objectAt(fileData, g_refs.filePlayer);
    if (path.empty() || !player) return;
    std::map<std::string, PendingPrefix> pending;
    {
        std::lock_guard<std::mutex> lock(g_mx);
        const auto it = g_pendingPrefix.find(path);
        if (it == g_pendingPrefix.end() || it->second.empty()) return;
        pending = it->second;
    }
    int attempted = 0;
    int restored = 0;
    int unresolved = 0;
    std::vector<std::string> completed;
    for (const Container& container : containersOf(player)) {
        for (int slot = 0; slot < container.size(); ++slot) {
            const std::string slotKey = placeKey(container.name, slot);
            const auto it = pending.find(slotKey);
            if (it == pending.end()) continue;
            Il2CppObject* item = container.at(slot);
            if (!item || field<int32_t>(item, g_refs.type) != it->second.type) continue;
            SavedItem saved{};
            saved.prefixKey = it->second.key;
            ++attempted;
            const int wanted = decodePrefix(saved);
            const bool ready = wanted > 0 &&
                rehydrateSavedManagedItem(item, it->second.type, wanted);
            if (ready && restoreSavedPrefix(item, saved)) {
                ++restored;
                completed.push_back(slotKey);
            } else {
                ++unresolved;
            }
        }
    }
    size_t remaining = 0;
    {
        std::lock_guard<std::mutex> lock(g_mx);
        auto active = g_pendingPrefix.find(path);
        if (active != g_pendingPrefix.end()) {
            for (const auto& key : completed) {
                auto original = pending.find(key);
                auto current = active->second.find(key);
                if (original != pending.end() && current != active->second.end() &&
                    original->second.type == current->second.type &&
                    original->second.key == current->second.key)
                    active->second.erase(current);
            }
            remaining = active->second.size();
            if (!remaining) g_pendingPrefix.erase(active);
        }
    }
    BL_INFO("csharp prefix: recuperacao apos mundo pronto tentados=%d repostos=%d falhos=%d pendentes=%zu",
            attempted, restored, unresolved, remaining);
}

void retryPendingManagedRaritiesInWorld() {
    if (!g_refs.ok || g_refs.rare < 0) return;
    auto& api = il2cpp::api();
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    const MethodInfo* getter = main
        ? api.class_get_method_from_name(main, "get_ActivePlayerFileData", 0) : nullptr;
    if (!getter) return;
    Il2CppObject* exception = nullptr;
    auto* file = api.runtime_invoke(getter, nullptr, nullptr, &exception);
    if (exception || !file) return;
    const std::string path = localPath(file);
    auto* player = objectAt(file, g_refs.filePlayer);
    if (path.empty() || !player) return;

    std::map<std::string, PendingPrefix> pendingBySlot;
    std::map<Il2CppObject*, PendingPrefix> pendingByItem;
    {
        std::lock_guard<std::mutex> lock(g_mx);
        const auto slots = g_pendingRarity.find(path);
        if (slots != g_pendingRarity.end()) pendingBySlot = slots->second;
        const auto items = g_pendingRarityByItem.find(path);
        if (items != g_pendingRarityByItem.end()) pendingByItem = items->second;
    }
    if (pendingBySlot.empty() && pendingByItem.empty()) return;

    struct Completed {
        std::string origin;
        Il2CppObject* item;
        int type;
        std::string key;
    };
    std::vector<Completed> completed;
    for (const Container& container : containersOf(player)) {
        for (int slot = 0; slot < container.size(); ++slot) {
            auto* item = container.at(slot);
            if (!item) continue;
            const int type = field<int32_t>(item, g_refs.type);
            const auto desired = item_save::selectTrackedRarity(
                pendingByItem, pendingBySlot, item, placeKey(container.name, slot), type);
            if (!desired) continue;
            if (!rarityFor(item).empty() || restoreRarity(item, desired->key))
                completed.push_back({desired->origin, item, desired->type, desired->key});
        }
    }
    if (completed.empty()) return;

    std::lock_guard<std::mutex> lock(g_mx);
    auto slots = g_pendingRarity.find(path);
    auto items = g_pendingRarityByItem.find(path);
    for (const auto& done : completed) {
        if (slots != g_pendingRarity.end() && !done.origin.empty()) {
            auto at = slots->second.find(done.origin);
            if (at != slots->second.end() && at->second.type == done.type &&
                at->second.key == done.key) slots->second.erase(at);
        }
        if (items != g_pendingRarityByItem.end()) {
            auto at = items->second.find(done.item);
            if (at != items->second.end() && at->second.type == done.type &&
                at->second.key == done.key) items->second.erase(at);
        }
    }
    if (slots != g_pendingRarity.end() && slots->second.empty())
        g_pendingRarity.erase(slots);
    if (items != g_pendingRarityByItem.end() && items->second.empty())
        g_pendingRarityByItem.erase(items);
}

// ------------------------------ mundo ------------------------------
//
// Os baus do mundo, com o mesmo arquivo ao lado (`<mundo>.wld.bl`). O bau e
// identificado pela coordenada, nao pelo indice: o jogo so grava os baus que
// existem e os renumera ao carregar.

struct ChestLayout {
    bool ok = false;
    FieldInfo* chests = nullptr;          // Main.chest
    FieldInfo* activeWorld = nullptr;     // Main.ActiveWorldFileData
    int32_t items = -1, x = -1, y = -1;   // Chest
    size_t stride = 0;                    // ChestItem no array
    size_t type = 0, stack = 0, prefix = 0, favorited = 0;   // dentro do ChestItem
};
ChestLayout g_chest;

struct ChestData {
    int type;
    std::string data;
    std::string rarityKey{};
};

std::map<void*, ChestData> g_chestData;

std::string chestData(void* slot, int type) {
    std::lock_guard<std::mutex> lock(g_mx);
    const auto it = g_chestData.find(slot);
    return it != g_chestData.end() && it->second.type == type ? it->second.data : std::string();
}

std::string chestRarity(void* slot, int type) {
    std::lock_guard<std::mutex> lock(g_mx);
    const auto it = g_chestData.find(slot);
    return it != g_chestData.end() && it->second.type == type ?
        it->second.rarityKey : std::string();
}

void setChestData(void* slot, int type, std::string data) {
    std::lock_guard<std::mutex> lock(g_mx);
    if (!slot) return;
    if (type <= 0) {
        g_chestData.erase(slot);
        return;
    }
    const auto found = g_chestData.find(slot);
    if (found != g_chestData.end() && found->second.type == type) {
        found->second.data = std::move(data);
        if (found->second.data.empty() && found->second.rarityKey.empty())
            g_chestData.erase(found);
    } else if (!data.empty()) {
        g_chestData[slot] = {type, std::move(data), {}};
    } else {
        g_chestData.erase(slot);
    }
}

void setChestRarity(void* slot, int type, std::string key) {
    if (!slot || type <= 0) return;
    std::lock_guard<std::mutex> lock(g_mx);
    const auto found = g_chestData.find(slot);
    if (found != g_chestData.end() && found->second.type == type) {
        found->second.rarityKey = std::move(key);
        if (found->second.data.empty() && found->second.rarityKey.empty())
            g_chestData.erase(found);
    } else if (!key.empty()) {
        g_chestData[slot] = {type, {}, std::move(key)};
    }
}

using ChestSetFn = void (*)(void*, Il2CppObject*, const MethodInfo*);
ChestSetFn g_origChestSet = nullptr;
void hkChestSet(void* slot, Il2CppObject* item, const MethodInfo* method) {
    std::string data;
    const int type = item ? field<int32_t>(item, g_refs.type) : 0;
    const std::string rarity = rarityFor(item);
    if (isModItem(type)) script::saveModItemData(item, data);

    g_origChestSet(slot, item, method);
    setChestData(slot, type, std::move(data));
    setChestRarity(slot, type, rarity);
}

using ChestExpandFn = Il2CppObject* (*)(void*, const MethodInfo*);
ChestExpandFn g_origChestExpand = nullptr;
Il2CppObject* hkChestExpand(void* slot, const MethodInfo* method) {
    Il2CppObject* item = g_origChestExpand(slot, method);
    if (item && isModItem(field<int32_t>(item, g_refs.type))) {
        const std::string data = chestData(slot, field<int32_t>(item, g_refs.type));
        script::loadModItemData(item, data);
    }
    if (item)
        restoreRarity(item, chestRarity(slot, field<int32_t>(item, g_refs.type)));
    return item;
}

using ChestResetFn = void (*)(void*, const MethodInfo*);
ChestResetFn g_origChestClear = nullptr, g_origChestAir = nullptr;
void hkChestClear(void* slot, const MethodInfo* method) {
    setChestData(slot, 0, {});
    g_origChestClear(slot, method);
}

void hkChestAir(void* slot, const MethodInfo* method) {
    setChestData(slot, 0, {});
    g_origChestAir(slot, method);
}

using ChestDefaultsFn = void (*)(void*, int32_t, const MethodInfo*);
ChestDefaultsFn g_origChestDefaults = nullptr, g_origChestNetDefaults = nullptr;
void hkChestDefaults(void* slot, int32_t type, const MethodInfo* method) {
    setChestData(slot, 0, {});
    g_origChestDefaults(slot, type, method);
}

void hkChestNetDefaults(void* slot, int32_t type, const MethodInfo* method) {
    setChestData(slot, 0, {});
    g_origChestNetDefaults(slot, type, method);
}

using ClearChestsFn = void (*)(const MethodInfo*);
ClearChestsFn g_origClearChests = nullptr;
void hkClearChests(const MethodInfo* method) {
    {
        std::lock_guard<std::mutex> lock(g_mx);
        g_chestData.clear();
    }

    g_origClearChests(method);
}

/** Um ChestItem dentro do array do bau. */
struct ChestSlot {
    char* data = nullptr;

    int16_t& type() const { return *reinterpret_cast<int16_t*>(data + g_chest.type); }
    int16_t& stack() const { return *reinterpret_cast<int16_t*>(data + g_chest.stack); }
    uint8_t& prefix() const { return *reinterpret_cast<uint8_t*>(data + g_chest.prefix); }
    uint8_t& favorited() const { return *reinterpret_cast<uint8_t*>(data + g_chest.favorited); }
};

ChestSlot slotOf(Il2CppArray* items, int slot) {
    if (slot < 0 || static_cast<uintptr_t>(slot) >= items->length) return {};
    return {static_cast<char*>(arrayData(items)) + static_cast<size_t>(slot) * g_chest.stride};
}

std::vector<ChestData> snapshotChest(Il2CppObject* chest, bool erase) {
    std::vector<ChestData> snapshot;
    auto* items = chest ? reinterpret_cast<Il2CppArray*>(objectAt(chest, g_chest.items)) : nullptr;
    if (!g_chest.ok || !items) return snapshot;

    for (uintptr_t i = 0; i < items->length; ++i) {
        const ChestSlot slot = slotOf(items, static_cast<int>(i));
        snapshot.push_back({slot.type(), chestData(slot.data, slot.type()),
            chestRarity(slot.data, slot.type())});
        if (erase) setChestData(slot.data, 0, {});
    }

    return snapshot;
}

void restoreChestData(Il2CppObject* chest, const std::vector<ChestData>& snapshot) {
    auto* items = chest ? reinterpret_cast<Il2CppArray*>(objectAt(chest, g_chest.items)) : nullptr;
    if (!g_chest.ok || !items) return;

    for (uintptr_t i = 0; i < items->length; ++i) {
        const ChestSlot slot = slotOf(items, static_cast<int>(i));
        const bool matching = i < snapshot.size() && slot.type() == snapshot[i].type;
        setChestData(slot.data, slot.type(), matching ? snapshot[i].data : "");
        setChestRarity(slot.data, slot.type(), matching ? snapshot[i].rarityKey : "");
    }
}

using FillChestFn = void (*)(Il2CppObject*, const MethodInfo*);
FillChestFn g_origFillChest = nullptr;
void hkFillChest(Il2CppObject* chest, const MethodInfo* method) {
    snapshotChest(chest, true);
    g_origFillChest(chest, method);
    snapshotChest(chest, true);
}

using ResizeChestFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
ResizeChestFn g_origResizeChest = nullptr;
void hkResizeChest(Il2CppObject* chest, int32_t size, const MethodInfo* method) {
    const auto snapshot = snapshotChest(chest, true);
    g_origResizeChest(chest, size, method);
    restoreChestData(chest, snapshot);
}

using CloneChestFn = Il2CppObject* (*)(Il2CppObject*, const MethodInfo*);
CloneChestFn g_origCloneChest = nullptr;
Il2CppObject* hkCloneChest(Il2CppObject* chest, const MethodInfo* method) {
    const auto snapshot = snapshotChest(chest, false);
    Il2CppObject* copy = g_origCloneChest(chest, method);
    restoreChestData(copy, snapshot);

    return copy;
}

std::string chestName(Il2CppObject* chest) {
    return "chest:" + std::to_string(field<int32_t>(chest, g_chest.x)) + "," +
           std::to_string(field<int32_t>(chest, g_chest.y));
}

/** Cada bau do mundo e o array de ChestItem dele. */
template <typename Fn>
void forEachChest(Fn fn) {
    Il2CppArray* chests = nullptr;
    il2cpp::api().field_static_get_value(g_chest.chests, &chests);
    if (!chests) return;
    for (uintptr_t i = 0; i < chests->length; ++i) {
        Il2CppObject* chest = static_cast<Il2CppObject**>(arrayData(chests))[i];
        auto* items = chest ? reinterpret_cast<Il2CppArray*>(objectAt(chest, g_chest.items)) : nullptr;
        if (items) fn(chest, items);
    }
}

std::string activeWorldPath() {
    Il2CppObject* data = nullptr;
    il2cpp::api().field_static_get_value(g_chest.activeWorld, &data);
    return localPath(data);
}

using SaveChestsFn = int32_t (*)(Il2CppObject*, const MethodInfo*);
SaveChestsFn g_origSaveChests = nullptr;

/** ANTES do original: e exatamente o que ele vai escrever no .wld. */
int32_t hkSaveChests(Il2CppObject* writer, const MethodInfo* m) {
    const std::string path = activeWorldPath();
    if (!path.empty()) {
        std::vector<SavedItem> items;
        forEachChest([&](Il2CppObject* chest, Il2CppArray* chestItems) {
            for (int i = 0; i < static_cast<int>(chestItems->length); ++i) {
                const ChestSlot s = slotOf(chestItems, i);
                if (s.type() <= 0 || s.stack() <= 0) continue;
                if (s.type() < kVanillaItemCount) {
                    const auto stable = chestRarity(s.data, s.type());
                    const bool customPrefix = s.prefix() >= vanillaPrefixCount() &&
                        !modPrefixKey(s.prefix()).empty();
                    if (customPrefix || !stable.empty()) {
                        SavedItem record = vanillaPrefixEntry(chestName(chest), i, s.type(),
                            customPrefix ? s.prefix() : 0);
                        record.rarityKey = stable;
                        items.push_back(std::move(record));
                    }
                    continue;
                }
                std::string key = modItemKey(s.type());
                if (key.empty()) continue;
                SavedItem it{chestName(chest), i, s.stack(), 0, s.favorited() != 0, std::move(key), {}};
                encodePrefix(it, s.prefix());
                it.data = chestData(s.data, s.type());
                it.rarityKey = chestRarity(s.data, s.type());
                if (s.prefix() == 0) it.prefixKey = pendingPrefixFor(path, it.container, i, s.type());
                items.push_back(std::move(it));
            }
        });
        size_t kept = 0;
        {
            std::lock_guard<std::mutex> l(g_mx);
            auto it = g_kept.find(path);
            if (it != g_kept.end()) {
                std::map<std::string, Il2CppArray*> byName;
                forEachChest([&](Il2CppObject* chest, Il2CppArray* a) { byName[chestName(chest)] = a; });
                for (const SavedItem& k : it->second) {
                    SavedItem remaining = k;
                    if (isVanillaPrefixEntry(k)) {
                        auto c = byName.find(k.container);
                        const ChestSlot slot = c == byName.end() ?
                            ChestSlot{} : slotOf(c->second, k.slot);
                        if (!slot.data || slot.type() != vanillaPrefixEntryType(k)) continue;
                        remaining = item_save::remainingVanillaMetadata(k,
                            slot.prefix(), chestRarity(slot.data, slot.type()));
                        if (!item_save::hasRemainingVanillaMetadata(remaining)) continue;
                    }
                    if (!item_save::mergePending(items, remaining))
                        items.push_back(std::move(remaining));
                    ++kept;
                }
            }
        }
        const bool sidecarSaved = writeFile(path + kSuffix, items);
        if (!items.empty() && sidecarSaved) {
            BL_INFO("save de itens de mod: mundo: %zu item(ns) em bau gravado(s) (%zu so no arquivo)",
                    items.size(), kept);
        }
    }
    return g_origSaveChests(writer, m);
}

using FixFn = void (*)(const MethodInfo*);
FixFn g_origFix = nullptr;

/**
 * ANTES do original: e ele que le ContentSamples[tipo] de cada item de bau, e
 * lancava KeyNotFound — o mundo nem abria — quando o mod do item nao estava.
 */
void hkFixAgainstExploits(const MethodInfo* m) {
    const std::string path = activeWorldPath();
    const std::vector<SavedItem> saved =
        path.empty() ? std::vector<SavedItem>{} : readFile(path + kSuffix);
    std::vector<SavedItem> kept;
    int restored = 0, unloaded = 0, stray = 0, prefixes = 0;
    std::map<std::string, PendingPrefix> pending;

    std::map<std::string, Il2CppArray*> byName;
    forEachChest([&](Il2CppObject* chest, Il2CppArray* items) { byName[chestName(chest)] = items; });
    // Lugares que o arquivo explicou; o resto com tipo de mod e numero cru.
    std::map<std::string, std::vector<bool>> covered;

    for (const SavedItem& s : saved) {
        auto it = byName.find(s.container);
        if (isVanillaPrefixEntry(s)) {
            // Item do jogo com prefixo de mod: o bau so guarda o numero, que e trocado.
            const ChestSlot slot = it == byName.end() ? ChestSlot{} : slotOf(it->second, s.slot);
            if (!slot.data || slot.type() != vanillaPrefixEntryType(s)) continue;
            std::vector<bool>& cov = covered[s.container];
            cov.resize(it->second->length);
            cov[static_cast<size_t>(s.slot)] = true;
            const int prefix = decodePrefix(s);
            if (item_save::hasSavedVanillaPrefix(s))
                slot.prefix() = static_cast<uint8_t>(prefix > 0 ? prefix : 0);
            setChestRarity(slot.data, slot.type(), s.rarityKey);
            if (prefix > 0) ++prefixes;
            else if (!s.prefixKey.empty()) kept.push_back(s);
            continue;
        }
        const int type = it == byName.end() ? -1 : typeForKey(s.key);
        const ChestSlot slot = type < 0 ? ChestSlot{} : slotOf(it->second, s.slot);
        // Item do JOGO no lugar: o bau foi mexido sem o loader, e ele fica.
        const int current = slot.data ? slot.type() : -1;
        if (!slot.data || (current > 0 && current < kVanillaItemCount)) {
            kept.push_back(s);
            continue;
        }
        slot.type() = static_cast<int16_t>(type);
        slot.stack() = static_cast<int16_t>(s.stack > 0 ? s.stack : 1);
        const int prefix = decodePrefix(s);
        slot.prefix() = static_cast<uint8_t>(prefix > 0 ? prefix : 0);
        slot.favorited() = s.favorited ? 1 : 0;
        setChestData(slot.data, type, s.data);
        setChestRarity(slot.data, type, s.rarityKey);
        if (prefix < 0 && !s.prefixKey.empty()) pending[placeKey(s.container, s.slot)] = {type, s.prefixKey};
        std::vector<bool>& cov = covered[s.container];
        cov.resize(it->second->length);
        cov[static_cast<size_t>(s.slot)] = true;
        ++restored;
        if (isUnloadedType(type)) ++unloaded;
    }
    forEachChest([&](Il2CppObject* chest, Il2CppArray* items) {
        auto cov = covered.find(chestName(chest));
        for (int i = 0; i < static_cast<int>(items->length); ++i) {
            const bool explained = cov != covered.end() && cov->second[static_cast<size_t>(i)];
            const ChestSlot s = slotOf(items, i);
            if (!explained && isStrayType(s.type())) {
                s.type() = 0;
                s.stack() = 0;
                s.prefix() = 0;
                s.favorited() = 0;
                ++stray;
            } else if (!explained && isStrayPrefix(s.prefix())) {
                // Numero de prefixo de mod sem linha no arquivo: de outra sessao.
                s.prefix() = 0;
                ++stray;
            }
        }
    });
    if (!path.empty()) {
        std::lock_guard<std::mutex> l(g_mx);
        if (kept.empty()) g_kept.erase(path);
        else g_kept[path] = kept;
        if (pending.empty()) g_pendingPrefix.erase(path);
        else g_pendingPrefix[path] = std::move(pending);
    }
    if (!saved.empty() || stray) {
        BL_INFO("save de itens de mod: mundo: %d item(ns) em bau reposto(s) (%d como \"?\"), %d "
                "prefixo(s) de mod em item do jogo, %zu so no arquivo, %d numero(s) invalido(s) "
                "limpo(s)", restored, unloaded, prefixes, kept.size(), stray);
    }
    g_origFix(m);
}

bool resolveChests() {
    using namespace il2cpp;
    auto& a = api();
    ChestLayout& c = g_chest;
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    Il2CppClass* chest = findClass({"Terraria", "Chest", {}});
    Il2CppClass* chestItem = findClass({"Terraria", "ChestItem", {}});
    if (!main || !chest || !chestItem) return false;
    c.chests = findField(main, "chest");
    c.activeWorld = findField(main, "ActiveWorldFileData");
    c.items = fieldOffset(chest, "item");
    c.x = fieldOffset(chest, "x");
    c.y = fieldOffset(chest, "y");
    uint32_t align = 0;
    c.stride = static_cast<size_t>(a.class_value_size(chestItem, &align));
    // Campo de struct: o IL2CPP conta o cabecalho de objeto (a struct em caixa).
    const int32_t header = static_cast<int32_t>(sizeof(Il2CppObject));
    const int32_t t = fieldOffset(chestItem, "type"), s = fieldOffset(chestItem, "stack"),
                  p = fieldOffset(chestItem, "prefix"), f = fieldOffset(chestItem, "favorited");
    if (t < header || s < header || p < header || f < header) return false;
    c.type = static_cast<size_t>(t - header);
    c.stack = static_cast<size_t>(s - header);
    c.prefix = static_cast<size_t>(p - header);
    c.favorited = static_cast<size_t>(f - header);
    c.ok = c.chests && c.activeWorld && c.items >= 0 && c.x >= 0 && c.y >= 0 && c.stride > 0 &&
           c.type + 2 <= c.stride && c.stack + 2 <= c.stride && c.prefix < c.stride &&
           c.favorited < c.stride;
    BL_DEBUG("save de itens de mod: ChestItem de %zu bytes (tipo +%zu, pilha +%zu, prefixo +%zu, "
             "favorito +%zu)", c.stride, c.type, c.stack, c.prefix, c.favorited);
    return c.ok;
}

// ------------------------------ o "?" ------------------------------

using SetDefaultsFn = void (*)(Il2CppObject*, int32_t, Il2CppObject*, const MethodInfo*);
SetDefaultsFn g_origSetDefaults = nullptr;

/**
 * O "?" nao tem mod, entao nao passa pelo hook JS dos itens de mod; e o
 * SetDefaults do jogo zera tipo acima de ItemID.Count. Um desvio nativo so
 * para ele, que o resto repassa sem custo.
 */
void hkSetDefaults(Il2CppObject* self, int32_t type, Il2CppObject* variant, const MethodInfo* m) {
    if (isUnloadedType(type)) {
        setupUnloadedItem(self, type);
        return;
    }
    g_origSetDefaults(self, type, variant, m);
}


bool resolve() {
    using namespace il2cpp;
    Refs& r = g_refs;
    Il2CppClass* fileData = findClass({"Terraria.IO", "FileData", {}});
    Il2CppClass* playerFileData = findClass({"Terraria.IO", "PlayerFileData", {}});
    Il2CppClass* player = findClass({"Terraria", "Player", {}});
    Il2CppClass* item = findClass({"Terraria", "Item", {}});
    Il2CppClass* storage = findClass({"Terraria", "InventoryStorage", {}});
    Il2CppClass* loadout = findClass({"Terraria", "EquipmentLoadout", {}});
    if (!fileData || !playerFileData || !player || !item || !storage || !loadout) return false;

    r.filePath = fieldOffset(fileData, "_path");
    r.fileCloud = fieldOffset(fileData, "_isCloudSave");
    r.filePlayer = fieldOffset(playerFileData, "_player");
    r.inventory = fieldOffset(player, "inventory");
    r.armor = fieldOffset(player, "armor");
    r.dye = fieldOffset(player, "dye");
    r.miscEquips = fieldOffset(player, "miscEquips");
    r.miscDyes = fieldOffset(player, "miscDyes");
    r.trash = fieldOffset(player, "trashItem");
    r.loadouts = fieldOffset(player, "Loadouts");
    const char* banks[4] = {"bank", "bank2", "bank3", "bank4"};
    for (int i = 0; i < 4; ++i) r.bank[i] = fieldOffset(player, banks[i]);
    r.storageItems = fieldOffset(storage, "item");
    r.loadoutArmor = fieldOffset(loadout, "Armor");
    r.loadoutDye = fieldOffset(loadout, "Dye");
    r.type = fieldOffset(item, "type");
    r.stack = fieldOffset(item, "stack");
    r.prefix = fieldOffset(item, "prefix");
    r.favorited = fieldOffset(item, "favorited");
    auto* rareField = il2cpp::findField(item, "rare");
    if (rareField && il2cpp::api().field_get_type &&
        il2cpp::api().class_from_il2cpp_type &&
        il2cpp::api().class_get_name && il2cpp::api().class_get_namespace &&
        il2cpp::api().field_get_flags &&
        !(il2cpp::api().field_get_flags(rareField) & 0x10u)) {
        auto* type = il2cpp::api().class_from_il2cpp_type(
            il2cpp::api().field_get_type(rareField));
        const char* ns = type ? il2cpp::api().class_get_namespace(type) : nullptr;
        const char* name = type ? il2cpp::api().class_get_name(type) : nullptr;
        if (ns && name && std::strcmp(ns, "System") == 0 &&
            std::strcmp(name, "Int32") == 0)
            r.rare = fieldOffset(item, "rare");
    }
    r.setDefaults = findMethodBySignature(item, parseSignature("void SetDefaults(int Type, ItemVariant variant)"));
    r.applyPrefix = findMethodBySignature(item, parseSignature("bool Prefix(int prefixWeWant)"));
    r.applyPrefixDetailed = findMethodBySignature(item, parseSignature(
        "bool Prefix(int prefixWeWant, out bool rolledPrefixIsTopTier)"));
    r.resetPrefix = findMethodBySignature(item, parseSignature("void ResetPrefix()"));
    BL_INFO("save de itens de mod: Item.Prefix com out bool=%d",
            r.applyPrefixDetailed ? 1 : 0);

    // Conjuntos (Loadouts), cofres e prefixo sao opcionais: sem eles, so
    // aquele lugar fica de fora. O resto e o minimo para nao perder item.
    r.ok = r.filePath >= 0 && r.fileCloud >= 0 && r.filePlayer >= 0 && r.inventory >= 0 &&
           r.type >= 0 && r.stack >= 0 && r.prefix >= 0 && r.favorited >= 0 && r.setDefaults;
    return r.ok;
}

} // namespace

std::string modChestItemData(Il2CppObject* chest, int slot) {
    if (!g_chest.ok || !chest) return {};

    auto* items = reinterpret_cast<Il2CppArray*>(objectAt(chest, g_chest.items));
    const ChestSlot value = items ? slotOf(items, slot) : ChestSlot{};

    return value.data ? chestData(value.data, value.type()) : std::string();
}

bool setModChestItemData(Il2CppObject* chest, int slot, const std::string& data) {
    if (!g_chest.ok || !chest) return false;

    auto* items = reinterpret_cast<Il2CppArray*>(objectAt(chest, g_chest.items));
    const ChestSlot value = items ? slotOf(items, slot) : ChestSlot{};
    if (!value.data) return false;

    setChestData(value.data, value.type(), data);

    return true;
}

int pendingManagedPrefixCountForActivePlayer() {
    auto& api = il2cpp::api();
    Il2CppClass* main = il2cpp::findClass({"Terraria", "Main", {}});
    const MethodInfo* getter = main
        ? api.class_get_method_from_name(main, "get_ActivePlayerFileData", 0)
        : nullptr;
    if (!getter) return 0;
    Il2CppObject* exception = nullptr;
    Il2CppObject* data = api.runtime_invoke(getter, nullptr, nullptr, &exception);
    if (exception || !data) return 0;
    const std::string path = localPath(data);
    if (path.empty()) return 0;
    std::lock_guard<std::mutex> lock(g_mx);
    auto it = g_pendingPrefix.find(path);
    if (it == g_pendingPrefix.end()) return 0;
    int unresolved = 0;
    for (const auto& [slot, prefix] : it->second) {
        const int id = modPrefixByKey(prefix.key);
        if (id > 0 && (bl::csharp::managedPrefixDescriptorReady(id) ||
                       bl::csharp::managedPrefixDescriptorPending(id)))
            ++unresolved;
    }
    return unresolved;
}

void retryPendingManagedPrefixesOnWorldReady() {
    retryPendingManagedPrefixesInWorld();
    retryPendingManagedRaritiesInWorld();
}

void installModItemSave() {
    if (!resolve()) {
        BL_ERROR("save de itens de mod: refs faltando; itens de mod somem ao sair do mundo");
        return;
    }
    Il2CppClass* player = il2cpp::findClass({"Terraria", "Player", {}});
    const MethodInfo* save = il2cpp::findMethodBySignature(
        player, il2cpp::parseSignature("void InternalSavePlayerFile(PlayerFileData playerFile)"));
    const MethodInfo* load = il2cpp::findMethodBySignature(
        player, il2cpp::parseSignature("PlayerFileData LoadPlayer(string playerPath, bool cloudSave)"));
    const bool okSave = save && hook::install(save, hkInternalSavePlayerFile, &g_origSave);
    const bool okLoad = load && hook::install(load, hkLoadPlayer, &g_origLoad);
    if (!okSave || !okLoad) {
        BL_ERROR("save de itens de mod: sem hook (save=%d load=%d); itens de mod somem ao sair "
                 "do mundo", okSave, okLoad);
        return;
    }

    if (!hook::install(g_refs.setDefaults, hkSetDefaults, &g_origSetDefaults)) {
        BL_ERROR("save de itens de mod: sem hook em Item.SetDefaults; item \"?\" nao funciona");
    }

    Il2CppClass* worldFile = il2cpp::findClass({"Terraria.IO", "WorldFile", {}});
    const MethodInfo* saveChests = worldFile ? il2cpp::findMethodBySignature(
        worldFile, il2cpp::parseSignature("int SaveChests(BinaryWriter writer)")) : nullptr;
    const MethodInfo* fix = worldFile ? il2cpp::api().class_get_method_from_name(
        worldFile, "FixAgainstExploits", 0) : nullptr;
    const bool okWorld = resolveChests() && saveChests && fix &&
                         hook::install(saveChests, hkSaveChests, &g_origSaveChests) &&
                         hook::install(fix, hkFixAgainstExploits, &g_origFix);
    if (!okWorld) {
        BL_ERROR("save de itens de mod: sem os baus do mundo; item de mod em bau fica pelo "
                 "numero, e sem o mod o mundo nao abre");
    }
    Il2CppClass* chestItem = il2cpp::findClass({"Terraria", "ChestItem", {}});
    Il2CppClass* chest = il2cpp::findClass({"Terraria", "Chest", {}});
    const auto method = [&](Il2CppClass* cls, const char* signature) {
        return cls ? il2cpp::findMethodBySignature(cls, il2cpp::parseSignature(signature)) : nullptr;
    };
    const bool okData = okWorld &&
        hook::install(method(chestItem, "void SetToItem(Item item)"), hkChestSet, &g_origChestSet) &&
        hook::install(method(chestItem, "Item ExpandItem()"), hkChestExpand, &g_origChestExpand) &&
        hook::install(method(chestItem, "void Clear()"), hkChestClear, &g_origChestClear) &&
        hook::install(method(chestItem, "void TurnToAir()"), hkChestAir, &g_origChestAir) &&
        hook::install(method(chestItem, "void SetDefaults(int id)"), hkChestDefaults, &g_origChestDefaults) &&
        hook::install(method(chestItem, "void netDefaults(int type)"), hkChestNetDefaults, &g_origChestNetDefaults) &&
        hook::install(method(chest, "void FillWithEmptyInstances()"), hkFillChest, &g_origFillChest) &&
        hook::install(method(chest, "void Resize(int newSize)"), hkResizeChest, &g_origResizeChest) &&
        hook::install(method(chest, "Chest CloneWithSeparateItems()"), hkCloneChest, &g_origCloneChest) &&
        hook::install(method(chest, "void Clear()"), hkClearChests, &g_origClearChests);
    if (!okData) BL_ERROR("ModItem: hooks de dados de bau incompletos");
    BL_DEBUG("save de itens de mod: pronto (personagem%s)", okWorld ? " e baus do mundo" : "");
}

} // namespace bl::runtime
