#include "experiments/csharp/NativeDamageClassTooltip.h"
#include "experiments/csharp/NativeDamageClassStats.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace bl::csharp;
using namespace bl::csharp::damage_combat;

namespace {

TooltipInput baseInput() {
    TooltipInput in;
    in.itemDamage = 12;
    in.damage = 15;
    in.critSuffix = u"% chance de acerto critico";
    const char16_t* speeds[] = {u"Insana", u"Muito alta", u"Alta", u"Media", u"Lenta", u"Muito lenta", u"Extrema", u"Lesma"};
    const char16_t* kbs[] = {u"Sem recuo", u"Extremamente fraco", u"Muito fraco", u"Fraco", u"Medio", u"Forte", u"Muito forte", u"Extremamente forte", u"Insano"};
    for (int i = 0; i < 8; ++i) in.speedTexts[i] = speeds[i];
    for (int i = 0; i < 9; ++i) in.knockbackTexts[i] = kbs[i];
    return in;
}

std::vector<TooltipLine> vanillaLines() {
    return {{u"Example C# Wave Wand"}, {u"12 danos"}, {u"Muito alta"}, {u"Muito fraco"}, {u"Material"}};
}

}  // namespace

int main() {
    // ExampleWaveWand: ExampleDamageClass esconde Speed (mascara 11), crit 8.
    {
        auto lines = vanillaLines();
        auto in = baseInput();
        in.className = u"dano de exemplo";
        in.show = kTooltipAll & ~kTooltipSpeed;
        in.hasCrit = true;
        in.crit = 8;
        in.speedIndex = 1;
        in.knockbackIndex = 2;
        assert(rewriteDamageClassTooltip(lines, 20, in));
        assert(lines.size() == 5);
        assert(lines[1].text == u"15 dano de exemplo");
        assert(lines[2].text == u"8% chance de acerto critico");
        assert(lines[3].text == u"Muito fraco");
        assert(lines[4].text == u"Material");
    }
    // Sem nome localizado: mantem o sufixo vanilla; velocidade e knockback recalculados.
    {
        auto lines = vanillaLines();
        auto in = baseInput();
        in.speedIndex = 3;
        in.knockbackIndex = 4;
        assert(rewriteDamageClassTooltip(lines, 20, in));
        assert(lines.size() == 5);
        assert(lines[1].text == u"15 danos");
        assert(lines[2].text == u"Media");
        assert(lines[3].text == u"Medio");
    }
    // Prefixo antes do dano nao confunde; numero diferente nao casa.
    {
        std::vector<TooltipLine> lines{{u"Nome"}, {u"120 algo"}, {u"12 danos"}, {u"Muito alta"}};
        auto in = baseInput();
        assert(rewriteDamageClassTooltip(lines, 20, in));
        assert(lines[1].text == u"120 algo" && lines[2].text == u"15 danos");
        assert(lines[3].text == u"Muito alta");
    }
    // Linha nao reconhecida: nada muda.
    {
        std::vector<TooltipLine> lines{{u"Nome"}, {u"danos 12"}};
        const auto copy = lines;
        assert(!rewriteDamageClassTooltip(lines, 20, baseInput()));
        assert(lines.size() == copy.size() && lines[1].text == copy[1].text);
    }
    // Sem capacidade para a linha de critico: nada muda.
    {
        auto lines = vanillaLines();
        auto in = baseInput();
        in.hasCrit = true;
        in.crit = 8;
        assert(!rewriteDamageClassTooltip(lines, 5, in));
        assert(lines[1].text == u"12 danos");
    }
    // Faixas do vanilla.
    assert(speedTooltipIndex(8) == 0 && speedTooltipIndex(16) == 1 && speedTooltipIndex(56) == 7);
    assert(knockbackTooltipIndex(0.f) == 0 && knockbackTooltipIndex(2.f) == 2 && knockbackTooltipIndex(12.f) == 8);

    // Snapshot: coluna 40 opcional com a mascara de ShowStatTooltipLine.
    {
        std::string row = "CLASS\tNs.Example\t0\t1\t1\t0\t0\t1\t1\t0\t8\t1\t10\t1";
        for (int i = 0; i < 25; ++i) row += i < 5 ? "\t1" : "\t0";
        NativeDamageClassStatRegistry registry;
        assert(registry.publish("mod", "DAMAGE_CLASS_STATS_V2\n" + row + "\t11\n"));
        NativeDamageClassStatDefaults stats;
        assert(registry.lookup("mod", "C:Ns.Example", &stats) && stats.tooltipLines == 11u);
        NativeDamageClassStatRegistry legacy;
        assert(legacy.publish("mod", "DAMAGE_CLASS_STATS_V2\n" + row + "\n"));
        assert(legacy.lookup("mod", "C:Ns.Example", &stats) && stats.tooltipLines == 15u);
        NativeDamageClassStatRegistry bad;
        assert(!bad.publish("mod", "DAMAGE_CLASS_STATS_V2\n" + row + "\t16\n"));
        assert(!bad.publish("mod", "DAMAGE_CLASS_STATS_V2\n" + row + "\tx\n"));
    }
    std::puts("NATIVE_DAMAGECLASS_TOOLTIP=PASS");
}
