#include "experiments/csharp/ManagedProjectileHook.h"

#include "content/common/GameRefs.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Log.h"
#include "experiments/csharp/SourceLoader.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace bl::csharp {
namespace {

struct Vec2 {
    float x;
    float y;
};

using KillFn = void (*)(Il2CppObject*, const MethodInfo*);
KillFn g_originalKill = nullptr;
FieldInfo* g_npcs = nullptr;
FieldInfo* g_netMode = nullptr;
const MethodInfo* g_addBuff = nullptr;
int32_t g_npcActive = -1;
int32_t g_npcFriendly = -1;
int32_t g_npcLife = -1;
SourceBoltKillPolicy g_policy{};
std::atomic<bool> g_installed{false};
std::atomic<bool> g_enabled{false};
std::atomic<int> g_type{-1};
std::atomic<int> g_kills{0};
std::atomic<int> g_applied{0};
std::atomic<int> g_errors{0};
std::atomic<int> g_lastDamage{-1};
std::atomic<int> g_lastRadius{0};
std::atomic<int> g_lastDuration{0};
std::atomic<int> g_lastAffected{0};
std::atomic<int> g_buffId{0};

void hkKill(Il2CppObject* projectile, const MethodInfo* method) {
    const auto& game = runtime::game();
    bool selected = projectile && g_enabled.load(std::memory_order_acquire);
    Vec2 position{};
    int width = 0;
    int height = 0;
    int damage = 0;
    if (selected) {
        selected = runtime::field<int32_t>(projectile, game.proj.type) ==
            g_type.load(std::memory_order_relaxed) &&
            runtime::field<uint8_t>(projectile, game.proj.active) != 0;
        if (selected) {
            position = runtime::field<Vec2>(projectile, game.entity.position);
            width = runtime::field<int32_t>(projectile, game.entity.width);
            height = runtime::field<int32_t>(projectile, game.entity.height);
            damage = runtime::field<int32_t>(projectile, game.proj.damage);
            selected = std::isfinite(position.x) && std::isfinite(position.y) &&
                width >= 0 && width <= 512 && height >= 0 && height <= 512;
        }
    }
    if (g_originalKill) g_originalKill(projectile, method);
    if (!selected || !g_enabled.load(std::memory_order_acquire)) return;
    if (!g_npcs || !g_netMode || !g_addBuff) return;

    int mode = -1;
    il2cpp::api().field_static_get_value(g_netMode, &mode);
    if (mode != 0) {
        g_errors.fetch_add(1, std::memory_order_relaxed);
        g_enabled.store(false, std::memory_order_release);
        return;
    }

    const int previous = g_kills.fetch_add(1, std::memory_order_relaxed);
    if (previous >= 2048) {
        g_enabled.store(false, std::memory_order_release);
        return;
    }
    const auto index = static_cast<size_t>(std::clamp(damage, 0, 255));
    const int radius = g_policy.radii[index];
    const int duration = g_policy.durations[index];
    g_lastDamage.store(damage, std::memory_order_relaxed);
    g_lastRadius.store(radius, std::memory_order_relaxed);
    g_lastDuration.store(duration, std::memory_order_relaxed);

    Il2CppArray* array = nullptr;
    il2cpp::api().field_static_get_value(g_npcs, &array);
    if (!array) return;
    auto** npcs = static_cast<Il2CppObject**>(arrayData(array));
    const float cx = position.x + 0.5f * static_cast<float>(width);
    const float cy = position.y + 0.5f * static_cast<float>(height);
    const float r2 = static_cast<float>(radius * radius);
    int affected = 0;
    const size_t limit = std::min<size_t>(200, static_cast<size_t>(array->length));

    for (size_t i = 0; i < limit && affected < 6; ++i) {
        Il2CppObject* npc = npcs[i];
        if (!npc || !runtime::field<uint8_t>(npc, g_npcActive) ||
            runtime::field<uint8_t>(npc, g_npcFriendly) ||
            runtime::field<int32_t>(npc, g_npcLife) <= 0) continue;

        const Vec2 npcPosition = runtime::field<Vec2>(npc, game.entity.position);
        const int npcWidth = runtime::field<int32_t>(npc, game.entity.width);
        const int npcHeight = runtime::field<int32_t>(npc, game.entity.height);
        if (!std::isfinite(npcPosition.x) || !std::isfinite(npcPosition.y) ||
            npcWidth < 0 || npcWidth > 4096 || npcHeight < 0 || npcHeight > 4096) continue;

        const float dx = npcPosition.x + npcWidth * 0.5f - cx;
        const float dy = npcPosition.y + npcHeight * 0.5f - cy;
        if (dx * dx + dy * dy > r2) continue;

        int buffId = g_buffId.load(std::memory_order_relaxed);
        int ticks = duration;
        uint8_t quiet = 0;
        void* args[] = { &buffId, &ticks, &quiet };
        Il2CppObject* error = nullptr;
        il2cpp::api().runtime_invoke(g_addBuff, npc, args, &error);
        if (error) {
            g_errors.fetch_add(1, std::memory_order_relaxed);
            g_enabled.store(false, std::memory_order_release);
            BL_ERROR("csharp OnKill: NPC.AddBuff retornou excecao, hook desativado");
            return;
        }
        ++affected;
        g_applied.fetch_add(1, std::memory_order_relaxed);
    }
    g_lastAffected.store(affected, std::memory_order_relaxed);
}

}

bool enableSourceBoltOnKill(const SourceBoltKillPolicy& policy, std::string* error) {
    if (!error) return false;
    error->clear();
    if (policy.buffId != 24) {
        *error = "OnKill aceita somente OnFire (24) neste teste";
        return false;
    }
    for (size_t i = 0; i < policy.radii.size(); ++i) {
        if (policy.radii[i] < 24 || policy.radii[i] > 120 ||
            policy.durations[i] < 30 || policy.durations[i] > 180) {
            *error = "Tabela OnKill C# fora dos limites";
            return false;
        }
    }
    if (!runtime::resolveGameRefs() || !runtime::modProjectilesSettled()) {
        *error = "Registros Terraria indisponiveis";
        return false;
    }
    constexpr const char* uid = "57a98f20-7a35-4d19-8ce8-079e25bb36a1";
    bool loaded = false;
    for (const auto& item : sourceModStatuses()) {
        if (item.id == uid && item.loaded && item.runtimeApplied && item.projectiles > 0) {
            loaded = true;
            break;
        }
    }
    const int type = runtime::modProjectileTypeByName(uid, "SourceBolt");
    if (!loaded || type < 0 || type >= runtime::projectileTypeCount()) {
        *error = "SourceBolt/SetDefaults nao registrado";
        return false;
    }

    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* npc = il2cpp::findClass({"Terraria", "NPC", {}});
    if (!main || !npc || !runtime::game().proj.kill) {
        *error = "Terraria.Main/NPC ou Projectile.Kill indisponivel";
        return false;
    }
    FieldInfo* netMode = il2cpp::findField(main, "netMode");
    FieldInfo* npcArray = il2cpp::findField(main, "npc");
    FieldInfo* menu = il2cpp::findField(main, "gameMenu");
    const int active = il2cpp::fieldOffset(npc, "active");
    const int friendly = il2cpp::fieldOffset(npc, "friendly");
    const int life = il2cpp::fieldOffset(npc, "life");
    const MethodInfo* addBuff = il2cpp::api().class_get_method_from_name(npc, "AddBuff", 3);
    if (!netMode || !npcArray || active < 0 || friendly < 0 || life < 0 || !addBuff) {
        *error = "Referencias Main/NPC para OnKill ausentes";
        return false;
    }
    int mode = -1;
    il2cpp::api().field_static_get_value(netMode, &mode);
    if (mode != 0) {
        *error = "OnKill C# limitado ao singleplayer";
        return false;
    }
    if (menu) {
        uint8_t inMenu = 0;
        il2cpp::api().field_static_get_value(menu, &inMenu);
        if (inMenu) {
            *error = "Entre no mundo antes de ativar OnKill";
            return false;
        }
    }

    g_enabled.store(false, std::memory_order_release);
    if (!g_installed.load(std::memory_order_acquire)) {
        if (!hook::install(runtime::game().proj.kill, hkKill, &g_originalKill)) {
            *error = "Falha ao instalar hook Projectile.Kill";
            return false;
        }
        g_installed.store(true, std::memory_order_release);
    }

    g_npcs = npcArray;
    g_netMode = netMode;
    g_npcActive = active;
    g_npcFriendly = friendly;
    g_npcLife = life;
    g_addBuff = addBuff;
    g_policy = policy;
    g_type.store(type, std::memory_order_relaxed);
    g_buffId.store(policy.buffId, std::memory_order_relaxed);
    g_kills.store(0, std::memory_order_relaxed);
    g_applied.store(0, std::memory_order_relaxed);
    g_errors.store(0, std::memory_order_relaxed);
    g_lastDamage.store(-1, std::memory_order_relaxed);
    g_lastRadius.store(0, std::memory_order_relaxed);
    g_lastDuration.store(0, std::memory_order_relaxed);
    g_lastAffected.store(0, std::memory_order_relaxed);
    g_enabled.store(true, std::memory_order_release);
    BL_INFO("csharp OnKill: SourceBolt tipo %d, buff %d, singleplayer", type, policy.buffId);
    return true;
}

bool pauseSourceBoltOnKill() {
    return g_enabled.exchange(false, std::memory_order_acq_rel);
}

SourceBoltKillStatus sourceBoltKillStatus() {
    SourceBoltKillStatus result;
    result.installed = g_installed.load(std::memory_order_acquire);
    result.enabled = g_enabled.load(std::memory_order_acquire);
    result.killEvents = g_kills.load(std::memory_order_relaxed);
    result.buffsApplied = g_applied.load(std::memory_order_relaxed);
    result.errors = g_errors.load(std::memory_order_relaxed);
    result.lastDamage = g_lastDamage.load(std::memory_order_relaxed);
    result.lastRadius = g_lastRadius.load(std::memory_order_relaxed);
    result.lastDuration = g_lastDuration.load(std::memory_order_relaxed);
    result.lastAffected = g_lastAffected.load(std::memory_order_relaxed);
    result.buffId = g_buffId.load(std::memory_order_relaxed);
    return result;
}

}
