#pragma once

// Estado de DamageClass por jogador no frame atual e bonus de classe vindos
// de itens de mod. Tudo roda na thread do jogo (Player.Update/ItemCheck);
// nenhuma chamada ao Mono por frame.
//
// Ciclo do frame de cada jogador (Terraria.Player):
//   ResetEffects        -> reset(): zera o rastreio; o critico comum apos o
//                          reset (4) é o padrao do Generic.
//   fontes de bonus      -> beginSource()/endSource() em volta de
//                          ApplyEquipFunctional, GrantPrefixBenefits,
//                          GrantArmorBenefits, UpdateArmorSets e UpdateBuffs.
//                          A parte comum a todos os campos vira Generic.
//   bonus de mod         -> addGeneric()/addCustom() direto, fora da medicao.
//   GetWeapon*/ApplyItem*-> read()/custom() para calcular a classe.

#include "experiments/csharp/NativeDamageClassCombat.h"

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp::damage_combat {

inline constexpr int kMaxPlayers = 256;

struct PlayerFrame {
    GenericTrack generic;
    std::map<std::string, ClassBonus> custom;  // "mod|C:Namespace.Tipo"
    std::array<float, 4> beforeDamage{};
    std::array<float, 3> beforeCrit{};
    int depth = 0;
    bool active = false;
};

inline std::string customKey(std::string_view mod, std::string_view cls) {
    std::string key;
    key.reserve(mod.size() + 1 + cls.size());
    key.append(mod).push_back('|');
    key.append(cls);
    return key;
}

class PlayerTracker {
    std::array<PlayerFrame, kMaxPlayers> frames_{};

    PlayerFrame* frame(int who) {
        return who >= 0 && who < kMaxPlayers ? &frames_[static_cast<size_t>(who)] : nullptr;
    }
    const PlayerFrame* frame(int who) const {
        return who >= 0 && who < kMaxPlayers ? &frames_[static_cast<size_t>(who)] : nullptr;
    }

public:
    void reset(int who, const std::array<float, 3>& critAfterReset) {
        auto* f = frame(who);
        if (!f) return;
        f->generic = {};
        f->generic.crit = std::min({critAfterReset[0], critAfterReset[1], critAfterReset[2]});
        if (!std::isfinite(f->generic.crit)) f->generic.crit = 0.f;
        f->custom.clear();
        f->depth = 0;
        f->active = true;
    }

    // Fontes aninhadas (ex.: prefixo dentro do equipamento) contam como uma.
    void beginSource(int who, const std::array<float, 4>& damage, const std::array<float, 3>& crit) {
        auto* f = frame(who);
        if (!f || !f->active) return;
        if (f->depth++ == 0) {
            f->beforeDamage = damage;
            f->beforeCrit = crit;
        }
    }

    void endSource(int who, const std::array<float, 4>& damage, const std::array<float, 3>& crit) {
        auto* f = frame(who);
        if (!f || !f->active || f->depth <= 0) return;
        if (--f->depth != 0) return;
        const float mul = f->generic.damageMultiplier > 0.f ? f->generic.damageMultiplier : 1.f;
        const float d = commonDelta(f->beforeDamage, damage) / mul;
        const float c = commonDelta(f->beforeCrit, crit);
        if (finite(d)) f->generic.damage += d;
        if (finite(c)) f->generic.crit += c;
    }

    void addGeneric(int who, float additive, float multiplier, float crit) {
        auto* f = frame(who);
        if (!f || !f->active) return;
        if (finite(additive)) f->generic.damage += additive;
        if (finite(multiplier) && multiplier > 0.f) f->generic.damageMultiplier *= multiplier;
        if (finite(crit)) f->generic.crit += crit;
    }

    void addCustom(int who, const std::string& key, const ClassBonus& bonus) {
        auto* f = frame(who);
        if (!f || !f->active || !bonus.valid() || key.size() > 512) return;
        if (f->custom.size() >= 256 && !f->custom.count(key)) return;
        f->custom[key].add(bonus);
    }

    bool read(int who, GenericTrack* generic) const {
        const auto* f = frame(who);
        if (!f || !f->active || !generic) return false;
        *generic = f->generic;
        return true;
    }

    ClassBonus custom(int who, const std::string& key) const {
        const auto* f = frame(who);
        if (!f || !f->active) return {};
        const auto it = f->custom.find(key);
        return it == f->custom.end() ? ClassBonus{} : it->second;
    }

    int depth(int who) const {
        const auto* f = frame(who);
        return f ? f->depth : -1;
    }
};

inline PlayerTracker& playerTracker() {
    static PlayerTracker tracker;
    return tracker;
}

// Classes aceitas num bonus de equipamento no Terraria Android.
//   V1 Generic, V2 Melee, V4 Ranged, V5 Magic, V6 Summon: campos vanilla.
//   C:Namespace.Tipo: classe customizada do mesmo mod.
// As outras vanilla (Default, MeleeNoSpeed, SummonMeleeSpeed, MagicSummonHybrid,
// Throwing) nao tem campo proprio no jogo mobile e sao recusadas no registro.
inline bool classBonusKeySupported(std::string_view key) {
    return key == "V1" || key == "V2" || key == "V4" || key == "V5" || key == "V6" ||
           (key.size() > 2 && key.size() <= 242 && key.starts_with("C:"));
}

// Validacao por classe: o que nao tem campo vanilla precisa ser neutro.
inline const char* classBonusIssue(std::string_view key, const ClassBonus& b) {
    if (!classBonusKeySupported(key)) return "UnsupportedDamageClass";
    if (!b.valid()) return "NonFiniteValue";
    if (b.damage.multiplicative <= 0.f || b.knockback.multiplicative <= 0.f) return "NonPositiveMultiplier";
    if (key.starts_with("C:")) return nullptr;
    const bool neutralKnockback = b.knockback.base == 0.f && b.knockback.additive == 1.f &&
                                  b.knockback.multiplicative == 1.f && b.knockback.flat == 0.f;
    if (!neutralKnockback) return "VanillaKnockbackUnsupported";
    if (b.damage.base != 0.f || b.damage.flat != 0.f) return "VanillaFlatDamageUnsupported";
    if (key == "V6" && b.crit != 0.f) return "SummonCritUnsupported";
    if (key != "V2" && b.attackSpeed != 0.f) return "VanillaAttackSpeedOnlyMelee";
    if (key != "V1" && b.armorPenetration != 0.f) return "VanillaArmorPenetrationOnlyGeneric";
    return nullptr;
}

struct ClassBonusRow {
    std::string mod;
    int itemType = -1;
    std::string classKey;
    ClassBonus bonus;
};

class ClassBonusRegistry {
public:
    using Table = std::map<int, std::vector<ClassBonusRow>>;

    bool publish(const std::string& mod, const std::vector<ClassBonusRow>& rows, std::string* error) {
        Table incoming;
        for (const auto& row : rows) {
            const char* issue = row.itemType < 0 ? "InvalidItem" :
                row.mod != mod ? "OwnerMismatch" : classBonusIssue(row.classKey, row.bonus);
            if (issue) {
                if (error) *error = std::string(issue) + " Class=" + row.classKey +
                                    " ItemType=" + std::to_string(row.itemType);
                return false;
            }
            auto& list = incoming[row.itemType];
            if (list.size() >= 16) {
                if (error) *error = "TooManyBonusesPerItem ItemType=" + std::to_string(row.itemType);
                return false;
            }
            list.push_back(row);
        }
        std::lock_guard guard(mutex_);
        auto next = std::make_shared<Table>(*std::atomic_load_explicit(&active_, std::memory_order_acquire));
        for (auto& [type, list] : incoming) {
            if (next->count(type)) {
                if (error) *error = "ItemAlreadyHasBonuses ItemType=" + std::to_string(type);
                return false;
            }
            (*next)[type] = list;
        }
        std::atomic_store_explicit(&active_, std::shared_ptr<const Table>(std::move(next)),
                                   std::memory_order_release);
        return true;
    }

    std::shared_ptr<const Table> snapshot() const {
        return std::atomic_load_explicit(&active_, std::memory_order_acquire);
    }

    void unload(const std::string& mod) {
        std::lock_guard guard(mutex_);
        auto next = std::make_shared<Table>(*std::atomic_load_explicit(&active_, std::memory_order_acquire));
        for (auto it = next->begin(); it != next->end();) {
            auto& list = it->second;
            list.erase(std::remove_if(list.begin(), list.end(),
                [&](const ClassBonusRow& r) { return r.mod == mod; }), list.end());
            it = list.empty() ? next->erase(it) : std::next(it);
        }
        std::atomic_store_explicit(&active_, std::shared_ptr<const Table>(std::move(next)),
                                   std::memory_order_release);
    }

    void reset() {
        std::lock_guard guard(mutex_);
        std::atomic_store_explicit(&active_, std::make_shared<const Table>(), std::memory_order_release);
    }

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const Table> active_ = std::make_shared<const Table>();
};

inline ClassBonusRegistry& classBonuses() {
    static ClassBonusRegistry registry;
    return registry;
}

// Linha V2: CLASS_BONUS <Item> <classe> <dmgBase> <dmgAdd> <dmgMul> <dmgFlat>
//           <kbBase> <kbAdd> <kbMul> <kbFlat> <crit> <attackSpeed> <armorPen>
inline constexpr size_t kClassBonusWireFields = 14;

}  // namespace bl::csharp::damage_combat
