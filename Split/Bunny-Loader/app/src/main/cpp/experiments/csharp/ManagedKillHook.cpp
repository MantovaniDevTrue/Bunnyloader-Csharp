#include "experiments/csharp/ManagedProjectileHook.h"

#include "content/common/GameRefs.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Log.h"
#include "experiments/csharp/SourceLoader.h"
#include "experiments/csharp/ManagedRuntime.h"
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

struct AutoKillEntry {
    std::string modId;
    std::string projectile;
    int type = -1;
    SourceBoltKillPolicy policy{};
    std::atomic<bool> enabled{false};
    std::atomic<int> events{0};
    std::atomic<int> applied{0};
    std::atomic<int> errors{0};
};
std::array<AutoKillEntry, 8> g_autoKills{};
std::array<std::atomic<int16_t>, 32768> g_autoKillByType{};
std::atomic<int> g_autoKillCount{0};

void hkKill(Il2CppObject* projectile, const MethodInfo* method) {
    const auto& game = runtime::game();
    int type = -1;
    int autoSlot = -1;
    bool legacy = false;
    bool additionalLive = false;
    Vec2 position{};
    int width = 0, height = 0, damage = 0;
    bool selected = false;
    if (projectile) {
        type = runtime::field<int32_t>(projectile, game.proj.type);
        if (type >= 0 && static_cast<size_t>(type) < g_autoKillByType.size()) {
            autoSlot = g_autoKillByType[static_cast<size_t>(type)].load(
                std::memory_order_acquire) - 1;
            if (autoSlot < 0 || autoSlot >= g_autoKillCount.load(
                    std::memory_order_acquire) ||
                !g_autoKills[static_cast<size_t>(autoSlot)].enabled.load(
                    std::memory_order_acquire))
                autoSlot = -1;
        }
        legacy = g_enabled.load(std::memory_order_acquire) &&
            type == g_type.load(std::memory_order_relaxed);
        additionalLive = isManagedLiveProjectileBound(type);
        selected = (legacy || autoSlot >= 0 || additionalLive) &&
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
    if (projectile && hasManagedEntityProjectileType(type))
        queueManagedSourceProjectileCombat(5, projectile, -1,
            runtime::field<int>(projectile, game.proj.damage));
    if (g_originalKill) g_originalKill(projectile, method);
    if (!selected || !g_npcs || !g_netMode || !g_addBuff) return;
    int mode = -1;
    il2cpp::api().field_static_get_value(g_netMode, &mode);
    if (mode != 0) {
        if (autoSlot >= 0) {
            auto& entry = g_autoKills[static_cast<size_t>(autoSlot)];
            entry.enabled.store(false, std::memory_order_release);
            entry.errors.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_errors.fetch_add(1, std::memory_order_relaxed);
            g_enabled.store(false, std::memory_order_release);
        }
        return;
    }
    if (additionalLive && !legacy && autoSlot < 0) {
        queueManagedLiveKill(type, damage);
        return;
    }
    const SourceBoltKillPolicy* policy = &g_policy;
    if (autoSlot >= 0) {
        auto& entry = g_autoKills[static_cast<size_t>(autoSlot)];
        if (entry.events.fetch_add(1, std::memory_order_relaxed) >= 2048) {
            entry.enabled.store(false, std::memory_order_release);
            return;
        }
        policy = &entry.policy;
    } else if (g_kills.fetch_add(1, std::memory_order_relaxed) >= 2048) {
        g_enabled.store(false, std::memory_order_release);
        return;
    }
    queueManagedLiveKill(type, damage);
    const size_t damageIndex = static_cast<size_t>(std::clamp(damage, 0, 255));
    const int radius = policy->radii[damageIndex];
    const int duration = policy->durations[damageIndex];
    const int buffId = policy->buffId;
    if (autoSlot < 0) {
        g_lastDamage.store(damage, std::memory_order_relaxed);
        g_lastRadius.store(radius, std::memory_order_relaxed);
        g_lastDuration.store(duration, std::memory_order_relaxed);
    }
    Il2CppArray* npcArray = nullptr;
    il2cpp::api().field_static_get_value(g_npcs, &npcArray);
    if (!npcArray) return;
    auto** npcs = static_cast<Il2CppObject**>(arrayData(npcArray));
    const float centerX = position.x + 0.5f * static_cast<float>(width);
    const float centerY = position.y + 0.5f * static_cast<float>(height);
    const float radiusSquared = static_cast<float>(radius * radius);
    int affected = 0;
    const size_t count = std::min<size_t>(200, static_cast<size_t>(npcArray->length));
    for (size_t i = 0; i < count && affected < 6; ++i) {
        auto* npc = npcs[i];
        if (!npc || !runtime::field<uint8_t>(npc, g_npcActive) ||
            runtime::field<uint8_t>(npc, g_npcFriendly) ||
            runtime::field<int32_t>(npc, g_npcLife) <= 0) continue;
        const Vec2 p = runtime::field<Vec2>(npc, game.entity.position);
        int w = runtime::field<int32_t>(npc, game.entity.width);
        int h = runtime::field<int32_t>(npc, game.entity.height);
        if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
            w < 0 || w > 4096 || h < 0 || h > 4096) continue;
        const float dx = p.x + w * 0.5f - centerX;
        const float dy = p.y + h * 0.5f - centerY;
        if (dx * dx + dy * dy > radiusSquared) continue;
        int buff = buffId;
        int ticks = duration;
        uint8_t quiet = 0;
        void* args[] = { &buff, &ticks, &quiet };
        Il2CppObject* exception = nullptr;
        il2cpp::api().runtime_invoke(g_addBuff, npc, args, &exception);
        if (exception) {
            if (autoSlot >= 0) {
                auto& entry = g_autoKills[static_cast<size_t>(autoSlot)];
                entry.errors.fetch_add(1, std::memory_order_relaxed);
                entry.enabled.store(false, std::memory_order_release);
            } else {
                g_errors.fetch_add(1, std::memory_order_relaxed);
                g_enabled.store(false, std::memory_order_release);
            }
            BL_ERROR("csharp OnKill: NPC.AddBuff retornou excecao");
            return;
        }
        ++affected;
        if (autoSlot >= 0)
            g_autoKills[static_cast<size_t>(autoSlot)].applied.fetch_add(
                1, std::memory_order_relaxed);
        else
            g_applied.fetch_add(1, std::memory_order_relaxed);
    }
    if (autoSlot < 0)
        g_lastAffected.store(affected, std::memory_order_relaxed);
}

}

bool installManagedSourceEntityKillHook() {
    if (g_installed.load(std::memory_order_acquire)) return true;
    if (!runtime::resolveGameRefs() || !runtime::game().proj.kill) return false;
    if (!hook::install(runtime::game().proj.kill, hkKill, &g_originalKill))
        return false;
    g_installed.store(true, std::memory_order_release);
    BL_INFO("csharp source kill: Projectile.Kill hook compartilhado instalado");
    return true;
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


bool enableAutoProjectileOnKill(const std::string& modId,
                                const std::string& projectileName,
                                const SourceBoltKillPolicy& policy,
                                std::string* error) {
    if (!error) return false;
    error->clear();
    if (policy.buffId != 24) {
        *error = "OnKill automatico aceita somente OnFire (24)";
        return false;
    }
    for (size_t i = 0; i < policy.radii.size(); ++i)
        if (policy.radii[i] < 24 || policy.radii[i] > 120 ||
            policy.durations[i] < 30 || policy.durations[i] > 180) {
            *error = "Tabela C# OnKill fora dos limites";
            return false;
        }
    if (!runtime::resolveGameRefs() || !runtime::modProjectilesSettled()) {
        *error = "Conteudo Terraria ainda indisponivel";
        return false;
    }
    bool loaded = false;
    for (const auto& mod : sourceModStatuses())
        if (mod.id == modId && mod.loaded && mod.runtimeApplied &&
            mod.registeredTypes.contains(projectileName)) loaded = true;
    const int type = runtime::modProjectileTypeByName(modId, projectileName);
    if (!loaded || type < 0 || type >= runtime::projectileTypeCount() ||
        static_cast<size_t>(type) >= g_autoKillByType.size()) {
        *error = "OnKill: projetil C# nao registrado";
        return false;
    }
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* npc = il2cpp::findClass({"Terraria", "NPC", {}});
    const auto& game = runtime::game();
    if (!main || !npc || !game.proj.kill) {
        *error = "Referencias Terraria OnKill indisponiveis";
        return false;
    }
    FieldInfo* mode = il2cpp::findField(main, "netMode");
    FieldInfo* npcs = il2cpp::findField(main, "npc");
    int active = il2cpp::fieldOffset(npc, "active");
    int friendly = il2cpp::fieldOffset(npc, "friendly");
    int life = il2cpp::fieldOffset(npc, "life");
    const MethodInfo* addBuff = il2cpp::api().class_get_method_from_name(npc, "AddBuff", 3);
    if (!mode || !npcs || active < 0 || friendly < 0 || life < 0 || !addBuff) {
        *error = "NPC.AddBuff ou Terraria.Main.npc indisponivel";
        return false;
    }
    int netMode = -1;
    il2cpp::api().field_static_get_value(mode, &netMode);
    if (netMode != 0) {
        *error = "OnKill automatico requer singleplayer";
        return false;
    }
    if (g_enabled.load(std::memory_order_acquire) &&
        g_type.load(std::memory_order_relaxed) == type) {
        *error = "Politica manual OnKill ja controla o mesmo projetil";
        return false;
    }
    if (!g_installed.load(std::memory_order_acquire)) {
        if (!hook::install(game.proj.kill, hkKill, &g_originalKill)) {
            *error = "Falha no hook Projectile.Kill";
            return false;
        }
        g_installed.store(true, std::memory_order_release);
    }
    g_npcs = npcs;
    g_netMode = mode;
    g_npcActive = active;
    g_npcFriendly = friendly;
    g_npcLife = life;
    g_addBuff = addBuff;
    int index = -1;
    const int count = g_autoKillCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i)
        if (g_autoKills[static_cast<size_t>(i)].modId == modId &&
            g_autoKills[static_cast<size_t>(i)].projectile == projectileName)
            index = i;
    if (index < 0) {
        if (count >= static_cast<int>(g_autoKills.size())) {
            *error = "Limite de oito efeitos OnKill automaticos atingido";
            return false;
        }
        index = count;
    }
    auto& entry = g_autoKills[static_cast<size_t>(index)];
    entry.enabled.store(false, std::memory_order_release);
    entry.modId = modId;
    entry.projectile = projectileName;
    entry.type = type;
    entry.policy = policy;
    entry.events.store(0, std::memory_order_relaxed);
    entry.applied.store(0, std::memory_order_relaxed);
    entry.errors.store(0, std::memory_order_relaxed);
    if (index == count)
        g_autoKillCount.store(count + 1, std::memory_order_release);
    entry.enabled.store(true, std::memory_order_release);
    g_autoKillByType[static_cast<size_t>(type)].store(
        static_cast<int16_t>(index + 1), std::memory_order_release);
    BL_INFO("csharp auto OnKill: %s/%s type=%d",
            modId.c_str(), projectileName.c_str(), type);
    return true;
}

AutoCombatEffectStatus autoProjectileKillStatus(
    const std::string& modId, const std::string& projectileName) {
    AutoCombatEffectStatus status;
    status.installed = g_installed.load(std::memory_order_acquire);
    const int count = g_autoKillCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        const auto& entry = g_autoKills[static_cast<size_t>(i)];
        if (entry.modId != modId || entry.projectile != projectileName) continue;
        status.registered = true;
        status.enabled = entry.enabled.load(std::memory_order_acquire);
        status.type = entry.type;
        status.events = entry.events.load(std::memory_order_relaxed);
        status.buffsApplied = entry.applied.load(std::memory_order_relaxed);
        status.errors = entry.errors.load(std::memory_order_relaxed);
        status.buffId = entry.policy.buffId;
        break;
    }
    return status;
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
