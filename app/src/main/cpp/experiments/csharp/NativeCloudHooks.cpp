#include "experiments/csharp/NativeCloudHooks.h"

#include "experiments/csharp/NativeCloudAssets.h"
#include "experiments/csharp/NativeCloudHookBindings.h"
#include "experiments/csharp/NativeCloudWeightWire.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <thread>

namespace bl::csharp {
namespace {

using AddCloudFn = void (*)(const MethodInfo*);
using RollRareFn = int32_t (*)(const MethodInfo*);

NativeCloudHookBindings g_fields{};
const NativeCloudRegistry* g_registry = nullptr;
NativeCloudRollCoordinator g_roll{};
NativeCloudSpawnLedger g_spawn{};
NativeCloudSpawnAttempt g_attempt{};
AddCloudFn g_originalAdd = nullptr;
RollRareFn g_originalRare = nullptr;
std::thread::id g_gameThread;
bool g_enabled = false;
bool g_insideAdd = false;
std::map<std::string, std::vector<int>> g_managedCloudTypes;
std::set<std::string> g_pendingWeightMods;
std::vector<NativeCloudWeight> g_stagedWeights;
bool g_collectingWeights = false;
int g_weightAge = 0;
int g_weightRetryCooldown = 0;
uint64_t g_weightGeneration = 0;


// Cheap native PRNG. Each roll consumes one 53-bit fraction in [0,1).
// This never calls MonoVM, JNI or JS from Terraria's Cloud.addCloud hook.
uint64_t g_random = 0;
double random01() {
    if (!g_random) {
        g_random = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        if (!g_random) g_random = 0x9E3779B97F4A7C15ULL;
    }
    uint64_t value = (g_random += 0x9E3779B97F4A7C15ULL);
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    value ^= value >> 31;
    return static_cast<double>(value >> 11) * (1.0 / 9007199254740992.0);
}

bool gameThread() {
    return std::this_thread::get_id() == g_gameThread;
}

Il2CppArray* clouds() {
    if (!g_fields.mainCloudArray) return nullptr;
    Il2CppArray* value = nullptr;
    il2cpp::api().field_static_get_value(g_fields.mainCloudArray, &value);
    return value && value->length >= NativeCloudSpawnLedger::kCloudSlots
        ? value : nullptr;
}

Il2CppObject* at(Il2CppArray* values, int slot) {
    if (!values || slot < 0 || slot >= NativeCloudSpawnLedger::kCloudSlots)
        return nullptr;
    return static_cast<Il2CppObject**>(arrayData(values))[slot];
}

template<typename T>
T& field(Il2CppObject* cloud, int offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(cloud) + offset);
}

int freeSlot() {
    Il2CppArray* array = clouds();
    if (!array) return -1;
    for (int slot = 0; slot < NativeCloudSpawnLedger::kCloudSlots; ++slot) {
        Il2CppObject* cloud = at(array, slot);
        if (cloud && !field<uint8_t>(cloud, g_fields.active))
            return slot;
    }
    return -1;
}

bool cloudIsValid(const NativeCloudRegistry::Entry* entry) {
    return entry && entry->ready && entry->width > 0 && entry->height > 0 &&
        nativeCloudAssetsInstalled();
}

void consumeOriginalAdd(const MethodInfo* method) {
    if (g_originalAdd) g_originalAdd(method);
}

int32_t hkRollRareCloud(const MethodInfo* method) {
    if (!g_originalRare) return 0;
    if (!g_enabled || !g_insideAdd || !gameThread())
        return g_originalRare(method);

    uint8_t anniversary = 0;
    il2cpp::api().field_static_get_value(
        g_fields.tenthAnniversaryWorld, &anniversary);
    const int chosen = g_roll.chooseRare(random01(), anniversary != 0);
    const auto* entry = g_registry ? g_registry->find(chosen) : nullptr;
    const int type = cloudIsValid(entry) ? chosen : 0;
    g_attempt.rareRolled(type, type > 0 ? freeSlot() : -1);
    return type > 0 ? type : g_originalRare(method);
}

void hkAddCloud(const MethodInfo* method) {
    if (!g_originalAdd) return;
    if (!g_enabled || !gameThread() || g_insideAdd ||
        !nativeCloudAssetsInstalled()) {
        consumeOriginalAdd(method);
        return;
    }

    // The two hooks share state only on the Terraria game thread; the nested
    // RollRareCloud hook wins over any earlier common-cloud roll.
    g_insideAdd = true;
    const int candidate = g_roll.chooseCommon(random01());
    const auto* proposed = g_registry ? g_registry->find(candidate) : nullptr;
    const int common = cloudIsValid(proposed) ? candidate : 0;
    g_attempt.begin(common, common ? freeSlot() : -1);

    consumeOriginalAdd(method);
    g_insideAdd = false;

    const int slot = g_attempt.candidateSlot();
    Il2CppObject* cloud = at(clouds(), slot);
    if (!cloud || !field<uint8_t>(cloud, g_fields.active)) return;

    const int actualType = field<int32_t>(cloud, g_fields.type);
    const auto spawned = g_attempt.result(true, actualType);
    if (spawned.type <= 0 || spawned.slot != slot) return;
    const auto* entry = g_registry ? g_registry->find(spawned.type) : nullptr;
    if (!cloudIsValid(entry)) return;

    float scale = field<float>(cloud, g_fields.scale);
    if (!std::isfinite(scale) || scale <= 0.f || scale > 10.f)
        return;

    if (spawned.replaceType) {
        // Vanilla positioned the cloud first. Copy only the mod type and
        // dimensions; leave its AI, position, velocity and brightness alone.
        // Floats are bounded so integer conversion cannot overflow.
        field<int32_t>(cloud, g_fields.type) = spawned.type;
        field<int32_t>(cloud, g_fields.width) =
            static_cast<int32_t>(entry->width * scale);
        field<int32_t>(cloud, g_fields.height) =
            static_cast<int32_t>(entry->height * scale);
    }

    // Texture-only clouds do not declare OnSpawn, so they do not need
    // MonoVM jobs. Avoid even the queue write for those clouds.
    const auto owner = g_managedCloudTypes.find(entry->modId);
    if (owner != g_managedCloudTypes.end() &&
        std::find(owner->second.begin(), owner->second.end(), spawned.type)
            != owner->second.end()) {
        const int sprite = field<int32_t>(cloud, g_fields.spriteEffects);
        g_spawn.push(spawned.type, spawned.slot, scale, sprite);
    }
}

bool fail(std::string* error, const char* detail) {
    if (error) *error = detail;
    return false;
}

} // namespace

bool installNativeCloudHooks(
    const NativeCloudRegistry* registry,
    const std::map<std::string, std::vector<int>>& managedCloudIds,
    bool monoWorkerConfirmed, std::string* error) {
    if (error) error->clear();
    if (!registry || registry->count() == 0 || !monoWorkerConfirmed ||
        !nativeCloudAssetsInstalled())
        return fail(error, "Stage=Install Reason=CloudMasksOrMonoWorkerNotReady");

    for (int type = registry->firstType();
         type < registry->firstType() + static_cast<int>(registry->count());
         ++type)
        if (!cloudIsValid(registry->find(type)))
            return fail(error, "Stage=Install Reason=CloudEntryNotDrawable");

    // Reject an incomplete C# registration map before touching any hook.
    // Texture-only IDs must NOT be included in the managed ID set.
    std::set<int> seen;
    for (const auto& [mod, ids] : managedCloudIds) {
        if (ids.empty()) return fail(error, "Stage=Install Reason=EmptyManagedCloudOwner");
        if (!managedCloudRegistrationReady(mod))
            return fail(error, "Stage=Install Reason=MonoCloudClassNotBound");
        for (int type : ids) {
            const auto* entry = registry->find(type);
            if (!entry || entry->modId != mod || !seen.insert(type).second)
                return fail(error, "Stage=Install Reason=CloudIdOwnerMismatch");
        }
    }
    NativeCloudHookBindings bindings{};
    if (!resolveNativeCloudHookBindings(&bindings, error)) return false;
    if (!g_originalRare &&
        !hook::install(bindings.rollRareCloud, hkRollRareCloud, &g_originalRare))
        return fail(error, "Stage=Install Reason=RollRareCloudHookRejected");
    if (!g_originalAdd &&
        !hook::install(bindings.addCloud, hkAddCloud, &g_originalAdd))
        return fail(error, "Stage=Install Reason=AddCloudHookRejected");

    // A partial hook installation is safe because the callbacks above
    // always delegate to vanilla while g_enabled is false.
    g_registry = registry;
    g_managedCloudTypes = managedCloudIds;
    g_pendingWeightMods.clear();
    g_stagedWeights.clear();
    g_collectingWeights = false;
    g_weightAge = 0;
    g_weightRetryCooldown = 0;
    ++g_weightGeneration;
    if (g_weightGeneration == 0) ++g_weightGeneration;
    g_fields = bindings;
    g_gameThread = std::this_thread::get_id();
    g_insideAdd = false;
    g_roll.clear();
    g_spawn.clear();
    g_enabled = true;
    return true;
}

bool publishNativeCloudWeights(const std::vector<NativeCloudWeight>& weights) {
    return g_enabled && gameThread() && g_registry &&
        g_roll.publish(*g_registry, weights);
}

bool nativeCloudWeightsRefreshNeeded() {
    return g_enabled && gameThread() && g_roll.refreshRequested();
}

bool takeNativeCloudSpawnOnGameThread(NativeCloudSpawnLedger::Event* event) {
    return g_enabled && gameThread() && event && g_spawn.take(event);
}

bool applyNativeCloudSpriteResultOnGameThread(
    const NativeCloudSpawnLedger::Event& event, int spriteDir) {
    if (!g_enabled || !gameThread() || !nativeCloudAssetsInstalled())
        return false;
    Il2CppObject* cloud = at(clouds(), event.slot);
    if (!cloud) return false;
    const bool active = field<uint8_t>(cloud, g_fields.active) != 0;
    const int type = field<int32_t>(cloud, g_fields.type);
    const float scale = field<float>(cloud, g_fields.scale);
    // A mod's OnSpawn response must not overwrite a newer game-owned
    // sprite direction, even when type and scale still match.
    if (field<int32_t>(cloud, g_fields.spriteEffects) != event.spriteEffects ||
        !g_spawn.stillCurrent(event, active, type, scale, spriteDir))
        return false;
    // The original ExampleAdvancedCloud.OnSpawn writes cloud.spriteDir.
    field<int32_t>(cloud, g_fields.spriteEffects) = spriteDir;
    return true;
}

// Called from Terraria's game UPDATE, never from Cloud.addCloud or a
// rendering hook. Mono callbacks are dispatched asynchronously through
// ManagedRuntime's already-existing pinned worker and never block frames.
void pumpNativeCloudManagedCallbacksOnGameThread(
    bool gameMenu, bool tenthAnniversaryWorld, const std::string& biomeKeys) {
    if (!g_enabled || !gameThread() || !g_registry ||
        !nativeCloudAssetsInstalled()) return;

    // A typical frame has zero cloud events. Drain at most eight each frame
    // to keep a pathological mod from monopolizing the update budget.
    NativeCloudSpawnLedger::Event event{};
    for (int i = 0; i < 8 && g_spawn.take(&event); ++i) {
        const auto* entry = g_registry->find(event.type);
        if (!entry || !cloudIsValid(entry)) continue;
        queueManagedCloudOnSpawn(entry->modId, event.type, event.slot,
                                 event.generation, event.scale,
                                 event.spriteEffects);
    }

    ManagedCloudSpawnReply reply{};
    for (int i = 0; i < 8 && takeManagedCloudOnSpawnReply(&reply); ++i) {
        const NativeCloudSpawnLedger::Event original{
            reply.type, reply.slot, reply.generation, reply.scale,
            reply.originalSpriteDir
        };
        applyNativeCloudSpriteResultOnGameThread(original, reply.spriteDir);
    }

    // SpawnChance is sampled away from addCloud. A pending batch is collected
    // from ALL mods before replacing the active weighted pools; a partial
    // or malformed response can never change another mod's probabilities.
    if (!g_collectingWeights) {
        if (!g_roll.refreshRequested() || biomeKeys.size() > 4096)
            return;
        if (g_weightRetryCooldown > 0) {
            --g_weightRetryCooldown;
            return;
        }

        // Do not assume a class has been registered merely because its
        // source was compiled. Every binding must be ACKed by Mono first.
        for (const auto& [mod, ids] : g_managedCloudTypes)
            if (!managedCloudRegistrationReady(mod)) return;

        g_stagedWeights.clear();
        g_pendingWeightMods.clear();
        for (int i = 0; i < static_cast<int>(g_registry->count()); ++i) {
            const auto* entry = g_registry->find(g_registry->firstType() + i);
            if (!entry) continue;
            const auto classes = g_managedCloudTypes.find(entry->modId);
            const bool managed = classes != g_managedCloudTypes.end() &&
                std::find(classes->second.begin(), classes->second.end(),
                          entry->type) != classes->second.end();
            // A PNG-only cloud is exactly SimpleModCloud with weight 1.
            if (!managed) g_stagedWeights.push_back({entry->type, 1});
        }
        for (const auto& [mod, ids] : g_managedCloudTypes)
            g_pendingWeightMods.insert(mod);
        g_collectingWeights = true;
        g_weightAge = 0;
        ++g_weightGeneration;
        if (g_weightGeneration == 0) ++g_weightGeneration;
    }

    // A worker may be busy; re-asking is cheap and nonblocking, and the
    // ManagedRuntime coalesces repeated requests per mod.
    std::vector<std::string> received;
    for (const std::string& mod : g_pendingWeightMods) {
        std::string payload;
        if (!takeManagedCloudWeights(mod, g_weightGeneration, &payload)) {
            requestManagedCloudWeights(mod, gameMenu,
                                       tenthAnniversaryWorld, biomeKeys,
                                       g_weightGeneration);
            continue;
        }
        std::string error;
        if (!appendNativeCloudWeightWire(
                *g_registry, mod, payload, &g_stagedWeights, &error)) {
            // Keep the previous complete pool. No partial publication.
            g_collectingWeights = false;
            g_pendingWeightMods.clear();
            g_stagedWeights.clear();
            g_weightRetryCooldown = 120;
            return;
        }
        received.push_back(mod);
    }
    for (const auto& mod : received) g_pendingWeightMods.erase(mod);
    if (!g_pendingWeightMods.empty()) {
        // A timed-out batch must not keep the game thread waiting and must
        // never publish incomplete weights. Vanilla remains available.
        if (++g_weightAge > 180) {
            g_collectingWeights = false;
            g_pendingWeightMods.clear();
            g_stagedWeights.clear();
            g_weightRetryCooldown = 120;
        }
        return;
    }

    std::sort(g_stagedWeights.begin(), g_stagedWeights.end(),
              [](const NativeCloudWeight& a, const NativeCloudWeight& b) {
                  return a.type < b.type;
              });
    if (!g_roll.publish(*g_registry, g_stagedWeights))
        g_weightRetryCooldown = 120;
    g_stagedWeights.clear();
    g_collectingWeights = false;
}

void pauseNativeCloudHooks() {
    g_enabled = false;
    g_insideAdd = false;
    g_registry = nullptr;
    g_managedCloudTypes.clear();
    g_pendingWeightMods.clear();
    g_stagedWeights.clear();
    g_collectingWeights = false;
    g_weightAge = 0;
    g_weightRetryCooldown = 0;
    ++g_weightGeneration;
    if (g_weightGeneration == 0) ++g_weightGeneration;
    g_roll.clear();
    g_spawn.clear();
}

} // namespace bl::csharp
