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
#include <cstdint>

namespace bl::csharp {
namespace {
using HitFn = void (*)(Il2CppObject*, int, const MethodInfo*);
HitFn g_originalHit = nullptr;
FieldInfo* g_npcs = nullptr;
FieldInfo* g_netMode = nullptr;
const MethodInfo* g_addBuff = nullptr;
int32_t g_npcActive = -1;
int32_t g_npcFriendly = -1;
int32_t g_npcLife = -1;
std::array<int, 256> g_hitDurations{};
std::atomic<bool> g_hitInstalled{false};
std::atomic<bool> g_hitEnabled{false};
std::atomic<int> g_hitType{-1};
std::atomic<int> g_hitEvents{0};
std::atomic<int> g_hitApplied{0};
std::atomic<int> g_hitErrors{0};
std::atomic<int> g_lastNpc{-1};
std::atomic<int> g_lastDamage{-1};
std::atomic<int> g_lastDuration{0};
std::atomic<int> g_hitBuffId{0};

struct AutoHitEntry {
    std::string modId;
    std::string projectile;
    int type = -1;
    int buffId = 24;
    std::array<int, 256> durations{};
    std::atomic<bool> enabled{false};
    std::atomic<int> events{0};
    std::atomic<int> applied{0};
    std::atomic<int> errors{0};
};

std::array<AutoHitEntry, 8> g_autoHits{};
std::array<std::atomic<int16_t>, 32768> g_autoHitByType{};
std::atomic<int> g_autoHitCount{0};

void hkStatusNPC(Il2CppObject* projectile, int index, const MethodInfo* method) {
    int type = -1;
    int damage = 0;
    int autoSlot = -1;
    bool legacy = false;
    if (projectile) {
        const auto& game = runtime::game();
        type = runtime::field<int32_t>(projectile, game.proj.type);
        if (type >= 0 && static_cast<size_t>(type) < g_autoHitByType.size()) {
            autoSlot = g_autoHitByType[static_cast<size_t>(type)].load(
                std::memory_order_acquire) - 1;
            if (autoSlot < 0 || autoSlot >= g_autoHitCount.load(
                    std::memory_order_acquire) ||
                !g_autoHits[static_cast<size_t>(autoSlot)].enabled.load(
                    std::memory_order_acquire))
                autoSlot = -1;
        }
        legacy = g_hitEnabled.load(std::memory_order_acquire) &&
            type == g_hitType.load(std::memory_order_relaxed);
        if (legacy || autoSlot >= 0)
            damage = runtime::field<int32_t>(projectile, game.proj.damage);
    }
    if (g_originalHit) g_originalHit(projectile, index, method);
    if ((!legacy && autoSlot < 0) || index < 0 || index >= 200 ||
        !g_addBuff || !g_npcs || !g_netMode) return;

    int mode = -1;
    il2cpp::api().field_static_get_value(g_netMode, &mode);
    if (mode != 0) {
        if (autoSlot >= 0) {
            auto& entry = g_autoHits[static_cast<size_t>(autoSlot)];
            entry.enabled.store(false, std::memory_order_release);
            entry.errors.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_hitEnabled.store(false, std::memory_order_release);
            g_hitErrors.fetch_add(1, std::memory_order_relaxed);
        }
        return;
    }
    Il2CppArray* npcs = nullptr;
    il2cpp::api().field_static_get_value(g_npcs, &npcs);
    if (!npcs || static_cast<size_t>(index) >= static_cast<size_t>(npcs->length)) return;
    auto** data = static_cast<Il2CppObject**>(arrayData(npcs));
    Il2CppObject* npc = data[index];
    if (!npc || !runtime::field<uint8_t>(npc, g_npcActive) ||
        runtime::field<uint8_t>(npc, g_npcFriendly) ||
        runtime::field<int32_t>(npc, g_npcLife) <= 0) return;

    int duration = 0;
    int buff = 24;
    if (autoSlot >= 0) {
        auto& entry = g_autoHits[static_cast<size_t>(autoSlot)];
        if (entry.events.fetch_add(1, std::memory_order_relaxed) >= 4096) {
            entry.enabled.store(false, std::memory_order_release);
            return;
        }
        duration = entry.durations[static_cast<size_t>(std::clamp(damage, 0, 255))];
        buff = entry.buffId;
    } else {
        if (g_hitEvents.fetch_add(1, std::memory_order_relaxed) >= 4096) {
            g_hitEnabled.store(false, std::memory_order_release);
            return;
        }
        duration = g_hitDurations[static_cast<size_t>(std::clamp(damage, 0, 255))];
        buff = g_hitBuffId.load(std::memory_order_relaxed);
        g_lastNpc.store(index, std::memory_order_relaxed);
        g_lastDamage.store(damage, std::memory_order_relaxed);
        g_lastDuration.store(duration, std::memory_order_relaxed);
    }
    uint8_t quiet = 0;
    void* args[] = { &buff, &duration, &quiet };
    Il2CppObject* exception = nullptr;
    il2cpp::api().runtime_invoke(g_addBuff, npc, args, &exception);
    if (exception) {
        if (autoSlot >= 0) {
            auto& entry = g_autoHits[static_cast<size_t>(autoSlot)];
            entry.errors.fetch_add(1, std::memory_order_relaxed);
            entry.enabled.store(false, std::memory_order_release);
        } else {
            g_hitErrors.fetch_add(1, std::memory_order_relaxed);
            g_hitEnabled.store(false, std::memory_order_release);
        }
        BL_ERROR("csharp OnHitNPC: NPC.AddBuff retornou excecao");
        return;
    }
    if (autoSlot >= 0)
        g_autoHits[static_cast<size_t>(autoSlot)].applied.fetch_add(
            1, std::memory_order_relaxed);
    else
        g_hitApplied.fetch_add(1, std::memory_order_relaxed);
}
}

bool enableSourceBoltOnHit(const SourceBoltHitPolicy& policy, std::string* error) {
    if (!error) return false;
    error->clear();
    if (policy.buffId != 24) {
        *error = "Somente efeito vanilla OnFire (24) e permitido neste teste";
        return false;
    }
    for (int ticks : policy.durations)
        if (ticks < 30 || ticks > 180) {
            *error = "Duracao OnHitNPC fora do intervalo permitido";
            return false;
        }

    if (!runtime::resolveGameRefs() || !runtime::modProjectilesSettled()) {
        *error = "Registros de projeteis do Terraria indisponiveis";
        return false;
    }
    constexpr const char* uid = "57a98f20-7a35-4d19-8ce8-079e25bb36a1";
    bool loaded = false;
    for (const auto& item : sourceModStatuses())
        if (item.id == uid && item.loaded && item.runtimeApplied && item.projectiles > 0)
            loaded = true;
    const int type = runtime::modProjectileTypeByName(uid, "SourceBolt");
    if (!loaded || type < 0 || type >= runtime::projectileTypeCount()) {
        *error = "SourceBolt nao registrado ou SetDefaults nao aplicado";
        return false;
    }
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* npc = il2cpp::findClass({"Terraria", "NPC", {}});
    auto* projectile = runtime::game().proj.cls;
    if (!main || !npc || !projectile) {
        *error = "Terraria.Main/NPC/Projectile indisponivel";
        return false;
    }
    FieldInfo* netMode = il2cpp::findField(main, "netMode");
    FieldInfo* npcArray = il2cpp::findField(main, "npc");
    FieldInfo* menu = il2cpp::findField(main, "gameMenu");
    const int32_t active = il2cpp::fieldOffset(npc, "active");
    const int32_t friendly = il2cpp::fieldOffset(npc, "friendly");
    const int32_t life = il2cpp::fieldOffset(npc, "life");
    const MethodInfo* addBuff = il2cpp::api().class_get_method_from_name(npc, "AddBuff", 3);
    const MethodInfo* hit = il2cpp::api().class_get_method_from_name(projectile, "StatusNPC", 1);
    if (!netMode || !npcArray || active < 0 || friendly < 0 || life < 0 ||
        !addBuff || !hit) {
        *error = "Referencias OnHitNPC ausentes (Main.netMode/npc, NPC.AddBuff ou Projectile.StatusNPC)";
        return false;
    }
    int mode = -1;
    il2cpp::api().field_static_get_value(netMode, &mode);
    if (mode != 0) {
        *error = "Teste OnHitNPC permitido somente em mundo solo";
        return false;
    }
    if (menu) {
        uint8_t inMenu = 0;
        il2cpp::api().field_static_get_value(menu, &inMenu);
        if (inMenu) {
            *error = "Entre no mundo antes de ativar OnHitNPC";
            return false;
        }
    }
    g_hitEnabled.store(false, std::memory_order_release);
    if (!g_hitInstalled.load(std::memory_order_acquire)) {
        if (!hook::install(hit, hkStatusNPC, &g_originalHit)) {
            *error = "Falha ao instalar hook nativo Projectile.StatusNPC";
            return false;
        }
        g_hitInstalled.store(true, std::memory_order_release);
    }
    g_npcs = npcArray;
    g_netMode = netMode;
    g_npcActive = active;
    g_npcFriendly = friendly;
    g_npcLife = life;
    g_addBuff = addBuff;
    g_hitType.store(type, std::memory_order_relaxed);
    g_hitDurations = policy.durations;
    g_hitBuffId.store(policy.buffId, std::memory_order_relaxed);
    g_hitEvents.store(0, std::memory_order_relaxed);
    g_hitApplied.store(0, std::memory_order_relaxed);
    g_hitErrors.store(0, std::memory_order_relaxed);
    g_lastNpc.store(-1, std::memory_order_relaxed);
    g_lastDamage.store(-1, std::memory_order_relaxed);
    g_lastDuration.store(0, std::memory_order_relaxed);
    g_hitEnabled.store(true, std::memory_order_release);
    BL_INFO("csharp OnHitNPC: SourceBolt tipo %d, buff %d, modo solo", type, policy.buffId);
    return true;
}


bool enableAutoProjectileOnHit(const std::string& modId,
                               const std::string& projectileName,
                               const SourceBoltHitPolicy& policy,
                               std::string* error) {
    if (!error) return false;
    error->clear();
    if (policy.buffId != 24) {
        *error = "OnHitNPC automatico aceita somente OnFire (24)";
        return false;
    }
    for (int ticks : policy.durations)
        if (ticks < 30 || ticks > 180) {
            *error = "Duracao OnHitNPC invalida";
            return false;
        }
    if (!runtime::resolveGameRefs() || !runtime::modProjectilesSettled()) {
        *error = "Conteudo do Terraria ainda indisponivel";
        return false;
    }
    bool loaded = false;
    for (const auto& mod : sourceModStatuses())
        if (mod.id == modId && mod.loaded && mod.runtimeApplied &&
            mod.registeredTypes.contains(projectileName)) loaded = true;
    const int type = runtime::modProjectileTypeByName(modId, projectileName);
    if (!loaded || type < 0 || type >= runtime::projectileTypeCount() ||
        static_cast<size_t>(type) >= g_autoHitByType.size()) {
        *error = "OnHitNPC: projetil C# nao registrado";
        return false;
    }
    auto* main = il2cpp::findClass({"Terraria", "Main", {}});
    auto* npc = il2cpp::findClass({"Terraria", "NPC", {}});
    auto* projectile = runtime::game().proj.cls;
    if (!main || !npc || !projectile) {
        *error = "Referencias Terraria OnHitNPC indisponiveis";
        return false;
    }
    FieldInfo* mode = il2cpp::findField(main, "netMode");
    FieldInfo* npcs = il2cpp::findField(main, "npc");
    int active = il2cpp::fieldOffset(npc, "active");
    int friendly = il2cpp::fieldOffset(npc, "friendly");
    int life = il2cpp::fieldOffset(npc, "life");
    const MethodInfo* addBuff = il2cpp::api().class_get_method_from_name(npc, "AddBuff", 3);
    const MethodInfo* hit = il2cpp::api().class_get_method_from_name(projectile, "StatusNPC", 1);
    if (!mode || !npcs || active < 0 || friendly < 0 || life < 0 ||
        !addBuff || !hit) {
        *error = "NPC.AddBuff ou Projectile.StatusNPC indisponivel";
        return false;
    }
    int netMode = -1;
    il2cpp::api().field_static_get_value(mode, &netMode);
    if (netMode != 0) {
        *error = "OnHitNPC automatico requer singleplayer";
        return false;
    }
    if (g_hitEnabled.load(std::memory_order_acquire) &&
        g_hitType.load(std::memory_order_relaxed) == type) {
        *error = "Politica manual OnHitNPC ja controla o mesmo projetil";
        return false;
    }
    if (!g_hitInstalled.load(std::memory_order_acquire)) {
        if (!hook::install(hit, hkStatusNPC, &g_originalHit)) {
            *error = "Falha no hook Projectile.StatusNPC";
            return false;
        }
        g_hitInstalled.store(true, std::memory_order_release);
    }
    g_npcs = npcs;
    g_netMode = mode;
    g_npcActive = active;
    g_npcFriendly = friendly;
    g_npcLife = life;
    g_addBuff = addBuff;

    int index = -1;
    const int count = g_autoHitCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i)
        if (g_autoHits[static_cast<size_t>(i)].modId == modId &&
            g_autoHits[static_cast<size_t>(i)].projectile == projectileName)
            index = i;
    if (index < 0) {
        if (count >= static_cast<int>(g_autoHits.size())) {
            *error = "Limite de oito efeitos OnHitNPC automaticos atingido";
            return false;
        }
        index = count;
    }
    auto& entry = g_autoHits[static_cast<size_t>(index)];
    entry.enabled.store(false, std::memory_order_release);
    entry.modId = modId;
    entry.projectile = projectileName;
    entry.type = type;
    entry.buffId = policy.buffId;
    entry.durations = policy.durations;
    entry.events.store(0, std::memory_order_relaxed);
    entry.applied.store(0, std::memory_order_relaxed);
    entry.errors.store(0, std::memory_order_relaxed);
    if (index == count)
        g_autoHitCount.store(count + 1, std::memory_order_release);
    entry.enabled.store(true, std::memory_order_release);
    g_autoHitByType[static_cast<size_t>(type)].store(
        static_cast<int16_t>(index + 1), std::memory_order_release);
    BL_INFO("csharp auto OnHitNPC: %s/%s type=%d",
            modId.c_str(), projectileName.c_str(), type);
    return true;
}

AutoCombatEffectStatus autoProjectileHitStatus(
    const std::string& modId, const std::string& projectileName) {
    AutoCombatEffectStatus status;
    status.installed = g_hitInstalled.load(std::memory_order_acquire);
    const int count = g_autoHitCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        const auto& entry = g_autoHits[static_cast<size_t>(i)];
        if (entry.modId != modId || entry.projectile != projectileName) continue;
        status.registered = true;
        status.enabled = entry.enabled.load(std::memory_order_acquire);
        status.type = entry.type;
        status.events = entry.events.load(std::memory_order_relaxed);
        status.buffsApplied = entry.applied.load(std::memory_order_relaxed);
        status.errors = entry.errors.load(std::memory_order_relaxed);
        status.buffId = entry.buffId;
        break;
    }
    return status;
}


bool pauseSourceBoltOnHit() {
    return g_hitEnabled.exchange(false, std::memory_order_acq_rel);
}

SourceBoltHitStatus sourceBoltHitStatus() {
    SourceBoltHitStatus status;
    status.installed = g_hitInstalled.load(std::memory_order_acquire);
    status.enabled = g_hitEnabled.load(std::memory_order_acquire);
    status.events = g_hitEvents.load(std::memory_order_relaxed);
    status.applied = g_hitApplied.load(std::memory_order_relaxed);
    status.errors = g_hitErrors.load(std::memory_order_relaxed);
    status.lastNpc = g_lastNpc.load(std::memory_order_relaxed);
    status.lastDamage = g_lastDamage.load(std::memory_order_relaxed);
    status.lastDuration = g_lastDuration.load(std::memory_order_relaxed);
    status.buffId = g_hitBuffId.load(std::memory_order_relaxed);
    return status;
}
}
