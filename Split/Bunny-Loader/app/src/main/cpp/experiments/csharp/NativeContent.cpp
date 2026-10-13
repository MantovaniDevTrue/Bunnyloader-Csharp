#include "experiments/csharp/NativeContent.h"

#include "content/common/GameRefs.h"
#include "content/items/ModItems.h"
#include "content/common/TypeTables.h"
#include "content/npcs/ModNpcs.h"
#include "content/buffs/ModBuffs.h"
#include "il2cpp/Api.h"
#include "content/projectiles/ModProjectiles.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <map>
#include <cmath>
#include <string>
#include <sys/stat.h>

namespace bl::csharp {
namespace {

struct NativeRefs {
    int itemDamage = -1;
    int itemUseTime = -1;
    int itemUseAnimation = -1;
    int itemUseStyle = -1;
    int itemShoot = -1;
    int itemShootSpeed = -1;
    int itemKnockBack = -1;
    int itemRanged = -1;
    int itemNoMelee = -1;
    int itemAutoReuse = -1;
    int itemRare = -1;
    int itemMaxStack = -1;
    int projAiStyle = -1;
    int projFriendly = -1;
    int projHostile = -1;
    int projRanged = -1;
    int projPenetrate = -1;
    int projTimeLeft = -1;
    int projIgnoreWater = -1;
    int projTileCollide = -1;
    int projWidth = -1;
    int projHeight = -1;
    int itemAccessory = -1;
    int itemBuffType = -1;
    int itemBuffTime = -1;
    int itemConsumable = -1;
    int npcWidth = -1;
    int npcHeight = -1;
    int npcDamage = -1;
    int npcDefense = -1;
    int npcLifeMax = -1;
    int npcAiStyle = -1;
    int npcType = -1;
    int playerStatDefense = -1;
    int playerBuffType = -1;
    int playerBuffTime = -1;
    int itemType = -1;
    int playerMoveSpeed = -1;
    int playerArmor = -1;
    int playerMaxRunSpeed = -1;
    int playerRunAcceleration = -1;
    int playerFrameShadow = -1;
    int playerFrameOutline = -1;
    int playerWhoAmI = -1;
    int playerLifeRegen = -1;
    int playerManaRegen = -1;
};

struct NativeWeaponDefaults {
    int damage;
    int useTime;
    float shootSpeed;
    int shootType;
};

struct NativeAccessoryDefaults {
    int defenseBonus;
    int moveSpeedPercent = 0;
    int movementAccelerationPercent = 0;
    bool frameShadow = false;
    std::string worldJoinMessage;
    int lifeRegenBonus = 0;
    int manaRegenBonus = 0;
};

struct NativeBuffDefaults {
    int defenseBonus;
};

struct NativePotionDefaults {
    int buffType;
    int duration;
};

struct NativeNpcDefaults {
    int width;
    int height;
    int damage;
    int defense;
    int lifeMax;
    int aiStyle;
};

struct NativeProjectileDefaults {
    int width;
    int height;
    int aiStyle;
    int penetrate;
    int timeLeft;
    bool friendly;
    bool hostile;
};

using ItemDefaultsFn = void (*)(Il2CppObject*, int32_t, int32_t, const MethodInfo*);
using ProjectileDefaultsFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
using NpcDefaultsPtrFn = void (*)(Il2CppObject*, int32_t, const void*, const MethodInfo*);
using ApplyEquipFn = void (*)(Il2CppObject*, int32_t, Il2CppObject*, const MethodInfo*);
using UpdateBuffsFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
using HorizontalMovementFn = void (*)(Il2CppObject*, const MethodInfo*);
using SetArmorEffectVisualsFn = void (*)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
using PlayerSpawnFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
using PlayerUpdateFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
using PlayerStageFn = void (*)(Il2CppObject*, const MethodInfo*);

NativeRefs g_refs{};
std::map<int, NativeWeaponDefaults> g_weapons;
std::map<int, NativeProjectileDefaults> g_projectiles;
std::map<int, NativeNpcDefaults> g_npcs;
std::map<int, NativeAccessoryDefaults> g_accessories;
std::map<std::string, std::string> g_globalWorldMessages;
std::map<std::string, std::string> g_respawnMessages;
std::map<std::string, std::string> g_deadMessages;
bool g_preUpdateEnabled = false;
bool g_postUpdateEnabled = false;
bool g_deadEventDelivered = false;
bool g_anyLifeRegen = false;
bool g_anyManaRegen = false;
std::map<int, NativeBuffDefaults> g_buffs;
std::map<int, NativePotionDefaults> g_potions;
ItemDefaultsFn g_originalItem = nullptr;
ProjectileDefaultsFn g_originalProjectile = nullptr;
NpcDefaultsPtrFn g_originalNpc = nullptr;
ApplyEquipFn g_originalEquip = nullptr;
UpdateBuffsFn g_originalUpdateBuffs = nullptr;
HorizontalMovementFn g_originalHorizontalMovement = nullptr;
SetArmorEffectVisualsFn g_originalSetArmorEffectVisuals = nullptr;
PlayerSpawnFn g_originalPlayerSpawn = nullptr;
PlayerUpdateFn g_originalPlayerUpdate = nullptr;
PlayerStageFn g_originalUpdateDead = nullptr;
PlayerStageFn g_originalLifeRegen = nullptr;
PlayerStageFn g_originalManaRegen = nullptr;
constexpr int g_reviveSpawnContext = 0;
bool g_ready = false;
bool g_npcReady = false;
bool g_effectsReady = false;
bool g_playerPoliciesReady = false;

bool validName(const char* value) {
    if (!value || !*value) return false;
    size_t length = 0;
    for (const unsigned char* s = reinterpret_cast<const unsigned char*>(value); *s; ++s) {
        ++length;
        if (length > 72) return false;
        const unsigned char c = *s;
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

bool fileExists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

bool validTexture(const char* path) {
    if (!path || !*path || path[0] == '/' || path[0] == '\\') return false;
    std::string p(path);
    return p.size() <= 160 && p.find("..") == std::string::npos &&
           p.find('\\') == std::string::npos &&
           p.compare(0, 8, "Content/") == 0 &&
           p.size() > 12 && p.substr(p.size() - 4) == ".png";
}

bool enableManagedPrefixEligibility(int type) {
    using namespace bl::il2cpp;
    static FieldInfo* prefixSet = nullptr;
    static Il2CppClass* prefixSetsClass = nullptr;
    if (!prefixSet) {
        Il2CppClass* itemId = findClass({"Terraria.ID", "ItemID", {}});
        Il2CppClass* sets = itemId ? findNested(itemId, "Sets") : nullptr;
        prefixSetsClass = sets;
        prefixSet = sets ? findField(sets, "CanGetPrefixes") : nullptr;
    }
    auto& runtime = api();
    if (!prefixSet || type < 0 || type >= bl::runtime::itemTypeCount() ||
        !runtime.field_static_get_value || !runtime.field_static_set_value)
        return false;
    if (prefixSetsClass && runtime.runtime_class_init)
        runtime.runtime_class_init(prefixSetsClass);
    Il2CppArray* entries = nullptr;
    runtime.field_static_get_value(prefixSet, &entries);
    if (!entries || static_cast<uintptr_t>(type) >= entries->length) {
        if (!entries) {
            BL_WARN("csharp prefix: tabela CanGetPrefixes ainda indisponivel para item %d", type);
            return false;
        }
        const uintptr_t required = static_cast<uintptr_t>(bl::runtime::itemTypeCount());
        auto* expanded = bl::runtime::TypeTables::resizedCopy(entries, required);
        if (!expanded) {
            BL_WARN("csharp prefix: falha ao ampliar CanGetPrefixes para item %d", type);
            return false;
        }
        runtime.field_static_set_value(prefixSet, expanded);
        entries = expanded;
        BL_INFO("csharp prefix: CanGetPrefixes ampliado para %zu itens", required);
    }
    auto* flags = static_cast<uint8_t*>(arrayData(entries));
    if (flags[type]) return true;
    flags[type] = 1;
    BL_INFO("csharp prefix: item %d habilitado para reforja", type);
    return true;
}

void hkItemDefaults(Il2CppObject* self, int32_t type, int32_t variant, const MethodInfo* method) {
    auto it = g_weapons.find(type);
    auto accessory = g_accessories.find(type);
    auto potion = g_potions.find(type);
    if ((it == g_weapons.end() && accessory == g_accessories.end() &&
         potion == g_potions.end()) || !self) {
        if (g_originalItem) g_originalItem(self, type, variant, method);
        return;
    }
    bl::runtime::prepareModItem(self, type);
    const auto& r = g_refs;
    if (accessory != g_accessories.end()) {
        bl::runtime::field<int32_t>(self, r.itemMaxStack) = 1;
        bl::runtime::field<int32_t>(self, r.itemRare) = 2;
        bl::runtime::field<uint8_t>(self, r.itemAccessory) = 1;
        enableManagedPrefixEligibility(type);
        bl::runtime::finishModItem(self, type);
        return;
    }
    if (potion != g_potions.end()) {
        bl::runtime::field<int32_t>(self, r.itemMaxStack) = 30;
        bl::runtime::field<int32_t>(self, r.itemRare) = 2;
        bl::runtime::field<int32_t>(self, r.itemUseTime) = 15;
        bl::runtime::field<int32_t>(self, r.itemUseAnimation) = 15;
        bl::runtime::field<int32_t>(self, r.itemUseStyle) = 2;
        bl::runtime::field<uint8_t>(self, r.itemConsumable) = 1;
        bl::runtime::field<int32_t>(self, r.itemBuffType) = potion->second.buffType;
        bl::runtime::field<int32_t>(self, r.itemBuffTime) = potion->second.duration;
        bl::runtime::finishModItem(self, type);
        return;
    }
    const auto& d = it->second;
    bl::runtime::field<int32_t>(self, r.itemDamage) = d.damage;
    bl::runtime::field<int32_t>(self, r.itemUseTime) = d.useTime;
    bl::runtime::field<int32_t>(self, r.itemUseAnimation) = d.useTime;
    bl::runtime::field<int32_t>(self, r.itemUseStyle) = 5;
    bl::runtime::field<int32_t>(self, r.itemShoot) = d.shootType;
    bl::runtime::field<float>(self, r.itemShootSpeed) = d.shootSpeed;
    bl::runtime::field<float>(self, r.itemKnockBack) = 2.0f;
    bl::runtime::field<uint8_t>(self, r.itemRanged) = 1;
    bl::runtime::field<uint8_t>(self, r.itemNoMelee) = 1;
    bl::runtime::field<uint8_t>(self, r.itemAutoReuse) = 1;
    bl::runtime::field<int32_t>(self, r.itemRare) = 2;
    bl::runtime::field<int32_t>(self, r.itemMaxStack) = 1;
    enableManagedPrefixEligibility(type);
    bl::runtime::finishModItem(self, type);
}

void hkProjectileDefaults(Il2CppObject* self, int32_t type, const MethodInfo* method) {
    if (g_originalProjectile) g_originalProjectile(self, type, method);
    auto it = g_projectiles.find(type);
    if (it == g_projectiles.end() || !self) return;
    const auto& r = g_refs;
    const auto& d = it->second;
    bl::runtime::field<int32_t>(self, r.projAiStyle) = d.aiStyle;
    bl::runtime::field<uint8_t>(self, r.projFriendly) = d.friendly ? 1 : 0;
    bl::runtime::field<uint8_t>(self, r.projHostile) = d.hostile ? 1 : 0;
    bl::runtime::field<uint8_t>(self, r.projRanged) = 1;
    bl::runtime::field<int32_t>(self, r.projPenetrate) = d.penetrate;
    bl::runtime::field<int32_t>(self, r.projTimeLeft) = d.timeLeft;
    bl::runtime::field<uint8_t>(self, r.projIgnoreWater) = 1;
    bl::runtime::field<uint8_t>(self, r.projTileCollide) = 1;
    bl::runtime::field<int32_t>(self, r.projWidth) = d.width;
    bl::runtime::field<int32_t>(self, r.projHeight) = d.height;
    bl::runtime::finishModProjectile(self, type);
}

void hkNpcDefaults(Il2CppObject* self, int32_t type, const void* params, const MethodInfo* method) {
    if (g_originalNpc) g_originalNpc(self, type, params, method);
    auto it = g_npcs.find(type);
    if (it == g_npcs.end() || !self) return;
    const auto& d = it->second;
    const auto& r = g_refs;
    bl::runtime::field<int32_t>(self, r.npcWidth) = d.width;
    bl::runtime::field<int32_t>(self, r.npcHeight) = d.height;
    bl::runtime::field<int32_t>(self, r.npcDamage) = d.damage;
    bl::runtime::field<int32_t>(self, r.npcDefense) = d.defense;
    bl::runtime::field<int32_t>(self, r.npcLifeMax) = d.lifeMax;
    bl::runtime::field<int32_t>(self, r.npcAiStyle) = d.aiStyle;
    bl::runtime::finishModNpc(self, type);
}

void hkApplyEquip(Il2CppObject* self, int32_t slot, Il2CppObject* item, const MethodInfo* method) {
    if (g_originalEquip) g_originalEquip(self, slot, item, method);
    if (!self || !item) return;
    const int type = bl::runtime::field<int32_t>(item, g_refs.itemType);
    const auto it = g_accessories.find(type);
    if (it == g_accessories.end()) return;
    auto& defense = bl::runtime::field<int32_t>(self, g_refs.playerStatDefense);
    defense += it->second.defenseBonus;
}

void hkHorizontalMovement(Il2CppObject* self, const MethodInfo* method) {
    if (!self) {
        if (g_originalHorizontalMovement)
            g_originalHorizontalMovement(self, method);
        return;
    }
    int movementPercent = 0;
    int movementAcceleration = 0;
    if (g_refs.playerArmor >= 0) {
        auto* armor = bl::runtime::field<Il2CppArray*>(self, g_refs.playerArmor);
        if (armor && armor->length >= 10 && armor->length <= 40) {
            auto** equipped = static_cast<Il2CppObject**>(arrayData(armor));
            for (uintptr_t i = 3; i < 10; ++i) {
                Il2CppObject* item = equipped[i];
                if (!item || !bl::runtime::field<uint8_t>(item, g_refs.itemAccessory))
                    continue;
                const int type = bl::runtime::field<int32_t>(item, g_refs.itemType);
                const auto found = g_accessories.find(type);
                if (found != g_accessories.end()) {
                    movementPercent += found->second.moveSpeedPercent;
                    movementAcceleration += found->second.movementAccelerationPercent;
                }
            }
        }
    }
    if (movementPercent > 0 && movementPercent <= 100) {
        const float factor = 1.f + static_cast<float>(movementPercent) / 100.f;
        auto& moveSpeed = bl::runtime::field<float>(self, g_refs.playerMoveSpeed);
        auto& maxSpeed = bl::runtime::field<float>(self, g_refs.playerMaxRunSpeed);
        auto& acceleration = bl::runtime::field<float>(self, g_refs.playerRunAcceleration);
        const float before = maxSpeed;
        moveSpeed += static_cast<float>(movementPercent) / 100.f;
        maxSpeed *= factor;
        acceleration *= factor;
        static int logged = 0;
        if (logged < 4) {
            ++logged;
            BL_INFO("csharp player: velocidade nativa +%d%% aplicada ao correr maxRunSpeed=%.3f->%.3f",
                    movementPercent, before, maxSpeed);
        }
    }
    if (movementAcceleration > 0 && movementAcceleration <= 60) {
        auto& acceleration = bl::runtime::field<float>(self, g_refs.playerRunAcceleration);
        acceleration *= 1.f + static_cast<float>(movementAcceleration) / 100.f;
        static int accelerationLogs = 0;
        if (accelerationLogs++ < 3)
            BL_INFO("csharp BunnyPlayer UpdateMovement: +%d%% aceleracao nativa",
                    movementAcceleration);
    }
    if (g_originalHorizontalMovement)
        g_originalHorizontalMovement(self, method);
}

bool managedShadowEquipped(Il2CppObject* player) {
    if (!player || g_refs.playerArmor < 0) return false;
    auto* armor = bl::runtime::field<Il2CppArray*>(player, g_refs.playerArmor);
    if (!armor || armor->length < 10 || armor->length > 40) return false;
    auto** equipped = static_cast<Il2CppObject**>(arrayData(armor));
    for (uintptr_t slot = 3; slot < 10; ++slot) {
        Il2CppObject* item = equipped[slot];
        if (!item || !bl::runtime::field<uint8_t>(item, g_refs.itemAccessory))
            continue;
        const int type = bl::runtime::field<int32_t>(item, g_refs.itemType);
        const auto found = g_accessories.find(type);
        if (found != g_accessories.end() && found->second.frameShadow)
            return true;
    }
    return false;
}

void hkSetArmorEffectVisuals(Il2CppObject* self, Il2CppObject* drawPlayer,
                             const MethodInfo* method) {
    if (g_originalSetArmorEffectVisuals)
        g_originalSetArmorEffectVisuals(self, drawPlayer, method);
    if (!self || !managedShadowEquipped(drawPlayer ? drawPlayer : self))
        return;
    if (g_refs.playerFrameShadow >= 0)
        bl::runtime::field<uint8_t>(self, g_refs.playerFrameShadow) = 1;
    if (g_refs.playerFrameOutline >= 0)
        bl::runtime::field<uint8_t>(self, g_refs.playerFrameOutline) = 1;
    static int visualLogs = 0;
    if (visualLogs < 3) {
        ++visualLogs;
        BL_INFO("csharp BunnyPlayer FrameEffects: sombra de desenho e contorno ativos");
    }
}

bool sendPlayerLifecycleMessage(const std::string& message) {
    if (message.empty()) return false;
    using namespace bl::il2cpp;
    auto& runtime = api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    const MethodInfo* say = main ? findMethodBySignature(
        main, parseSignature(
            "void NewText(string newText, byte R, byte G, byte B, bool onlyCurrentPlayer)"))
        : nullptr;
    if (!say || !runtime.string_new || !runtime.runtime_invoke) return false;
    Il2CppString* line = runtime.string_new(message.c_str());
    if (!line) return false;
    uint8_t r = 255, g = 200, b = 0, only = 1;
    void* args[5] = {line, &r, &g, &b, &only};
    Il2CppObject* error = nullptr;
    runtime.runtime_invoke(say, nullptr, args, &error);
    return !error;
}

void hkPlayerSpawn(Il2CppObject* self, int32_t context, const MethodInfo* method) {
    if (g_originalPlayerSpawn) g_originalPlayerSpawn(self, context, method);
    if (!self) return;
    static int spawnDiagnostics = 0;
    if (spawnDiagnostics < 6) {
        ++spawnDiagnostics;
        BL_INFO("csharp BunnyPlayer Spawn: context=%d revive=%d policies=%d callbacks=%zu",
                context, g_reviveSpawnContext, g_playerPoliciesReady ? 1 : 0,
                g_respawnMessages.size());
    }
    if (!g_playerPoliciesReady || context != g_reviveSpawnContext ||
        g_respawnMessages.empty()) return;
    using namespace bl::il2cpp;
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    auto& runtime = api();
    const MethodInfo* getter = main && runtime.class_get_method_from_name
        ? runtime.class_get_method_from_name(main, "get_LocalPlayer", 0)
        : nullptr;
    if (!getter || !runtime.runtime_invoke) {
        BL_WARN("csharp BunnyPlayer OnRespawn: getter Main.LocalPlayer indisponivel");
        return;
    }
    Il2CppObject* error = nullptr;
    Il2CppObject* localPlayer = runtime.runtime_invoke(getter, nullptr, nullptr, &error);
    if (error || !localPlayer) {
        BL_WARN("csharp BunnyPlayer OnRespawn: Main.LocalPlayer indisponivel no Spawn");
        return;
    }
    const int32_t who = g_refs.playerWhoAmI >= 0
        ? bl::runtime::field<int32_t>(self, g_refs.playerWhoAmI) : -1;
    const int32_t localWho = g_refs.playerWhoAmI >= 0
        ? bl::runtime::field<int32_t>(localPlayer, g_refs.playerWhoAmI) : -1;
    const bool localMatch = self == localPlayer ||
        (who >= 0 && localWho >= 0 && who == localWho);
    BL_INFO("csharp BunnyPlayer OnRespawn: jogador=%d local=%d corresponde=%d",
            who, localWho, localMatch ? 1 : 0);
    if (!localMatch) return;
    int count = 0;
    for (const auto& [name, message] : g_respawnMessages)
        if (sendPlayerLifecycleMessage(message)) ++count;
    if (count > 0) g_deadEventDelivered = false;
    BL_INFO("csharp BunnyPlayer OnRespawn: %d mensagem(ns) enviadas(s)", count);
}

void hkPlayerUpdate(Il2CppObject* self, int32_t index, const MethodInfo* method) {
    static bool loggedPre = false;
    static bool loggedPost = false;
    if (self && g_playerPoliciesReady && g_preUpdateEnabled && !loggedPre) {
        loggedPre = true;
        BL_INFO("csharp BunnyPlayer PreUpdate: politica nativa executada");
    }
    if (g_originalPlayerUpdate)
        g_originalPlayerUpdate(self, index, method);
    if (self && g_playerPoliciesReady && g_postUpdateEnabled && !loggedPost) {
        loggedPost = true;
        BL_INFO("csharp BunnyPlayer PostUpdate: politica nativa executada");
    }
}

void hkPlayerUpdateDead(Il2CppObject* self, const MethodInfo* method) {
    if (g_originalUpdateDead) g_originalUpdateDead(self, method);
    if (!self || !g_playerPoliciesReady || g_deadEventDelivered ||
        g_deadMessages.empty()) return;
    using namespace bl::il2cpp;
    auto& runtime = api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    const MethodInfo* getter = main && runtime.class_get_method_from_name
        ? runtime.class_get_method_from_name(main, "get_LocalPlayer", 0)
        : nullptr;
    if (!getter || !runtime.runtime_invoke) return;
    Il2CppObject* error = nullptr;
    Il2CppObject* local = runtime.runtime_invoke(getter, nullptr, nullptr, &error);
    if (error || local != self) return;
    int delivered = 0;
    for (const auto& [key, message] : g_deadMessages)
        if (sendPlayerLifecycleMessage(message)) ++delivered;
    if (delivered > 0) g_deadEventDelivered = true;
    BL_INFO("csharp BunnyPlayer UpdateDead: %d mensagem(ns) enviadas(s)", delivered);
}

int equippedRegeneration(Il2CppObject* self, bool mana) {
    if (!self || g_refs.playerArmor < 0 || g_refs.itemAccessory < 0 ||
        g_refs.itemType < 0) return 0;
    auto* armor = bl::runtime::field<Il2CppArray*>(self, g_refs.playerArmor);
    if (!armor || armor->length < 10 || armor->length > 40) return 0;
    auto** equipped = static_cast<Il2CppObject**>(arrayData(armor));
    int sum = 0;
    for (uintptr_t slot = 3; slot < 10; ++slot) {
        Il2CppObject* item = equipped[slot];
        if (!item || !bl::runtime::field<uint8_t>(item, g_refs.itemAccessory))
            continue;
        const int type = bl::runtime::field<int32_t>(item, g_refs.itemType);
        auto it = g_accessories.find(type);
        if (it == g_accessories.end()) continue;
        sum += mana ? it->second.manaRegenBonus : it->second.lifeRegenBonus;
    }
    return sum < 0 ? 0 : (sum > 100 ? 100 : sum);
}

void hkPlayerLifeRegen(Il2CppObject* self, const MethodInfo* method) {
    if (g_originalLifeRegen) g_originalLifeRegen(self, method);
    if (!self || !g_playerPoliciesReady || !g_anyLifeRegen ||
        g_refs.playerLifeRegen < 0) return;
    const int bonus = equippedRegeneration(self, false);
    if (!bonus) return;
    bl::runtime::field<int32_t>(self, g_refs.playerLifeRegen) += bonus;
    static int logged = 0;
    if (logged++ < 2)
        BL_INFO("csharp BunnyPlayer UpdateLifeRegen: bonus nativo +%d", bonus);
}

void hkPlayerManaRegen(Il2CppObject* self, const MethodInfo* method) {
    if (g_originalManaRegen) g_originalManaRegen(self, method);
    if (!self || !g_playerPoliciesReady || !g_anyManaRegen ||
        g_refs.playerManaRegen < 0) return;
    const int bonus = equippedRegeneration(self, true);
    if (!bonus) return;
    bl::runtime::field<int32_t>(self, g_refs.playerManaRegen) += bonus;
    static int logged = 0;
    if (logged++ < 2)
        BL_INFO("csharp BunnyPlayer UpdateManaRegen: bonus nativo +%d", bonus);
}

void hkUpdateBuffs(Il2CppObject* self, int32_t index, const MethodInfo* method) {
    if (g_originalUpdateBuffs) g_originalUpdateBuffs(self, index, method);
    if (!self || g_buffs.empty()) return;
    Il2CppArray* types = bl::runtime::field<Il2CppArray*>(self, g_refs.playerBuffType);
    Il2CppArray* times = bl::runtime::field<Il2CppArray*>(self, g_refs.playerBuffTime);
    if (!types || !times) return;
    const uintptr_t count = std::min(types->length, times->length);
    if (count == 0 || count > 60 || index < 0 ||
        static_cast<uintptr_t>(index) >= count) return;
    const auto* typeValues = reinterpret_cast<const int32_t*>(arrayData(types));
    const auto* timeValues = reinterpret_cast<const int32_t*>(arrayData(times));
    const auto slot = static_cast<uintptr_t>(index);
    if (timeValues[slot] <= 0) return;
    const auto it = g_buffs.find(typeValues[slot]);
    if (it != g_buffs.end())
        bl::runtime::field<int32_t>(self, g_refs.playerStatDefense) +=
            it->second.defenseBonus;
}

}

bool prepareNativeContent() {
    if (g_ready) return true;
    using namespace bl::il2cpp;
    Il2CppClass* item = findClass({"Terraria", "Item", {}});
    Il2CppClass* projectile = findClass({"Terraria", "Projectile", {}});
    Il2CppClass* npc = findClass({"Terraria", "NPC", {}});
    Il2CppClass* player = findClass({"Terraria", "Player", {}});
    if (!item || !projectile || !npc || !player) return false;

    NativeRefs r{};
    r.itemDamage = fieldOffset(item, "damage");
    r.itemUseTime = fieldOffset(item, "useTime");
    r.itemUseAnimation = fieldOffset(item, "useAnimation");
    r.itemUseStyle = fieldOffset(item, "useStyle");
    r.itemShoot = fieldOffset(item, "shoot");
    r.itemShootSpeed = fieldOffset(item, "shootSpeed");
    r.itemKnockBack = fieldOffset(item, "knockBack");
    r.itemRanged = fieldOffset(item, "ranged");
    r.itemNoMelee = fieldOffset(item, "noMelee");
    r.itemAutoReuse = fieldOffset(item, "autoReuse");
    r.itemRare = fieldOffset(item, "rare");
    r.itemMaxStack = fieldOffset(item, "maxStack");
    r.projAiStyle = fieldOffset(projectile, "aiStyle");
    r.projFriendly = fieldOffset(projectile, "friendly");
    r.projHostile = fieldOffset(projectile, "hostile");
    r.projRanged = fieldOffset(projectile, "ranged");
    r.projPenetrate = fieldOffset(projectile, "penetrate");
    r.projTimeLeft = fieldOffset(projectile, "timeLeft");
    r.projIgnoreWater = fieldOffset(projectile, "ignoreWater");
    r.projTileCollide = fieldOffset(projectile, "tileCollide");
    r.projWidth = fieldOffset(projectile, "width");
    r.projHeight = fieldOffset(projectile, "height");
    r.itemAccessory = fieldOffset(item, "accessory");
    r.itemBuffType = fieldOffset(item, "buffType");
    r.itemBuffTime = fieldOffset(item, "buffTime");
    r.itemConsumable = fieldOffset(item, "consumable");
    r.itemType = fieldOffset(item, "type");
    r.npcWidth = fieldOffset(npc, "width");
    r.npcHeight = fieldOffset(npc, "height");
    r.npcDamage = fieldOffset(npc, "damage");
    r.npcDefense = fieldOffset(npc, "defense");
    r.npcLifeMax = fieldOffset(npc, "lifeMax");
    r.npcAiStyle = fieldOffset(npc, "aiStyle");
    r.npcType = fieldOffset(npc, "type");
    r.playerStatDefense = fieldOffset(player, "statDefense");
    r.playerMoveSpeed = fieldOffset(player, "moveSpeed");
    r.playerArmor = fieldOffset(player, "armor");
    r.playerMaxRunSpeed = fieldOffset(player, "maxRunSpeed");
    r.playerRunAcceleration = fieldOffset(player, "runAcceleration");
    r.playerFrameShadow = fieldOffset(player, "armorEffectDrawShadow");
    r.playerFrameOutline = fieldOffset(player, "armorEffectDrawOutlines");
    r.playerWhoAmI = fieldOffset(player, "whoAmI");
    r.playerLifeRegen = fieldOffset(player, "lifeRegen");
    r.playerManaRegen = fieldOffset(player, "manaRegen");
    r.playerBuffType = fieldOffset(player, "buffType");
    r.playerBuffTime = fieldOffset(player, "buffTime");

    const int* all = &r.itemDamage;
    for (size_t i = 0; i < offsetof(NativeRefs, itemAccessory) / sizeof(int); ++i) {
        if (all[i] < 0) {
            BL_ERROR("csharp native content: campo %zu indisponivel", i);
            return false;
        }
    }
    const MethodInfo* itemMethod = findMethodBySignature(
        item, parseSignature("void SetDefaults(int Type, ItemVariant variant)"));
    const MethodInfo* projectileMethod = findMethodBySignature(
        projectile, parseSignature("void SetDefaults(int Type)"));
    if (!itemMethod || !projectileMethod) return false;

    g_refs = r;
    if (!bl::hook::install(itemMethod, hkItemDefaults, &g_originalItem)) return false;
    if (!bl::hook::install(projectileMethod, hkProjectileDefaults, &g_originalProjectile)) return false;
 
    g_ready = true;
    BL_INFO("csharp native content: hooks C++ de item e projetil instalados");
    return true;
}

bool prepareNativeNpc() {
    if (g_npcReady) return true;
    using namespace bl::il2cpp;
    Il2CppClass* npc = findClass({"Terraria", "NPC", {}});
    if (!npc || g_refs.npcWidth < 0 || g_refs.npcHeight < 0 ||
        g_refs.npcDamage < 0 || g_refs.npcDefense < 0 ||
        g_refs.npcLifeMax < 0 || g_refs.npcAiStyle < 0) return false;
    const MethodInfo* method = findMethodBySignature(
        npc, parseSignature("void SetDefaults(int Type, NPCSpawnParams spawnparams)"));
    if (!method) return false;
    const Il2CppType* p = api().method_get_param(method, 1);
    Il2CppClass* cls = p ? api().class_from_il2cpp_type(p) : nullptr;
    uint32_t align = 0;
    const int size = cls ? api().class_value_size(cls, &align) : 0;
    if (size <= 16 || size > 1024) {
        BL_ERROR("csharp native: NPCSpawnParams ABI nao verificada (size=%d)", size);
        return false;
    }
    if (!bl::hook::install(method, hkNpcDefaults, &g_originalNpc)) return false;
    g_npcReady = true;
    BL_INFO("csharp native: NPC defaults registrado com ABI por referencia (size=%d)", size);
    return true;
}

bool prepareNativePlayerEffects() {
    if (g_effectsReady) return true;
    using namespace bl::il2cpp;
    const auto& r = g_refs;
    if (r.playerStatDefense < 0 || r.playerMoveSpeed < 0 ||
        r.playerArmor < 0 || r.playerMaxRunSpeed < 0 ||
        r.playerRunAcceleration < 0 || r.playerFrameShadow < 0 ||
        r.playerFrameOutline < 0 ||
        r.playerBuffType < 0 ||
        r.playerBuffTime < 0 || r.itemType < 0 ||
        r.itemAccessory < 0 || r.itemBuffType < 0 ||
        r.itemBuffTime < 0 || r.itemConsumable < 0) return false;
    Il2CppClass* player = findClass({"Terraria", "Player", {}});
    if (!player) return false;
    const MethodInfo* equip = findMethodBySignature(
        player, parseSignature("void ApplyEquipFunctional(int itemSlot, Item currentItem)"));
    const MethodInfo* buffs = findMethodBySignature(
        player, parseSignature("void UpdateBuffs(int i)"));
    const MethodInfo* runSpeeds = findMethodBySignature(
        player, parseSignature("void HorizontalMovement()"));
    const MethodInfo* armorVisuals = findMethodBySignature(
        player, parseSignature("void SetArmorEffectVisuals(Player drawPlayer)"));
    if (!equip || !buffs || !runSpeeds || !armorVisuals) return false;
    if (!bl::hook::install(equip, hkApplyEquip, &g_originalEquip)) return false;
    if (!bl::hook::install(buffs, hkUpdateBuffs, &g_originalUpdateBuffs)) return false;
    if (!bl::hook::install(runSpeeds, hkHorizontalMovement,
                           &g_originalHorizontalMovement)) return false;
    if (!bl::hook::install(armorVisuals, hkSetArmorEffectVisuals,
                           &g_originalSetArmorEffectVisuals)) return false;
    const MethodInfo* spawn = findMethodBySignature(
        player, parseSignature("void Spawn(PlayerSpawnContext context)"));
    if (!spawn)
        spawn = findMethodBySignature(
            player, parseSignature("void Spawn(PlayerSpawnContext)"));
    if (spawn) {
        if (!bl::hook::install(spawn, hkPlayerSpawn, &g_originalPlayerSpawn))
            BL_WARN("csharp BunnyPlayer OnRespawn: hook nao instalado");
        else BL_INFO("csharp BunnyPlayer OnRespawn: hook instalado; reviveContext=%d",
                     g_reviveSpawnContext);
    } else {
        BL_WARN("csharp BunnyPlayer OnRespawn: assinatura de Player.Spawn indisponivel");
    }
    const MethodInfo* update = findMethodBySignature(
        player, parseSignature("void Update(int i)"));
    const MethodInfo* dead = findMethodBySignature(
        player, parseSignature("void UpdateDead()"));
    const MethodInfo* lifeRegen = findMethodBySignature(
        player, parseSignature("void UpdateLifeRegen()"));
    const MethodInfo* manaRegen = findMethodBySignature(
        player, parseSignature("void UpdateManaRegen()"));
    if (update && bl::hook::install(update, hkPlayerUpdate, &g_originalPlayerUpdate))
        BL_INFO("csharp BunnyPlayer PreUpdate/PostUpdate: hooks instalados");
    else BL_WARN("csharp BunnyPlayer PreUpdate/PostUpdate: hook indisponivel");
    if (dead && bl::hook::install(dead, hkPlayerUpdateDead, &g_originalUpdateDead))
        BL_INFO("csharp BunnyPlayer UpdateDead: hook instalado");
    else BL_WARN("csharp BunnyPlayer UpdateDead: hook indisponivel");
    if (r.playerLifeRegen >= 0 && lifeRegen &&
        bl::hook::install(lifeRegen, hkPlayerLifeRegen, &g_originalLifeRegen))
        BL_INFO("csharp BunnyPlayer UpdateLifeRegen: hook instalado");
    else BL_WARN("csharp BunnyPlayer UpdateLifeRegen: hook indisponivel");
    if (r.playerManaRegen >= 0 && manaRegen &&
        bl::hook::install(manaRegen, hkPlayerManaRegen, &g_originalManaRegen))
        BL_INFO("csharp BunnyPlayer UpdateManaRegen: hook instalado");
    else BL_WARN("csharp BunnyPlayer UpdateManaRegen: hook indisponivel");
    g_effectsReady = true;
    BL_INFO("csharp native: hooks de acessorio e buff instalados");
    return true;
}


bool applyManagedNativeDefaults(
    const std::string& modId, const std::string& projectileName, int projectileType,
    const ManagedProjectileDefaults& projectile, const std::string& weaponName,
    int weaponType, const ManagedWeaponDefaults& weapon) {
    if (!g_ready ||
        bl::runtime::modProjectileTypeByName(modId, projectileName) != projectileType ||
        bl::runtime::modItemTypeByKey(modId + "/" + weaponName) != weaponType ||
        g_projectiles.count(projectileType) == 0 || g_weapons.count(weaponType) == 0 ||
        projectile.width < 1 || projectile.width > 512 ||
        projectile.height < 1 || projectile.height > 512 ||
        projectile.aiStyle < 0 || projectile.aiStyle > 255 ||
        projectile.penetrate < -1 || projectile.penetrate > 100 ||
        projectile.timeLeft < 1 || projectile.timeLeft > 36000 ||
        !projectile.friendly ||
        weapon.damage < 1 || weapon.damage > 1000 ||
        weapon.useTime < 2 || weapon.useTime > 120 ||
        !std::isfinite(weapon.shootSpeed) ||
        weapon.shootSpeed < 1.f || weapon.shootSpeed > 40.f)
        return false;

    g_projectiles[projectileType] = {
        projectile.width, projectile.height, projectile.aiStyle,
        projectile.penetrate, projectile.timeLeft, true, false
    };
    g_weapons[weaponType] = {
        weapon.damage, weapon.useTime, weapon.shootSpeed, projectileType
    };
    BL_INFO("csharp native: managed SetDefaults applied for %s/%s and %s/%s",
            modId.c_str(), projectileName.c_str(), modId.c_str(), weaponName.c_str());
    return true;
}

bool applyManagedNativeDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedProjectileEntry>& projectiles,
    const std::vector<ManagedWeaponEntry>& weapons) {
    if (!g_ready || projectiles.empty() || weapons.empty() ||
        projectiles.size() > 16 || weapons.size() > 16) return false;

    std::map<int, NativeProjectileDefaults> stagedProjectiles;
    std::map<int, NativeWeaponDefaults> stagedWeapons;
    for (const auto& entry : projectiles) {
        const auto& data = entry.values;
        if (entry.type < 0 ||
            bl::runtime::modProjectileTypeByName(modId, entry.name) != entry.type ||
            g_projectiles.count(entry.type) != 1 ||
            !stagedProjectiles.emplace(entry.type, NativeProjectileDefaults{
                data.width, data.height, data.aiStyle, data.penetrate,
                data.timeLeft, true, false}).second ||
            data.width < 1 || data.width > 512 ||
            data.height < 1 || data.height > 512 ||
            data.aiStyle < 0 || data.aiStyle > 255 ||
            data.penetrate < -1 || data.penetrate > 100 ||
            data.timeLeft < 1 || data.timeLeft > 36000 ||
            !data.friendly)
            return false;
    }
    for (const auto& entry : weapons) {
        const auto& data = entry.values;
        if (entry.type < 0 ||
            bl::runtime::modItemTypeByKey(modId + "/" + entry.name) != entry.type ||
            g_weapons.count(entry.type) != 1 ||
            stagedProjectiles.count(entry.shootType) != 1 ||
            !stagedWeapons.emplace(entry.type, NativeWeaponDefaults{
                data.damage, data.useTime, data.shootSpeed, entry.shootType}).second ||
            data.damage < 1 || data.damage > 1000 ||
            data.useTime < 2 || data.useTime > 120 ||
            !std::isfinite(data.shootSpeed) ||
            data.shootSpeed < 1.f || data.shootSpeed > 40.f)
            return false;
    }
    for (const auto& item : stagedProjectiles)
        g_projectiles[item.first] = item.second;
    for (const auto& item : stagedWeapons) {
        g_weapons[item.first] = item.second;
        enableManagedPrefixEligibility(item.first);
    }
    BL_INFO("csharp native: managed batch applied for %s (%zu projectile, %zu weapon)",
            modId.c_str(), stagedProjectiles.size(), stagedWeapons.size());
    return true;
}

bool applyManagedNativeBuffDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedBuffEntry>& buffs,
    const std::vector<ManagedPotionEntry>& potions) {
    if (!g_effectsReady) return false;
    for (const auto& buff : buffs) {
        const auto it = g_buffs.find(buff.type);
        if (it == g_buffs.end() ||
            bl::runtime::modBuffTypeByName(modId, buff.name) != buff.type ||
            buff.defenseBonus < -50 || buff.defenseBonus > 100)
            return false;
    }
    for (const auto& potion : potions) {
        const auto it = g_potions.find(potion.type);
        if (it == g_potions.end() ||
            bl::runtime::modItemTypeByKey(modId + "/" + potion.name) != potion.type ||
            it->second.buffType != potion.buffType ||
            potion.duration < 1 || potion.duration > 216000)
            return false;
    }
    for (const auto& buff : buffs)
        g_buffs[buff.type].defenseBonus = buff.defenseBonus;
    for (const auto& potion : potions)
        g_potions[potion.type].duration = potion.duration;
    BL_INFO("csharp native: managed buffs applied for %s (%zu buffs, %zu potions)",
            modId.c_str(), buffs.size(), potions.size());
    return true;
}

bool applyManagedNativePlayerDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedAccessoryEntry>& accessories,
    const std::vector<ManagedPlayerEntry>& players) {
    if (!g_effectsReady || accessories.size() > 16 || players.size() > 16)
        return false;
    std::map<int, NativeAccessoryDefaults> values;
    for (const auto& accessory : accessories) {
        if (accessory.type < 0 || accessory.defenseBonus < 0 ||
            accessory.defenseBonus > 100 || accessory.tooltip.size() > 180 ||
            bl::runtime::modItemTypeByKey(modId + "/" + accessory.name) != accessory.type ||
            g_accessories.count(accessory.type) != 1 ||
            !values.emplace(accessory.type,
                NativeAccessoryDefaults{accessory.defenseBonus, 0, 0, false, ""}).second)
            return false;
    }
    std::map<int, bool> linked;
    std::map<std::string, std::string> globalMessages;
    std::map<std::string, std::string> respawnMessages;
    std::map<std::string, std::string> deadMessages;
    bool newPreUpdate = false;
    bool newPostUpdate = false;
    for (const auto& player : players) {
        const std::string key = modId + "/" + player.name;
        if (player.respawnMessage.size() > 100 || player.deadMessage.size() > 100 ||
            player.lifeRegenBonus < 0 || player.lifeRegenBonus > 30 ||
            player.manaRegenBonus < 0 || player.manaRegenBonus > 30) return false;
        if (player.preUpdate) newPreUpdate = true;
        if (player.postUpdate) newPostUpdate = true;
        if (!player.deadMessage.empty()) deadMessages[key] = player.deadMessage;
        if (player.accessoryType < 0) {
            if (player.accessoryName != "-" || player.equippedDefenseBonus != 0 ||
                player.moveSpeedPercent != 0 || player.runAccelerationPercent != 0 ||
                player.frameShadow || player.worldJoinMessage.size() > 100 ||
                player.lifeRegenBonus != 0 || player.manaRegenBonus != 0) return false;
            if (!player.worldJoinMessage.empty())
                globalMessages[key] = player.worldJoinMessage;
            if (!player.respawnMessage.empty())
                respawnMessages[key] = player.respawnMessage;
            continue;
        }
        if (
            player.equippedDefenseBonus < 0 || player.equippedDefenseBonus > 100 ||
            player.moveSpeedPercent < 0 || player.moveSpeedPercent > 50 ||
            player.runAccelerationPercent < 0 || player.runAccelerationPercent > 30 ||
            player.worldJoinMessage.size() > 100 ||
            bl::runtime::modItemTypeByKey(modId + "/" + player.accessoryName) != player.accessoryType ||
            !values.contains(player.accessoryType) ||
            !linked.emplace(player.accessoryType, true).second)
            return false;
        auto& bonus = values[player.accessoryType];
        if (bonus.defenseBonus + player.equippedDefenseBonus > 100) return false;
        bonus.defenseBonus += player.equippedDefenseBonus;
        bonus.moveSpeedPercent = player.moveSpeedPercent;
        bonus.movementAccelerationPercent = player.runAccelerationPercent;
        bonus.frameShadow = player.frameShadow;
        bonus.worldJoinMessage = player.worldJoinMessage;
        bonus.lifeRegenBonus = player.lifeRegenBonus;
        bonus.manaRegenBonus = player.manaRegenBonus;
        if (!player.respawnMessage.empty())
            respawnMessages[key] = player.respawnMessage;
    }
    for (const auto& accessory : accessories) {
        if (!accessory.tooltip.empty())
            bl::runtime::setModItemTooltip(accessory.type, {{"", accessory.tooltip}});
    }
    for (const auto& [type, effects] : values) {
        g_accessories[type] = effects;
        enableManagedPrefixEligibility(type);
    }
    for (const auto& [key, message] : globalMessages)
        g_globalWorldMessages[key] = message;
    for (const auto& [key, message] : respawnMessages)
        g_respawnMessages[key] = message;
    for (const auto& [key, message] : deadMessages)
        g_deadMessages[key] = message;
    g_preUpdateEnabled = g_preUpdateEnabled || newPreUpdate;
    g_postUpdateEnabled = g_postUpdateEnabled || newPostUpdate;
    for (const auto& [type, effect] : g_accessories) {
        g_anyLifeRegen = g_anyLifeRegen || effect.lifeRegenBonus > 0;
        g_anyManaRegen = g_anyManaRegen || effect.manaRegenBonus > 0;
    }
    g_playerPoliciesReady = true;
    BL_INFO("csharp native: managed player equipment applied for %s (%zu accessory, %zu player)",
            modId.c_str(), accessories.size(), players.size());
    return true;
}

bool applyManagedNativeNpcDefaultsBatch(
    const std::string& modId,
    const std::vector<ManagedNpcEntry>& npcs) {
    if (npcs.empty()) return true;
    if (!g_npcReady || npcs.size() > 16) return false;
    std::map<int, NativeNpcDefaults> staged;
    for (const auto& npc : npcs) {
        if (npc.type < 0 ||
            bl::runtime::modNpcTypeByName(modId, npc.name) != npc.type ||
            g_npcs.count(npc.type) != 1 ||
            npc.width < 8 || npc.width > 256 ||
            npc.height < 8 || npc.height > 256 ||
            npc.damage < 0 || npc.damage > 500 ||
            npc.defense < 0 || npc.defense > 200 ||
            npc.lifeMax < 1 || npc.lifeMax > 100000 ||
            npc.aiStyle != 1 || npc.animationType != 1 ||
            npc.frames != 2 ||
            !staged.emplace(npc.type, NativeNpcDefaults{
                npc.width, npc.height, npc.damage, npc.defense,
                npc.lifeMax, npc.aiStyle}).second) return false;
    }
    for (const auto& entry : staged)
        g_npcs[entry.first] = entry.second;
    BL_INFO("csharp native: managed NPC defaults applied for %s (%zu NPC)",
            modId.c_str(), staged.size());
    return true;
}

bool showManagedPlayerWorldJoinMessages() {
    if (!g_playerPoliciesReady) return false;
    using namespace bl::il2cpp;
    auto& runtime = api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    const MethodInfo* say = main ? findMethodBySignature(
        main, parseSignature(
            "void NewText(string newText, byte R, byte G, byte B, bool onlyCurrentPlayer)"))
        : nullptr;
    if (!say || !runtime.string_new) {
        BL_WARN("csharp BunnyPlayer OnEnterWorld: Terraria.Main.NewText indisponivel");
        return true;
    }
    int shown = 0;
    for (const auto& [key, message] : g_globalWorldMessages)
        if (sendPlayerLifecycleMessage(message)) ++shown;
    for (const auto& [type, effects] : g_accessories) {
        if (effects.worldJoinMessage.empty()) continue;
        Il2CppString* line = runtime.string_new(effects.worldJoinMessage.c_str());
        if (!line) continue;
        uint8_t r = 255, g = 200, b = 0, only = 1;
        void* arguments[5] = {line, &r, &g, &b, &only};
        Il2CppObject* error = nullptr;
        runtime.runtime_invoke(say, nullptr, arguments, &error);
        if (!error) ++shown;
    }
    BL_INFO("csharp BunnyPlayer OnEnterWorld: %d mensagem(ns) enviada(s) ao entrar", shown);
    return true;
}

int managedNativeAccessoryDefense(int type) {
    const auto it = g_accessories.find(type);
    return it == g_accessories.end() ? 0 : it->second.defenseBonus;
}

int managedNativeAccessoryMoveSpeedPercent(int type) {
    const auto it = g_accessories.find(type);
    return it == g_accessories.end() ? 0 : it->second.moveSpeedPercent;
}

int managedNativePrefixItemKind(int type) {
    if (g_weapons.contains(type)) return 1;
    if (g_accessories.contains(type)) return 2;
    return 0;
}

void refreshManagedPrefixEligibilityOnWorldReady() {
    int corrected = 0;
    for (const auto& weapon : g_weapons)
        if (enableManagedPrefixEligibility(weapon.first)) ++corrected;
    for (const auto& accessory : g_accessories)
        if (enableManagedPrefixEligibility(accessory.first)) ++corrected;
    BL_INFO("csharp prefix: mundo pronto; elegibilidade C# revista para %zu item(ns), %d corrigido(s)",
            g_weapons.size() + g_accessories.size(), corrected);
}

int registerNativeContent(void* context, const NativeContentSpec* spec) {
    if (!g_ready || !context || !spec || !validName(spec->name) ||
        !validTexture(spec->texture)) return -1;
    auto& ctx = *static_cast<NativeRegisterContext*>(context);
    const std::string path = ctx.modDir + "/content/" + spec->texture;
    if (!fileExists(path)) {
        BL_ERROR("csharp native: textura inexistente %s", path.c_str());
        return -1;
    }
    const std::string display = spec->displayName && *spec->displayName
        ? std::string(spec->displayName).substr(0, 128)
        : std::string(spec->name);
    if (spec->kind == NativeProjectile) {
        if (spec->width < 1 || spec->width > 512 ||
            spec->height < 1 || spec->height > 512 ||
            spec->timeLeft < 1 || spec->timeLeft > 36000 ||
            spec->aiStyle < 0 || spec->aiStyle > 255 ||
            spec->penetrate < -1 || spec->penetrate > 100) return -1;
        bl::runtime::ModProjectileDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names.push_back({"", display});
        const int type = bl::runtime::registerModProjectile(std::move(def));
        if (type >= 0) {
            g_projectiles[type] = {spec->width, spec->height, spec->aiStyle,
                                   spec->penetrate, spec->timeLeft, true, false};
            ++ctx.projectiles;
        }
        return type;
    }
    if (spec->kind == NativeNpc) {
        if (!prepareNativeNpc()) return -1;
        if (spec->width < 8 || spec->width > 256 ||
            spec->height < 8 || spec->height > 256 ||
            spec->damage < 0 || spec->damage > 500 ||
            spec->useTime < 0 || spec->useTime > 200 ||
            spec->shootType < 1 || spec->shootType > 100000 ||
            spec->aiStyle < 0 || spec->aiStyle > 200 ||
            spec->penetrate < 0 || spec->penetrate >= bl::runtime::kVanillaNpcCount ||
            spec->timeLeft < 1 || spec->timeLeft > 30) return -1;
        bl::runtime::ModNpcDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names.push_back({"", display});
        def.frames = spec->timeLeft;
        def.animationType = spec->penetrate;
        const int type = bl::runtime::registerModNpc(std::move(def));
        if (type >= 0) {
            g_npcs[type] = {spec->width, spec->height, spec->damage,
                            spec->useTime, spec->shootType, spec->aiStyle};
            if (ctx.firstNpcType < 0) ctx.firstNpcType = type;
            ++ctx.npcs;
        }
        return type;
    }
    if (spec->kind == NativeBuff) {
        if (!prepareNativePlayerEffects()) return -1;
        if (spec->damage < -50 || spec->damage > 100) return -1;
        bl::runtime::ModBuffDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names.push_back({"", display});
        const int type = bl::runtime::registerModBuff(std::move(def));
        if (type >= 0) {
            g_buffs[type] = {spec->damage};
            if (ctx.firstBuffType < 0) ctx.firstBuffType = type;
            ++ctx.buffs;
        }
        return type;
    }
    if (spec->kind == NativeAccessory) {
        if (!prepareNativePlayerEffects()) return -1;
        if (spec->damage < 0 || spec->damage > 100) return -1;
        bl::runtime::ModItemDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names.push_back({"", display});
        const int type = bl::runtime::registerModItem(std::move(def));
        if (type >= 0) {
            g_accessories[type] = {spec->damage, 0, 0, false, ""};
            if (ctx.firstAccessoryType < 0) ctx.firstAccessoryType = type;
            ++ctx.items;
            ++ctx.accessories;
        }
        return type;
    }
    if (spec->kind == NativeBuffPotion) {
        if (!prepareNativePlayerEffects()) return -1;
        if (spec->timeLeft < 1 || spec->timeLeft > 216000 ||
            bl::runtime::modBuffTypeByName(ctx.modId, spec->name) >= 0) return -1;
        if (g_buffs.count(spec->shootType) == 0) return -1;
        bl::runtime::ModItemDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names.push_back({"", display});
        const int type = bl::runtime::registerModItem(std::move(def));
        if (type >= 0) {
            g_potions[type] = {spec->shootType, spec->timeLeft};
            ++ctx.items;
            ++ctx.potions;
        }
        return type;
    }
    if (spec->kind == NativeWeapon) {
        if (spec->damage < 1 || spec->damage > 1000 ||
            spec->useTime < 2 || spec->useTime > 120 ||
            spec->shootSpeed < 1.f || spec->shootSpeed > 40.f ||
            !bl::runtime::modProjectileBelongsTo(spec->shootType, ctx.modId)) return -1;
        bl::runtime::ModItemDef def{};
        def.mod = ctx.modId;
        def.name = spec->name;
        def.texture = path;
        def.names.push_back({"", display});
        const int type = bl::runtime::registerModItem(std::move(def));
        if (type >= 0) {
            g_weapons[type] = {spec->damage, spec->useTime, spec->shootSpeed, spec->shootType};
            ++ctx.items;
        }
        return type;
    }
    return -1;
}

}
