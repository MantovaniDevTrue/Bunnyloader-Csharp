#include "experiments/csharp/ManagedGoreRuntime.h"
#include "experiments/csharp/ManagedGoreCatalog.h"

#include "content/common/ContentAssets.h"
#include "content/common/TypeTables.h"
#include "core/Log.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp::gore {
namespace {

constexpr int kMaxTypeExclusive = 65536;
constexpr size_t kMaxQueue = 128;
constexpr size_t kMaxSpawnPerFrame = 32;
using Key = std::pair<std::string, std::string>;
using TypeIndex = std::map<Key, int>;

struct Vec2 { float x, y; };
struct Spawn {
    int type = 0;
    Vec2 position{}, velocity{};
    float scale = 1.f;
    uint64_t generation = 0;
};
struct NativeEntry {
    Entry entry;
    uint32_t root = 0;
    NativeEntry(const Entry& value, uint32_t handle) : entry(value), root(handle) {}
};
struct Tables {
    FieldInfo* texture = nullptr;
    FieldInfo* special = nullptr;
    FieldInfo* speed = nullptr;
    FieldInfo* speedAlpha = nullptr;
    FieldInfo* drip = nullptr;
    FieldInfo* safe = nullptr;
    const MethodInfo* setValue = nullptr;
    const MethodInfo* newGore = nullptr;
    bool ready = false;
};

Tables g_tables;
std::mutex g_writer;
std::vector<NativeEntry> g_entries;
std::shared_ptr<const TypeIndex> g_types = std::make_shared<const TypeIndex>();
std::mutex g_queueMutex;
std::deque<Spawn> g_queue;
std::atomic<uint64_t> g_generation{1};
std::atomic<bool> g_worldReady{false};
uint64_t g_frame = 0;
int g_highWater = 0;

Il2CppArray* staticArray(FieldInfo* field) {
    Il2CppArray* value = nullptr;
    if (field) il2cpp::api().field_static_get_value(field, &value);
    return value;
}

bool initializedStaticArray(FieldInfo* field, const char* expected) {
    auto& a = il2cpp::api();
    if (!field || !a.field_get_flags || !(a.field_get_flags(field) & 0x10u) ||
        (a.field_get_flags(field) & 0x40u)) return false;
    Il2CppArray* array = staticArray(field);
    if (!array || array->length == 0 || array->length > kMaxTypeExclusive)
        return false;
    Il2CppClass* cls = a.object_get_class(reinterpret_cast<Il2CppObject*>(array));
    if (!cls || a.class_get_rank(cls) != 1) return false;
    Il2CppClass* element = a.class_get_element_class(cls);
    if (!element) return false;
    if (!expected) return element == runtime::content::textureAssetClass();
    const char* ns = a.class_get_namespace(element);
    const char* name = a.class_get_name(element);
    if (!ns || std::strcmp(ns, "System") != 0 || !name) return false;
    if (std::strcmp(expected, "Boolean") == 0)
        return std::strcmp(name, "Boolean") == 0;
    return std::strcmp(name, "Byte") == 0 ||
           std::strcmp(name, "Int32") == 0 ||
           std::strcmp(name, "Single") == 0;
}

bool init(std::string* error) {
    if (g_tables.ready) return true;
    auto& a = il2cpp::api();
    auto* assets = il2cpp::findClass({"Terraria.GameContent", "TextureAssets", {}});
    auto* goreId = il2cpp::findClass({"Terraria.ID", "GoreID", {}});
    auto* sets = goreId ? il2cpp::findNested(goreId, "Sets") : nullptr;
    auto* safety = il2cpp::findClass({"Terraria.GameContent", "ChildSafety", {}});
    auto* gore = il2cpp::findClass({"Terraria", "Gore", {}});
    auto* array = il2cpp::findClass({"System", "Array", {}});
    if (a.runtime_class_init) {
        for (auto* c : {assets, sets, safety})
            if (c) a.runtime_class_init(c);
    }
    Tables next;
    next.texture = assets ? il2cpp::findField(assets, "Gore") : nullptr;
    next.special = sets ? il2cpp::findField(sets, "SpecialAI") : nullptr;
    next.speed = sets ? il2cpp::findField(sets, "DisappearSpeed") : nullptr;
    next.speedAlpha = sets ? il2cpp::findField(sets, "DisappearSpeedAlpha") : nullptr;
    next.drip = sets ? il2cpp::findField(sets, "IsDrip") : nullptr;
    next.safe = safety ? il2cpp::findField(safety, "SafeGore") : nullptr;
    next.setValue = array ? il2cpp::findMethodBySignature(array,
        il2cpp::parseSignature("void SetValue(object value, int index)")) : nullptr;
    next.newGore = gore ? il2cpp::findMethodBySignature(gore,
        il2cpp::parseSignature("int NewGore(Vector2 Position, Vector2 Velocity, int Type, float Scale)")) : nullptr;
    if (!initializedStaticArray(next.texture, nullptr) ||
        !initializedStaticArray(next.special, "Numeric") ||
        !initializedStaticArray(next.speed, "Numeric") ||
        !initializedStaticArray(next.speedAlpha, "Numeric") ||
        !initializedStaticArray(next.drip, "Boolean") ||
        !initializedStaticArray(next.safe, "Boolean") ||
        !next.setValue || !next.newGore ||
        !a.method_is_instance || a.method_is_instance(next.newGore)) {
        *error = "tabelas GoreID/TextureAssets ou NewGore incompativeis com Terraria Android";
        return false;
    }
    next.ready = true;
    g_tables = next;
    return true;
}

bool setObject(Il2CppArray* array, int index, Il2CppObject* object) {
    if (!array || index < 0 || static_cast<uintptr_t>(index) >= array->length)
        return false;
    void* args[2] = {object, &index};
    Il2CppObject* exception = nullptr;
    il2cpp::api().runtime_invoke(g_tables.setValue, array, args, &exception);
    return exception == nullptr;
}

bool writeInt(Il2CppArray* arr, int index, int number) {
    if (!arr || index < 0 || static_cast<uintptr_t>(index) >= arr->length)
        return false;
    auto& api = il2cpp::api();
    auto* elem = api.class_get_element_class(api.object_get_class(
        reinterpret_cast<Il2CppObject*>(arr)));
    if (!elem) return false;
    const char* name = api.class_get_name(elem);
    if (!name) return false;
    if (!std::strcmp(name, "Byte") || !std::strcmp(name, "Boolean")) {
        static_cast<uint8_t*>(arrayData(arr))[index] = static_cast<uint8_t>(number);
        return true;
    }
    if (!std::strcmp(name, "Int32")) {
        static_cast<int32_t*>(arrayData(arr))[index] = number;
        return true;
    }
    if (!std::strcmp(name, "Single")) {
        static_cast<float*>(arrayData(arr))[index] = static_cast<float>(number);
        return true;
    }
    return false;
}

std::array<FieldInfo*, 6> fields() {
    return {g_tables.texture, g_tables.special, g_tables.speed,
            g_tables.speedAlpha, g_tables.drip, g_tables.safe};
}

struct PinnedArrays {
    std::array<uint32_t, 6> handles{};
    ~PinnedArrays() {
        auto& api = il2cpp::api();
        for (uint32_t handle : handles)
            if (handle) api.gchandle_free(handle);
    }
    bool pin(size_t index, Il2CppArray* array) {
        if (!array) return false;
        auto& api = il2cpp::api();
        handles[index] = api.gchandle_new(
            reinterpret_cast<Il2CppObject*>(array), false);
        return handles[index] != 0;
    }
};

void destroyUncommitted(const std::vector<uint32_t>& roots) {
    auto& a = il2cpp::api();
    for (auto handle : roots) {
        if (!handle) continue;
        if (auto* asset = a.gchandle_get_target(handle))
            runtime::content::destroyTextureAsset(asset);
        a.gchandle_free(handle);
    }
}

bool applyDefaults(std::array<Il2CppArray*, 6>& arrays, int start,
                   const std::vector<Entry>& entries,
                   const std::vector<uint32_t>& roots) {
    auto& a = il2cpp::api();
    for (size_t i = 0; i < entries.size(); ++i) {
        const int type = start + static_cast<int>(i);
        Il2CppObject* asset = a.gchandle_get_target(roots[i]);
        if (!asset || !setObject(arrays[0], type, asset) ||
            !writeInt(arrays[1], type, 0) ||
            !writeInt(arrays[2], type, 1) ||
            !writeInt(arrays[3], type, 1) ||
            !writeInt(arrays[4], type, 0) ||
            !writeInt(arrays[5], type, 0))
            return false;
    }
    return true;
}

bool repairOnGameThread() {
    if (!g_tables.ready || g_entries.empty()) return true;
    const auto f = fields();
    std::array<Il2CppArray*, 6> previous{}, candidate{};
    PinnedArrays pins;
    bool changed = false;
    for (size_t i = 0; i < f.size(); ++i) {
        previous[i] = staticArray(f[i]);
        if (!previous[i]) return false;
        candidate[i] = previous[i];
        if (previous[i]->length < static_cast<uintptr_t>(g_highWater)) {
            candidate[i] = runtime::TypeTables::resizedCopy(
                previous[i], static_cast<uintptr_t>(g_highWater));
            if (!candidate[i] || !pins.pin(i, candidate[i])) return false;
            changed = true;
        }
    }
    if (!changed) return true;
    for (const auto& record : g_entries) {
        const int type = record.entry.type;
        if (!setObject(candidate[0], type,
                il2cpp::api().gchandle_get_target(record.root)) ||
            !writeInt(candidate[1], type, 0) ||
            !writeInt(candidate[2], type, 1) ||
            !writeInt(candidate[3], type, 1) ||
            !writeInt(candidate[4], type, 0) ||
            !writeInt(candidate[5], type, 0))
            return false;
    }
    for (size_t i = 0; i < f.size(); ++i)
        if (candidate[i] != previous[i])
            il2cpp::api().field_static_set_value(f[i], candidate[i]);
    BL_INFO("csharp GoreLoader: tabelas nativas refeitas; %zu gores preservadas", g_entries.size());
    return true;
}

bool callSpawn(const Spawn& request) {
    if (!g_tables.ready || !g_tables.newGore) return false;
    Vec2 position = request.position;
    Vec2 velocity = request.velocity;
    int type = request.type;
    float scale = request.scale;
    void* args[] = {&position, &velocity, &type, &scale};
    Il2CppObject* exception = nullptr;
    il2cpp::api().runtime_invoke(g_tables.newGore, nullptr, args, &exception);
    return exception == nullptr;
}

bool validText(const char* input, size_t max) {
    if (!input) return false;
    const size_t n = strnlen(input, max + 1);
    return n > 0 && n <= max;
}

}

bool install(const std::string& modId, const std::vector<Source>& sources,
             std::string* error) {
    if (!error) return false;
    error->clear();
    if (sources.empty()) return true;
    std::lock_guard guard(g_writer);
    if (!init(error)) return false;
    Catalog batch;
    if (!batch.stage(modId, sources, error)) return false;
    const auto f = fields();
    std::array<Il2CppArray*, 6> previous{}, expanded{};
    PinnedArrays pins;
    int first = std::max(1, g_highWater);
    for (size_t i = 0; i < f.size(); ++i) {
        previous[i] = staticArray(f[i]);
        if (!previous[i]) {
            *error = "campo nativo de gore sem array";
            return false;
        }
        first = std::max(first, static_cast<int>(previous[i]->length));
    }
    if (sources.size() > static_cast<size_t>(kMaxTypeExclusive - first) ||
        !batch.allocate(first, kMaxTypeExclusive, error)) return false;
    const int end = batch.endType();
    auto current = std::atomic_load_explicit(&g_types, std::memory_order_acquire);
    for (const auto& entry : batch.entries())
        if (current->contains({entry.modId, entry.source.name})) {
            *error = "gore ja registrado: " + entry.modId + "/" + entry.source.name;
            return false;
        }
    for (size_t i = 0; i < f.size(); ++i) {
        expanded[i] = runtime::TypeTables::resizedCopy(previous[i],
            static_cast<uintptr_t>(end));
        if (!expanded[i] || !pins.pin(i, expanded[i])) {
            *error = "nao foi possivel aumentar/fixar array nativo de gore no GC";
            return false;
        }
    }
    std::vector<uint32_t> roots;
    roots.reserve(sources.size());
    auto& api = il2cpp::api();
    for (const auto& entry : batch.entries()) {
        std::vector<unsigned char> png;
        if (!readOwnedPng(entry.source, &png, error)) {
            destroyUncommitted(roots);
            return false;
        }
        int width = 0, height = 0;
        // Decodifica a copia validada, sem reabrir um caminho alteravel.
        Il2CppObject* asset = runtime::content::loadTextureAsset(
            entry.source.absolutePath, png.data(), png.size(),
            entry.modId + "/Gores/" + entry.source.name, &width, &height);
        if (!asset || width != static_cast<int>(entry.source.width) ||
            height != static_cast<int>(entry.source.height)) {
            if (asset) runtime::content::destroyTextureAsset(asset);
            destroyUncommitted(roots);
            *error = "PNG nao decodificado ou tamanho alterado: " +
                     entry.source.relativePath;
            return false;
        }
        const uint32_t root = api.gchandle_new(asset, false);
        if (!root) {
            runtime::content::destroyTextureAsset(asset);
            destroyUncommitted(roots);
            *error = "nao foi possivel manter raiz GC de gore";
            return false;
        }
        roots.push_back(root);
    }
    if (!applyDefaults(expanded, first, batch.entries(), roots)) {
        destroyUncommitted(roots);
        *error = "falha ao preencher tabelas tipadas de gore";
        return false;
    }
    // As seis tabelas so sao publicadas apos todas as texturas terem sido
    // decodificadas e todas as entradas terem sido preenchidas com sucesso.
    for (size_t i = 0; i < f.size(); ++i)
        api.field_static_set_value(f[i], expanded[i]);
    auto next = std::make_shared<TypeIndex>(*current);
    for (size_t i = 0; i < batch.entries().size(); ++i) {
        const auto& entry = batch.entries()[i];
        g_entries.emplace_back(entry, roots[i]);
        next->emplace(Key{entry.modId, entry.source.name}, entry.type);
    }
    g_highWater = end;
    std::atomic_store_explicit(&g_types,
        std::shared_ptr<const TypeIndex>(next), std::memory_order_release);
    BL_INFO("csharp GoreLoader %s: %zu gores nativas registradas (%d..%d)",
            modId.c_str(), sources.size(), first, end - 1);
    return true;
}

int typeOf(const std::string& modId, const std::string& name) {
    const auto view = std::atomic_load_explicit(&g_types, std::memory_order_acquire);
    const auto it = view->find({modId, name});
    return it == view->end() ? 0 : it->second;
}

void worldBoundary() {
    g_worldReady.store(false, std::memory_order_release);
    g_generation.fetch_add(1, std::memory_order_acq_rel);
    std::lock_guard lock(g_queueMutex);
    g_queue.clear();
}

void worldReady() {
    g_worldReady.store(true, std::memory_order_release);
}

void pumpOnGameThread() {
    if (!g_worldReady.load(std::memory_order_acquire)) return;
    if (++g_frame % 20 == 0 && !repairOnGameThread()) {
        static unsigned failures = 0;
        if (failures++ < 3)
            BL_WARN("csharp GoreLoader: falha ao recuperar tabela nativa");
    }
    std::array<Spawn, kMaxSpawnPerFrame> requests{};
    size_t count = 0;
    {
        std::lock_guard lock(g_queueMutex);
        while (count < requests.size() && !g_queue.empty()) {
            requests[count++] = g_queue.front();
            g_queue.pop_front();
        }
    }
    const uint64_t current = g_generation.load(std::memory_order_acquire);
    for (size_t i = 0; i < count; ++i) {
        if (requests[i].generation == current && !callSpawn(requests[i]))
            BL_WARN("csharp GoreLoader: NewGore recusou evento nativo type=%d",
                    requests[i].type);
    }
}

extern "C" __attribute__((visibility("default")))
int bunny_csharp_gore_type(const char* modId, const char* name) {
    if (!validText(modId, 128) || !validText(name, 180)) return 0;
    return typeOf(modId, name);
}

extern "C" __attribute__((visibility("default")))
int bunny_csharp_gore_spawn(const char* modId, const char* name, float x, float y,
                           float vx, float vy, float scale) {
    if (!validText(modId, 128) || !validText(name, 180) ||
        !g_worldReady.load(std::memory_order_acquire) ||
        !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(vx) || !std::isfinite(vy) ||
        !std::isfinite(scale) || scale < 0.1f || scale > 4.f ||
        std::abs(x) > 100000000.f || std::abs(y) > 100000000.f ||
        std::abs(vx) > 10000.f || std::abs(vy) > 10000.f)
        return 0;
    const int type = typeOf(modId, name);
    if (type <= 0) return 0;
    const uint64_t generation = g_generation.load(std::memory_order_acquire);
    std::lock_guard lock(g_queueMutex);
    if (g_queue.size() >= kMaxQueue ||
        !g_worldReady.load(std::memory_order_acquire) ||
        g_generation.load(std::memory_order_acquire) != generation)
        return 0;
    g_queue.push_back({type, {x, y}, {vx, vy}, scale, generation});
    return 1;
}

}
