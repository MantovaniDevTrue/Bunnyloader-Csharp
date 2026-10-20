#include "experiments/csharp/ManagedSourceClassCount.h"
#include "experiments/csharp/SourceParser.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    using namespace bl::csharp;
    assert(argc == 2);
    SourceModStatus mod;
    mod.id = "full-example";
    std::vector<ParsedSourceType> types;
    std::string error;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[1])) {
        if (entry.path().extension() != ".cs") continue;
        std::ifstream input(entry.path());
        assert(input.good());
        std::string source((std::istreambuf_iterator<char>(input)), {});
        if (!parseCSharpSource(source, &types, &error, false,
                               &mod.localizationModules)) {
            std::cerr << entry.path() << ": " << error << '\n';
            return 1;
        }
    }
    for (const auto& type : types) {
        const auto& base = type.base;
        if (base == "BunnyItem" || base == "BunnyWeapon" ||
            base == "BunnyAccessory" || base == "BunnyBuffPotion")
            ++mod.items;
        else if (base == "BunnyTile") ++mod.tiles;
        else if (base == "BunnyWall") ++mod.walls;
        else if (base == "BunnyDust") ++mod.dusts;
        else if (base == "BunnyProjectile") ++mod.projectiles;
        else if (base == "BunnyBuff") ++mod.buffs;
        else if (base == "BunnyPlayer") ++mod.players;
        else if (base == "BunnyNPC") ++mod.npcs;
        else if (base == "BunnyPrefix") ++mod.prefixes;
        else if (base == "BunnyRarity") ++mod.rarities;
        else if (base == "BunnyCommand") ++mod.commands;
        else if (base == "BunnyScreenEffect") ++mod.screenEffects;
        else {
            std::cerr << "Unrecognized content base: " << base << '\n';
            return 1;
        }
    }
    assert(types.size() == 70);
    assert(mod.dusts == 1);
    assert(mod.walls == 1);
    assert(mod.rarities == 2);
    assert(mod.localizationModules == 1);
    assert(mod.commands == 3);
    assert(expectedManagedSourceTypes(mod) == 71);
    const std::string status =
        "state=finished\nmodId=full-example\nphase=compile-verified\n"
        "assemblyReady=1\nexecutedTypes=71\n";
    assert(managedSourceCompileFinished(status, mod.id, mod));
    --mod.dusts;
    assert(!managedSourceCompileFinished(status, mod.id, mod));
    std::cout << "FULL_EXAMPLE_NATIVE_MANAGED_COUNT=PASS expected=71"
              << " rarities=2 dust=1 wall=1 localization=1 commands=3\n";
}
