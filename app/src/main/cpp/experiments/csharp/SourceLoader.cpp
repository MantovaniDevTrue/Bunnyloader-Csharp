#include "experiments/csharp/SourceLimits.h"
#include "experiments/csharp/ManagedBannerRegistry.h"
#include "experiments/csharp/ManagedBannerLookupHooks.h"
#include "experiments/csharp/ManagedBannerArrayBridge.h"
#include "experiments/csharp/ManagedBannerGameCounters.h"
#include "experiments/csharp/ManagedBannerWorldSession.h"
#include "experiments/csharp/ManagedBannerSaveHooks.h"
#include "experiments/csharp/NativeTileWirePolicy.h"
#include "experiments/csharp/SourceLoader.h"
#include "experiments/csharp/NativeRarityRegistry.h"
#include "experiments/csharp/ManagedDustContract.h"

#include "experiments/csharp/NativeContent.h"
#include "experiments/csharp/NativeDefaults.h"
#include "experiments/csharp/NativeHoldout.h"
#include "experiments/csharp/NativeShooting.h"
#include "experiments/csharp/NativeWallRuntime.h"
#include "experiments/csharp/NativeLocalizationRuntime.h"
#include "content/walls/ModWalls.h"
#include "content/projectiles/ModProjectiles.h"
#include "graphics/CSharpScreenEffects.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "experiments/csharp/NativeAnglerQuestRegistry.h"
#include "experiments/csharp/NativeRecipeCallbacks.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "content/buffs/ModBuffs.h"
#include "content/tiles/ModTiles.h"
#include "content/dusts/ModDusts.h"
#include "content/items/ModItems.h"
#include "content/items/ModPrefixes.h"
#include "experiments/csharp/ManagedPrefixBridge.h"
#include "content/npcs/ModNpcs.h"
#include "experiments/csharp/SourceParser.h"
#include "experiments/csharp/SourceContentIdentity.h"
#include "core/Log.h"
#include "mods/ModLoader.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <set>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace bl::csharp {
namespace {

std::vector<SourceModStatus> g_statuses;

bool isSourceName(const std::string& name) {
    return name.size() > 3 && name.substr(name.size() - 3) == ".cs";
}

bool collect(const std::string& folder, const std::string& rel,
             std::vector<std::string>* paths, std::string* error, int depth) {
    if (depth > kSourceMaxDepth) { *error = "pastas C# excedem o limite"; return false; }
    DIR* d = opendir((folder + (rel.empty() ? "" : "/" + rel)).c_str());
    if (!d) { *error = "pasta de fontes inacessivel"; return false; }
    std::vector<std::string> names;
    while (auto* entry = readdir(d)) {
        const std::string name = entry->d_name;
        if (name != "." && name != ".." && name != "" && name.front() != '.')
            names.push_back(name);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    for (const auto& name : names) {
        const std::string child = rel.empty() ? name : rel + "/" + name;
        const std::string absolute = folder + "/" + child;
        struct stat st{};
        if (lstat(absolute.c_str(), &st) != 0) continue;
        if (S_ISLNK(st.st_mode)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!collect(folder, child, paths, error, depth + 1)) return false;
        } else if (S_ISREG(st.st_mode) && isSourceName(name)) {
            if (st.st_size < 0 || static_cast<uint64_t>(st.st_size) > kSourceMaxFileBytes || paths->size() >= kSourceMaxFiles) {
                *error = "content/" + child +
                    ": [CSharp][ERROR][SourceLoader] Stage=Collect Expected=Files<="+std::to_string(kSourceMaxFiles)+",FileBytes<="+std::to_string(kSourceMaxFileBytes)+" Actual=Files:"+std::to_string(paths->size()+1)+",FileBytes:"+std::to_string(st.st_size)+" Reason=SourceCapacityExceeded Result=ModRejected";
                return false;
            }
            paths->push_back(child);
        }
    }
    return true;
}

bool read(const std::string& path, std::string* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out->clear();
    char buffer[16384];
    while (f) {
        f.read(buffer, sizeof(buffer));
        const auto count = static_cast<size_t>(f.gcount());
        if (count > kSourceMaxFileBytes - out->size()) return false;
        out->append(buffer, count);
    }
    return !f.bad();
}

bool integer(const std::string& value, int* out) {
    if (value.empty() || value.size() > 14) return false;
    errno = 0;
    char* end = nullptr;
    long n = std::strtol(value.c_str(), &end, 10);
    if (errno || !end || *end || n < -2147483647L - 1 || n > 2147483647L) return false;
    *out = static_cast<int>(n);
    return true;
}

bool real(const std::string& value, float* out) {
    if (value.empty() || value.size() > 22) return false;
    errno = 0;
    char* end = nullptr;
    float result = std::strtof(value.c_str(), &end);
    if (end && (*end == 'f' || *end == 'F')) ++end;
    if (errno || !end || *end || !std::isfinite(result)) return false;
    *out = result;
    return true;
}

bool fields(const ParsedSourceType& type, NativeContentSpec* spec,
            std::string* shoot, std::string* display, std::string* texture,
            std::string* error) {
    const bool projectile = type.base == "BunnyProjectile";
    const bool basic = type.base == "BunnyItem";
    const bool tile = type.base == "BunnyTile";
    const bool wall = type.base == "BunnyWall";
    const bool weapon = type.base == "BunnyWeapon";
    const bool buff = type.base == "BunnyBuff";
    const bool potion = type.base == "BunnyBuffPotion";
    const bool accessory = type.base == "BunnyAccessory";
    const bool npc = type.base == "BunnyNPC";
    if (!basic && !tile && !wall && !projectile && !weapon && !buff && !potion && !accessory && !npc) {
        *error = type.name + ": tipo de conteudo C# desconhecido";
        return false;
    }
    spec->width = npc ? 32 : projectile ? 8 : 0;
    spec->height = npc ? 24 : projectile ? 8 : 0;
    spec->aiStyle = (projectile || npc) ? 1 : 0;
    spec->penetrate = npc ? 1 : projectile ? 1 : 0;
    spec->timeLeft = npc ? 2 : projectile ? 360 : potion ? 600 : 0;
    spec->damage = npc ? 7 : weapon ? 20 : 0;
    spec->useTime = npc ? 2 : weapon ? 18 : 0;
    spec->shootType = npc ? 25 : 0;
    spec->shootSpeed = weapon ? 12.f : 0.f;
    spec->kind = wall ? NativeWall : tile ? NativeTile : basic ? NativeBasicItem : projectile ? NativeProjectile :
                 weapon ? NativeWeapon : buff ? NativeBuff :
                 accessory ? NativeAccessory : npc ? NativeNpc : NativeBuffPotion;
    *texture = wall ? "Content/Walls/" + type.name + ".png" : tile ? "Content/Tiles/" + type.name + ".png" :
               basic ? "Content/Items/" + type.name + ".png" : npc ? "Content/NPCs/" + type.name + ".png" :
               projectile ? "Content/Projectiles/" + type.name + ".png" :
               weapon ? "Content/Items/Weapons/" + type.name + ".png" :
               buff ? "Content/Buffs/" + type.name + ".png" :
               accessory ? "Content/Items/Accessories/" + type.name + ".png" :
                      "Content/Items/Consumables/" + type.name + ".png";
    *display = type.name;
    for (const auto& [key, value] : type.properties) {
        if (key == "DisplayName") { *display = value; continue; }
        if (key == "Texture") { *texture = value; continue; }
        if (tile || wall) {
            *error = type.name + ": "+type.base+" permite somente Texture e DisplayName no registro antecipado";
            return false;
        }
        if (basic) {
            const std::string allowed="|Tooltip|Width|Height|MaxStack|Value|Rare|Defense|Damage|Crit|KnockBack|UseTime|UseAnimation|UseStyle|Melee|NoMelee|AutoReuse|UseTurn|Consumable|Pick|Axe|Hammer|UseSound|";
            if (allowed.find("|"+key+"|")==std::string::npos) {
                *error=type.name+": propriedade BunnyItem nao suportada: "+key;return false;
            }
            continue; // valores efetivos vem do SetDefaults executado pelo Mono
        }
        if (accessory && key == "Tooltip") continue;
        if (accessory && key == "VisualMode") {
            if (value != "None" && value != "EquippedSprite" &&
                value != "EnergyBarrier" && value != "RunningTrail") {
                *error = type.name + ": VisualMode invalido";
                return false;
            }
            continue;
        }
        if (accessory && key == "VisualTexture") {
            if (value.empty() || value.size() > 512 ||
                value.front() == '/' || value.find("..") != std::string::npos ||
                value.find(static_cast<char>(92)) != std::string::npos ||
                value.find(':') != std::string::npos ||
                value.size() < 5 || value.substr(value.size() - 4) != ".png") {
                *error = type.name + ": VisualTexture invalida";
                return false;
            }
            continue;
        }
        if (accessory && (key == "VisualScale" || key == "VisualOpacity")) {
            float parsed = 0.f;
            if (!real(value, &parsed) ||
                (key == "VisualScale" && (parsed < 0.25f || parsed > 3.f)) ||
                (key == "VisualOpacity" && (parsed < 0.f || parsed > 1.f))) {
                *error = type.name + ": " + key + " invalido";
                return false;
            }
            continue;
        }
        if (accessory && key == "VisualColor") {
            if (value.size() < 3 || value.size() > 10 ||
                !(value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))) {
                *error = type.name + ": VisualColor invalido";
                return false;
            }
            for (size_t i = 2; i < value.size(); ++i) {
                if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
                    *error = type.name + ": VisualColor invalido";
                    return false;
                }
            }
            continue;
        }

        if(projectile&&(key=="MinionMovement"||key=="AnimationFrameTicks"||key=="MinionScanTicks"||key=="MinionTargetRange"))continue;
        if (buff && (key == "MinionProjectile" || key == "DefenseMultiplierPercent")) continue;  // validado na linha V2 apos o Mono
        if (buff && (key == "Description" || key == "IsDebuff" ||
                     key == "NoSave" || key == "NoTimeDisplay" || key == "NurseCannotRemove")) {
            const bool text = key == "Description";
            const bool badText = text && (value.size() > 512 ||
                std::any_of(value.begin(), value.end(),
                    [](unsigned char c) { return (c < 32 && c != 10) || c == 127; }));
            if ((!text && value != "true" && value != "false") || badText) {
                *error = type.name + ": valor invalido em " + key;
                return false;
            }
            continue;
        }
        if (npc && (key == "Banner" || key == "BannerItem")) {
            if (value.size() > 72 ||
                value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos) {
                *error = type.name + ": classe de banner invalida: " + key;
                return false;
            }
            continue;
        }
        if (weapon && key == "Shoot") { *shoot = value; continue; }
        if (weapon && key == "Summon") {
            if (value != "true" && value != "false") {
                *error = type.name + ": Summon invalido";
                return false;
            }
            continue;
        }
        if (weapon && key == "MinionBuff") {
            if (value.empty() || value.size() > 72 ||
                value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos) {
                *error = type.name + ": MinionBuff invalido";
                return false;
            }
            continue;
        }
        if (weapon && key == "MinionBuffTime") {
            int ticks = 0;
            if (!integer(value, &ticks) || ticks < 1 || ticks > 36000) {
                *error = type.name + ": MinionBuffTime invalido";
                return false;
            }
            continue;
        }
        if (potion && key == "Buff") { *shoot = value; continue; }
        if (projectile && (key == "Minion" || key == "MinionContactDamage" || key == "TileCollide" || key == "IgnoreWater")) {
            if (value != "true" && value != "false") {
                *error = type.name + ": valor booleano invalido em " + key;
                return false;
            }
            continue;
        }
        if (projectile && key == "Frames") {
            int frames = 0;
            if (!integer(value, &frames) || frames < 1 || frames > 32) {
                *error = type.name + ": Frames deve ser entre 1 e 32";
                return false;
            }
            continue;
        }
        if (projectile && key == "MinionSlots") {
            float slots = 0.f;
            if (!real(value, &slots) || slots < 0.f || slots > 10.f) {
                *error = type.name + ": MinionSlots fora dos limites";
                return false;
            }
            continue;
        }
        if (key == "Friendly" && projectile) {
            if (value != "true") {
                *error = "somente projeteis Friendly=true nesta etapa";
                return false;
            }
            continue;
        }
        int* integerField = nullptr;
        if (projectile && key == "Width") integerField = &spec->width;
        else if (projectile && key == "Height") integerField = &spec->height;
        else if (projectile && key == "AiStyle") integerField = &spec->aiStyle;
        else if (projectile && key == "Penetrate") integerField = &spec->penetrate;
        else if (projectile && key == "TimeLeft") integerField = &spec->timeLeft;
        else if (weapon && key == "Damage") integerField = &spec->damage;
        else if (weapon && key == "UseTime") integerField = &spec->useTime;
        else if (buff && key == "DefenseBonus") integerField = &spec->damage;
        else if (accessory && key == "DefenseBonus") integerField = &spec->damage;
        else if (potion && key == "Duration") integerField = &spec->timeLeft;
        else if (npc && key == "Width") integerField = &spec->width;
        else if (npc && key == "Height") integerField = &spec->height;
        else if (npc && key == "Damage") integerField = &spec->damage;
        else if (npc && key == "Defense") integerField = &spec->useTime;
        else if (npc && key == "LifeMax") integerField = &spec->shootType;
        else if (npc && key == "AiStyle") integerField = &spec->aiStyle;
        else if (npc && key == "AnimationType") integerField = &spec->penetrate;
        else if (npc && key == "Frames") integerField = &spec->timeLeft;
        if (integerField) {
            if (!integer(value, integerField)) {
                *error = type.name + ": inteiro invalido em " + key;
                return false;
            }
            continue;
        }
        if (weapon && key == "ShootSpeed") {
            if (!real(value, &spec->shootSpeed)) {
                *error = type.name + ": ShootSpeed invalido";
                return false;
            }
            continue;
        }
        *error = type.name + ": propriedade C# nao suportada: " + key;
        return false;
    }
    if ((weapon || potion) && shoot->empty()) {
        *error = type.name + (weapon ? ": Shoot" : ": Buff") +
            " = nameof(Classe) obrigatorio";
        return false;
    }
    return true;
}

bool load(const bl::mods::LoadedMod& mod, SourceModStatus* status) {
    const std::string folder = mod.dir + "/content";
    std::vector<std::string> paths;
    if (!collect(folder, "", &paths, &status->error, 0)) return false;
    if (paths.empty() || paths.size() > kSourceMaxFiles) {
        status->error = "pacote C# sem fontes .cs";
        return false;
    }
    std::vector<ParsedSourceType> defs;
    std::map<std::string,std::string> sourcePaths;
    size_t totalSourceBytes=0;
    for (const auto& path : paths) {
        std::string contents;
        if (!read(folder + "/" + path, &contents)) {
            status->error = "nao foi possivel ler " + path;
            return false;
        }
        totalSourceBytes+=contents.size();
        if(totalSourceBytes>kSourceMaxTotalBytes) {status->error="Fontes excedem 32 MiB";return false;}
        const size_t previousTypes=defs.size();
        if (!parseCSharpSource(contents, &defs, &status->error, false,
                               &status->localizationModules)) {
            status->error = path + ": " + status->error;
            return false;
        }
        for(size_t i=previousTypes;i<defs.size();++i)
            sourcePaths[sourceIdentityForBase(defs[i].base, defs[i].name)]=path;
        ++status->sourceFiles;
        if (defs.size() > kSourceMaxTypes) {
            status->error = "mod C# excede 8192 tipos registrados";
            return false;
        }
    }
    if (defs.empty()) {
        status->error = "mod C# sem conteudo concreto registrado; use uma base publica Bunny ou [BunnyContent] com override de defaults para heranca propria";
        return false;
    }
    if (defs.size() > kSourceMaxTypes) { status->error = "tipos C# demais"; return false; }
    std::map<std::string, int> typed;
    for (const auto& def : defs) {
        const std::string key = sourceIdentityForBase(def.base, def.name);
        if (key.empty() || !typed.emplace(key, -1).second) {
            status->error = "identidade nativa C# duplicada/invalida: " + key +
                " namespace=" + def.namespaceName +
                " fonte=content/" + sourcePaths[key];
            return false;
        }
    }
    const bool hasNativeContent = std::any_of(defs.begin(), defs.end(),
        [](const ParsedSourceType& def) { return def.base != "BunnyCommand"; });
    if (hasNativeContent && !prepareNativeContent()) {
        status->error = "Host API de conteudo nativo indisponivel";
        return false;
    }
    NativeRegisterContext ctx{mod.id, mod.dir, 0, 0};
    const std::array<std::string, 12> order = {
        "BunnyDust", "BunnyTile", "BunnyWall", "BunnyItem",
        "BunnyBuff", "BunnyProjectile", "BunnyAccessory",
        "BunnyNPC", "BunnyBuffPotion", "BunnyWeapon", "BunnyPrefix", "BunnyRarity"
    };
    for (const auto& base : order) {
        for (const auto& def : defs) {
            if (def.base != base) continue;
            if (base == "BunnyRarity") {
                if (!def.properties.empty()) {
                    status->error = def.name + ": BunnyRarity usa callbacks gerenciados, nao propriedades literais";
                    return false;
                }
                const int type = nativeRarities().registerType(mod.id, def.name);
                if (type < 0) {
                    status->error = def.name + ": identidade C# de raridade duplicada ou invalida";
                    return false;
                }
                typed[sourceIdentityForBase(def.base, def.name)] = type;
                ++status->rarities;
                continue;
            } else if (base == "BunnyDust") {
                for (const auto& [key, value] : def.properties) {
                    if (key != "Texture" && key != "DisplayName") {
                        status->error = "[CSharp][DustLoader] Class=" + def.name +
                            " Stage=EarlyRegister Reason=UnsupportedProperty:" + key;
                        return false;
                    }
                }
                std::string texture;
                const auto it = def.properties.find("Texture");
                if (it != def.properties.end()) texture = it->second;
                if (!texture.empty()) {
                    if (texture.size() > 240 || texture[0] == '/' ||
                        texture.find("..") != std::string::npos ||
                        texture.find(static_cast<char>(92)) != std::string::npos ||
                        texture.find(':') != std::string::npos ||
                        texture.find('|') != std::string::npos ||
                        !texture.ends_with(".png")) {
                        status->error = def.name + ": textura BunnyDust invalida";
                        return false;
                    }
                    for (unsigned char ch : texture) {
                        if (ch < 32 || ch == 127) {
                            status->error = def.name + ": caractere invalido em textura BunnyDust";
                            return false;
                        }
                    }
                    const std::string asset = mod.dir + "/content/" + texture;
                    struct stat st{};
                    if (stat(asset.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
                        status->error = def.name + ": asset BunnyDust nao encontrado: " + texture;
                        return false;
                    }
                    texture = asset;
                }
                bl::runtime::ModDustDef dust;
                dust.mod = mod.id;
                dust.name = def.name;
                dust.texture = std::move(texture);
                dust.hooks = 0;
                const int type = bl::runtime::registerModDust(std::move(dust));
                if (type < 0) {
                    status->error = def.name + ": falha ao reservar BunnyDust";
                    return false;
                }
                typed[sourceIdentityForBase(def.base, def.name)] = type;
                status->dustTextures[def.name] = it == def.properties.end() ? "" : it->second;
                ++status->dusts;
                continue;
            }
            if (base == "BunnyPrefix") {
                for (const auto& [key, value] : def.properties)
                    if (key != "DisplayName" && key != "Category" &&
                        key != "RollWeight") {
                        status->error = def.name + ": propriedade de prefixo nao suportada: " + key;
                        return false;
                    }
                const int type = bl::runtime::registerModPrefix(mod.id, def.name);
                if (type < 0) {
                    status->error = def.name + ": registro nativo de prefixo falhou";
                    return false;
                }
                typed[sourceIdentityForBase(def.base, def.name)] = type;
                bl::csharp::reserveManagedPrefixDescriptor(type);
                ++status->prefixes;
                continue;
            }
            NativeContentSpec spec{};
            std::string target, display, texture;
            if (!fields(def, &spec, &target, &display, &texture, &status->error))
                return false;
            if (base == "BunnyWeapon" || base == "BunnyBuffPotion") {
                bool targetRegistered = false;
                for (const auto& other : defs) {
                    if (other.name != target) continue;
                    if (other.base == (base == "BunnyWeapon" ? "BunnyProjectile" : "BunnyBuff"))
                        targetRegistered = true;
                }
                const std::string targetBase =
                    base == "BunnyWeapon" ? "BunnyProjectile" : "BunnyBuff";
                const auto it = typed.find(sourceIdentityForBase(targetBase, target));
                if (!targetRegistered || it == typed.end() || it->second < 0) {
                    status->error = def.name + ": alvo nao encontrado: " + target;
                    return false;
                }
                spec.shootType = it->second;
            }
            if (base == "BunnyBuff") {
                auto flag = [&](const char* key) {
                    const auto it = def.properties.find(key);
                    return it != def.properties.end() && it->second == "true";
                };
                spec.flags = (flag("IsDebuff") ? kNativeBuffDebuff : 0) |
                             (flag("NoSave") ? kNativeBuffNoSave : 0) |
                             (flag("NoTimeDisplay") ? kNativeBuffNoTimeDisplay : 0) | (flag("NurseCannotRemove") ? kNativeBuffNurseCannotRemove : 0);

            }
            spec.name = def.name.c_str();
            spec.texture = texture.c_str();
            spec.displayName = display.c_str();
            const std::string sourceKey = sourceIdentityForBase(def.base, def.name);
            ctx.sourceFile="content/"+sourcePaths[sourceKey];
            ctx.sourceNames=def.localizedNames;
            const int type = registerNativeContent(&ctx, &spec);
            if (type < 0) {
                status->error="[CSharp][ERROR][Register] Mod="+mod.id+" Class="+def.name+
                    " File=content/"+sourcePaths[sourceKey]+" Category="+base+" Stage=NativeRegister Expected=NonNegativeContentId Actual="+
                    std::to_string(type)+" Result=LoadInterrupted Detail="+
                    (ctx.lastError.empty()?"NativeRegistrationRejected; confira diagnostico anterior do registrador":ctx.lastError);
                return false;
            }
            typed[sourceKey] = type;
            if (base == "BunnyAccessory") {
                const auto tooltip = def.properties.find("Tooltip");
                if (tooltip != def.properties.end() && !tooltip->second.empty()) {
                    if (tooltip->second.size() > 180 ||
                        std::any_of(tooltip->second.begin(), tooltip->second.end(),
                            [](unsigned char c) { return c < 32 || c == 127; })) {
                        status->error = def.name + ": Tooltip C# invalido";
                        return false;
                    }
                    bl::runtime::setModItemTooltip(type, {{"", tooltip->second}});
                    BL_INFO("csharp accessory: tooltip inicial registrado para %s (type=%d)",
                            def.name.c_str(), type);
                }
            }
        }
    }
    for (const auto& def : defs) {
        if (def.base != "BunnyPlayer") continue;
        const auto link = def.properties.find("Accessory");
        bool validFishing = true;
        bool hasFishingName = false;
        bool hasFishingRules = false;
        for(const auto& [property,value]:def.properties) {
            if(property=="Accessory")continue;
            if(property=="FishingQuestFishName") {
                hasFishingName=!value.empty();
                continue;
            }
            if(property=="FishingCatchRules") {
                hasFishingRules=true;
                validFishing=validFishing &&
                    (value=="None"||value=="InvertedGravity"||
                     value=="Uncommon"||value=="InvertedGravityAndUncommon");
                continue;
            }
            validFishing=false;
        }
        if(!validFishing||(hasFishingRules&&!hasFishingName)) {
            status->error=def.name+": propriedades BunnyPlayer nao suportadas para pesca";
            return false;
        }
        if (link != def.properties.end()) {
            bool valid = false;
            for (const auto& other : defs)
                if (other.base == "BunnyAccessory" && other.name == link->second &&
                    typed.contains(sourceIdentityForBase("BunnyAccessory", link->second)) &&
                    typed[sourceIdentityForBase("BunnyAccessory", link->second)] >= 0)
                    valid = true;
            if (!valid) {
                status->error = def.name + ": acessorio C# inexistente ou propriedades invalidas";
                return false;
            }
        }
        status->playerClasses.push_back(def.name);
    }
    for (const auto& def : defs)
        if (!sourceRequiresNativeRegistration(def.base))
            typed.erase(sourceIdentityForBase(def.base, def.name));
    status->players = static_cast<int>(status->playerClasses.size());
    status->accessories = ctx.accessories;
    status->registeredKindTypes = typed;
    for(const auto& def:defs) {
        const auto identity=sourceIdentityForBase(def.base,def.name);
        status->registeredBases[identity]=def.base;
        status->sourcePaths[identity]="content/"+sourcePaths[identity];
    }
    for (const auto& [key, type] : typed) {
        const auto colon = key.find(':');
        if (colon != std::string::npos && type >= 0)
            status->registeredTypes.try_emplace(key.substr(colon + 1), type);
    }
    status->tiles = ctx.tiles;
    status->walls = ctx.walls;
    status->items = ctx.items;
    status->projectiles = ctx.projectiles;
    status->buffs = ctx.buffs;
    status->npcs = ctx.npcs;
    status->potions = ctx.potions;
    status->basicItems = ctx.basicItems;

    std::vector<bl::graphics::CSharpScreenEffectSpec> screenEffects;
    for (const auto& def : defs) {
        if (def.base != "BunnyScreenEffect") continue;
        bl::graphics::CSharpScreenEffectSpec effect;
        effect.className = def.name;
        for (const auto& [key, value] : def.properties) {
            if (key == "DisplayName") continue;
            if (key == "ShaderFile") {
                effect.shaderFile = value;
                continue;
            }
            if (key == "PipelineFile") {
                effect.pipelineFile = value;
                continue;
            }
            if (key == "Layer" || key == "Order") {
                char* end = nullptr;
                errno = 0;
                const long parsed = std::strtol(value.c_str(), &end, 10);
                if (errno != 0 || !end || *end != '\0' ||
                    parsed < -4096 || parsed > 4096) {
                    status->error = def.name + ": " + key + " deve ser um inteiro literal valido";
                    return false;
                }
                if (key == "Layer") effect.layer = static_cast<int>(parsed);
                else effect.order = static_cast<int>(parsed);
                continue;
            }
            if (key == "Intensity") {
                char* end = nullptr;
                errno = 0;
                const float parsed = std::strtof(value.c_str(), &end);
                if (end && (*end == 'f' || *end == 'F')) ++end;
                if (errno != 0 || !end || *end != '\0' ||
                    !std::isfinite(parsed) || parsed < 0.f || parsed > 4.f) {
                    status->error = def.name + ": Intensity deve ser literal de 0 a 4";
                    return false;
                }
                effect.intensity = parsed;
                continue;
            }
            if (key.starts_with("Uniform_")) {
                const std::string parameter = key.substr(8);
                if (parameter.empty() || parameter.size() > 64 || effect.parameters.size() >= 24 ||
                    !std::isalpha(static_cast<unsigned char>(parameter[0]))) {
                    status->error = def.name + ": uniforme C# invalido";
                    return false;
                }
                for (char ch : parameter) {
                    if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
                        status->error = def.name + ": nome de uniforme C# invalido";
                        return false;
                    }
                }
                char* end = nullptr;
                errno = 0;
                float parsed = std::strtof(value.c_str(), &end);
                if (end && (*end == 'f' || *end == 'F')) ++end;
                if (errno || !end || *end || !std::isfinite(parsed) ||
                    parsed < -100.f || parsed > 100.f ||
                    !effect.parameters.emplace(parameter, parsed).second) {
                    status->error = def.name + ": uniforme C# requer valor literal unico entre -100 e 100";
                    return false;
                }
                continue;
            }
            status->error = def.name + ": propriedade de BunnyScreenEffect nao suportada: " + key;
            return false;
        }
        if (effect.shaderFile.empty() == effect.pipelineFile.empty()) {
            status->error = def.name + ": use ShaderFile ou PipelineFile, nunca ambos";
            return false;
        }
        screenEffects.push_back(std::move(effect));
    }
    status->screenEffects = static_cast<int>(screenEffects.size());
    for (const auto& def : defs)
        if (def.base == "BunnyCommand")
            status->commandClasses.push_back(def.namespaceName.empty() ? def.name :
                def.namespaceName + "." + def.name);
    status->commands = static_cast<int>(status->commandClasses.size());
    if (status->commands > 128) {
        status->error = "ModCommand: maximo de 128 classes por mod";
        return false;
    }
    if (status->dusts+status->items+status->tiles+status->projectiles+status->buffs+
        status->npcs+status->prefixes+status->rarities+status->players+status->screenEffects+status->commands==0) {
        status->error = "mod C# sem conteudo registrado";
        return false;
    }
    if (!screenEffects.empty() &&
        !bl::graphics::registerCSharpScreenEffects(mod.id, folder, screenEffects, &status->error))
        return false;
    return true;
}

}

void resetCSharpSourceModSession() {
    banners::setBannerLookupsActive(false);
    for (const auto& mod : g_statuses) {
        angler::registry().unload(mod.id);
        banners::registry(runtime::kVanillaNpcCount).unload(mod.id);
        unbindNativeRecipeAssembly(mod.id);
        invalidateManagedLiveSourceMod(mod.id);
        invalidateManagedDeferredSourceMod(mod.id);
    }
    g_statuses.clear();
}

bool loadCSharpSourceMod(const bl::mods::LoadedMod& mod) {
    auto previous = std::find_if(g_statuses.begin(), g_statuses.end(),
        [&](const SourceModStatus& entry) { return entry.id == mod.id; });
    if (previous != g_statuses.end()) {
        banners::setBannerLookupsActive(false);
        banners::registry(runtime::kVanillaNpcCount).unload(mod.id);
        angler::registry().unload(mod.id);
        unbindNativeRecipeAssembly(mod.id);
        invalidateManagedLiveSourceMod(mod.id);
        invalidateManagedDeferredSourceMod(mod.id);
    }
    SourceModStatus status;
    status.id = mod.id;
    status.directory = mod.dir;
    status.loaded = load(mod, &status);
    if (status.loaded) {
        BL_INFO("C# source %s: %d arquivo(s), %d item(ns), %d projetil(is), %d buff(s), %d pocao(oes), %d NPC(s), %d Dust(s)",
                mod.id.c_str(), status.sourceFiles, status.items,
                status.projectiles, status.buffs, status.potions, status.npcs, status.dusts);
    } else {
        BL_ERROR("C# source %s: %s", mod.id.c_str(), status.error.c_str());
    }
    if (previous != g_statuses.end()) {
        *previous = std::move(status);
        return previous->loaded;
    }
    g_statuses.push_back(std::move(status));
    return g_statuses.back().loaded;
}

bool applyManagedSourceDefaults(
    const std::string& modId, const std::string& runtimeValues, std::string* error) {
    if (!error) return false;
    error->clear();
    if (runtimeValues.empty() || runtimeValues.size() > kSourceDefaultsBytes) {
        *error = "Resultado gerenciado inexistente ou grande demais";
        return false;
    }
    SourceModStatus* mod = nullptr;
    for (auto& entry : g_statuses) {
        if (entry.id == modId && entry.loaded) {
            mod = &entry;
            break;
        }
    }
    if (!mod || mod->runtimeApplied) {
        *error = "Mod fonte nao carregado ou SetDefaults ja aplicado";
        return false;
    }
    std::istringstream lines(runtimeValues);
    std::string header;
    if (!std::getline(lines, header) || header != "BUNNY_MANAGED_DEFAULTS_V2") {
        *error = "Formato gerenciado V2 esperado";
        return false;
    }

    std::map<std::string, int> projectilesByName;
    std::map<std::string, int> buffsByName;
    std::map<std::string, bool> seen;
    std::vector<ManagedTileEntry> tiles;
    std::vector<walls::Definition> wallDefinitions;
    int dustRows = 0;
    std::vector<ManagedProjectileEntry> projectiles;
    std::vector<ManagedWeaponEntry> weapons;
    std::vector<BasicItemEntry> basicItems;
    std::vector<angler::Quest> anglerQuests;
    std::map<std::string, bool> anglerPlayerRules;
    std::vector<NativeDefaultsRequest> nativeDefaults;
    std::vector<holdout::Entry> holdouts;
    std::vector<shooting::Entry> shooting;
    std::vector<useSounds::Entry> useSounds;
    std::vector<ManagedBuffEntry> buffs;
    std::vector<ManagedPotionEntry> potions;
    std::vector<ManagedAccessoryEntry> accessories;
    std::vector<ManagedPlayerEntry> players;
    std::vector<ManagedNpcEntry> npcs;
    std::vector<banners::Binding> bannerBindings;
    std::set<std::string> seenBannerClasses;
    std::vector<ManagedPrefixEntry> prefixes;
    std::vector<localization::Entry> localized;
    std::set<std::string> seenLocale;
    bool seenLocalizationModules = false;
    bool seenLocalizationRuntime = false;
    size_t rarityCount = 0;
    std::map<std::string, int> itemRarityOverrides;
    std::vector<SourceRecipeOwner> recipeOwners;
    std::map<std::string,bool> seenRecipeOwners;
    std::map<std::string, int> accessoryTypes;
    std::vector<std::string> shootNames;
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty()) continue;
        std::istringstream parts(line);
        std::vector<std::string> fields;
        std::string part;
        while (std::getline(parts, part, '\t')) fields.push_back(part);
        if (!fields.empty() && fields[0] == "NPC_BANNER") {
            if (fields.size() != 4 ||
                !seenBannerClasses.insert(fields.size() > 1 ? fields[1] : "").second) {
                *error = "[CSharp][BannerLoader] Stage=Decode Reason=InvalidOrDuplicateBannerDescriptor";
                return false;
            }
            const std::string npcKey = sourceIdentityForWire("NPC", fields[1]);
            const std::string ownerKey = sourceIdentityForWire("NPC", fields[2]);
            const std::string itemKey = sourceIdentityForWire("ITEM", fields[3]);
            const auto npc = mod->registeredKindTypes.find(npcKey);
            const auto owner = mod->registeredKindTypes.find(ownerKey);
            const auto item = mod->registeredKindTypes.find(itemKey);
            const auto itemBase = mod->registeredBases.find(itemKey);
            if (npc == mod->registeredKindTypes.end() ||
                owner == mod->registeredKindTypes.end() ||
                item == mod->registeredKindTypes.end() ||
                itemBase == mod->registeredBases.end() ||
                itemBase->second != "BunnyItem" ||
                runtime::modNpcTypeByName(modId, fields[1]) != npc->second ||
                runtime::modNpcTypeByName(modId, fields[2]) != owner->second ||
                runtime::modItemTypeByKey(modId + "/" + fields[3]) != item->second ||
                bannerBindings.size() >= kSourceMaxCategory) {
                *error = "[CSharp][BannerLoader] Stage=Resolve Reason=MissingLocalNpcOrBannerItem";
                return false;
            }
            bannerBindings.push_back({modId, fields[1], fields[2], fields[3],
                                      npc->second, owner->second, item->second});
            continue;
        }
        if(!fields.empty()&&fields[0]=="LOCALE_MODULES") {
            int count=0;
            if (seenLocalizationModules || fields.size()!=2 ||
                !integer(fields[1],&count) || count<1 || count>32) {
                *error="[CSharp][ERROR][Localization] Stage=Decode Reason=InvalidModuleMarker";
                return false;
            }
            seenLocalizationModules=true;
            continue;
        }
        if(!fields.empty()&&fields[0]=="LOCALE_RUNTIME") {
            if (seenLocalizationRuntime || fields.size()!=2 || fields[1]!="1") {
                *error="[CSharp][ERROR][Localization] Stage=Decode Reason=InvalidRuntimeMarker";
                return false;
            }
            seenLocalizationRuntime=true;
            continue;
        }
        if(!fields.empty()&&fields[0]=="LOCALE") {
            localization::Entry entry;
            if (!localization::decodeWire(fields,modId,&entry,error))return false;
            if (!seenLocale.insert(entry.culture+"\t"+entry.key).second ||
                localized.size() >= 8192u*32u) {
                *error="[CSharp][ERROR][Localization] Stage=Decode Result=Rejected Reason=DuplicateOrOverflow";
                return false;
            }
            localized.push_back(std::move(entry));
            continue;
        }
        if (!fields.empty() && fields[0] == "ITEM_RARITY") {
            if (fields.size() != 3) {
                *error = "[CSharp][ERROR][RarityLoader] Mod=" + modId +
                    " Stage=DecodeItemRarity Reason=InvalidFieldCount";
                return false;
            }
            const auto itemKey = sourceIdentityForWire("ITEM", fields[1]);
            const auto item = mod->registeredKindTypes.find(itemKey);
            const auto base = mod->registeredBases.find(itemKey);
            const auto rarity = mod->registeredKindTypes.find(sourceIdentityForWire("RARITY", fields[2]));
            const bool validFamily = base != mod->registeredBases.end() &&
                (base->second == "BunnyItem" || base->second == "BunnyWeapon" ||
                 base->second == "BunnyAccessory" || base->second == "BunnyBuffPotion");
            if (item == mod->registeredKindTypes.end() || !validFamily ||
                rarity == mod->registeredKindTypes.end() ||
                nativeRarities().findType(modId, fields[2]) != rarity->second ||
                rarity->second < NativeRarityRegistry::kVanillaCount ||
                rarity->second >= NativeRarityRegistry::kVanillaCount +
                    NativeRarityRegistry::kMaximumCustom ||
                !itemRarityOverrides.emplace(fields[1], rarity->second).second) {
                *error = "[CSharp][ERROR][RarityLoader] Mod=" + modId +
                    " Class=" + fields[1] + " Stage=ResolveItemRarity Reason=UnknownOrDuplicateOwnerOrRarity";
                return false;
            }
            continue;
        }
        if(!fields.empty()&&fields[0]=="RECIPES") {
            SourceRecipeOwner owner;
            if(!parseSourceRecipeOwner(fields,modId,&owner,error))return false;
            const auto identity=sourceIdentityForWire(owner.kind,owner.name);
            const auto type=mod->registeredKindTypes.find(identity);
            const auto base=mod->registeredBases.find(identity);
            const auto source=mod->sourcePaths.find(identity);
            if(type==mod->registeredKindTypes.end()||base==mod->registeredBases.end()||
               base->second!=recipeOwnerBase(owner.kind)||source==mod->sourcePaths.end()||
               runtime::modItemTypeByKey(modId+"/"+owner.name)!=type->second||
               !seenRecipeOwners.emplace(identity,true).second) {
                *error="[CSharp][ERROR][RecipeLoader] Mod="+modId+" Class="+owner.name+
                    " Stage=ResolveOwner Expected=UniqueRegistered:"+recipeOwnerBase(owner.kind)+
                    " Actual="+(base==mod->registeredBases.end()?"Missing":base->second)+
                    " Result=Rejected Reason=OwnerIdentityMismatch";return false;
            }
            owner.type=type->second;owner.source=source->second;
            recipeOwners.push_back(std::move(owner));
            if(recipeOwners.size()>kSourceMaxCategory){*error="[CSharp][ERROR][RecipeLoader] Mod="+modId+" Stage=DecodeOwners Reason=OwnerCapacityExceeded Expected=4096 Actual="+std::to_string(recipeOwners.size());return false;}
            continue;
        }
        if(!fields.empty()&&fields[0]=="SHOOT") {
            const auto key=fields.size()>1?sourceIdentityForWire("ITEM",fields[1]):"";
            const auto target=mod->registeredKindTypes.find(key);int mask=0;
            if(fields.size()!=3||target==mod->registeredKindTypes.end()||!integer(fields[2],&mask)||mask<1||mask>7) {
                *error="[CSharp][ERROR][Shoot] Mod="+modId+" Class="+(fields.size()>1?fields[1]:"<missing>")+" Stage=Decode Expected=RegisteredItemAndCallbackMask1..7 Reason=InvalidShootDescriptor Result=Rejected";return false;
            }
            const auto source=mod->sourcePaths.find(key);
            shooting.push_back({modId,fields[1],source==mod->sourcePaths.end()?"<unavailable>":source->second,"",target->second,mask});continue;
        }
        if(!fields.empty()&&fields[0]=="QUEST_FISH") {
            if(fields.size()!=6&&fields.size()!=7) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+" Stage=Decode Reason=InvalidFieldCount";
                return false;
            }
            const auto key=sourceIdentityForWire("ITEM",fields[1]);
            const auto owner=mod->registeredKindTypes.find(key);
            const auto base=mod->registeredBases.find(key);
            int pre=-1, hard=-1, catchRules=0;
            if(fields.size()==7&&(!integer(fields[6],&catchRules)||catchRules<0||catchRules>3)) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+" Class="+fields[1]+
                    " Stage=Decode Reason=InvalidCatchRules";
                return false;
            }
            std::string description, location;
            if(owner==mod->registeredKindTypes.end()||base==mod->registeredBases.end()||
               base->second!="BunnyItem"||!isBasicItem(owner->second)||
               !integer(fields[2],&pre)||!integer(fields[3],&hard)||
               (pre!=0&&pre!=1)||(hard!=0&&hard!=1)||
               !angler::decodeHex(fields[4],&description)||
               !angler::decodeHex(fields[5],&location)) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+" Class="+fields[1]+
                    " Stage=Decode Reason=OwnerOrQuestDescriptorInvalid";
                return false;
            }
            angler::Quest quest{modId,fields[1],owner->second,pre==1,hard==1,
                                std::move(description),std::move(location),catchRules};
            if(std::any_of(anglerQuests.begin(),anglerQuests.end(),
                [&](const angler::Quest& previous) {
                    return previous.type==quest.type||previous.name==quest.name;
                })) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+" Class="+fields[1]+
                    " Stage=Decode Reason=DuplicateQuest";
                return false;
            }
            if(anglerQuests.size()>=kSourceMaxCategory) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+" Stage=Decode Reason=CapacityExceeded";
                return false;
            }
            quest.catchEnabled = quest.catchRules != 0;
            anglerQuests.push_back(std::move(quest));
            continue;
        }
        if(!fields.empty()&&fields[0]=="QUEST_LOCALE") {
            if(fields.size()!=5 || !angler::validCulture(fields[2])) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+
                    " Stage=DecodeLocale Reason=InvalidCultureOrFieldCount";
                return false;
            }
            auto quest=std::find_if(anglerQuests.begin(),anglerQuests.end(),
                [&](const angler::Quest& q) { return q.name==fields[1]; });
            std::string description, location;
            if(quest==anglerQuests.end()||
               !angler::decodeHex(fields[3],&description) ||
               !angler::decodeHex(fields[4],&location) ||
               (description.empty()&&location.empty()) ||
               quest->locales.size()>=8 ||
               !quest->locales.emplace(fields[2],std::make_pair(
                    std::move(description),std::move(location))).second) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+" Class="+fields[1]+
                    " Stage=DecodeLocale Reason=InvalidTranslationOrDuplicate";
                return false;
            }
            continue;
        }
        if(!fields.empty()&&fields[0]=="QUEST_CALLBACK") {
            int mask=0;
            if(fields.size()!=3 || !integer(fields[2],&mask) ||
               mask<1 || mask>3) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+
                    " Stage=DecodeCallback Reason=InvalidDescriptor";
                return false;
            }
            auto quest=std::find_if(anglerQuests.begin(),anglerQuests.end(),
                [&](const angler::Quest& entry) { return entry.name==fields[1]; });
            if(quest==anglerQuests.end() || quest->callbackMask!=0) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+
                    " Stage=DecodeCallback Reason=InvalidOwnerOrDuplicate";
                return false;
            }
            quest->callbackMask=mask;
            continue;
        }
        if(!fields.empty()&&fields[0]=="QUEST_CATCH") {
            int rules = -1;
            if(fields.size()!=4 ||
               std::find(mod->playerClasses.begin(),mod->playerClasses.end(),
                         fields.size()>1?fields[1]:"") == mod->playerClasses.end() ||
               !integer(fields[3],&rules) || rules<0 || rules>3 ||
               !anglerPlayerRules.emplace(fields[2],true).second) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+
                    " Stage=ResolveFishingPlayer Reason=InvalidPlayerOrDuplicateCatchRule";
                return false;
            }
            auto quest=std::find_if(anglerQuests.begin(),anglerQuests.end(),
                [&](const angler::Quest& q) { return q.name==fields[2]; });
            if(quest==anglerQuests.end() ||
               (quest->catchRules!=0&&quest->catchRules!=rules)) {
                *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+" Class="+fields[1]+
                    " Stage=ResolveFishingPlayer Reason=QuestFishMissingOrConflictingPolicy";
                return false;
            }
            quest->catchRules=rules;
            quest->catchEnabled=true;
            continue;
        }
        if(!fields.empty()&&fields[0]=="USE_SOUND") {
            useSounds::Entry context;context.mod=modId;context.directory=mod->directory;
            context.name=fields.size()>1?fields[1]:"<missing>";
            const auto key=sourceIdentityForWire("ITEM",context.name);auto target=mod->registeredKindTypes.find(key);
            if(target==mod->registeredKindTypes.end())
                return useSounds::reject(context,error,"ResolveOwner","ItemNotRegistered","RegisteredItemContent",context.name);
            context.type=target->second;auto source=mod->sourcePaths.find(key);
            context.source=source==mod->sourcePaths.end()?"<unavailable>":source->second;
            useSounds::Entry e;if(!useSounds::decode(fields,context,&e,error)) return false;
            useSounds.push_back(std::move(e));continue;
        }
        if(fields.size()==4&&fields[0]=="HOLDOUT") {
            const auto key=sourceIdentityForWire("ITEM",fields[1]);auto it=mod->registeredKindTypes.find(key);float x=0,y=0;
            if(it==mod->registeredKindTypes.end()||!real(fields[2],&x)||!real(fields[3],&y)) {
                *error="[CSharp][ERROR][Holdout] Mod="+modId+" Class="+fields[1]+" Stage=Decode Reason=InvalidItemOrOffsets Result=Rejected";return false;
            }
            const auto source=mod->sourcePaths.find(key);
            holdouts.push_back({modId,fields[1],source==mod->sourcePaths.end()?"<unavailable>":source->second,it->second,x,y});continue;
        }
        if(fields.size()==3&&(fields[0]=="NATIVE_ITEM"||fields[0]=="NATIVE_PROJECTILE")) {
            const bool projectile=fields[0]=="NATIVE_PROJECTILE";
            const auto key=sourceIdentityForWire(projectile?"PROJECTILE":"ITEM",fields[1]);
            const auto registered=mod->registeredKindTypes.find(key);
            if(registered==mod->registeredKindTypes.end()) {
                *error="[CSharp][ERROR][NativeDefaults] Mod="+modId+" Class="+fields[1]+" Stage=Decode Result=Rejected Reason=ContentCategoryNotRegistered";return false;
            }
            const auto source=mod->sourcePaths.find(key);
            nativeDefaults.push_back({fields[1],modId,source==mod->sourcePaths.end()?"<unavailable>":source->second,fields[2],registered->second,projectile});
            continue;
        }
        if(fields.size()>1&&fields[0]=="WALL") {
            const auto key=sourceIdentityForWire("WALL",fields[1]);auto target=mod->registeredKindTypes.find(key);
            walls::Definition definition;
            if(target==mod->registeredKindTypes.end()||runtime::modWallTypeByName(modId,fields[1])!=target->second) {
                *error="[CSharp][ERROR][WallDefaults] Mod="+modId+" Class="+fields[1]+" Stage=ResolveContent Result=Rejected Reason=WallNotRegistered";return false;
            }
            auto source=mod->sourcePaths.find(key);const std::string sourcePath=source==mod->sourcePaths.end()?"<unavailable>":source->second;
            if(!walls::parse(fields,&definition,error)){*error="[CSharp][ERROR][Wall] Mod="+modId+" Class="+fields[1]+" File="+sourcePath+" ContentId="+std::to_string(target->second)+" Stage=Decode Result=Rejected Detail="+*error;return false;}
            definition.mod=modId;definition.type=target->second;definition.source=sourcePath;
            if(!seen.emplace(key,true).second){*error="Duplicate WALL: "+fields[1];return false;}
            wallDefinitions.push_back(std::move(definition));continue;
        }
        const std::string identity = fields.size() >= 2
            ? sourceIdentityForWire(fields[0], fields[1]) : "";
        if (identity.empty() || !seen.emplace(identity, true).second) {
            *error = "Classe C# duplicada ou registro invalido";
            return false;
        }
        if (fields[0] == "DUST" && fields.size() == 5) {
            const auto native = mod->registeredKindTypes.find(identity);
            const auto source = mod->dustTextures.find(fields[1]);
            int updateType = -1;
            int hooks = -1;
            if (native == mod->registeredKindTypes.end() ||
                source == mod->dustTextures.end() ||
                runtime::modDustTypeByName(modId, fields[1]) != native->second ||
                !integer(fields[2], &updateType) ||
                !integer(fields[3], &hooks) ||
                updateType < -1 || updateType >= runtime::kVanillaDustCount ||
                hooks < 0 || hooks > 31 ||
                fields[4] != (source->second.empty() ? "-" : source->second)) {
                *error = "[CSharp][ERROR][DustLoader] Mod=" + modId +
                    " Class=" + fields[1] +
                    " Stage=ApplyDefaults Reason=NativeIdentityOrDescriptorMismatch";
                return false;
            }
            if (hooks != 0 && !managedDustCallbacksConnected()) {
                *error = "[CSharp][ERROR][DustLoader] Mod=" + modId +
                    " Class=" + fields[1] +
                    " Stage=BindCallbacks Result=Rejected Reason=MonoDustBridgeNotConnected";
                return false;
            }
            if (!runtime::setModDustManagedHooks(native->second, static_cast<unsigned>(hooks))) {
                *error = "[CSharp][ERROR][DustLoader] Stage=Publish Reason=NativeHookMaskRejected";
                return false;
            }
            runtime::setModDustUpdateType(native->second, updateType);
            mod->dustMasks[fields[1]] = static_cast<unsigned>(hooks);
            ++dustRows;
            if (dustRows > static_cast<int>(kSourceMaxCategory)) {
                *error = "[CSharp][ERROR][DustLoader] Stage=Decode Reason=CategoryCapacityExceeded";
                return false;
            }
            continue;
        }
        if (fields[0] == "TILE" && tile_wire::supported(fields.size())) {
            const auto it = mod->registeredKindTypes.find(identity);
            if (it == mod->registeredKindTypes.end() ||
                runtime::modTileTypeByName(modId, fields[1]) != it->second) {
                *error = fields[1] + ": BunnyTile nao registrado"; return false;
            }
            ManagedTileEntry tile;
            tile.name = fields[1]; tile.type = it->second;
            const bool flagsEncoded = integer(fields[2], &tile.flags);
            if (!flagsEncoded || tile.flags < 0 || tile.flags > 65535) {
                const auto source = mod->sourcePaths.find(identity);
                *error = "[CSharp][ERROR][TileDefaults] Mod=" + modId + " Class=" + fields[1] +
                    " File=" + (source == mod->sourcePaths.end() ? "<unavailable>" : source->second) +
                    " API=BunnyTile.Flags Field=Flags ContentId=" + std::to_string(tile.type) +
                    " Stage=Decode Expected=Int32:KnownMask0xFFFF Actual=" + fields[2] +
                    " Result=Rejected Reason=" + (flagsEncoded ? "UnknownFlagBits" : "InvalidInt32");
                return false;
            }
            if (!integer(fields[3], &tile.shape) ||
                !integer(fields[4], &tile.coordinateHeight) ||
                !integer(fields[5], &tile.adjacentTile) ||
                !integer(fields[7], &tile.mapR) ||
                !integer(fields[8], &tile.mapG) ||
                !integer(fields[9], &tile.mapB) ||
                !integer(fields[11], &tile.failDust) ||
                !integer(fields[12], &tile.normalDust) ||
                (tile_wire::hasAnimationHeight(fields.size()) && !integer(fields[13], &tile.animationFrameHeight)) ||
                (tile_wire::hasAnimationPolicy(fields.size()) &&
                 (!integer(fields[14], &tile.animationFrames) ||
                  !integer(fields[15], &tile.animationFrameTicks))) ||
                tile.shape < 0 || tile.shape > 2 ||
                tile.coordinateHeight != 18 ||
                tile.adjacentTile < -1 || tile.adjacentTile >= 754 ||
                tile.mapR < 0 || tile.mapR > 255 ||
                tile.mapG < 0 || tile.mapG > 255 ||
                tile.mapB < 0 || tile.mapB > 255 ||
                tile.failDust < 0 || tile.failDust > 20 ||
                tile.normalDust < 0 || tile.normalDust > 20 ||
                tile.animationFrameHeight < 0 || tile.animationFrameHeight > 4096 ||
                tile.animationFrames < 1 || tile.animationFrames > 64 ||
                tile.animationFrameTicks < 0 || tile.animationFrameTicks > 600 ||
                (tile.animationFrames > 1 &&
                 (tile.animationFrameHeight == 0 || tile.animationFrameTicks == 0)) ||
                (tile.animationFrames == 1 && tile.animationFrameTicks != 0) ||
                fields[6].size() > 72 || fields[10].size() > 160) {
                *error = fields[1] + ": descritor BunnyTile fora dos limites"; return false;
            }
            tile.dustName = fields[6] == "-" ? "" : fields[6];
            if (fields[10] != "-") {
                const std::string& hex = fields[10];
                if (hex.size() % 2) { *error = "Nome do mapa tile invalido"; return false; }
                for (size_t k = 0; k < hex.size(); k += 2) {
                    const auto nibble = [](char c) -> int {
                        if (c >= '0' && c <= '9') return c - '0';
                        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                        return -1;
                    };
                    const int left = nibble(hex[k]), right = nibble(hex[k + 1]);
                    if (left < 0 || right < 0) { *error = "Nome do mapa tile invalido"; return false; }
                    tile.mapName.push_back(static_cast<char>(left * 16 + right));
                }
            }
            if (tile_wire::hasAdjacencies(fields.size()) && fields[16] != "-") {
                std::istringstream adjacentStream(fields[16]);
                std::string token;
                while (std::getline(adjacentStream, token, ',')) {
                    int alias = -1;
                    if (!integer(token, &alias) || alias < 0 || alias >= runtime::kVanillaTileCount ||
                        alias == tile.adjacentTile || tile.adjacentTiles.size() >= 16 ||
                        std::find(tile.adjacentTiles.begin(), tile.adjacentTiles.end(), alias) != tile.adjacentTiles.end()) {
                        *error = fields[1] + ": lista AdjacentTiles invalida"; return false;
                    }
                    tile.adjacentTiles.push_back(alias);
                }
                if (fields[16].empty() || fields[16].back() == ',') {
                    *error = fields[1] + ": lista AdjacentTiles malformada"; return false;
                }
            }
            if (tile_wire::hasBreakPolicy(fields.size())) {
                if ((fields[17] != "0" && fields[17] != "1") ||
                    (fields[18] != "0" && fields[18] != "1")) {
                    *error = fields[1] + ": flags AllowBreaking/AllowDrops invalidas";
                    return false;
                }
                tile.allowBreaking = fields[17] == "1";
                tile.allowDrops = fields[18] == "1";
            }
            if (tile_wire::hasCallbacks(fields.size()) &&
                (!integer(fields[19], &tile.callbackMask) || tile.callbackMask < 0 || tile.callbackMask > 65535)) {
                *error = fields[1] + ": TileCallbackMask invalido"; return false;
            }
            if (tile_wire::hasDrawCachePolicy(fields.size())) {
                if (fields[20] != "0" && fields[20] != "1") {
                    *error = fields[1] + ": CacheDrawData deve ser 0 ou 1"; return false;
                }
                tile.cacheDrawData = fields[20] == "1";
            }
            tiles.push_back(std::move(tile));
            if (tiles.size() > kSourceMaxCategory) {
                *error = "[CSharp][ERROR][Tile] Mod="+modId+" Stage=Decode Expected=AtMost"+std::to_string(kSourceMaxCategory)+" Actual="+std::to_string(tiles.size())+" Reason=CategoryCapacityExceeded Result=ModRejected"; return false;
            }
            continue;
        }
        if (fields[0] == "ITEM") {
            BasicItemEntry record;
            const auto type=mod->registeredKindTypes.find(identity);
            if (type==mod->registeredKindTypes.end() || !isBasicItem(type->second)) {
                *error=fields[1]+": BunnyItem nao registrado";return false;
            }
            if(!parseBasicItemWire(fields,&record,error))return false;
            record.type=type->second;basicItems.push_back(std::move(record));
            if (basicItems.size() > kSourceMaxCategory) {
                *error = "[CSharp][ERROR][Item] Mod="+modId+" Stage=Decode Expected=AtMost"+std::to_string(kSourceMaxCategory)+" Actual="+std::to_string(basicItems.size())+" Reason=CategoryCapacityExceeded Result=ModRejected";
                return false;
            }
            continue;
        } else if (fields[0] == "PLAYER" && (fields.size() == 4 || fields.size() == 5 || fields.size() == 8 || fields.size() == 9 || fields.size() == 14)) {
            if (std::find(mod->playerClasses.begin(), mod->playerClasses.end(),
                          fields[1]) == mod->playerClasses.end()) {
                *error = "BunnyPlayer C# nao registrado: " + fields[1];
                return false;
            }
            ManagedPlayerEntry record;
            record.name = fields[1];
            record.accessoryName = fields[2];
            const bool globalPlayer = record.accessoryName == "-";
            const auto target = accessoryTypes.find(record.accessoryName);
            if ((!globalPlayer && target == accessoryTypes.end()) ||
                !integer(fields[3], &record.equippedDefenseBonus) ||
                record.equippedDefenseBonus < 0 || record.equippedDefenseBonus > 100 ||
                (fields.size() >= 5 && !integer(fields[4], &record.moveSpeedPercent)) ||
                record.moveSpeedPercent < 0 || record.moveSpeedPercent > 50 ||
                (fields.size() >= 8 &&
                    (!integer(fields[5], &record.runAccelerationPercent) ||
                     record.runAccelerationPercent < 0 || record.runAccelerationPercent > 30 ||
                     (fields[6] != "0" && fields[6] != "1") ||
                     fields[7].size() > 100 || fields[7].empty() ||
                     std::any_of(fields[7].begin(), fields[7].end(),
                         [](unsigned char c) { return c < 32 || c > 126; }))) ||
                (fields.size() >= 9 &&
                    (fields[8].size() > 100 || fields[8].empty() ||
                     std::any_of(fields[8].begin(), fields[8].end(),
                         [](unsigned char c) { return c < 32 || c > 126; })))) {
                *error = "Politica BunnyPlayer invalida: " + record.name;
                return false;
            }
            record.accessoryType = globalPlayer ? -1 : target->second;
            if (fields.size() >= 8) {
                record.frameShadow = fields[6] == "1";
                if (fields[7] != "-") record.worldJoinMessage = fields[7];
            }
            if (fields.size() >= 9 && fields[8] != "-")
                record.respawnMessage = fields[8];
            if (fields.size() == 14) {
                if ((fields[9] != "0" && fields[9] != "1") ||
                    (fields[10] != "0" && fields[10] != "1") ||
                    fields[11].empty() || fields[11].size() > 100 ||
                    std::any_of(fields[11].begin(), fields[11].end(),
                        [](unsigned char c) { return c < 32 || c > 126; }) ||
                    !integer(fields[12], &record.lifeRegenBonus) ||
                    !integer(fields[13], &record.manaRegenBonus) ||
                    record.lifeRegenBonus < 0 || record.lifeRegenBonus > 30 ||
                    record.manaRegenBonus < 0 || record.manaRegenBonus > 30) {
                    *error = "Politica C# de regeneracao/ciclo invalida: " + record.name;
                    return false;
                }
                record.preUpdate = fields[9] == "1";
                record.postUpdate = fields[10] == "1";
                if (fields[11] != "-") record.deadMessage = fields[11];
            }
            if (globalPlayer && (record.equippedDefenseBonus != 0 ||
                record.moveSpeedPercent != 0 || record.runAccelerationPercent != 0 ||
                record.frameShadow || record.lifeRegenBonus != 0 ||
                record.manaRegenBonus != 0)) {
                *error = "BunnyPlayer global nao pode aplicar bonus de acessorio: " + record.name;
                return false;
            }
            players.push_back(std::move(record));
            continue;
        }
        const auto registered = mod->registeredKindTypes.find(identity);
        if (registered == mod->registeredKindTypes.end() || registered->second < 0) {
            *error = "Classe C# nao registrada no Terraria: " + fields[1];
            return false;
        }
        if (fields[0] == "RARITY" && fields.size() == 2) {
            if (nativeRarities().findType(modId, fields[1]) != registered->second) {
                *error = "[CSharp][ERROR][RarityLoader] Mod=" + modId + " Class=" + fields[1] +
                    " Stage=BindIdentity Reason=NativeIdMismatch Result=Rejected";
                return false;
            }
            ++rarityCount;
        } else if (fields[0] == "PROJECTILE" && (fields.size() == 8 || fields.size() == 12 || fields.size() == 13 || fields.size() == 14 ||
                 fields.size() == 16 || fields.size() == 17 || fields.size() == 18)) {
            ManagedProjectileEntry record;
            record.name = fields[1];
            record.type = registered->second;
            int friendly = 0;
            if (!integer(fields[2], &record.values.width) ||
                !integer(fields[3], &record.values.height) ||
                !integer(fields[4], &record.values.aiStyle) ||
                !integer(fields[5], &record.values.penetrate) ||
                !integer(fields[6], &record.values.timeLeft) ||
                !integer(fields[7], &friendly) || friendly != 1) {
                *error = "Valores invalidos de projetil C#: " + record.name;
                return false;
            }
            record.values.friendly = true;
            if (fields.size() >= 12) {
                int minion = -1, collide = -1, ignoreWater = -1;
                if (!integer(fields[8], &minion) ||
                    !real(fields[9], &record.values.minionSlots) ||
                    !integer(fields[10], &collide) ||
                    !integer(fields[11], &ignoreWater) ||
                    (minion != 0 && minion != 1) ||
                    (collide != 0 && collide != 1) ||
                    (ignoreWater != 0 && ignoreWater != 1) ||
                    (minion == 1 && (record.values.minionSlots <= 0.f || record.values.minionSlots > 10.f)) ||
                    (minion == 0 && record.values.minionSlots != 0.f)) {
                    *error = "Contrato C# de minion/projetil invalido: " + record.name;
                    return false;
                }
                record.values.minion = minion == 1;
                record.values.tileCollide = collide == 1;
                record.values.ignoreWater = ignoreWater == 1;
                if (fields.size() >= 13 &&
                    (!integer(fields[12], &record.values.frames) ||
                     record.values.frames < 1 || record.values.frames > 32)) {
                    *error = "Frames invalidos do projetil: " + record.name;
                    return false;
                }
            }
            ProjectilePolicy policy{record.values.frames,record.values.animationFrameTicks,record.values.minionScanTicks,record.values.minionTargetRange,record.values.minion};
            std::string detail;
            if(((fields.size()>=16)&&!parseProjectileTuning(fields,&policy,&detail))||!validateProjectilePolicy(policy,&detail)) {
                *error="[CSharp][ERROR][ProjectilePolicy] Mod="+modId+" Class="+record.name+" "+detail;return false;
            }
            record.values.animationFrameTicks=policy.animationFrameTicks;record.values.minionScanTicks=policy.minionScanTicks;record.values.minionTargetRange=policy.minionTargetRange;
            if (fields.size() == 14 || fields.size() >= 17) {
                int contact = 0;
                const auto& value = fields[fields.size()==14?13:16];
                if (!integer(value, &contact) || (contact != 0 && contact != 1) || !record.values.minion) {
                    *error = "MinionContactDamage invalido: " + record.name;
                    return false;
                }
                record.values.minionContactDamage = contact == 1;
            }
            if(fields.size()==18&&!parseSummonMovement(fields[17],&record.values.movement,&detail)) {
                *error="[CSharp][ERROR][Summon] Mod="+modId+" Class="+record.name+" Stage=DecodeMovement Result=Rejected "+detail;return false;
            }
            projectilesByName.emplace(record.name, record.type);
            projectiles.push_back(std::move(record));
        } else if (fields[0] == "WEAPON" && (fields.size() == 6 || fields.size() == 9)) {
            ManagedWeaponEntry record;
            record.name = fields[1];
            record.type = registered->second;
            if (!integer(fields[2], &record.values.damage) ||
                !integer(fields[3], &record.values.useTime) ||
                !real(fields[4], &record.values.shootSpeed) ||
                fields[5].empty()) {
                *error = "Valores invalidos de arma C#: " + record.name;
                return false;
            }
            if (fields.size() == 9) {
                int summon = -1;
                if (!integer(fields[6], &summon) || summon != 1 ||
                    fields[7].empty() || fields[7] == "-" || fields[7].size() > 72 ||
                    !integer(fields[8], &record.minionBuffTime) ||
                    record.minionBuffTime < 1 || record.minionBuffTime > 36000) {
                    *error = "Contrato de arma summon invalido: " + record.name;
                    return false;
                }
                record.summon = true;
                record.minionBuffName = fields[7];
            }
            shootNames.push_back(fields[5]);
            weapons.push_back(std::move(record));
        } else if (fields[0] == "ACCESSORY" &&
                   (fields.size() == 3 || fields.size() == 4 || fields.size() == 9)) {
            ManagedAccessoryEntry record;
            record.name = fields[1];
            record.type = registered->second;
            if (!integer(fields[2], &record.defenseBonus) ||
                record.defenseBonus < 0 || record.defenseBonus > 100 ||
                (fields.size() >= 4 &&
                    (fields[3].size() > 180 ||
                     std::any_of(fields[3].begin(), fields[3].end(),
                         [](unsigned char c) { return c < 32 || c == 127; }))) ||
                runtime::modItemTypeByKey(modId + "/" + record.name) != record.type) {
                *error = "Valores invalidos de acessorio C#: " + record.name;
                return false;
            }
            if (fields.size() >= 4 && fields[3] != "-")
                record.tooltip = fields[3];
            if (fields.size() == 9) {
                char* end = nullptr;
                errno = 0;
                const unsigned long long color = std::strtoull(fields[8].c_str(), &end, 10);
                if (!integer(fields[4], &record.visualMode) ||
                    record.visualMode < 1 || record.visualMode > 3 ||
                    fields[5].empty() || fields[5].size() > 180 ||
                    fields[5].front() == '/' ||
                    fields[5].find("..") != std::string::npos ||
                    fields[5].find(static_cast<char>(92)) != std::string::npos ||
                    fields[5].find(':') != std::string::npos ||
                    fields[5].find('|') != std::string::npos ||
                    fields[5].substr(fields[5].size() >= 4 ? fields[5].size() - 4 : 0) != ".png" ||
                    !real(fields[6], &record.visualScale) ||
                    record.visualScale < 0.25f || record.visualScale > 3.f ||
                    !real(fields[7], &record.visualOpacity) ||
                    record.visualOpacity < 0.f || record.visualOpacity > 1.f ||
                    errno || !end || *end || color > 0xffffffffULL) {
                    *error = "Politica visual invalida no acessorio C#: " + record.name;
                    return false;
                }
                record.visualColor = static_cast<uint32_t>(color);
                record.visualTexture = fields[5];
            }
            accessoryTypes[record.name] = record.type;
            accessories.push_back(std::move(record));
        } else if (fields[0] == "PREFIX" && fields.size() == 16) {
            ManagedPrefixEntry record;
            record.name = fields[1];
            record.displayName = fields[15];
            record.type = registered->second;
            if (!integer(fields[2], &record.category) ||
                !real(fields[3], &record.rollWeight) ||
                !integer(fields[10], &record.crit) ||
                !integer(fields[11], &record.tagDamage) ||
                !integer(fields[12], &record.armorPenetration) ||
                !real(fields[13], &record.valueMultiplier) ||
                !integer(fields[14], &record.accessoryDefense) ||
                bl::runtime::modPrefixByKey(modId + "/" + record.name) != record.type) {
                *error = "Descritor de prefixo C# invalido: " + record.name;
                return false;
            }
            for (size_t i = 0; i < record.multipliers.size(); ++i)
                if (!real(fields[4 + i], &record.multipliers[i])) {
                    *error = "Multiplicador de prefixo C# invalido: " + record.name;
                    return false;
                }
            prefixes.push_back(std::move(record));
        } else if (fields[0] == "NPC" && fields.size() == 10) {
            ManagedNpcEntry record;
            record.name = fields[1];
            record.type = registered->second;
            if (!integer(fields[2], &record.width) ||
                !integer(fields[3], &record.height) ||
                !integer(fields[4], &record.damage) ||
                !integer(fields[5], &record.defense) ||
                !integer(fields[6], &record.lifeMax) ||
                !integer(fields[7], &record.aiStyle) ||
                !integer(fields[8], &record.animationType) ||
                !integer(fields[9], &record.frames) ||
                record.width < 8 || record.width > 256 ||
                record.height < 8 || record.height > 256 ||
                record.damage < 0 || record.damage > 500 ||
                record.defense < 0 || record.defense > 200 ||
                record.lifeMax < 1 || record.lifeMax > 100000 ||
                record.aiStyle != 1 || record.animationType != 1 ||
                record.frames != 2 ||
                runtime::modNpcTypeByName(modId, record.name) != record.type) {
                *error = "Atributos ou registro do BunnyNPC invalido: " + record.name;
                return false;
            }
            npcs.push_back(std::move(record));
        } else if (fields[0] == "BUFF" && (fields.size() == 3 || fields.size() == 4 || fields.size() == 6 || fields.size() == 7)) {
            ManagedBuffEntry record;
            record.name = fields[1];
            record.type = registered->second;
            if (!integer(fields[2], &record.defenseBonus) ||
                record.defenseBonus < -50 || record.defenseBonus > 100 ||
                runtime::modBuffTypeByName(modId, record.name) != record.type) {
                *error = "Valores invalidos de buff C#: " + record.name;
                return false;
            }
            if (fields.size() >= 4 && fields[3] != "-") {
                const auto minion = projectilesByName.find(fields[3]);
                if (minion == projectilesByName.end()) {
                    *error = "[CSharp][ERROR][Buff] Mod="+modId+" Class="+record.name+" API=BunnyBuff.MinionProjectile Stage=ResolveProjectile Expected=RegisteredLocalProjectile Actual="+fields[3]+" Result=Rejected Reason=MissingDependency";
                    return false;
                }
                record.minionProjectileType = minion->second;
            }
            if(fields.size()>=6) {
                if(!integer(fields[4],&record.flags)||record.flags<0||record.flags>15){*error="[CSharp][ERROR][Buff] Mod="+modId+" Class="+record.name+" Field=Flags Expected=0..15 Actual="+fields[4]+" Reason=InvalidFlags";return false;}
                if(fields[5]!="-") {
                    const auto& hex=fields[5];
                    if(hex.size()%2||hex.size()>1024){*error=record.name+": Field=Description Reason=InvalidHexLength";return false;}
                    auto nibble=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;};
                    for(size_t n=0;n<hex.size();n+=2){int a=nibble(hex[n]),b=nibble(hex[n+1]);
                        if(a<0||b<0||((a<<4)|b)==0||((a<<4)|b)==127||(((a<<4)|b)<32&&((a<<4)|b)!=10)){*error=record.name+": Field=Description Reason=InvalidDescriptionHex";return false;}
                        record.description+=static_cast<char>((a<<4)|b);
                    }
                }
            }
            if(fields.size()==7&&(!integer(fields[6],&record.defenseMultiplierPercent)||record.defenseMultiplierPercent<0||record.defenseMultiplierPercent>100)) {
                *error="[CSharp][ERROR][Buff] Mod="+modId+" Class="+record.name+" Field=DefenseMultiplierPercent Expected=0..100 Actual="+fields[6]+" Reason=InvalidMultiplier";return false;
            }
            buffsByName[record.name] = record.type;
            buffs.push_back(std::move(record));
        } else if (fields[0] == "POTION" && fields.size() == 4) {
            ManagedPotionEntry record;
            record.name = fields[1];
            record.type = registered->second;
            if (!integer(fields[2], &record.duration) ||
                record.duration < 1 || record.duration > 216000 ||
                runtime::modItemTypeByKey(modId + "/" + record.name) != record.type) {
                *error = "Duracao ou registro da pocao C# invalido";
                return false;
            }
            auto target = buffsByName.find(fields[3]);
            if (target == buffsByName.end()) {
                *error = "Buff da pocao C# inexistente: " + fields[3];
                return false;
            }
            record.buffType = target->second;
            potions.push_back(std::move(record));
        } else {
            *error = "Linha V2 inesperada: tipo=" + fields[0] +
                     " classe=" + fields[1] +
                     " campos=" + std::to_string(fields.size());
            return false;
        }
        if (tiles.size() > kSourceMaxCategory || basicItems.size() > kSourceMaxCategory || projectiles.size() > kSourceMaxCategory || weapons.size() > kSourceMaxCategory ||
            buffs.size() > kSourceMaxCategory || potions.size() > kSourceMaxCategory ||
            accessories.size() > kSourceMaxCategory || players.size() > kSourceMaxCategory ||
            npcs.size() > kSourceMaxCategory || prefixes.size() > kSourceMaxCategory || rarityCount > kSourceMaxCategory) {
            *error = "Quantidade de conteudo C# excede 4096 por categoria";
            return false;
        }
    }
    if (dustRows != mod->dusts ||
        tiles.size() != static_cast<size_t>(mod->tiles) ||
        projectiles.size() != static_cast<size_t>(mod->projectiles) ||
        weapons.size() + potions.size() + accessories.size() + basicItems.size() !=
            static_cast<size_t>(mod->items) ||
        buffs.size() != static_cast<size_t>(mod->buffs) ||
        potions.size() != static_cast<size_t>(mod->potions) ||
        basicItems.size() != static_cast<size_t>(mod->basicItems) ||
        accessories.size() != static_cast<size_t>(mod->accessories) ||
        players.size() != static_cast<size_t>(mod->players) ||
        npcs.size() != static_cast<size_t>(mod->npcs) ||
        prefixes.size() != static_cast<size_t>(mod->prefixes) ||
        rarityCount != static_cast<size_t>(mod->rarities) ||
        seen.size() != mod->registeredKindTypes.size() + mod->playerClasses.size()) {
        *error = "As classes compiladas nao correspondem ao conteudo registrado";
        return false;
    }
    for (size_t i = 0; i < weapons.size(); ++i) {
        const auto projectile = projectilesByName.find(shootNames[i]);
        if (projectile == projectilesByName.end()) {
            *error = "Shoot referencia projetil C# inexistente: " + shootNames[i];
            return false;
        }
        weapons[i].shootType = projectile->second;
        if (weapons[i].summon) {
            const auto buff = buffsByName.find(weapons[i].minionBuffName);
            const auto minion = std::find_if(projectiles.begin(), projectiles.end(),
                [&](const ManagedProjectileEntry& candidate) {
                    return candidate.type == weapons[i].shootType && candidate.values.minion;
                });
            if (buff == buffsByName.end() || minion == projectiles.end()) {
                *error = "Summon requer buff e projetil minion do proprio mod: " + weapons[i].name;
                return false;
            }
            weapons[i].minionBuffType = buff->second;
            const auto buffDefinition = std::find_if(buffs.begin(), buffs.end(),
                [&](const ManagedBuffEntry& candidate) { return candidate.type == buff->second; });
            if (buffDefinition == buffs.end() ||
                buffDefinition->minionProjectileType != weapons[i].shootType) {
                *error = "Summon buff deve referenciar o mesmo projetil: " + weapons[i].name;
                return false;
            }
        }
    }
    if (!banners::registry(runtime::kVanillaNpcCount).validate(modId, bannerBindings, error)) {
        *error = "[CSharp][BannerLoader] Stage=Preflight Result=Rejected Reason=" + *error;
        return false;
    }
    if (!applyManagedNativeBuffDefaultsBatch(modId, buffs, potions)) {
        *error = "Falha ao validar buffs e pocoes C# gerenciados";
        return false;
    }
    if ((!projectiles.empty() || !weapons.empty()) &&
        !applyManagedNativeDefaultsBatch(modId, projectiles, weapons,error)) {
        if(error->empty())*error = "[CSharp][ERROR][NativeContent] Mod="+modId+" API=BunnyProjectile/BunnyWeapon Stage=ApplyDefaults Result=Rejected Reason=BatchIdentityOrValueContractMismatch";
        return false;
    }
    if (!applyManagedNativePlayerDefaultsBatch(modId, accessories, players)) {
        *error = "Falha ao validar ou aplicar politica BunnyPlayer C#";
        return false;
    }
    if (!applyManagedNativeNpcDefaultsBatch(modId, npcs)) {
        *error = "Falha ao validar ou aplicar os valores BunnyNPC gerenciados";
        return false;
    }
    if (!applyManagedPrefixDescriptors(modId, prefixes)) {
        *error = "Falha ao validar descritores de prefixo C#";
        return false;
    }
    if(!wallDefinitions.empty()&&!walls::publish(std::move(wallDefinitions),error))return false;
    if (!tiles.empty() && !applyManagedNativeTileDefaultsBatch(modId, tiles)) {
        *error = "Falha ao aplicar os defaults nativos de BunnyTile"; return false;
    }
    if (!useSounds::publish(useSounds,error)) return false;
    for (auto& item : basicItems) {
        const auto overrideType = itemRarityOverrides.find(item.name);
        if (overrideType != itemRarityOverrides.end())
            item.values[BasicRare] = overrideType->second;
    }
    for (const auto& [name, rarity] : itemRarityOverrides) {
        const auto itemKey = sourceIdentityForWire("ITEM", name);
        const auto item = mod->registeredKindTypes.find(itemKey);
        const auto base = mod->registeredBases.find(itemKey);
        if (item == mod->registeredKindTypes.end() ||
            base == mod->registeredBases.end()) {
            *error = "[CSharp][ERROR][RarityLoader] Mod=" + modId +
                " Class=" + name + " Stage=ApplyRarity Reason=MissingItemOwner";
            return false;
        }
        auto native = std::find_if(nativeDefaults.begin(), nativeDefaults.end(),
            [&](const NativeDefaultsRequest& request) {
                return !request.projectile && request.name == name;
            });
        if (base->second == "BunnyItem") {
            if (std::none_of(basicItems.begin(), basicItems.end(),
                [&](const BasicItemEntry& basic) { return basic.name == name; })) {
                *error = "[CSharp][ERROR][RarityLoader] Mod=" + modId +
                    " Class=" + name + " Stage=ApplyRarity Reason=MissingBasicItem";
                return false;
            }
            if (native == nativeDefaults.end()) continue;
        } else if (native == nativeDefaults.end()) {
            const auto source = mod->sourcePaths.find(itemKey);
            nativeDefaults.push_back({name, modId,
                source == mod->sourcePaths.end() ? "<unavailable>" : source->second,
                "rare:i:" + std::to_string(rarity), item->second, false});
            continue;
        }
        if (native->descriptor.starts_with("rare:") ||
            native->descriptor.find(";rare:") != std::string::npos) {
            *error = "[CSharp][ERROR][RarityLoader] Mod=" + modId +
                " Class=" + name + " Stage=ApplyRarity Reason=ConflictingNativeRare";
            return false;
        }
        if (base->second != "BunnyItem") {
            if (!native->descriptor.empty()) native->descriptor += ";";
            native->descriptor += "rare:i:" + std::to_string(rarity);
        }
    }
    if (!applyNativeBasicItemDefaults(modId,basicItems,error)) return false;
    for (const auto& item : basicItems) {
        if (item.defense>0 && !setBasicItemDefense(item.type,item.defense,error)) return false;
    }
    for(const auto& quest:anglerQuests) {
        if(std::none_of(basicItems.begin(),basicItems.end(),
            [&](const BasicItemEntry& item) {
                return item.type==quest.type&&item.name==quest.name;
            })) {
            *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+" Class="+quest.name+
                " Stage=Publish Reason=PrimaryItemDefaultsMissing";
            return false;
        }
    }
    if(!enqueueOwnedSourceRecipes(modId,recipeOwners,error))return false;
    for(const auto& request:nativeDefaults)if(!seen.contains(sourceIdentityForWire(request.projectile?"PROJECTILE":"ITEM",request.name))) {
        *error="[CSharp][ERROR][NativeDefaults] Mod="+modId+" Class="+request.name+" Stage=Publish Result=Rejected Reason=PrimaryDefaultsMissing";return false;
    }
    if(!holdout::publish(holdouts,error))return false;
    if(!nativeDefaults.empty()) {
        if(!publishNativeDefaults(nativeDefaults,error,[](const std::string& key){
            const auto slash=key.find('/');return slash==std::string::npos?-1:runtime::modProjectileTypeByName(key.substr(0,slash),key.substr(slash+1));
        }))return false;
        requestBasicItemInstanceRefresh("NativeDefaultsPublished");
        BL_INFO("[CSharp][NativeDefaults] Mod=%s Stage=Publish Contents=%zu Result=Ready",modId.c_str(),nativeDefaults.size());
    }
    if(!shooting::publish(shooting,error))return false;
    localization::stage(modId,std::move(localized));
    if(!angler::registry().publish(modId,anglerQuests,error)) {
        *error="[CSharp][ERROR][AnglerQuest] Mod="+modId+
            " Stage=Publish Reason="+*error;
        return false;
    }
    if (!banners::registry(runtime::kVanillaNpcCount).publish(modId, bannerBindings, error)) {
        *error = "[CSharp][BannerLoader] Stage=Publish Result=Rejected Reason=" + *error;
        return false;
    }
    mod->runtimeApplied = true;
    return true;
}

std::vector<SourceModStatus> sourceModStatuses() { return g_statuses; }

bool prepareBannerWorldArraysOnGameThread(std::string* error) {
    if (!banners::growGameArraysOnMainThread(error)) return false;
    const auto state = banners::registry(runtime::kVanillaNpcCount).snapshot();
    if (state->bannerToItem.empty()) return true;
    if (!banners::installBannerLookupHooks()) {
        if (error) *error = "BannerSystem: falha ao instalar 3 hooks na thread do jogo";
        return false;
    }
    if (!banners::installBannerSaveHooks()) {
        if (error) *error = "BannerSystem: falha ao instalar hooks Save/WriteFullState";
        return false;
    }
    // Nao ativar antes do sidecar, refresh, rede e tile completos.
    banners::setBannerLookupsActive(false);
    return true;
}

bool sourceHasBannerMappings() {
    return !banners::registry(runtime::kVanillaNpcCount)
        .snapshot()->bannerToItem.empty();
}

bool restoreBannerWorldCountersOnGameThread(uint64_t worldEpoch,
                                            std::string* error) {
    if (!error || !worldEpoch) return false;
    if (!sourceHasBannerMappings()) return true;
    banners::WorldEntries values;
    if (!banners::worldSession().view(worldEpoch, &values)) {
        *error = "BannerLoader: mundo sem sidecar confiavel ou cloud save";
        return false;
    }
    if (!prepareBannerWorldArraysOnGameThread(error)) return false;
    if (!banners::restoreGameBannerCounters(values, error)) return false;
    return true;
}

bool saveBannerWorldCountersOnGameThread(uint64_t worldEpoch,
                                         bool authoritative,
                                         std::string* error) {
    if (!error || !worldEpoch || !authoritative) return false;
    if (!sourceHasBannerMappings()) return true;
    banners::WorldEntries values;
    if (!banners::worldSession().view(worldEpoch, &values)) {
        *error = "BannerLoader: mundo sem sidecar confiavel ou cloud save";
        return false;
    }
    if (!banners::captureGameBannerCounters(&values, error)) return false;
    if (!banners::worldSession().commit(worldEpoch, authoritative, values)) {
        *error = "BannerLoader: falha ao gravar sidecar sem perder o anterior";
        return false;
    }
    return true;
}

bool sourceRuntimeApplied(const std::string& modId) {
    for (const auto& mod : g_statuses)
        if (mod.id == modId && mod.loaded && mod.runtimeApplied) return true;
    return false;
}

}


