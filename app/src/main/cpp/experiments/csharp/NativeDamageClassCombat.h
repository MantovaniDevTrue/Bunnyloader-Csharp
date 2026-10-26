#pragma once

// Combate de DamageClass no Terraria Android, sem Mono por frame.
//
// Espelha tModLoader 1.4.5 (MIT): StatModifier (ApplyTo/CombineWith/Scale) e
// Player.TML.cs GetTotalDamage/GetTotalCritChance/GetTotalKnockback/
// GetTotalAttackSpeed. O Terraria mobile 1.4.5 nao tem allDamage: bonus
// genericos somam o mesmo valor em meleeDamage/rangedDamage/magicDamage/
// minionDamage (e meleeCrit/rangedCrit/magicCrit). O genérico é rastreado na
// origem: cada fonte (equipamento, prefixo, peca/conjunto de armadura, buff) é
// medida antes/depois, e a parte comum aos quatro campos conta como Generic.
//
// Linhas de heranca (NativeDamageClassStatDefaults::inheritance):
//   0 Generic, 1 Melee, 2 Ranged, 3 Magic, 4 Summon.

#include "experiments/csharp/NativeDamageClassStats.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace bl::csharp::damage_combat {

inline constexpr float kLimit = 100000.f;

inline bool finite(float value) { return std::isfinite(value) && std::abs(value) <= kLimit; }

struct Modifier {
    float base = 0.f;
    float additive = 1.f;
    float multiplicative = 1.f;
    float flat = 0.f;

    static Modifier from(const NativeDamageStatModifier& m) {
        return {m.base, m.additive, m.multiplicative, m.flat};
    }
    // StatModifier.CombineWith
    Modifier combine(const Modifier& o) const {
        return {base + o.base, additive + o.additive - 1.f,
                multiplicative * o.multiplicative, flat + o.flat};
    }
    // StatModifier.Scale
    Modifier scale(float s) const {
        return {base * s, 1.f + (additive - 1.f) * s,
                1.f + (multiplicative - 1.f) * s, flat * s};
    }
    // StatModifier.ApplyTo
    float applyTo(float value) const {
        return (value + base) * additive * multiplicative + flat;
    }
    bool valid() const {
        return finite(base) && finite(additive) && finite(multiplicative) && finite(flat);
    }
};

// Bonus de uma classe acumulados num jogador durante o frame (equipamentos e
// buffs de mod). Neutro = nenhum bonus.
struct ClassBonus {
    Modifier damage;
    Modifier knockback;
    float crit = 0.f;
    float attackSpeed = 0.f;   // excedente sobre 1 (tML: velocidade neutra = 1)
    float armorPenetration = 0.f;

    void add(const ClassBonus& o) {
        damage = damage.combine(o.damage);
        knockback = knockback.combine(o.knockback);
        crit += o.crit;
        attackSpeed += o.attackSpeed;
        armorPenetration += o.armorPenetration;
    }
    bool valid() const {
        return damage.valid() && knockback.valid() && finite(crit) &&
               finite(attackSpeed) && finite(armorPenetration);
    }
};

// Valores vanilla do jogador no momento do uso da arma.
struct VanillaStats {
    std::array<float, 4> damage{1.f, 1.f, 1.f, 1.f};  // melee, ranged, magic, minion
    std::array<float, 3> crit{0.f, 0.f, 0.f};         // melee, ranged, magic
    float meleeSpeed = 1.f;
};

// Genérico rastreado na origem neste frame.
struct GenericTrack {
    float damage = 0.f;            // soma aditiva comum aos quatro campos
    float crit = 0.f;              // inclui o padrao (4) capturado apos ResetEffects
    float damageMultiplier = 1.f;  // GetDamage(Generic) *= x vindo de bonus de mod
};

// Parte comum de uma fonte: se todos os campos mudaram no mesmo sentido, o
// menor modulo é a parte generica; senao a fonte é toda especifica de classe.
template <size_t N>
inline float commonDelta(const std::array<float, N>& before, const std::array<float, N>& after) {
    float common = 0.f;
    bool positive = false, negative = false;
    for (size_t i = 0; i < N; ++i) {
        const float d = after[i] - before[i];
        if (!std::isfinite(d)) return 0.f;
        if (d > 1e-6f) positive = true;
        else if (d < -1e-6f) negative = true;
        else return 0.f;  // algum campo nao mudou: nao é generico
    }
    if (positive == negative) return 0.f;
    common = after[0] - before[0];
    for (size_t i = 1; i < N; ++i) {
        const float d = after[i] - before[i];
        common = positive ? std::min(common, d) : std::max(common, d);
    }
    return common;
}

// Modificador de dano de cada fonte vanilla (0..4) separando Generic.
inline std::array<Modifier, 5> vanillaDamageSources(const VanillaStats& v, const GenericTrack& g) {
    // Campo vanilla = (1 + Generic + especifico) * multiplicador generico.
    std::array<Modifier, 5> out{};
    const float mul = g.damageMultiplier > 0.f ? g.damageMultiplier : 1.f;
    out[0].additive = 1.f + g.damage;
    out[0].multiplicative = mul;
    for (size_t c = 0; c < 4; ++c) out[c + 1].additive = v.damage[c] / mul - g.damage;
    return out;
}

inline std::array<float, 5> vanillaCritSources(const VanillaStats& v, const GenericTrack& g) {
    return {g.crit, v.crit[0] - g.crit, v.crit[1] - g.crit, v.crit[2] - g.crit, 0.f};
}

// GetTotalDamage(cls): padrao da classe + bonus do jogador, combinado com as
// fontes vanilla escaladas pela heranca.
inline Modifier totalDamage(const NativeDamageClassStatDefaults& cls, const ClassBonus& own,
                            const VanillaStats& v, const GenericTrack& g) {
    Modifier total = Modifier::from(cls.damage).combine(own.damage);
    const auto sources = vanillaDamageSources(v, g);
    for (size_t s = 0; s < 5; ++s) total = total.combine(sources[s].scale(cls.inheritance[s].damage));
    return total;
}

// Generic.SetDefaultStats: crit 4. O snapshot V2 exporta GetTotal* de um
// jogador zerado, entao seu crit ja inclui 4 * heranca de Generic; esse
// padrao volta pelo GenericTrack (o jogo inicia meleeCrit/rangedCrit/magicCrit
// em 4) e precisa sair do valor da classe para nao contar duas vezes.
inline constexpr float kVanillaGenericCrit = 4.f;

inline float ownCrit(const NativeDamageClassStatDefaults& cls) {
    return cls.crit - kVanillaGenericCrit * cls.inheritance[0].crit;
}

inline float totalCrit(const NativeDamageClassStatDefaults& cls, const ClassBonus& own,
                       const VanillaStats& v, const GenericTrack& g) {
    float total = ownCrit(cls) + own.crit;
    const auto sources = vanillaCritSources(v, g);
    for (size_t s = 0; s < 5; ++s) total += sources[s] * cls.inheritance[s].crit;
    return total;
}

// Velocidade: so Melee tem campo vanilla (meleeSpeed, neutro 1).
inline float totalAttackSpeed(const NativeDamageClassStatDefaults& cls, const ClassBonus& own,
                              const VanillaStats& v) {
    return cls.attackSpeed + own.attackSpeed + (v.meleeSpeed - 1.f) * cls.inheritance[1].attackSpeed;
}

// Knockback vanilla por fonte como mapa afim kb -> a*kb + b, medido no proprio
// GetWeaponKnockback original (sem classe = Generic; com a flag de cada classe).
struct Affine { float a = 1.f, b = 0.f; };

inline Modifier affineAsModifier(const Affine& m) {
    Modifier out;
    out.multiplicative = m.a;
    out.flat = m.b;
    return out;
}

inline Modifier totalKnockback(const NativeDamageClassStatDefaults& cls, const ClassBonus& own,
                               const std::array<Affine, 5>& vanilla) {
    Modifier total = Modifier::from(cls.knockback).combine(own.knockback);
    // Fontes de classe: so o excedente sobre o mapa sem classe, para nao
    // contar duas vezes o que o jogo aplica a todas.
    total = total.combine(affineAsModifier(vanilla[0]).scale(cls.inheritance[0].knockback));
    for (size_t s = 1; s < 5; ++s) {
        Affine extra{vanilla[0].a != 0.f ? vanilla[s].a / vanilla[0].a : 1.f, vanilla[s].b - vanilla[0].b};
        total = total.combine(affineAsModifier(extra).scale(cls.inheritance[s].knockback));
    }
    return total;
}

// Player.GetWeaponDamage do tModLoader: (int)(ApplyTo(damage) + 5E-06f).
inline bool weaponDamage(const Modifier& total, int32_t itemDamage, int32_t* out) {
    if (!out || !total.valid() || itemDamage < 0) return false;
    const float value = total.applyTo(static_cast<float>(itemDamage)) + 5e-06f;
    if (!std::isfinite(value) || value < 0.f || value > 2147483000.f) return false;
    *out = static_cast<int32_t>(value);
    return true;
}

// Crit final = crit do item + total da classe (UseStandardCritCalcs).
inline bool weaponCrit(float total, int32_t itemCrit, int32_t* out) {
    if (!out || !std::isfinite(total)) return false;
    const float value = static_cast<float>(itemCrit) + total;
    if (!std::isfinite(value) || std::abs(value) > kLimit) return false;
    *out = static_cast<int32_t>(value);
    return true;
}

inline bool weaponKnockback(const Modifier& total, float knockback, float* out) {
    if (!out || !total.valid() || !std::isfinite(knockback)) return false;
    const float value = std::max(0.f, total.applyTo(knockback));
    if (!std::isfinite(value) || value > kLimit) return false;
    *out = value;
    return true;
}

// Quadros de animacao com a velocidade total (> 0); minimo 1 quadro.
inline bool animationFrames(int32_t useAnimation, float speed, int32_t* out) {
    if (!out || useAnimation <= 0 || !std::isfinite(speed) || speed <= 0.01f) return false;
    const float frames = static_cast<float>(useAnimation) / speed;
    if (!std::isfinite(frames) || frames > 1000000.f) return false;
    // Tolerancia para 30 / 1.2f nao truncar em 24 (mesma ideia do +5E-06f do dano).
    *out = std::max<int32_t>(1, static_cast<int32_t>(frames + 1e-4f));
    return true;
}

}  // namespace bl::csharp::damage_combat
