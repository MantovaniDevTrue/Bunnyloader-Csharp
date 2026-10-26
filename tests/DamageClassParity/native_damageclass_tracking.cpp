// Rastreio do genérico por fonte no frame do jogador e registro de bonus de
// classe por item (Terraria Android, sem allDamage).
#include "experiments/csharp/NativeDamageClassPlayerStats.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace bl::csharp::damage_combat;
static bool near(float a, float b) { return std::abs(a - b) < 1e-4f; }

int main() {
    PlayerTracker t;
    using D = std::array<float, 4>;
    using C = std::array<float, 3>;
    GenericTrack g;
    assert(!t.read(0, &g));                       // sem ResetEffects: inativo
    t.reset(0, C{4, 4, 4});
    assert(t.read(0, &g) && near(g.crit, 4.f) && near(g.damage, 0.f));

    // Emblema do Vingador (+12% todos), Emblema do Guerreiro (+15% melee),
    // Bomba de Fumaca hipotetica (+5% critico em tudo).
    D d{1, 1, 1, 1}; C c{4, 4, 4};
    t.beginSource(0, d, c); d = {1.12f, 1.12f, 1.12f, 1.12f}; t.endSource(0, d, c);
    t.beginSource(0, d, c); d[0] += 0.15f; t.endSource(0, d, c);
    t.beginSource(0, d, c); c = {9, 9, 9}; t.endSource(0, d, c);
    assert(t.read(0, &g) && near(g.damage, 0.12f) && near(g.crit, 9.f));

    // Fonte aninhada (prefixo dentro do equipamento) conta uma vez.
    t.beginSource(0, d, c);
    t.beginSource(0, d, c); d = {d[0] + 0.04f, d[1] + 0.04f, d[2] + 0.04f, d[3] + 0.04f};
    assert(t.depth(0) == 2); t.endSource(0, d, c);
    assert(t.depth(0) == 1); t.endSource(0, d, c);
    assert(t.read(0, &g) && near(g.damage, 0.16f));

    // Generic multiplicativo de mod: deltas medidos depois sao divididos.
    t.addGeneric(0, 0.f, 1.5f, 0.f);
    t.beginSource(0, d, c); for (auto& v : d) v += 0.15f; t.endSource(0, d, c);  // +10% aditivo x1.5
    assert(t.read(0, &g) && near(g.damage, 0.26f) && near(g.damageMultiplier, 1.5f));

    // Bonus de classe customizada acumulam e somem no proximo ResetEffects.
    ClassBonus b; b.damage.additive = 1.1f; b.crit = 3.f;
    t.addCustom(0, customKey("mod", "C:Example"), b);
    t.addCustom(0, customKey("mod", "C:Example"), b);
    auto got = t.custom(0, customKey("mod", "C:Example"));
    assert(near(got.damage.additive, 1.2f) && near(got.crit, 6.f));
    assert(near(t.custom(0, customKey("outro", "C:Example")).crit, 0.f));
    t.reset(0, C{4, 4, 4});
    assert(near(t.custom(0, customKey("mod", "C:Example")).crit, 0.f));
    assert(t.read(0, &g) && near(g.damage, 0.f) && near(g.damageMultiplier, 1.f));
    assert(!t.read(-1, &g) && !t.read(256, &g));

    // Registro: chaves suportadas, neutralidade do que nao tem campo vanilla.
    ClassBonusRegistry r; std::string error;
    ClassBonus melee; melee.damage.additive = 1.1f; melee.attackSpeed = 0.1f;
    ClassBonus custom; custom.knockback.additive = 1.2f; custom.armorPenetration = 5.f;
    assert(r.publish("mod", {{"mod", 10, "V2", melee}, {"mod", 10, "C:Ns.Example", custom}}, &error));
    assert(r.snapshot()->at(10).size() == 2);
    ClassBonus rangedSpeed; rangedSpeed.attackSpeed = 0.1f;
    assert(!r.publish("mod", {{"mod", 11, "V4", rangedSpeed}}, &error) && error.starts_with("VanillaAttackSpeedOnlyMelee"));
    assert(!r.publish("mod", {{"mod", 11, "V9", ClassBonus{}}}, &error) && error.starts_with("UnsupportedDamageClass"));
    ClassBonus summonCrit; summonCrit.crit = 2.f;
    assert(!r.publish("mod", {{"mod", 11, "V6", summonCrit}}, &error) && error.starts_with("SummonCritUnsupported"));
    ClassBonus kb; kb.knockback.flat = 1.f;
    assert(!r.publish("mod", {{"mod", 11, "V2", kb}}, &error) && error.starts_with("VanillaKnockbackUnsupported"));
    assert(!r.publish("mod", {{"outro", 11, "V2", melee}}, &error) && error.starts_with("OwnerMismatch"));
    assert(!r.publish("mod2", {{"mod2", 10, "V2", melee}}, &error) && error.starts_with("ItemAlreadyHasBonuses"));
    ClassBonus bad; bad.damage.multiplicative = 0.f;
    assert(!r.publish("mod", {{"mod", 12, "C:X", bad}}, &error));
    assert(r.snapshot()->count(11) == 0 && r.snapshot()->count(12) == 0);  // falha nao publica parcial
    r.unload("mod");
    assert(r.snapshot()->empty());

    std::puts("NATIVE_DAMAGECLASS_TRACKING=PASS");
}
