#include "experiments/csharp/NativeGlobalItems.h"

#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/NativeDamageClassPlayerStats.h"
#include "experiments/csharp/NativeDefaults.h"
#include "experiments/csharp/NativeGlobalItemWire.h"
#include "content/common/GameRefs.h"
#include "content/items/ModItems.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace bl::csharp::global_items {
namespace {

using runtime::field;

// Mesma lista (e ordem) de BunnyVanillaItem.Fields no SDK C#.
struct SnapshotField { const char* name; const char* type; size_t width; };
constexpr SnapshotField kFields[] = {
    {"damage", "System.Int32", 4}, {"crit", "System.Int32", 4}, {"knockBack", "System.Single", 4},
    {"useTime", "System.Int32", 4}, {"useAnimation", "System.Int32", 4}, {"useStyle", "System.Int32", 4},
    {"melee", "System.Boolean", 1}, {"ranged", "System.Boolean", 1}, {"magic", "System.Boolean", 1},
    {"summon", "System.Boolean", 1}, {"accessory", "System.Boolean", 1}, {"vanity", "System.Boolean", 1},
    {"consumable", "System.Boolean", 1}, {"maxStack", "System.Int32", 4}, {"value", "System.Int32", 4},
    {"rare", "System.Int32", 4}, {"defense", "System.Int32", 4}, {"shoot", "System.Int32", 4},
    {"shootSpeed", "System.Single", 4}, {"ammo", "System.Int32", 4}, {"useAmmo", "System.Int32", 4},
    {"pick", "System.Int32", 4}, {"axe", "System.Int32", 4}, {"hammer", "System.Int32", 4},
    {"mana", "System.Int32", 4}, {"autoReuse", "System.Boolean", 1}, {"width", "System.Int32", 4},
    {"height", "System.Int32", 4}, {"healLife", "System.Int32", 4}, {"healMana", "System.Int32", 4},
    {"noMelee", "System.Boolean", 1}, {"potion", "System.Boolean", 1}, {"useTurn", "System.Boolean", 1},
    {"tileBoost", "System.Int32", 4}, {"createTile", "System.Int32", 4},
};

enum class State { Declared, Requested, Delivered, Published, Failed };

struct ModState {
    std::string assembly;
    std::vector<std::string> names;
    State state = State::Declared;
    std::string wire;
    std::string snapshotPath;
    std::chrono::steady_clock::time_point requestedAt{};
    int requestAttempts = 0;
};

struct Policy {
    std::string owner;
    NativeDefaultsBindings defaults;
    TypeWire wire;
};
using PolicyTable = std::map<int, std::vector<Policy>>;

std::mutex g_mutex;
std::map<std::string, ModState> g_mods;
std::shared_ptr<const PolicyTable> g_policies = std::make_shared<const PolicyTable>();
std::atomic<bool> g_anyPolicies{false};
thread_local bool t_snapshotting = false;

struct Refs {
    bool resolved = false, ok = false;
    Il2CppClass* item = nullptr;
    const MethodInfo* ctor = nullptr;
    const MethodInfo* setDefaults = nullptr;
    const MethodInfo* prefix = nullptr;
    const MethodInfo* localPlayer = nullptr;
    int offsets[sizeof(kFields) / sizeof(kFields[0])]{};
    int type = -1, stack = -1, prefixField = -1, favorited = -1, itemDamage = -1;
    int flags[4]{-1, -1, -1, -1};          // melee ranged magic summon
    int playerDamage[4]{-1, -1, -1, -1};   // meleeDamage rangedDamage magicDamage minionDamage
};
Refs g_refs;

Refs& refs() {
    auto& r = g_refs;
    if (r.resolved) return r;
    r.resolved = true;
    auto& a = il2cpp::api();
    r.item = il2cpp::findClass({"Terraria", "Item", {}});
    auto* player = il2cpp::findClass({"Terraria", "Player", {}});
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    if (!r.item || !player || !main || !a.object_new || !a.runtime_invoke) return r;
    r.ctor = a.class_get_method_from_name ? a.class_get_method_from_name(r.item, ".ctor", 0) : nullptr;
    r.setDefaults = il2cpp::findMethodBySignature(r.item, il2cpp::parseSignature("void SetDefaults(int Type, ItemVariant variant)"));
    r.prefix = il2cpp::findMethodBySignature(r.item, il2cpp::parseSignature("bool Prefix(int pre, bool topTier)"));
    r.localPlayer = il2cpp::findMethodBySignature(main, il2cpp::parseSignature("Player get_LocalPlayer()"));
    std::string error;
    for (size_t i = 0; i < sizeof(kFields) / sizeof(kFields[0]); ++i) {
        r.offsets[i] = -1;
        il2cpp::nativeFieldContract(r.item, kFields[i].name, kFields[i].type, false, kFields[i].width,
                                    nullptr, &r.offsets[i], &error);
    }
    auto bind = [&](Il2CppClass* owner, const char* name, const char* type, size_t width, int* out) {
        if (!il2cpp::nativeFieldContract(owner, name, type, false, width, nullptr, out, &error)) *out = -1;
    };
    bind(r.item, "type", "System.Int32", 4, &r.type);
    bind(r.item, "stack", "System.Int32", 4, &r.stack);
    bind(r.item, "prefix", "System.Byte", 1, &r.prefixField);
    bind(r.item, "favorited", "System.Boolean", 1, &r.favorited);
    bind(r.item, "damage", "System.Int32", 4, &r.itemDamage);
    const char* flagNames[] = {"melee", "ranged", "magic", "summon"};
    const char* damageNames[] = {"meleeDamage", "rangedDamage", "magicDamage", "minionDamage"};
    for (int i = 0; i < 4; ++i) {
        bind(r.item, flagNames[i], "System.Boolean", 1, &r.flags[i]);
        bind(player, damageNames[i], "System.Single", 4, &r.playerDamage[i]);
    }
    r.ok = r.ctor && r.setDefaults && r.type >= 0;
    if (!r.ok)
        BL_WARN("[CSharp][GlobalItem] Stage=Resolve Result=Missing Ctor=%d SetDefaults=%d Type=%d",
                r.ctor ? 1 : 0, r.setDefaults ? 1 : 0, r.type >= 0 ? 1 : 0);
    return r;
}

std::shared_ptr<const PolicyTable> policies() {
    return std::atomic_load_explicit(&g_policies, std::memory_order_acquire);
}

// ---- foto dos itens vanilla ----

struct SnapshotBuild {
    bool done = false;
    int cursor = 1;
    std::string text;
};
SnapshotBuild g_snapshot;

void appendValue(std::string& out, Il2CppObject* object, const SnapshotField& f, int offset) {
    char buffer[48];
    if (offset < 0) { out += "0"; return; }
    if (f.width == 1) out += field<uint8_t>(object, offset) ? "1" : "0";
    else if (std::strcmp(f.type, "System.Single") == 0) {
        std::snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(field<float>(object, offset)));
        out += buffer;
    } else {
        std::snprintf(buffer, sizeof(buffer), "%d", field<int32_t>(object, offset));
        out += buffer;
    }
}

// Avança a foto em blocos para não travar um quadro do menu.
bool stepSnapshot() {
    auto& r = refs();
    if (!r.ok) return false;
    auto& s = g_snapshot;
    if (s.done) return true;
    if (s.text.empty()) {
        s.text = "BUNNY_ITEM_SNAPSHOT_V1\ttype";
        for (size_t i = 0; i < sizeof(kFields) / sizeof(kFields[0]); ++i)
            if (r.offsets[i] >= 0) (s.text += '\t') += kFields[i].name;
        s.text += '\n';
    }
    auto& a = il2cpp::api();
    Il2CppObject* scratch = a.object_new(r.item);
    if (!scratch) return false;
    Il2CppObject* error = nullptr;
    a.runtime_invoke(r.ctor, scratch, nullptr, &error);
    if (error) return false;
    const int end = std::min(s.cursor + 512, runtime::kVanillaItemCount);
    t_snapshotting = true;
    for (; s.cursor < end; ++s.cursor) {
        int type = s.cursor;
        void* args[] = {&type, nullptr};
        error = nullptr;
        a.runtime_invoke(r.setDefaults, scratch, args, &error);
        if (error || field<int32_t>(scratch, r.type) != type) continue;
        s.text += '\t';
        s.text += std::to_string(type);
        for (size_t i = 0; i < sizeof(kFields) / sizeof(kFields[0]); ++i) {
            if (r.offsets[i] < 0) continue;
            s.text += '\t';
            appendValue(s.text, scratch, kFields[i], r.offsets[i]);
        }
        s.text += '\n';
    }
    t_snapshotting = false;
    // Cada linha começa com TAB: a coluna 0 é o marcador do cabeçalho e a 1 é "type".
    if (s.cursor >= runtime::kVanillaItemCount) s.done = true;
    return s.done;
}

std::string snapshotPathFor(const std::string& assembly) {
    const size_t slash = assembly.find_last_of('/');
    return (slash == std::string::npos ? std::string(".") : assembly.substr(0, slash)) +
           "/global-items-snapshot.txt";
}

bool writeSnapshot(const std::string& path) {
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(g_snapshot.text.data(), static_cast<std::streamsize>(g_snapshot.text.size()));
        if (!out) return false;
    }
    return std::rename(temp.c_str(), path.c_str()) == 0;
}

// ---- publicação ----

void refreshLiveItems(const std::set<int>& types);

bool publishMod(const std::string& mod, const std::string& wire, std::string* error) {
    WireTable table;
    if (!parseWire(wire, &table, error)) return false;
    auto& r = refs();
    std::vector<damage_combat::ClassBonusRow> bonusRows;
    std::map<int, Policy> incoming;
    for (auto& [type, entry] : table) {
        if (type >= runtime::kVanillaItemCount) { *error = "NotVanillaType:" + std::to_string(type); return false; }
        Policy policy;
        policy.owner = mod;
        if (!entry.defaults.empty()) {
            NativeDefaultsRequest request{"GlobalItem", mod, "BunnyGlobalItem", entry.defaults, type, false};
            if (!bindNativeDefaults(r.item, request, &policy.defaults, error)) return false;
            for (auto& write : policy.defaults) {
                write.owner = mod;
                if (write.staticSet || write.sound || write.drinkColors) {
                    *error = "UnsupportedGlobalDefault Type=" + std::to_string(type);
                    return false;
                }
            }
        }
        for (const auto& bonus : entry.classBonuses) {
            damage_combat::ClassBonusRow row;
            row.mod = mod;
            row.itemType = type;
            row.classKey = bonus.classKey;
            const float* v = bonus.values;
            row.bonus.damage = {v[0], v[1], v[2], v[3]};
            row.bonus.knockback = {v[4], v[5], v[6], v[7]};
            row.bonus.crit = v[8];
            row.bonus.attackSpeed = v[9];
            row.bonus.armorPenetration = v[10];
            bonusRows.push_back(std::move(row));
        }
        policy.wire = std::move(entry);
        policy.wire.classBonuses.clear();
        incoming.emplace(type, std::move(policy));
    }
    if (!bonusRows.empty() && !damage_combat::classBonuses().publish(mod, bonusRows, error)) return false;
    std::set<int> touched;
    {
        std::lock_guard guard(g_mutex);
        auto next = std::make_shared<PolicyTable>(*policies());
        for (auto& [type, list] : *next)
            list.erase(std::remove_if(list.begin(), list.end(), [&](const Policy& p) { return p.owner == mod; }), list.end());
        for (auto& [type, policy] : incoming) {
            if (!policy.defaults.empty()) touched.insert(type);
            (*next)[type].push_back(std::move(policy));
        }
        for (auto it = next->begin(); it != next->end();) it = it->second.empty() ? next->erase(it) : std::next(it);
        g_anyPolicies.store(!next->empty(), std::memory_order_release);
        std::atomic_store_explicit(&g_policies, std::shared_ptr<const PolicyTable>(std::move(next)), std::memory_order_release);
    }
    if (!touched.empty()) refreshLiveItems(touched);
    BL_INFO("[CSharp][GlobalItem] Mod=%s Stage=Publish Items=%zu Defaults=%zu ClassBonuses=%zu Result=Ready",
            mod.c_str(), incoming.size(), touched.size(), bonusRows.size());
    return true;
}

// Refaz os itens vivos dos tipos alterados (inventários e amostras), como o
// tModLoader faz ao recarregar: SetDefaults com a política nova, preservando
// pilha, favorito e prefixo.
void refreshLiveItems(const std::set<int>& types) {
    auto& r = refs();
    auto& a = il2cpp::api();
    if (!r.ok) return;
    int refreshed = 0;
    auto redo = [&](Il2CppObject* object) {
        if (!object) return;
        int type = field<int32_t>(object, r.type);
        if (!types.contains(type)) return;
        const int stack = r.stack >= 0 ? field<int32_t>(object, r.stack) : 1;
        const bool favorited = r.favorited >= 0 && field<uint8_t>(object, r.favorited);
        int pre = r.prefixField >= 0 ? field<uint8_t>(object, r.prefixField) : 0;
        void* args[] = {&type, nullptr};
        Il2CppObject* error = nullptr;
        a.runtime_invoke(r.setDefaults, object, args, &error);
        if (error) return;
        if (r.stack >= 0) field<int32_t>(object, r.stack) = stack;
        if (r.favorited >= 0) field<uint8_t>(object, r.favorited) = favorited;
        if (pre > 0 && r.prefix) {
            bool topTier = false;
            void* prefixArgs[] = {&pre, &topTier};
            error = nullptr;
            a.runtime_invoke(r.prefix, object, prefixArgs, &error);
        }
        ++refreshed;
    };
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* player = il2cpp::findClass({"Terraria", "Player", {}});
    FieldInfo* playersField = main ? il2cpp::findField(main, "player") : nullptr;
    Il2CppArray* players = nullptr;
    if (playersField && a.field_static_get_value) a.field_static_get_value(playersField, &players);
    if (players && player && players->length <= 256) {
        auto** list = static_cast<Il2CppObject**>(arrayData(players));
        for (uintptr_t i = 0; i < players->length; ++i) {
            if (!list[i]) continue;
            for (const char* name : {"inventory", "armor", "dye", "miscEquips", "miscDyes"}) {
                const int offset = il2cpp::fieldOffset(player, name);
                auto* items = offset >= 0 ? field<Il2CppArray*>(list[i], offset) : nullptr;
                if (!items || items->length > 10000) continue;
                auto** objects = static_cast<Il2CppObject**>(arrayData(items));
                for (uintptr_t j = 0; j < items->length; ++j) redo(objects[j]);
            }
        }
    }
    // Amostras do ContentSamples (tooltip, pesquisa, receitas).
    auto* samples = il2cpp::findClass({"Terraria.ID", "ContentSamples", {}});
    FieldInfo* byType = samples ? il2cpp::findField(samples, "ItemsByType") : nullptr;
    Il2CppObject* dictionary = nullptr;
    if (byType && a.field_static_get_value) a.field_static_get_value(byType, &dictionary);
    const MethodInfo* getter = dictionary && a.class_get_method_from_name
        ? a.class_get_method_from_name(dictionary->klass, "TryGetValue", 2) : nullptr;
    if (getter) {
        for (int type : types) {
            int key = type;
            Il2CppObject* sample = nullptr;
            void* args[] = {&key, &sample};
            Il2CppObject* error = nullptr;
            a.runtime_invoke(getter, dictionary, args, &error);
            if (!error && sample) redo(sample);
        }
    }
    BL_INFO("[CSharp][GlobalItem] Stage=RefreshLiveItems Types=%zu Items=%d", types.size(), refreshed);
}

// ---- consultas ----

const std::vector<Policy>* policiesFor(const PolicyTable& table, int type) {
    const auto it = table.find(type);
    return it == table.end() ? nullptr : &it->second;
}

bool combinedDamage(const std::vector<Policy>& list, Modifier* out) {
    bool any = false;
    Modifier total;
    for (const auto& p : list)
        if (p.wire.hasDamage) {
            any = true;
            total.base += p.wire.damage.base;
            total.additive += p.wire.damage.additive - 1.f;
            total.multiplicative *= p.wire.damage.multiplicative;
            total.flat += p.wire.damage.flat;
        }
    *out = total;
    return any;
}

float classField(Il2CppObject* player, Il2CppObject* item) {
    auto& r = refs();
    if (!player) return 1.f;
    for (int i = 0; i < 4; ++i)
        if (r.flags[i] >= 0 && r.playerDamage[i] >= 0 && field<uint8_t>(item, r.flags[i]))
            return field<float>(player, r.playerDamage[i]);
    return 1.f;
}

// ---- tooltip ----

using LinesFn = void (*)(Il2CppObject*, int32_t*, int32_t*, int32_t*, float, int32_t*, Il2CppArray*,
                         Il2CppArray*, Il2CppArray*, int32_t*, void*, int32_t*, const MethodInfo*);
LinesFn g_lines = nullptr;

std::u16string u16(const Il2CppString* s) {
    if (!s || s->length <= 0 || s->length > 512) return {};
    return std::u16string(s->chars, static_cast<size_t>(s->length));
}

void hkLines(Il2CppObject* item, int32_t* yoyo, int32_t* research, int32_t* materials, float oldKb,
             int32_t* count, Il2CppArray* lines, Il2CppArray* prefix, Il2CppArray* bad,
             int32_t* setBonus, void* setColor, int32_t* shared, const MethodInfo* m) {
    if (g_lines) g_lines(item, yoyo, research, materials, oldKb, count, lines, prefix, bad,
                         setBonus, setColor, shared, m);
    if (!g_anyPolicies.load(std::memory_order_acquire) || !item || !count || !lines || !prefix || !bad) return;
    auto& r = refs();
    if (!r.ok) return;
    const auto table = policies();
    const auto* list = policiesFor(*table, field<int32_t>(item, r.type));
    if (!list) return;
    auto& a = il2cpp::api();
    if (!a.string_new || !a.gc_wbarrier_set_field) return;
    const uintptr_t capacity = std::min({lines->length, prefix->length, bad->length});
    auto** strings = static_cast<Il2CppString**>(arrayData(lines));
    auto* prefixFlags = static_cast<uint8_t*>(arrayData(prefix));
    auto* badFlags = static_cast<uint8_t*>(arrayData(bad));
    // Dano com ModifyWeaponDamage: troca o número da linha de dano vanilla.
    Modifier damage;
    if (combinedDamage(*list, &damage) && r.itemDamage >= 0 && r.localPlayer && *count > 0) {
        Il2CppObject* error = nullptr;
        Il2CppObject* player = a.runtime_invoke(r.localPlayer, nullptr, nullptr, &error);
        const float F = classField(error ? nullptr : player, item);
        const int base = field<int32_t>(item, r.itemDamage);
        const int shown = static_cast<int>(static_cast<float>(base) * F + 5e-06f);
        const int final = weaponDamage(base, F, damage, shown);
        const std::string digits = std::to_string(shown);
        const std::u16string number(digits.begin(), digits.end());
        for (int i = 1; i < *count && static_cast<uintptr_t>(i) < capacity; ++i) {
            const auto text = u16(strings[i]);
            if (text.size() <= number.size() || text.compare(0, number.size(), number) != 0 ||
                (text[number.size()] >= u'0' && text[number.size()] <= u'9')) continue;
            std::string rest;
            for (size_t k = number.size(); k < text.size(); ++k) {
                const char16_t c = text[k];
                if (c < 0x80) rest.push_back(static_cast<char>(c));
                else if (c < 0x800) { rest.push_back(static_cast<char>(0xC0 | (c >> 6))); rest.push_back(static_cast<char>(0x80 | (c & 0x3F))); }
                else { rest.push_back(static_cast<char>(0xE0 | (c >> 12))); rest.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F))); rest.push_back(static_cast<char>(0x80 | (c & 0x3F))); }
            }
            if (Il2CppString* value = a.string_new((std::to_string(final) + rest).c_str()))
                a.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(lines), reinterpret_cast<void**>(&strings[i]), value);
            break;
        }
    }
    for (const auto& policy : *list)
        for (const auto& text : policy.wire.tooltips) {
            if (*count < 0 || static_cast<uintptr_t>(*count) >= capacity) return;
            Il2CppString* value = a.string_new(text.c_str());
            if (!value) return;
            a.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(lines), reinterpret_cast<void**>(&strings[*count]), value);
            prefixFlags[*count] = 0;
            badFlags[*count] = 0;
            ++*count;
        }
}

}  // namespace

void declare(const std::string& mod, const std::string& assemblyPath, std::vector<std::string> names) {
    std::lock_guard guard(g_mutex);
    auto& state = g_mods[mod];
    if (state.assembly != assemblyPath || state.state == State::Failed) {
        state = ModState{};
        state.assembly = assemblyPath;
    }
    state.names = std::move(names);
}

void unload(const std::string& mod) {
    std::lock_guard guard(g_mutex);
    g_mods.erase(mod);
    auto next = std::make_shared<PolicyTable>(*policies());
    for (auto& [type, list] : *next)
        list.erase(std::remove_if(list.begin(), list.end(), [&](const Policy& p) { return p.owner == mod; }), list.end());
    for (auto it = next->begin(); it != next->end();) it = it->second.empty() ? next->erase(it) : std::next(it);
    g_anyPolicies.store(!next->empty(), std::memory_order_release);
    std::atomic_store_explicit(&g_policies, std::shared_ptr<const PolicyTable>(std::move(next)), std::memory_order_release);
}

bool declared(const std::string& mod) {
    std::lock_guard guard(g_mutex);
    return g_mods.contains(mod);
}

void deliver(const std::string& mod, std::string wire) {
    std::lock_guard guard(g_mutex);
    const auto it = g_mods.find(mod);
    if (it == g_mods.end() || it->second.state != State::Requested) return;
    it->second.wire = std::move(wire);
    it->second.state = State::Delivered;
}

void pumpOnGameThread() {
    std::vector<std::pair<std::string, std::string>> toRequest, toPublish;
    {
        std::lock_guard guard(g_mutex);
        if (g_mods.empty()) return;
        const auto now = std::chrono::steady_clock::now();
        for (auto& [mod, state] : g_mods) {
            if (state.state == State::Delivered) {
                toPublish.emplace_back(mod, std::move(state.wire));
                state.wire.clear();
            } else if (state.state == State::Declared ||
                       (state.state == State::Requested && now - state.requestedAt > std::chrono::seconds(20) &&
                        state.requestAttempts < 3)) {
                toRequest.emplace_back(mod, state.assembly);
            }
        }
    }
    for (auto& [mod, wire] : toPublish) {
        std::string error;
        const bool ok = publishMod(mod, wire, &error);
        if (!ok) BL_ERROR("[CSharp][GlobalItem] Mod=%s Stage=Publish Result=Rejected Reason=%s", mod.c_str(), error.c_str());
        std::lock_guard guard(g_mutex);
        if (auto it = g_mods.find(mod); it != g_mods.end()) it->second.state = ok ? State::Published : State::Failed;
    }
    if (toRequest.empty()) return;
    if (!stepSnapshot()) return;
    for (const auto& [mod, unused] : toRequest) {
        const std::string assembly = managedEntityAssemblyPath(mod);
        if (assembly.empty()) continue;  // MonoVM do jogo ainda não registrou o mod
        const std::string path = snapshotPathFor(assembly);
        if (!writeSnapshot(path)) {
            BL_ERROR("[CSharp][GlobalItem] Mod=%s Stage=WriteSnapshot Result=Failed Path=%s", mod.c_str(), path.c_str());
            std::lock_guard guard(g_mutex);
            if (auto it = g_mods.find(mod); it != g_mods.end()) it->second.state = State::Failed;
            continue;
        }
        if (!requestManagedGlobalItems(mod, path)) continue;  // binding ainda não existe: tenta depois
        std::lock_guard guard(g_mutex);
        if (auto it = g_mods.find(mod); it != g_mods.end()) {
            it->second.state = State::Requested;
            it->second.snapshotPath = path;
            it->second.requestedAt = std::chrono::steady_clock::now();
            ++it->second.requestAttempts;
        }
        BL_INFO("[CSharp][GlobalItem] Mod=%s Stage=RequestEvaluation Snapshot=%zuBytes", mod.c_str(), g_snapshot.text.size());
    }
}

bool ready() {
    std::lock_guard guard(g_mutex);
    for (const auto& [mod, state] : g_mods)
        if (state.state != State::Published && state.state != State::Failed) return false;
    return true;
}

void applyDefaults(Il2CppObject* item, int type) {
    if (t_snapshotting || !item || !g_anyPolicies.load(std::memory_order_acquire)) return;
    const auto table = policies();
    const auto* list = policiesFor(*table, type);
    if (!list) return;
    for (const auto& policy : *list)
        for (const auto& write : policy.defaults)
            if (write.offset >= 0 && !write.bytes.empty())
                std::memcpy(reinterpret_cast<char*>(item) + write.offset, write.bytes.data(), write.bytes.size());
}

bool modifyWeaponDamage(Il2CppObject* player, Il2CppObject* item, int32_t* value) {
    if (!item || !value || !g_anyPolicies.load(std::memory_order_acquire)) return false;
    auto& r = refs();
    if (!r.ok || r.itemDamage < 0) return false;
    const auto table = policies();
    const auto* list = policiesFor(*table, field<int32_t>(item, r.type));
    Modifier damage;
    if (!list || !combinedDamage(*list, &damage)) return false;
    *value = weaponDamage(field<int32_t>(item, r.itemDamage), classField(player, item), damage, *value);
    return true;
}

bool modifyWeaponCrit(Il2CppObject* item, int32_t* value) {
    if (!item || !value || !g_anyPolicies.load(std::memory_order_acquire)) return false;
    auto& r = refs();
    if (!r.ok) return false;
    const auto table = policies();
    const auto* list = policiesFor(*table, field<int32_t>(item, r.type));
    if (!list) return false;
    float crit = 0.f;
    for (const auto& p : *list) crit += p.wire.crit;
    if (crit == 0.f) return false;
    *value += static_cast<int32_t>(crit);
    return true;
}

bool modifyWeaponKnockback(Il2CppObject* item, float* value) {
    if (!item || !value || !g_anyPolicies.load(std::memory_order_acquire)) return false;
    auto& r = refs();
    if (!r.ok) return false;
    const auto table = policies();
    const auto* list = policiesFor(*table, field<int32_t>(item, r.type));
    if (!list) return false;
    bool any = false;
    for (const auto& p : *list)
        if (p.wire.hasKnockback) { *value = weaponKnockback(*value, p.wire.knockback); any = true; }
    return any;
}

bool installTooltipHook() {
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    const auto* method = main ? il2cpp::findMethodBySignature(main, il2cpp::parseSignature(
        "void MouseText_DrawItemTooltip_GetLinesInfo(Item item, ref int yoyoLogo, ref int researchLine, ref int materialsLine, float oldKB, ref int numLines, string[] toolTipLine, bool[] preFixLine, bool[] badPreFixLine, ref int setBonusLine, ref Color setBonusColour, ref int sharedLine)")) : nullptr;
    const bool ok = method && hook::install(method, hkLines, &g_lines);
    if (!ok) BL_WARN("[CSharp][GlobalItem] Stage=Hook Method=\"GetLinesInfo\" Result=Unavailable");
    return ok;
}

}  // namespace bl::csharp::global_items
