#pragma once

#include <string>
#include <map>
#include <vector>

namespace bl::mods { struct LoadedMod; }

namespace bl::csharp {

struct SourceModStatus {
    std::string id;
    std::string directory;
    std::string error;
    int sourceFiles = 0;
    int items = 0;
    int tiles = 0;
    int walls = 0;
    int dusts = 0;
    int trees = 0;
    int gores = 0;
    int projectiles = 0;
    int buffs = 0;
    int potions = 0;
    int basicItems = 0;
    int accessories = 0;
    int players = 0;
    int npcs = 0;
    int prefixes = 0;
    int rarities = 0;
    int localizationModules = 0;
    int screenEffects = 0;
    int commands = 0;

    std::vector<std::string> commandClasses;
    int goreSourcesDiscovered = 0;
    int worldSystems = 0;
    int hookModules = 0;
    std::vector<std::string> worldSystemClasses;
    std::vector<std::string> playerClasses;
    bool loaded = false;
    bool runtimeApplied = false;
    std::map<std::string, unsigned> dustMasks;
    std::map<std::string, std::string> dustTextures;
    std::map<std::string, int> registeredTypes;
    std::map<std::string, int> registeredKindTypes;
    std::map<std::string, std::string> registeredBases,sourcePaths;
};

void resetCSharpSourceModSession();
bool loadCSharpSourceMod(const bl::mods::LoadedMod& mod);
bool applyManagedSourceDefaults(const std::string& modId,
                                const std::string& runtimeValues, std::string* error);
std::vector<SourceModStatus> sourceModStatuses();
bool sourceRuntimeApplied(const std::string& modId);

}

