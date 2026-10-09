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

void hkStatusNPC(Il2CppObject* projectile, int index, const MethodInfo* method) {
    const bool run = projectile && g_hitEnabled.load(std::memory_order_acquire);
    int type = -1;
    int damage = 0;
    if (run) {
        const auto& game = runtime::game();
        type = runtime::field<int32_t>(projectile, game.proj.type);
        if (type == g_hitType.load(std::memory_order_relaxed))
            damage = runtime::field<int32_t>(projectile, game.proj.damage);
    }
    if (g_originalHit) g_originalHit(projectile, index, method);
    if (!run || type != g_hitType.load(std::memory_order_relaxed)) return;
    if (index < 0 || index >= 200 || !g_addBuff || !g_npcs || !g_netMode) return;

    int mode = -1;
    il2cpp::api().field_static_get_value(g_netMode, &mode);
    if (mode != 0) {
        g_hitEnabled.store(false, std::memory_order_release);
        g_hitErrors.fetch_add(1, std::memory_order_relaxed);
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
    const int count = g_hitEvents.fetch_add(1, std::memory_order_relaxed);
    if (count >= 4096) {
        g_hitEnabled.store(false, std::memory_order_release);
        return;
    }
    g_lastNpc.store(index, std::memory_order_relaxed);
    const int duration = g_hitDurations[static_cast<size_t>(std::clamp(damage, 0, 255))];
    g_lastDamage.store(damage, std::memory_order_relaxed);
    g_lastDuration.store(duration, std::memory_order_relaxed);
    int id = g_hitBuffId.load(std::memory_order_relaxed);
    int time = duration;
    uint8_t quiet = 0;
    void* args[] = { &id, &time, &quiet };
    Il2CppObject* error = nullptr;
    il2cpp::api().runtime_invoke(g_addBuff, npc, args, &error);
    if (error) {
        g_hitErrors.fetch_add(1, std::memory_order_relaxed);
        g_hitEnabled.store(false, std::memory_order_release);
        BL_ERROR("csharp OnHitNPC: AddBuff retornou excecao, hook desativado");
        return;
    }
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
