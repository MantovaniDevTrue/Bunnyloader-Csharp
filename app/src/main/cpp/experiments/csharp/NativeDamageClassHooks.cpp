#include "experiments/csharp/NativeDamageClassHooks.h"

#include "experiments/csharp/NativeDamageClassPlayerStats.h"
#include "experiments/csharp/NativeDamageClassTooltip.h"
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
#include <random>
#include <utility>
#include <string>
#include <vector>

// DamageClass no Terraria.Player real (Android 1.4.5, IL2CPP).
// Assinaturas conferidas no global-metadata v31 do runtime 301720:
//   ResetEffects(), ApplyEquipFunctional(int itemSlot, Item currentItem),
//   GrantPrefixBenefits(Item item), GrantArmorBenefits(Item armorPiece),
//   UpdateArmorSets(int i), UpdateBuffs(int i), ApplyItemAnimation(Item sItem),
//   ApplyItemTime(Item sItem), Projectile.Damage_PVE(ref Rectangle projRectangle,
//   float projectileSpecificDamageMultiplier), NPC.StrikeNPC(int Damage,
//   float knockBack, int hitDirection, bool crit, bool fromNet, int owner),
//   Main.MouseText_DrawItemTooltip_GetLinesInfo(...).

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

void installHitAndTooltip(Il2CppClass* player, Il2CppClass* item);

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
    installHitAndTooltip(player, item);
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

namespace {

// ---- critico de projetil e tooltip ----
// O Damage_PVE do celular rola o critico pelos campos meleeCrit/rangedCrit/
// magicCrit conforme a flag do projetil; projetil de classe customizada nao
// tem flag e nunca critaria. Como o Projectile.CritChance do tModLoader, o
// critico vem de GetTotalCritChance(classe) + crit da arma de origem.

struct HitRefs {
    FieldInfo* mainPlayer = nullptr;
    const MethodInfo* localPlayer = nullptr;  // Main.get_LocalPlayer (myPlayer é propriedade no Android)
    FieldInfo* langTip = nullptr;
    const MethodInfo* localizedValue = nullptr;
    const MethodInfo* exists = nullptr;
    const MethodInfo* getText = nullptr;
    const MethodInfo* heldItem = nullptr;
    const MethodInfo* weaponDamage = nullptr;
    const MethodInfo* weaponCrit = nullptr;
    const MethodInfo* weaponKnockback = nullptr;
    int projType = -1, projOwner = -1, itemKnockBack = -1;
};
HitRefs h;

using PveFn = void (*)(Il2CppObject*, void*, float, const MethodInfo*);
using StrikeFn = int32_t (*)(Il2CppObject*, int32_t, float, int32_t, bool, bool, int32_t, const MethodInfo*);
using LinesFn = void (*)(Il2CppObject*, int32_t*, int32_t*, int32_t*, float, int32_t*, Il2CppArray*,
                         Il2CppArray*, Il2CppArray*, int32_t*, void*, int32_t*, const MethodInfo*);
PveFn g_pve = nullptr;
StrikeFn g_strike = nullptr;
LinesFn g_lines = nullptr;
std::atomic<bool> g_critReady{false};
std::atomic<bool> g_tooltipReady{false};
std::atomic<int> g_critSamples{0};
std::atomic<int> g_tooltipSamples{0};
thread_local Il2CppObject* t_projectile = nullptr;

Il2CppObject* playerAt(int index) {
    auto& api = il2cpp::api();
    if (!h.mainPlayer || index < 0 || !api.field_static_get_value) return nullptr;
    Il2CppArray* players = nullptr;
    api.field_static_get_value(h.mainPlayer, &players);
    if (!players || static_cast<uintptr_t>(index) >= players->length) return nullptr;
    return static_cast<Il2CppObject**>(arrayData(players))[index];
}

Il2CppObject* localPlayer() {
    auto& api = il2cpp::api();
    if (!h.localPlayer || !api.runtime_invoke) return nullptr;
    Il2CppObject* error = nullptr;
    Il2CppObject* player = api.runtime_invoke(h.localPlayer, nullptr, nullptr, &error);
    return error ? nullptr : player;
}

std::u16string u16(const Il2CppString* text) {
    if (!text || text->length <= 0 || text->length > 512) return {};
    return std::u16string(text->chars, static_cast<size_t>(text->length));
}

std::string utf8(const std::u16string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        uint32_t c = text[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (text[i + 1] - 0xDC00);
            ++i;
        } else if (c >= 0xD800 && c <= 0xDFFF) continue;
        if (c < 0x80) out.push_back(static_cast<char>(c));
        else if (c < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (c >> 12)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (c >> 18)));
            out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

// Lang.tip[index].Value
std::u16string langTip(int index) {
    auto& api = il2cpp::api();
    if (!h.langTip || !h.localizedValue || !api.field_static_get_value || !api.runtime_invoke) return {};
    Il2CppArray* tips = nullptr;
    api.field_static_get_value(h.langTip, &tips);
    if (!tips || index < 0 || static_cast<uintptr_t>(index) >= tips->length) return {};
    Il2CppObject* entry = static_cast<Il2CppObject**>(arrayData(tips))[index];
    if (!entry) return {};
    Il2CppObject* error = nullptr;
    auto* value = reinterpret_cast<Il2CppString*>(api.runtime_invoke(h.localizedValue, entry, nullptr, &error));
    return error ? std::u16string{} : u16(value);
}

// Mods.<mod>.DamageClasses.<Tipo>.DisplayName, se o mod localizou.
std::u16string classDisplayName(const NativeDamageTypeBinding& binding) {
    auto& api = il2cpp::api();
    if (!h.exists || !h.getText || !api.runtime_invoke || !api.string_new ||
        !binding.damageClass.starts_with("C:")) return {};
    const std::string full = binding.damageClass.substr(2);
    const size_t dot = full.find_last_of(".+");
    const std::string name = dot == std::string::npos ? full : full.substr(dot + 1);
    const std::string key = "Mods." + binding.mod + ".DamageClasses." + name + ".DisplayName";
    void* args[] = {api.string_new(key.c_str())};
    if (!args[0]) return {};
    Il2CppObject* error = nullptr;
    Il2CppObject* boxed = api.runtime_invoke(h.exists, nullptr, args, &error);
    if (error || !boxed || !*reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(boxed) + sizeof(Il2CppObject)))
        return {};
    error = nullptr;
    auto* value = reinterpret_cast<Il2CppString*>(api.runtime_invoke(h.getText, nullptr, args, &error));
    return error ? std::u16string{} : u16(value);
}

// Player.GetWeapon* pela cadeia de hooks inteira (o valor que o jogo usaria).
template <typename T>
bool invokeWeapon(const MethodInfo* method, Il2CppObject* player, void** args, T* out) {
    auto& api = il2cpp::api();
    if (!method || !player || !api.runtime_invoke || !out) return false;
    Il2CppObject* error = nullptr;
    Il2CppObject* boxed = api.runtime_invoke(method, player, args, &error);
    if (error || !boxed) return false;
    std::memcpy(out, reinterpret_cast<char*>(boxed) + sizeof(Il2CppObject), sizeof(T));
    return true;
}

Il2CppObject* heldItem(Il2CppObject* player) {
    auto& api = il2cpp::api();
    if (!h.heldItem || !player || !api.runtime_invoke) return nullptr;
    Il2CppObject* error = nullptr;
    Il2CppObject* item = api.runtime_invoke(h.heldItem, player, nullptr, &error);
    return error ? nullptr : item;
}

thread_local std::minstd_rand t_rng{std::random_device{}()};

bool rollProjectileCrit(Il2CppObject* projectile, int* typeOut, float* chanceOut) {
    if (!g_ready.load(std::memory_order_acquire) || h.projType < 0 || h.projOwner < 0) return false;
    const int type = field<int32_t>(projectile, h.projType);
    *typeOut = type;
    NativeDamageTypeBinding binding;
    if (!nativeDamageTypes().isCustom(true, type) || !nativeDamageTypes().lookup(true, type, &binding) ||
        !binding.damageClass.starts_with("C:")) return false;
    NativeDamageClassStatDefaults stats;
    if (!nativeDamageClassStats().lookup(binding.mod, binding.damageClass, &stats) || !stats.standardCrit)
        return false;
    Il2CppObject* player = playerAt(field<int32_t>(projectile, h.projOwner));
    const int who = playerIndex(player);
    GenericTrack generic;
    if (who < 0 || !playerTracker().read(who, &generic)) return false;
    const auto own = playerTracker().custom(who, customKey(binding.mod, binding.damageClass));
    float chance = totalCrit(stats, own, vanilla(player), generic);
    // Crit da arma de origem: a arma segurada, se for da mesma classe.
    if (Il2CppObject* item = heldItem(player); item && g.itemCrit >= 0) {
        NativeDamageCombatSample sample;
        if (resolveNativeDamageCombatSample(itemType(item), &sample) &&
            sample.source.mod == binding.mod && sample.source.damageClass == binding.damageClass)
            chance += static_cast<float>(field<int32_t>(item, g.itemCrit));
    }
    if (!std::isfinite(chance)) return false;
    *chanceOut = chance;
    std::uniform_int_distribution<int> roll(0, 99);
    return static_cast<float>(roll(t_rng)) < chance;
}

void hkPve(Il2CppObject* self, void* rect, float multiplier, const MethodInfo* m) {
    Il2CppObject* previous = t_projectile;
    t_projectile = self;
    if (g_pve) g_pve(self, rect, multiplier, m);
    t_projectile = previous;
}

int32_t hkStrike(Il2CppObject* npc, int32_t damage, float knockback, int32_t direction,
                 bool crit, bool fromNet, int32_t owner, const MethodInfo* m) {
    if (!crit && !fromNet && t_projectile && g_critReady.load(std::memory_order_acquire)) {
        int type = -1;
        float chance = 0.f;
        const bool rolled = rollProjectileCrit(t_projectile, &type, &chance);
        if (rolled) crit = true;
        if (chance != 0.f && g_critSamples.fetch_add(1, std::memory_order_relaxed) < 6)
            BL_INFO("[CSharp][DamageClass][Combat] Stage=ProjectileCrit ProjectileType=%d Chance=%.1f Result=%s",
                    type, chance, rolled ? "Crit" : "NoCrit");
    }
    return g_strike ? g_strike(npc, damage, knockback, direction, crit, fromNet, owner, m) : 0;
}

void hkLines(Il2CppObject* item, int32_t* yoyo, int32_t* research, int32_t* materials, float oldKb,
             int32_t* count, Il2CppArray* lines, Il2CppArray* prefix, Il2CppArray* bad,
             int32_t* setBonus, void* setColor, int32_t* shared, const MethodInfo* m) {
    if (g_lines) g_lines(item, yoyo, research, materials, oldKb, count, lines, prefix, bad,
                         setBonus, setColor, shared, m);
    if (!g_tooltipReady.load(std::memory_order_acquire) || !g_ready.load(std::memory_order_acquire) ||
        !item || !count || !lines || !prefix || !bad || g.itemDamage < 0) return;
    NativeDamageCombatSample sample;
    if (!resolveNativeDamageCombatSample(itemType(item), &sample)) return;
    Il2CppObject* player = localPlayer();
    if (!player) return;
    auto& api = il2cpp::api();
    if (!api.string_new || !api.gc_wbarrier_set_field) return;
    const size_t capacity = std::min({lines->length, prefix->length, bad->length});
    if (*count <= 0 || static_cast<size_t>(*count) > capacity) return;

    TooltipInput in;
    in.itemDamage = field<int32_t>(item, g.itemDamage);
    in.show = sample.defaults.tooltipLines;
    void* itemArg[] = {item};
    if (!invokeWeapon(h.weaponDamage, player, itemArg, &in.damage)) return;
    in.className = classDisplayName(sample.source);
    in.critSuffix = langTip(5);
    if (sample.defaults.standardCrit && !in.critSuffix.empty() && in.critSuffix[0] == u'%')
        in.hasCrit = invokeWeapon(h.weaponCrit, player, itemArg, &in.crit);
    for (int i = 0; i < 8; ++i) in.speedTexts[static_cast<size_t>(i)] = langTip(6 + i);
    for (int i = 0; i < 9; ++i) in.knockbackTexts[static_cast<size_t>(i)] = langTip(14 + i);
    float speed = 1.f;
    int32_t frames = 0;
    if (g.useAnimation >= 0 && customSpeed(player, item, &speed) &&
        animationFrames(field<int32_t>(item, g.useAnimation), speed, &frames))
        in.speedIndex = speedTooltipIndex(frames);
    if (h.itemKnockBack >= 0) {
        float base = field<float>(item, h.itemKnockBack), knockback = 0.f;
        void* kbArgs[] = {item, &base};
        if (std::isfinite(base) && invokeWeapon(h.weaponKnockback, player, kbArgs, &knockback))
            in.knockbackIndex = knockbackTooltipIndex(knockback);
    }

    auto** strings = static_cast<Il2CppString**>(arrayData(lines));
    auto* prefixFlags = static_cast<uint8_t*>(arrayData(prefix));
    auto* badFlags = static_cast<uint8_t*>(arrayData(bad));
    std::vector<TooltipLine> current;
    current.reserve(static_cast<size_t>(*count));
    for (int i = 0; i < *count; ++i)
        current.push_back({u16(strings[i]), prefixFlags[i] != 0, badFlags[i] != 0});
    const auto before = current;
    const bool rewritten = rewriteDamageClassTooltip(current, capacity, in);
    if (g_tooltipSamples.fetch_add(1, std::memory_order_relaxed) < 3)
        BL_INFO("[CSharp][DamageClass][Combat] Stage=Tooltip ItemType=%d Damage=%d Crit=%d Lines=%d->%zu Show=%u Named=%d Result=%s",
                itemType(item), in.damage, in.hasCrit ? in.crit : -1, *count, current.size(),
                in.show, in.className.empty() ? 0 : 1, rewritten ? "Rewritten" : "Unrecognized");
    if (!rewritten) return;
    for (size_t i = 0; i < current.size(); ++i) {
        if (i < before.size() && before[i].text == current[i].text) {
            prefixFlags[i] = current[i].prefix; badFlags[i] = current[i].bad;
            continue;
        }
        Il2CppString* value = api.string_new(utf8(current[i].text).c_str());
        if (!value) return;
        api.gc_wbarrier_set_field(reinterpret_cast<Il2CppObject*>(lines),
                                  reinterpret_cast<void**>(&strings[i]), value);
        prefixFlags[i] = current[i].prefix;
        badFlags[i] = current[i].bad;
    }
    *count = static_cast<int32_t>(current.size());
}

void installHitAndTooltip(Il2CppClass* player, Il2CppClass* item) {
    using namespace il2cpp;
    auto& api = il2cpp::api();
    Il2CppClass* main = findClass({"Terraria", "Main", {}});
    Il2CppClass* projectile = findClass({"Terraria", "Projectile", {}});
    Il2CppClass* npc = findClass({"Terraria", "NPC", {}});
    Il2CppClass* lang = findClass({"Terraria", "Lang", {}});
    Il2CppClass* language = findClass({"Terraria.Localization", "Language", {}});
    Il2CppClass* localized = findClass({"Terraria.Localization", "LocalizedText", {}});
    if (main) {
        h.mainPlayer = findField(main, "player");
        h.localPlayer = findMethodBySignature(main, parseSignature("Player get_LocalPlayer()"));
    }
    if (lang) h.langTip = findField(lang, "tip");
    if (localized) h.localizedValue = findMethodBySignature(localized, parseSignature("string get_Value()"));
    if (language) {
        h.exists = findMethodBySignature(language, parseSignature("bool Exists(string key)"));
        h.getText = findMethodBySignature(language, parseSignature("string GetTextValue(string key)"));
    }
    h.heldItem = findMethodBySignature(player, parseSignature("Item get_HeldItem()"));
    h.weaponDamage = findMethodBySignature(player, parseSignature("int GetWeaponDamage(Item sItem)"));
    h.weaponCrit = findMethodBySignature(player, parseSignature("int GetWeaponCrit(Item sItem)"));
    h.weaponKnockback = findMethodBySignature(player, parseSignature("float GetWeaponKnockback(Item sItem, float KnockBack)"));
    bind(item, "knockBack", "System.Single", 4, &h.itemKnockBack);
    const bool players = h.mainPlayer && api.field_static_get_value;
    // Cada referencia ausente aparece no log: sem isso o recurso some calado.
    const std::pair<const char*, bool> refs[] = {
        {"Main.player", h.mainPlayer != nullptr}, {"Main.get_LocalPlayer", h.localPlayer != nullptr},
        {"Lang.tip", h.langTip != nullptr}, {"LocalizedText.get_Value", h.localizedValue != nullptr},
        {"Language.Exists", h.exists != nullptr}, {"Language.GetTextValue", h.getText != nullptr},
        {"Player.get_HeldItem", h.heldItem != nullptr}, {"Player.GetWeaponDamage", h.weaponDamage != nullptr},
        {"Player.GetWeaponCrit", h.weaponCrit != nullptr}, {"Player.GetWeaponKnockback", h.weaponKnockback != nullptr},
        {"Terraria.Projectile", projectile != nullptr}, {"Terraria.NPC", npc != nullptr}};
    for (const auto& [name, found] : refs)
        if (!found) BL_WARN("[CSharp][DamageClass][Combat] Stage=Resolve Member=%s Result=Missing", name);

    bool crit = false;
    if (projectile && npc && players &&
        bind(projectile, "type", "System.Int32", 4, &h.projType) &&
        bind(projectile, "owner", "System.Int32", 4, &h.projOwner) &&
        hookMethod(projectile, "void Damage_PVE(ref Rectangle projRectangle, float projectileSpecificDamageMultiplier)", hkPve, &g_pve) &&
        hookMethod(npc, "int StrikeNPC(int Damage, float knockBack, int hitDirection, bool crit, bool fromNet, int owner)", hkStrike, &g_strike)) {
        crit = true;
        g_critReady.store(true, std::memory_order_release);
    }
    bool tooltip = false;
    if (main && h.localPlayer && h.langTip && h.localizedValue && h.weaponDamage && h.weaponCrit && h.weaponKnockback &&
        api.runtime_invoke && api.string_new && api.gc_wbarrier_set_field) {
        const auto* lines = findMethodBySignature(main, parseSignature(
            "void MouseText_DrawItemTooltip_GetLinesInfo(Item item, ref int yoyoLogo, ref int researchLine, ref int materialsLine, float oldKB, ref int numLines, string[] toolTipLine, bool[] preFixLine, bool[] badPreFixLine, ref int setBonusLine, ref Color setBonusColour, ref int sharedLine)"));
        tooltip = lines && hook::install(lines, hkLines, &g_lines);
        if (tooltip) g_tooltipReady.store(true, std::memory_order_release);
        else BL_WARN("[CSharp][DamageClass][Combat] Stage=Hook Method=\"GetLinesInfo\" Result=Unavailable");
    }
    BL_INFO("[CSharp][DamageClass][Combat] Stage=InstallHit ProjectileCrit=%d Tooltip=%d", crit ? 1 : 0, tooltip ? 1 : 0);
}

}  // namespace

}  // namespace bl::csharp::damage_combat
