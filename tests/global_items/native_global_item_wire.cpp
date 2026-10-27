#include "experiments/csharp/NativeGlobalItemWire.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace bl::csharp::global_items;

int main() {
    // Wire completo: shortsword (SetDefaults + dano), Gel (tooltip), acessório (bônus de classe).
    const std::string wire =
        "GLOBAL_ITEMS_V1\n"
        "DEFAULTS\t3507\tuseTime:i:12;autoReuse:b:1;knockBack:f:4.5\n"
        "DAMAGE\t3507\t0\t1.1\t1\t2\n"
        "CRIT\t3507\t5\n"
        "KNOCKBACK\t3507\t0\t1\t1.5\t0\n"
        "TOOLTIP\t23\t4578616D706C65,4C696E6861203220C3A9\n"
        "CLASS_BONUS\t158\tV2\t0\t1.1\t1\t0\t0\t1\t1\t0\t4\t0.05\t0\n"
        "CLASS_BONUS\t158\tC:Mod.Dano\t0\t1.2\t1\t0\t0\t1\t1\t0\t0\t0\t3\n";
    WireTable table;
    std::string error;
    if (!parseWire(wire, &table, &error)) { std::printf("ERR %s\n", error.c_str()); return 1; }
    assert(table.size() == 3);
    const auto& sword = table.at(3507);
    assert(sword.defaults == "useTime:i:12;autoReuse:b:1;knockBack:f:4.5");
    assert(sword.hasDamage && sword.damage.additive == 1.1f && sword.damage.flat == 2.f);
    assert(sword.crit == 5.f && sword.hasKnockback && sword.knockback.multiplicative == 1.5f);
    assert(table.at(23).tooltips.size() == 2 && table.at(23).tooltips[0] == "Example");
    assert(table.at(23).tooltips[1] == "Linha 2 \xC3\xA9");
    assert(table.at(158).classBonuses.size() == 2 && table.at(158).classBonuses[1].classKey == "C:Mod.Dano");
    assert(table.at(158).classBonuses[0].values[8] == 4.f);

    // Erros rejeitam o lote inteiro.
    const char* bad[] = {
        "GLOBAL_ITEMS\n",
        "GLOBAL_ITEMS_V1\nDEFAULTS\t0\tdamage:i:1\n",
        "GLOBAL_ITEMS_V1\nDEFAULTS\t5\tdamage:i:1\nDEFAULTS\t5\tdamage:i:2\n",
        "GLOBAL_ITEMS_V1\nDAMAGE\t5\t0\tNaN\t1\t0\n",
        "GLOBAL_ITEMS_V1\nTOOLTIP\t5\t0A41\n",
        "GLOBAL_ITEMS_V1\nTOOLTIP\t5\tzz\n",
        "GLOBAL_ITEMS_V1\nSOMETHING\t5\tx\n",
        "GLOBAL_ITEMS_V1\nCRIT\t5\t1",
    };
    for (const char* text : bad) {
        WireTable t;
        assert(!parseWire(text, &t, &error));
    }
    WireTable managed;
    assert(!parseWire("ERROR\tInvalidOperationException: boom\n", &managed, &error));
    assert(error.rfind("Managed:InvalidOperationException", 0) == 0);
    WireTable empty;
    assert(parseWire("GLOBAL_ITEMS_V1\n", &empty, &error) && empty.empty());

    // Dano: campo da classe (aditivo vanilla) combinado com o StatModifier.
    Modifier m; m.additive = 1.1f; m.flat = 2.f;
    assert(weaponDamage(10, 1.f, m, -1) == 13);            // 10 * 1.1 + 2
    assert(weaponDamage(10, 1.2f, m, -1) == 15);           // 10 * (1.2 + 0.1) + 2
    assert(weaponDamage(10, 0.f, m, 7) == 7);              // campo inválido: mantém
    Modifier mul; mul.multiplicative = 2.f; mul.base = 1.f;
    assert(weaponDamage(10, 1.f, mul, -1) == 22);          // (10 + 1) * 2
    Modifier kb; kb.multiplicative = 1.5f;
    assert(weaponKnockback(4.f, kb) == 6.f);
    std::puts("NATIVE_GLOBAL_ITEM_WIRE=PASS");
}
