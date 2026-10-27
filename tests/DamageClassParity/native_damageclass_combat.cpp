// Combate de DamageClass: StatModifier e GetTotal* do tModLoader 1.4.5 e o
// rastreio do genérico na origem do Terraria Android (sem allDamage).
#include "experiments/csharp/NativeDamageClassCombat.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace bl::csharp;
using namespace bl::csharp::damage_combat;

static bool near(float a, float b) { return std::abs(a - b) < 1e-4f; }

static NativeDamageClassStatDefaults exampleClass() {
    // ExampleDamageClass do ExampleMod: herda Generic integralmente,
    // SetDefaultStats crit +4 e armorPen +10. Snapshot = totais num jogador
    // zerado: crit 8 (4 proprio + 4 do Generic), armorPen 10, speed 1.
    NativeDamageClassStatDefaults c{};
    c.crit = 8.f; c.attackSpeed = 1.f; c.armorPenetration = 10.f; c.standardCrit = true;
    c.inheritance[0] = {1.f, 1.f, 1.f, 1.f, 1.f};
    return c;
}

int main() {
    // StatModifier: (v + base) * add * mul + flat; Combine soma aditivo-1 e
    // multiplica; Scale escala os excedentes.
    Modifier m{2.f, 1.5f, 1.2f, 3.f};
    assert(near(m.applyTo(10.f), (10.f + 2.f) * 1.5f * 1.2f + 3.f));
    Modifier c = Modifier{0.f, 1.1f, 1.f, 0.f}.combine({0.f, 1.2f, 1.5f, 1.f});
    assert(near(c.additive, 1.3f) && near(c.multiplicative, 1.5f) && near(c.flat, 1.f));
    Modifier half = Modifier{2.f, 1.4f, 2.f, 4.f}.scale(0.5f);
    assert(near(half.base, 1.f) && near(half.additive, 1.2f) && near(half.multiplicative, 1.5f) && near(half.flat, 2.f));

    // Atribuicao por fonte.
    using A4 = std::array<float, 4>;
    assert(near(commonDelta(A4{1, 1, 1, 1}, A4{1.12f, 1.12f, 1.12f, 1.12f}), 0.12f));  // Emblema do Vingador
    assert(near(commonDelta(A4{1, 1, 1, 1}, A4{1.15f, 1, 1, 1}), 0.f));                  // Emblema do Guerreiro
    assert(near(commonDelta(A4{1, 1, 1, 1}, A4{1.25f, 1.1f, 1.1f, 1.1f}), 0.1f));       // +10% todos +15% melee
    assert(near(commonDelta(A4{1, 1, 1, 1}, A4{1.1f, 0.9f, 1.1f, 1.1f}), 0.f));         // sentidos opostos
    assert(near(commonDelta(A4{1, 1, 1, 1}, A4{0.9f, 0.85f, 0.9f, 0.9f}), -0.1f));      // reducao comum
    assert(near(commonDelta(std::array<float, 3>{4, 4, 4}, std::array<float, 3>{9, 9, 9}), 5.f));

    // Dano: classe que herda so Generic ignora o +15% melee e pega o +12% comum.
    const auto example = exampleClass();
    VanillaStats v; v.damage = {1.27f, 1.12f, 1.12f, 1.12f};
    GenericTrack g{0.12f, 4.f};
    Modifier total = totalDamage(example, {}, v, g);
    int32_t dmg = 0;
    assert(weaponDamage(total, 20, &dmg) && dmg == 22);
    // Classe que herda Generic e Melee integralmente = item melee vanilla.
    auto meleeLike = example; meleeLike.inheritance[1] = {1.f, 1.f, 1.f, 1.f, 1.f};
    assert(near(totalDamage(meleeLike, {}, v, g).additive, 1.27f));
    // Heranca parcial (50% Melee) soma metade da parte especifica.
    auto halfMelee = example; halfMelee.inheritance[1].damage = 0.5f;
    assert(near(totalDamage(halfMelee, {}, v, g).additive, 1.12f + 0.075f));
    // Bonus do jogador para a classe (+10% aditivo, x1.5 multiplicativo).
    ClassBonus own; own.damage.additive = 1.1f; own.damage.multiplicative = 1.5f;
    total = totalDamage(example, own, v, g);
    assert(weaponDamage(total, 20, &dmg) && dmg == static_cast<int32_t>(20.f * 1.22f * 1.5f + 5e-06f));

    // Critico: snapshot 8 = 4 proprio + 4 do Generic; sem contar duas vezes.
    VanillaStats vc; vc.crit = {4.f, 4.f, 4.f};
    assert(near(totalCrit(example, {}, vc, GenericTrack{0.f, 4.f}), 8.f));
    vc.crit = {9.f, 9.f, 9.f};  // +5% critico generico
    assert(near(totalCrit(example, {}, vc, GenericTrack{0.f, 9.f}), 13.f));
    vc.crit = {14.f, 9.f, 9.f}; // +5% critico melee nao entra
    assert(near(totalCrit(example, {}, vc, GenericTrack{0.f, 9.f}), 13.f));
    int32_t crit = 0;
    assert(weaponCrit(13.f, 4, &crit) && crit == 17);

    // Velocidade: so excedente de meleeSpeed herdado por Melee.
    VanillaStats vs; vs.meleeSpeed = 1.12f;
    assert(near(totalAttackSpeed(example, {}, vs), 1.f));
    assert(near(totalAttackSpeed(meleeLike, {}, vs), 1.12f));
    int32_t frames = 0;
    assert(animationFrames(30, 1.2f, &frames) && frames == 25);
    assert(animationFrames(1, 5.f, &frames) && frames == 1);
    assert(!animationFrames(30, 0.f, &frames));

    // Knockback: mapa sem classe = Generic; classes so com o excedente.
    std::array<Affine, 5> kb{};
    kb[0] = {1.5f, 0.f};          // buff de knockback para tudo
    kb[1] = {3.f, 0.f};           // melee: luva (x2) por cima do buff
    kb[4] = {1.5f, 2.f};          // invocacao: + minionKB
    float out = 0.f;
    assert(weaponKnockback(totalKnockback(example, {}, kb), 4.f, &out) && near(out, 6.f));
    auto summonLike = example; summonLike.inheritance[4] = {1.f, 1.f, 1.f, 1.f, 1.f};
    assert(weaponKnockback(totalKnockback(summonLike, {}, kb), 4.f, &out) && near(out, 4.f * 1.5f + 2.f));
    assert(weaponKnockback(totalKnockback(meleeLike, {}, kb), 4.f, &out) && near(out, 4.f * 1.5f * 2.f));

    // Limites.
    assert(!weaponDamage(Modifier{0, NAN, 1, 0}, 10, &dmg));
    assert(!weaponDamage(total, -1, &dmg));
    assert(weaponKnockback(Modifier{0, 1, 1, -100}, 4.f, &out) && out == 0.f);

    std::puts("NATIVE_DAMAGECLASS_COMBAT=PASS");
}
