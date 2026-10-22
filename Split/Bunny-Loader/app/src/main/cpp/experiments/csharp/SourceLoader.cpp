#include "experiments/csharp/SourceLimits.h"
#include "experiments/csharp/SourceLoader.h"

#include "experiments/csharp/NativeContent.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "content/buffs/ModBuffs.h"
#include "content/tiles/ModTiles.h"
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
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
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
                    ": limite C# excedido (512 arquivos; 256 KiB por arquivo)";
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
    const bool weapon = type.base == "BunnyWeapon";
    const bool buff = type.base == "BunnyBuff";
    const bool potion = type.base == "BunnyBuffPotion";
    const bool accessory = type.base == "BunnyAccessory";
    const bool npc = type.base == "BunnyNPC";
    if (!basic && !tile && !projectile && !weapon && !buff && !potion && !accessory && !npc) {
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
    spec->kind = tile ? NativeTile : basic ? NativeBasicItem : projectile ? NativeProjectile :
                 weapon ? NativeWeapon : buff ? NativeBuff :
                 accessory ? NativeAccessory : npc ? NativeNpc : NativeBuffPotion;
    *texture = tile ? "Content/Tiles/" + type.name + ".png" :
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
        if (tile) {
            *error = type.name + ": BunnyTile permite somente Texture e DisplayName no registro antecipado";
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
            if (value.empty() || value.size() > 180 ||
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

        if (buff && key == "MinionProjectile") continue;  // validado na linha V2 apos o Mono
        if (buff && (key == "Description" || key == "IsDebuff" ||
                     key == "NoSave" || key == "NoTimeDisplay")) {
            const bool text = key == "Description";
            const bool badText = text && (value.size() > 180 ||
                std::any_of(value.begin(), value.end(),
                    [](unsigned char c) { return c < 32 || c == 127; }));
            if ((!text && value != "true" && value != "false") || badText) {
                *error = type.name + ": valor invalido em " + key;
                return false;
            }
            continue;
        }
        if (weapon && key == "Shoot") { *shoot = value; continue; }
        if (potion && key == "Buff") { *shoot = value; continue; }
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
        if (!parseCSharpSource(contents, &defs, &status->error, false)) {
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
    if (!prepareNativeContent()) {
        status->error = "Host API de conteudo nativo indisponivel";
        return false;
    }
    NativeRegisterContext ctx{mod.id, mod.dir, 0, 0};
    const std::array<std::string, 9> order = {
        "BunnyTile", "BunnyItem",
        "BunnyBuff", "BunnyProjectile", "BunnyAccessory",
        "BunnyNPC", "BunnyBuffPotion", "BunnyWeapon", "BunnyPrefix"
    };
    for (const auto& base : order) {
        for (const auto& def : defs) {
            if (def.base != base) continue;
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
            std::string buffDescription;
            if (base == "BunnyBuff") {
                auto flag = [&](const char* key) {
                    const auto it = def.properties.find(key);
                    return it != def.properties.end() && it->second == "true";
                };
                spec.flags = (flag("IsDebuff") ? kNativeBuffDebuff : 0) |
                             (flag("NoSave") ? kNativeBuffNoSave : 0) |
                             (flag("NoTimeDisplay") ? kNativeBuffNoTimeDisplay : 0);
                const auto it = def.properties.find("Description");
                if (it != def.properties.end()) {
                    buffDescription = it->second;
                    const size_t at = buffDescription.find("{0}");
                    if (at != std::string::npos)
                        buffDescription.replace(at, 3, std::to_string(spec.damage));
                }
                spec.description = buffDescription.c_str();
            }
            spec.name = def.name.c_str();
            spec.texture = texture.c_str();
            spec.displayName = display.c_str();
            const std::string sourceKey = sourceIdentityForBase(def.base, def.name);
            ctx.sourceFile="content/"+sourcePaths[sourceKey];
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
        if (link == def.properties.end()) {
            if (!def.properties.empty()) {
                status->error = def.name + ": BunnyPlayer global deve ter SetDefaults vazio";
                return false;
            }
        } else {
            bool valid = false;
            for (const auto& other : defs)
                if (other.base == "BunnyAccessory" && other.name == link->second &&
                    typed.contains(sourceIdentityForBase("BunnyAccessory", link->second)) &&
                    typed[sourceIdentityForBase("BunnyAccessory", link->second)] >= 0)
                    valid = true;
            if (!valid || def.properties.size() != 1) {
                status->error = def.name + ": acessorio C# inexistente ou propriedades invalidas";
                return false;
            }
        }
        status->playerClasses.push_back(def.name);
    }
    for (const auto& name : status->playerClasses)
        typed.erase(sourceIdentityForBase("BunnyPlayer", name));
    status->players = static_cast<int>(status->playerClasses.size());
    status->accessories = ctx.accessories;
    status->registeredKindTypes = typed;
    for (const auto& [key, type] : typed) {
        const auto colon = key.find(':');
        if (colon != std::string::npos && type >= 0)
            status->registeredTypes.try_emplace(key.substr(colon + 1), type);
    }
    status->tiles = ctx.tiles;
    status->items = ctx.items;
    status->projectiles = ctx.projectiles;
    status->buffs = ctx.buffs;
    status->npcs = ctx.npcs;
    status->potions = ctx.potions;
    status->basicItems = ctx.basicItems;
    if (status->items+status->tiles+status->projectiles+status->buffs+status->npcs+status->prefixes+status->players==0) {
        status->error = "mod C# sem conteudo registrado";
        return false;
    }
    return true;
}

}

bool loadCSharpSourceMod(const bl::mods::LoadedMod& mod) {
    SourceModStatus status;
    status.id = mod.id;
    status.loaded = load(mod, &status);
    if (status.loaded) {
        BL_INFO("C# source %s: %d arquivo(s), %d item(ns), %d projetil(is), %d buff(s), %d pocao(oes), %d NPC(s)",
                mod.id.c_str(), status.sourceFiles, status.items,
                status.projectiles, status.buffs, status.potions, status.npcs);
    } else {
        BL_ERROR("C# source %s: %s", mod.id.c_str(), status.error.c_str());
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
    std::vector<ManagedProjectileEntry> projectiles;
    std::vector<ManagedWeaponEntry> weapons;
    std::vector<BasicItemEntry> basicItems;
    std::vector<ManagedBuffEntry> buffs;
    std::vector<ManagedPotionEntry> potions;
    std::vector<ManagedAccessoryEntry> accessories;
    std::vector<ManagedPlayerEntry> players;
    std::vector<ManagedNpcEntry> npcs;
    std::vector<ManagedPrefixEntry> prefixes;
    std::map<std::string, int> accessoryTypes;
    std::vector<std::string> shootNames;
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty()) continue;
        std::istringstream parts(line);
        std::vector<std::string> fields;
        std::string part;
        while (std::getline(parts, part, '\t')) fields.push_back(part);
        const std::string identity = fields.size() >= 2
            ? sourceIdentityForWire(fields[0], fields[1]) : "";
        if (identity.empty() || !seen.emplace(identity, true).second) {
            *error = "Classe C# duplicada ou registro invalido";
            return false;
        }
        if (fields[0] == "TILE" && fields.size() == 13) {
            const auto it = mod->registeredKindTypes.find(identity);
            if (it == mod->registeredKindTypes.end() ||
                runtime::modTileTypeByName(modId, fields[1]) != it->second) {
                *error = fields[1] + ": BunnyTile nao registrado"; return false;
            }
            ManagedTileEntry tile;
            tile.name = fields[1]; tile.type = it->second;
            if (!integer(fields[2], &tile.flags) ||
                !integer(fields[3], &tile.shape) ||
                !integer(fields[4], &tile.coordinateHeight) ||
                !integer(fields[5], &tile.adjacentTile) ||
                !integer(fields[7], &tile.mapR) ||
                !integer(fields[8], &tile.mapG) ||
                !integer(fields[9], &tile.mapB) ||
                !integer(fields[11], &tile.failDust) ||
                !integer(fields[12], &tile.normalDust) ||
                tile.flags < 0 || tile.flags > 511 ||
                tile.shape < 0 || tile.shape > 1 ||
                tile.coordinateHeight != 18 ||
                tile.adjacentTile < -1 || tile.adjacentTile >= 754 ||
                tile.mapR < 0 || tile.mapR > 255 ||
                tile.mapG < 0 || tile.mapG > 255 ||
                tile.mapB < 0 || tile.mapB > 255 ||
                tile.failDust < 0 || tile.failDust > 20 ||
                tile.normalDust < 0 || tile.normalDust > 20 ||
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
            tiles.push_back(std::move(tile));
            if (tiles.size() > kSourceMaxCategory) {
                *error = "Quantidade de BunnyTile C# excede 256"; return false;
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
                *error = "Quantidade de BunnyItem C# excede 256";
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
        if (fields[0] == "PROJECTILE" && fields.size() == 8) {
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
            projectilesByName.emplace(record.name, record.type);
            projectiles.push_back(std::move(record));
        } else if (fields[0] == "WEAPON" && fields.size() == 6) {
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
        } else if (fields[0] == "BUFF" && (fields.size() == 3 || fields.size() == 4)) {
            ManagedBuffEntry record;
            record.name = fields[1];
            record.type = registered->second;
            if (!integer(fields[2], &record.defenseBonus) ||
                record.defenseBonus < -50 || record.defenseBonus > 100 ||
                runtime::modBuffTypeByName(modId, record.name) != record.type) {
                *error = "Valores invalidos de buff C#: " + record.name;
                return false;
            }
            if (fields.size() == 4 && fields[3] != "-") {
                const auto minion = projectilesByName.find(fields[3]);
                if (minion == projectilesByName.end()) {
                    *error = "Projetil minion do buff C# inexistente: " + fields[3];
                    return false;
                }
                record.minionProjectileType = minion->second;
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
            npcs.size() > kSourceMaxCategory || prefixes.size() > kSourceMaxCategory) {
            *error = "Quantidade de conteudo C# excede 4096 por categoria";
            return false;
        }
    }
    if (tiles.size() != static_cast<size_t>(mod->tiles) ||
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
    }
    if (!applyManagedNativeBuffDefaultsBatch(modId, buffs, potions)) {
        *error = "Falha ao validar buffs e pocoes C# gerenciados";
        return false;
    }
    if ((!projectiles.empty() || !weapons.empty()) &&
        !applyManagedNativeDefaultsBatch(modId, projectiles, weapons)) {
        *error = "Falha ao validar ou aplicar lote SetDefaults ao Terraria";
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
    if (!tiles.empty() && !applyManagedNativeTileDefaultsBatch(modId, tiles)) {
        *error = "Falha ao aplicar os defaults nativos de BunnyTile"; return false;
    }
    if (!applyNativeBasicItemDefaults(modId,basicItems,error)) return false;
    for (const auto& item : basicItems) {
        if (item.defense>0 && !setBasicItemDefense(item.type,item.defense,error)) return false;
    }
    mod->runtimeApplied = true;
    return true;
}

std::vector<SourceModStatus> sourceModStatuses() { return g_statuses; }

}


