#include "experiments/csharp/NativeDamageClassHooks.h"

#include "experiments/csharp/NativeDamageClassPlayerStats.h"
#include "experiments/csharp/NativeDamageCombatProbe.h"
#include "content/common/GameRefs.h"
#include "core/Log.h"
#include "hook/HookManager.h"
#include "il2cpp/Api.h"
#include "il2cpp/FieldContract.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <array>
#include <atomic>
#include <cstring>
#include <string>

// DamageClass no Terraria.Player real (Android 1.4.5, IL2CPP).
// Assinaturas conferidas no global-metadata v31 do runtime 301720:
//   ResetEffects(), ApplyEquipFunctional(int itemSlot, Item currentItem),
//   GrantPrefixBenefits(Item item), GrantArmorBenefits(Item armorPiece),
//   UpdateArmorSets(int i), UpdateBuffs(int i), ApplyItemAnimation(Item sItem),
//   ApplyItemTime(Item sItem).

namespace bl::csharp::damage_combat {
namespace {

using runtime::field;

struct Offsets {
    std::array<int, 4> damage{-1, -1, -1, -1};  // meleeDamage rangedDamage magicDamage minionDamage
    std::array<int, 3> crit{-1, -1, -1};        // meleeCrit rangedCrit magicCrit
    int meleeSpeed = -1, whoAmI = -1, armorPenetration = -1;
    int itemAnimation = -1, itemAnimationMax = -1, itemTime = -1, itemTimeMax = -1;
    int itemType = -1, useAnimation = -1, useTime = -1, itemDamage = -1, itemCrit = -1;
    std::array<int, 4> itemFlags{-1, -1, -1, -1};  // melee ranged magic summon
};
Offsets g;
std::atomic<bool> g_ready{false};
std::atomic<bool> g_animationReady{false};

using ResetFn = void (*)(Il2CppObject*, const MethodInfo*);
using EquipFn = void (*)(Il2CppObject*, int32_t, Il2CppObject*, const MethodInfo*);
using ItemFn = void (*)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
using IndexFn = void (*)(Il2CppObject*, int32_t, const MethodInfo*);
ResetFn g_reset = nullptr;
EquipFn g_equip = nullptr;
ItemFn g_prefix = nullptr, g_armor = nullptr, g_animation = nullptr, g_time = nullptr;
IndexFn g_sets = nullptr, g_buffs = nullptr;

int playerIndex(Il2CppObject* player) {
    if (!player || g.whoAmI < 0) return -1;
    const int who = field<int32_t>(player, g.whoAmI);
    return who >= 0 && who < kMaxPlayers ? who : -1;
}

std::array<float, 4> readDamage(Il2CppObject* p) {
    return {field<float>(p, g.damage[0]), field<float>(p, g.damage[1]),
            field<float>(p, g.damage[2]), field<float>(p, g.damage[3])};
}
std::array<float, 3> readCrit(Il2CppObject* p) {
    return {static_cast<float>(field<int32_t>(p, g.crit[0])),
            static_cast<float>(field<int32_t>(p, g.crit[1])),
            static_cast<float>(field<int32_t>(p, g.crit[2]))};
}

struct SourceScope {
    Il2CppObject* player;
    int who;
    explicit SourceScope(Il2CppObject* p) : player(p), who(g_ready.load(std::memory_order_acquire) ? playerIndex(p) : -1) {
        if (who >= 0) playerTracker().beginSource(who, readDamage(player), readCrit(player));
    }
    ~SourceScope() {
        if (who >= 0) playerTracker().endSource(who, readDamage(player), readCrit(player));
    }
};

int itemType(Il2CppObject* item) {
    return item && g.itemType >= 0 ? field<int32_t>(item, g.itemType) : -1;
}

// Bonus de classe do item equipado: classes vanilla escrevem nos campos do
// jogador (itens vanilla tambem se beneficiam); Generic tambem entra no
// rastreio; classes customizadas vao para o canal do jogador.
void applyClassBonuses(Il2CppObject* player, Il2CppObject* item) {
    const int who = playerIndex(player);
    const int type = itemType(item);
    if (who < 0 || type < 0) return;
    const auto table = classBonuses().snapshot();
    const auto found = table->find(type);
    if (found == table->end()) return;
    GenericTrack generic;
    if (!playerTracker().read(who, &generic)) return;
    for (const auto& row : found->second) {
        const auto& b = row.bonus;
        if (row.classKey.starts_with("C:")) {
            playerTracker().addCustom(who, customKey(row.mod, row.classKey), b);
            continue;
        }
        const float scale = generic.damageMultiplier > 0.f ? generic.damageMultiplier : 1.f;
        const float add = (b.damage.additive - 1.f) * scale;
        auto bump = [&](int damageIndex) {
            float& value = field<float>(player, g.damage[static_cast<size_t>(damageIndex)]);
            value = (value + add) * b.damage.multiplicative;
        };
        auto critAdd = [&](int critIndex) {
            int32_t& value = field<int32_t>(player, g.crit[static_cast<size_t>(critIndex)]);
            value += static_cast<int32_t>(b.crit);
        };
        if (row.classKey == "V1") {
            for (int i = 0; i < 4; ++i) bump(i);
            for (int i = 0; i < 3; ++i) critAdd(i);
            if (g.armorPenetration >= 0)
                field<int32_t>(player, g.armorPenetration) += static_cast<int32_t>(b.armorPenetration);
            playerTracker().addGeneric(who, b.damage.additive - 1.f, b.damage.multiplicative, b.crit);
            generic.damageMultiplier *= b.damage.multiplicative;
        } else if (row.classKey == "V2") {
            bump(0); critAdd(0);
            if (g.meleeSpeed >= 0) field<float>(player, g.meleeSpeed) += b.attackSpeed;
        } else if (row.classKey == "V4") {
            bump(1); critAdd(1);
        } else if (row.classKey == "V5") {
            bump(2); critAdd(2);
        } else if (row.classKey == "V6") {
            bump(3);
        }
    }
}

void hkReset(Il2CppObject* self, const MethodInfo* m) {
    if (g_reset) g_reset(self, m);
    const int who = g_ready.load(std::memory_order_acquire) ? playerIndex(self) : -1;
    if (who >= 0) playerTracker().reset(who, readCrit(self));
}

void hkEquip(Il2CppObject* self, int32_t slot, Il2CppObject* item, const MethodInfo* m) {
    { SourceScope scope(self); if (g_equip) g_equip(self, slot, item, m); }
    if (g_ready.load(std::memory_order_acquire)) applyClassBonuses(self, item);
}
void hkPrefix(Il2CppObject* self, Il2CppObject* item, const MethodInfo* m) {
    SourceScope scope(self); if (g_prefix) g_prefix(self, item, m);
}
void hkArmor(Il2CppObject* self, Il2CppObject* item, const MethodInfo* m) {
    { SourceScope scope(self); if (g_armor) g_armor(self, item, m); }
    if (g_ready.load(std::memory_order_acquire)) applyClassBonuses(self, item);
}
void hkSets(Il2CppObject* self, int32_t i, const MethodInfo* m) {
    SourceScope scope(self); if (g_sets) g_sets(self, i, m);
}
void hkBuffs(Il2CppObject* self, int32_t i, const MethodInfo* m) {
    SourceScope scope(self); if (g_buffs) g_buffs(self, i, m);
}

// Velocidade total da classe customizada do item, se houver.
bool customSpeed(Il2CppObject* player, Il2CppObject* item, float* speed) {
    const int who = playerIndex(player);
    const int type = itemType(item);
    NativeDamageCombatSample sample;
    if (who < 0 || type < 0 || !resolveNativeDamageCombatSample(type, &sample)) return false;
    GenericTrack generic;
    if (!playerTracker().read(who, &generic)) return false;
    VanillaStats v;
    if (g.meleeSpeed >= 0) v.meleeSpeed = field<float>(player, g.meleeSpeed);
    const auto own = playerTracker().custom(who, customKey(sample.source.mod, sample.source.damageClass));
    *speed = totalAttackSpeed(sample.defaults, own, v);
    return std::isfinite(*speed) && *speed > 0.01f;
}

void hkAnimation(Il2CppObject* self, Il2CppObject* item, const MethodInfo* m) {
    if (g_animation) g_animation(self, item, m);
    float speed = 1.f;
    int32_t frames = 0;
    if (!g_animationReady.load(std::memory_order_acquire) || !customSpeed(self, item, &speed) ||
        !animationFrames(field<int32_t>(item, g.useAnimation), speed, &frames)) return;
    field<int32_t>(self, g.itemAnimation) = frames;
    field<int32_t>(self, g.itemAnimationMax) = frames;
}

void hkTime(Il2CppObject* self, Il2CppObject* item, const MethodInfo* m) {
    if (g_time) g_time(self, item, m);
    float speed = 1.f;
    int32_t frames = 0;
    if (!g_animationReady.load(std::memory_order_acquire) || !customSpeed(self, item, &speed) ||
        !animationFrames(field<int32_t>(item, g.useTime), speed, &frames)) return;
    field<int32_t>(self, g.itemTime) = frames;
    field<int32_t>(self, g.itemTimeMax) = frames;
}

bool bind(Il2CppClass* owner, const char* name, const char* type, size_t width, int* out, bool required = true) {
    std::string error;
    int offset = -1;
    if (!il2cpp::nativeFieldContract(owner, name, type, false, width, nullptr, &offset, &error)) {
        if (required) BL_WARN("[CSharp][DamageClass][Combat] Stage=BindField %s Result=Disabled", error.c_str());
        return false;
    }
    *out = offset;
    return true;
}

template <typename Fn>
bool hookMethod(Il2CppClass* cls, const char* signature, Fn replacement, Fn* original) {
    const auto* method = cls ? il2cpp::findMethodBySignature(cls, il2cpp::parseSignature(signature)) : nullptr;
    if (!method || !il2cpp::api().method_is_instance || !il2cpp::api().method_is_instance(method) ||
        !hook::install(method, replacement, original)) {
        BL_WARN("[CSharp][DamageClass][Combat] Stage=Hook Method=\"%s\" Result=Unavailable", signature);
        return false;
    }
    return true;
}

}  // namespace

bool installNativeDamageClassTracking() {
    static bool attempted = false;
    if (attempted) return g_ready.load(std::memory_order_acquire);
    attempted = true;
    auto* player = il2cpp::findClass({"Terraria", "Player", {}});
    auto* item = il2cpp::findClass({"Terraria", "Item", {}});
    if (!player || !item) return false;
    const char* damageNames[] = {"meleeDamage", "rangedDamage", "magicDamage", "minionDamage"};
    const char* critNames[] = {"meleeCrit", "rangedCrit", "magicCrit"};
    const char* flagNames[] = {"melee", "ranged", "magic", "summon"};
    bool fields = bind(player, "whoAmI", "System.Int32", 4, &g.whoAmI) &&
                  bind(item, "type", "System.Int32", 4, &g.itemType);
    for (size_t i = 0; fields && i < 4; ++i) fields = bind(player, damageNames[i], "System.Single", 4, &g.damage[i]);
    for (size_t i = 0; fields && i < 3; ++i) fields = bind(player, critNames[i], "System.Int32", 4, &g.crit[i]);
    for (size_t i = 0; fields && i < 4; ++i) fields = bind(item, flagNames[i], "System.Boolean", 1, &g.itemFlags[i]);
    if (!fields) return false;
    bind(player, "meleeSpeed", "System.Single", 4, &g.meleeSpeed, false);
    bind(player, "armorPenetration", "System.Int32", 4, &g.armorPenetration, false);
    bind(item, "damage", "System.Int32", 4, &g.itemDamage, false);
    bind(item, "crit", "System.Int32", 4, &g.itemCrit, false);
    // ResetEffects é obrigatorio: sem ele o rastreio nunca fica ativo e os
    // hooks de arma devolvem o vanilla.
    if (!hookMethod(player, "void ResetEffects()", hkReset, &g_reset)) return false;
    g_ready.store(true, std::memory_order_release);
    int sources = 0;
    sources += hookMethod(player, "void ApplyEquipFunctional(int itemSlot, Item currentItem)", hkEquip, &g_equip);
    sources += hookMethod(player, "void GrantPrefixBenefits(Item item)", hkPrefix, &g_prefix);
    sources += hookMethod(player, "void GrantArmorBenefits(Item armorPiece)", hkArmor, &g_armor);
    sources += hookMethod(player, "void UpdateArmorSets(int i)", hkSets, &g_sets);
    sources += hookMethod(player, "void UpdateBuffs(int i)", hkBuffs, &g_buffs);
    const bool animationFields =
        bind(player, "itemAnimation", "System.Int32", 4, &g.itemAnimation) &&
        bind(player, "itemAnimationMax", "System.Int32", 4, &g.itemAnimationMax) &&
        bind(player, "itemTime", "System.Int32", 4, &g.itemTime) &&
        bind(player, "itemTimeMax", "System.Int32", 4, &g.itemTimeMax) &&
        bind(item, "useAnimation", "System.Int32", 4, &g.useAnimation) &&
        bind(item, "useTime", "System.Int32", 4, &g.useTime);
    int speedHooks = 0;
    if (animationFields) {
        speedHooks += hookMethod(player, "void ApplyItemAnimation(Item sItem)", hkAnimation, &g_animation);
        speedHooks += hookMethod(player, "void ApplyItemTime(Item sItem)", hkTime, &g_time);
        g_animationReady.store(speedHooks > 0, std::memory_order_release);
    }
    BL_INFO("[CSharp][DamageClass][Combat] Stage=Install Result=Ready Sources=%d/5 SpeedHooks=%d/2 MeleeSpeed=%d ArmorPen=%d",
            sources, speedHooks, g.meleeSpeed >= 0 ? 1 : 0, g.armorPenetration >= 0 ? 1 : 0);
    return true;
}

namespace {

struct Context {
    int who = -1;
    NativeDamageCombatSample sample;
    GenericTrack generic;
    ClassBonus own;
};

bool context(Il2CppObject* player, Il2CppObject* item, Context* out) {
    if (!g_ready.load(std::memory_order_acquire)) return false;
    out->who = playerIndex(player);
    const int type = itemType(item);
    if (out->who < 0 || type < 0 || !resolveNativeDamageCombatSample(type, &out->sample) ||
        !playerTracker().read(out->who, &out->generic)) return false;
    out->own = playerTracker().custom(out->who, customKey(out->sample.source.mod, out->sample.source.damageClass));
    return true;
}

VanillaStats vanilla(Il2CppObject* player) {
    VanillaStats v;
    v.damage = readDamage(player);
    v.crit = readCrit(player);
    if (g.meleeSpeed >= 0) v.meleeSpeed = field<float>(player, g.meleeSpeed);
    return v;
}

}  // namespace

bool customWeaponDamage(Il2CppObject* player, Il2CppObject* item, int32_t original, int32_t* out) {
    Context c;
    if (!out || !context(player, item, &c)) return false;
    // Base = Item.damage; sem o campo, o GetWeaponDamage vanilla de um item
    // sem flag de classe (o proprio dano base).
    const int32_t base = g.itemDamage >= 0 ? field<int32_t>(item, g.itemDamage) : original;
    if (base <= 0) return false;
    return weaponDamage(totalDamage(c.sample.defaults, c.own, vanilla(player), c.generic), base, out);
}

bool customWeaponCrit(Il2CppObject* player, Il2CppObject* item, int32_t original, int32_t* out) {
    Context c;
    if (!out || !context(player, item, &c)) return false;
    if (!c.sample.defaults.standardCrit) return false;
    // tModLoader: crit do item + GetTotalCritChance(classe).
    const int32_t base = g.itemCrit >= 0 ? field<int32_t>(item, g.itemCrit) : original;
    return weaponCrit(totalCrit(c.sample.defaults, c.own, vanilla(player), c.generic), base, out);
}

bool customWeaponKnockback(Il2CppObject* player, Il2CppObject* item, float knockback,
                           WeaponKnockbackFn vanillaFn, const MethodInfo* method, float* out) {
    Context c;
    if (!out || !vanillaFn || !context(player, item, &c)) return false;
    // Mede o knockback vanilla de cada classe chamando o original com a flag
    // da classe no proprio item (kb 0 e kb 1 -> mapa afim) e restaura as flags.
    std::array<uint8_t, 4> saved{};
    for (size_t i = 0; i < 4; ++i) saved[i] = field<uint8_t>(item, g.itemFlags[i]);
    std::array<Affine, 5> maps{};
    for (int source = 0; source < 5; ++source) {
        for (size_t i = 0; i < 4; ++i)
            field<uint8_t>(item, g.itemFlags[i]) = static_cast<uint8_t>(source > 0 && i == static_cast<size_t>(source - 1));
        const float at0 = vanillaFn(player, item, 0.f, method);
        const float at1 = vanillaFn(player, item, 1.f, method);
        if (std::isfinite(at0) && std::isfinite(at1)) maps[static_cast<size_t>(source)] = {at1 - at0, at0};
    }
    for (size_t i = 0; i < 4; ++i) field<uint8_t>(item, g.itemFlags[i]) = saved[i];
    return weaponKnockback(totalKnockback(c.sample.defaults, c.own, maps), knockback, out);
}

}  // namespace bl::csharp::damage_combat
