#include "experiments/csharp/NativeBiomeGameBridge.h"

#include "experiments/csharp/NativeBiomeManifest.h"
#include "experiments/csharp/NativeCloudBiomeSampler.h"
#include "experiments/csharp/ManagedRuntime.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace bl::csharp {
namespace {

struct Owner {
    std::vector<NativeBiomeDescriptor> classes;
    std::vector<std::string> tiles;
    NativeCloudBiomeFacts snapshot{};
    uint64_t generation = 1;
    int age = 0;
    bool needsRequest = false;
    bool requested = false;
    bool tilesValid = false;
    std::vector<std::string> activeKeys;
    std::string tileSnapshot;
};

std::map<std::string, Owner> g_owners;
std::thread::id g_gameThread{};
bool g_started = false;

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

bool sameManifest(std::vector<NativeBiomeDescriptor> first,
                  std::vector<NativeBiomeDescriptor> second) {
    auto order = [](const NativeBiomeDescriptor& x,
                    const NativeBiomeDescriptor& y) {
        return x.modId == y.modId
            ? x.className < y.className : x.modId < y.modId;
    };
    std::sort(first.begin(), first.end(), order);
    std::sort(second.begin(), second.end(), order);
    if (first.size() != second.size()) return false;
    for (size_t i = 0; i < first.size(); ++i) {
        auto& a = first[i];
        auto& b = second[i];
        if (a.modId != b.modId || a.className != b.className)
            return false;
        std::sort(a.tileNames.begin(), a.tileNames.end());
        std::sort(b.tileNames.begin(), b.tileNames.end());
        if (a.tileNames != b.tileNames) return false;
    }
    return true;
}

bool gameThread() {
    return !g_started || std::this_thread::get_id() == g_gameThread;
}

} // namespace

bool stageNativeBiomeOwnerOnGameThread(const std::string& modId,
    const std::string& compiledManifest, std::string* error) {
    if (error) error->clear();
    if (!gameThread() || g_owners.size() >= 16 ||
        g_owners.count(modId) || !managedBiomeRegistrationReady(modId))
        return fail(error, "BiomeBridge: owner or Mono registration not ready");

    std::vector<NativeBiomeDescriptor> compiled, managed;
    if (!appendNativeBiomeManifest(modId, compiledManifest,
                                   &compiled, error))
        return false;

    std::string managedManifest;
    if (!takeManagedBiomeManifest(modId, &managedManifest) ||
        !appendNativeBiomeManifest(modId, managedManifest,
                                   &managed, error) ||
        !sameManifest(compiled, managed))
        return fail(error, "BiomeBridge: compiled versus loaded class mismatch");

    Owner owner;
    owner.classes = std::move(compiled);
    std::set<std::string> tiles;
    for (const auto& entry : owner.classes)
        tiles.insert(entry.tileNames.begin(), entry.tileNames.end());
    if (tiles.size() > 32)
        return fail(error, "BiomeBridge: tile dependency limit exceeded");
    owner.tiles.assign(tiles.begin(), tiles.end());
    if (!g_started) {
        g_started = true;
        g_gameThread = std::this_thread::get_id();
    }
    g_owners.emplace(modId, std::move(owner));
    return true;
}

bool pumpNativeBiomeOwnersOnGameThread(const NativeCloudBiomeFacts& facts,
    bool newSnapshot, std::string* biomeKeys) {
    if (!biomeKeys || !gameThread()) return false;
    if (!g_started || g_owners.empty()) return true;

    for (auto& [mod, owner] : g_owners) {
        if (newSnapshot) {
            ++owner.generation;
            if (owner.generation == 0) ++owner.generation;
            owner.activeKeys.clear();
            owner.snapshot = facts;
            owner.age = 0;
            owner.requested = false;
            owner.needsRequest = true;
            std::string counts;
            std::string error;
            owner.tilesValid = facts.playerActive && !facts.gameMenu &&
                readNativeBiomeTileCountsWire(
                    mod, owner.tiles, &counts, &error);
            owner.snapshot.exampleBiomeRegistered = false;
            owner.snapshot.exampleBiomeTileCount = -1;
            if (owner.tilesValid)
                owner.tileSnapshot = std::move(counts);
        }
        if (owner.needsRequest && !owner.requested) {
            std::string stale;
            takeManagedBiomeSnapshot(mod, owner.generation, &stale);
        }
        if (owner.needsRequest && !owner.requested &&
            owner.tilesValid && managedBiomeRegistrationReady(mod)) {
            if (requestManagedBiomeSnapshot(mod, facts.gameMenu,
                    facts.playerActive, owner.snapshot.playerPositionX,
                    owner.snapshot.maxTilesX, owner.snapshot.zoneSkyHeight,
                    owner.snapshot.zoneOverworldHeight,
                    owner.tileSnapshot, owner.generation)) {
                owner.requested = true;
                owner.needsRequest = false;
            }
        }
        if (owner.requested) {
            std::string result;
            if (takeManagedBiomeSnapshot(mod, owner.generation, &result)) {
                std::vector<std::string> keys;
                std::string error;
                if (appendNativeBiomeActiveKeys(owner.classes, mod, result,
                        facts.gameMenu, &keys, &error))
                    owner.activeKeys = std::move(keys);
                owner.requested = false;
            } else if (++owner.age > 180) {
                owner.requested = false;
                owner.needsRequest = false;
                owner.activeKeys.clear();
            }
        }
        for (const auto& key : owner.activeKeys) {
            if (biomeKeys->size() + key.size() + 1 > 4096)
                return false;
            if (!biomeKeys->empty()) biomeKeys->push_back(';');
            *biomeKeys += key;
        }
    }
    return true;
}

bool nativeBiomeOwnersStagedOnGameThread() {
    return gameThread() && g_started && !g_owners.empty();
}

void resetNativeBiomeWorldStateOnGameThread() {
    if (!gameThread()) return;
    for (auto& [mod, owner] : g_owners) {
        ++owner.generation;
        if (!owner.generation) ++owner.generation;
        owner.needsRequest = false;
        owner.requested = false;
        owner.tilesValid = false;
        owner.age = 0;
        owner.activeKeys.clear();
        owner.tileSnapshot.clear();
    }
}

void clearNativeBiomeOwnersOnGameThread() {
    if (!gameThread()) return;
    g_owners.clear();
}

} // namespace bl::csharp
