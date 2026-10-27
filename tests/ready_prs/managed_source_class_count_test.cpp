#include "experiments/csharp/ManagedSourceClassCount.h"
#include "experiments/csharp/SourceParser.h"
#include <cassert>
#include <algorithm>
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
        else if (base == "BunnyTree") ++mod.trees;
        else if (base == "BunnyGore") ++mod.gores;
        else if (base == "BunnyMount") ++mod.mounts;
        else if (base == "BunnyWall") ++mod.walls;
        else if (base == "BunnyDust") ++mod.dusts;
        else if (base == "BunnyProjectile") ++mod.projectiles;
        else if (base == "BunnyBuff") ++mod.buffs;
        else if (base == "BunnyPlayer") ++mod.players;
        else if (base == "BunnyNPC") ++mod.npcs;
        else if (base == "BunnyPrefix") ++mod.prefixes;
        else if (base == "BunnyRarity") ++mod.rarities;
        else if (base == "BunnyScreenEffect") ++mod.screenEffects;
        else if (base == "BunnyCommand") ++mod.commands;
        else if (base == "DamageClass") ++mod.damageClasses;
        else if (base == "BunnyGlobalItem") ++mod.globalItems;
        else if (base == "BunnyWorldSystem") ++mod.worldSystems;
        else if (base == "BunnyWorldUpdateLoader" || base == "BunnyHookModule") ++mod.hookModules;
        else {
            std::cerr << "Unrecognized content base: " << base << '\n';
            return 1;
        }
    }
    assert(types.size() >= 95);
    assert(mod.mounts == 2);
    assert(std::count_if(types.begin(),types.end(),[](const auto& type){
        return type.name=="ExampleMount" && type.base=="BunnyMount";
    })==1);
    assert(std::count_if(types.begin(),types.end(),[](const auto& type){
        return type.name=="ExampleMinecartMount" && type.base=="BunnyMount";
    })==1);
    assert(std::count_if(types.begin(),types.end(),[](const auto& type){
        return type.name=="ExampleGolfBall" && type.base=="BunnyItem";
    })==1);
    assert(std::count_if(types.begin(),types.end(),[](const auto& type){
        return type.name=="ExampleGolfBallProjectile" && type.base=="BunnyProjectile";
    })==1);
    assert(mod.dusts == 1);
    assert(mod.walls == 1);
    assert(mod.rarities == 2);
    assert(mod.localizationModules == 1);
    assert(mod.commands == 4);
    const int expected = expectedManagedSourceTypes(mod);
    assert(expected == static_cast<int>(types.size()) + mod.localizationModules);
    const std::string status =
        "state=finished\nmodId=full-example\nphase=compile-verified\n"
        "assemblyReady=1\nexecutedTypes=" + std::to_string(expected) + "\n";
    assert(managedSourceCompileFinished(status, mod.id, mod));
    --mod.dusts;
    assert(!managedSourceCompileFinished(status, mod.id, mod));
    std::cout << "FULL_EXAMPLE_NATIVE_MANAGED_COUNT=PASS expected=" << expected
              << " mounts=2 rarities=2 dust=1 wall=1 localization=1 commands=4\n";
}
