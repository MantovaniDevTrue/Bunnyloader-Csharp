#include "experiments/csharp/SourceLoader.h"

#include "experiments/csharp/NativeContent.h"
#include "experiments/csharp/SourceParser.h"
#include "core/Log.h"
#include "mods/ModLoader.h"

#include <algorithm>
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
    if (depth > 7) { *error = "pastas C# excedem o limite"; return false; }
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
            if (st.st_size > 256 * 1024 || st.st_size < 0 || paths->size() >= 32) {
                *error = "limite de arquivos/fontes C# excedido";
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
    out->assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return out->size() <= 256 * 1024;
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
    spec->width = projectile ? 8 : 0;
    spec->height = projectile ? 8 : 0;
    spec->aiStyle = projectile ? 1 : 0;
    spec->penetrate = projectile ? 1 : 0;
    spec->timeLeft = projectile ? 360 : 0;
    spec->damage = projectile ? 0 : 20;
    spec->useTime = projectile ? 0 : 18;
    spec->shootSpeed = projectile ? 0.f : 12.f;
    spec->kind = projectile ? NativeProjectile : NativeWeapon;
    *texture = projectile ? "Content/Projectiles/" + type.name + ".png" :
                            "Content/Items/Weapons/" + type.name + ".png";
    *display = type.name;
    for (const auto& [key, value] : type.properties) {
        if (key == "DisplayName") { *display = value; continue; }
        if (key == "Texture") { *texture = value; continue; }
        if (!projectile && key == "Shoot") { *shoot = value; continue; }
        if (key == "Friendly" && projectile) {
            if (value != "true") { *error = "somente projeteis Friendly=true nesta etapa"; return false; }
            continue;
        }
        int* integerField = nullptr;
        if (projectile && key == "Width") integerField = &spec->width;
        else if (projectile && key == "Height") integerField = &spec->height;
        else if (projectile && key == "AiStyle") integerField = &spec->aiStyle;
        else if (projectile && key == "Penetrate") integerField = &spec->penetrate;
        else if (projectile && key == "TimeLeft") integerField = &spec->timeLeft;
        else if (!projectile && key == "Damage") integerField = &spec->damage;
        else if (!projectile && key == "UseTime") integerField = &spec->useTime;
        if (integerField) {
            if (!integer(value, integerField)) {
                *error = type.name + ": inteiro invalido em " + key;
                return false;
            }
            continue;
        }
        if (!projectile && key == "ShootSpeed") {
            if (!real(value, &spec->shootSpeed)) {
                *error = type.name + ": ShootSpeed invalido";
                return false;
            }
            continue;
        }
        *error = type.name + ": propriedade C# nao suportada: " + key;
        return false;
    }
    if (!projectile && shoot->empty()) {
        *error = type.name + ": Shoot = nameof(Projetil) obrigatorio";
        return false;
    }
    return true;
}

bool load(const bl::mods::LoadedMod& mod, SourceModStatus* status) {
    const std::string folder = mod.dir + "/content";
    std::vector<std::string> paths;
    if (!collect(folder, "", &paths, &status->error, 0)) return false;
    if (paths.empty() || paths.size() > 32) {
        status->error = "pacote C# sem fontes .cs";
        return false;
    }
    std::vector<ParsedSourceType> defs;
    for (const auto& path : paths) {
        std::string contents;
        if (!read(folder + "/" + path, &contents)) {
            status->error = "nao foi possivel ler " + path;
            return false;
        }
        if (!parseCSharpSource(contents, &defs, &status->error, false)) {
            status->error = path + ": " + status->error;
            return false;
        }
        ++status->sourceFiles;
    }
    if (defs.empty()) {
        status->error = "mod C# sem nenhuma classe BunnyWeapon ou BunnyProjectile";
        return false;
    }
    if (defs.size() > 128) { status->error = "tipos C# demais"; return false; }
    std::map<std::string, int> types;
    for (const auto& def : defs) {
        if (!types.emplace(def.name, -1).second) {
            status->error = "tipo C# duplicado: " + def.name;
            return false;
        }
    }
    if (!prepareNativeContent()) {
        status->error = "Host API de conteudo nativo indisponivel";
        return false;
    }
    NativeRegisterContext ctx{mod.id, mod.dir, 0, 0};
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto& def : defs) {
            if ((def.base == "BunnyProjectile") != (pass == 0)) continue;
            NativeContentSpec spec{};
            std::string shoot, display, texture;
            if (!fields(def, &spec, &shoot, &display, &texture, &status->error)) return false;
            if (pass == 1) {
                auto it = types.find(shoot);
                if (it == types.end() || it->second < 0) {
                    status->error = def.name + ": projetil nao encontrado: " + shoot;
                    return false;
                }
                spec.shootType = it->second;
            }
            spec.name = def.name.c_str();
            spec.texture = texture.c_str();
            spec.displayName = display.c_str();
            const int type = registerNativeContent(&ctx, &spec);
            if (type < 0) {
                status->error = def.name + ": registro nativo falhou; confira PNG e propriedades";
                return false;
            }
            types[def.name] = type;
        }
    }
    status->registeredTypes = types;
    status->items = ctx.items;
    status->projectiles = ctx.projectiles;
    if (status->items < 1 || status->projectiles < 1) {
        status->error = "mod C# precisa registrar um BunnyWeapon e BunnyProjectile";
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
        BL_INFO("C# source %s: %d arquivo(s), %d item(ns), %d projetil(is)",
                mod.id.c_str(), status.sourceFiles, status.items, status.projectiles);
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
    if (runtimeValues.empty() || runtimeValues.size() > 2048) {
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
    if (!mod || mod->runtimeApplied || mod->registeredTypes.size() != 2) {
        *error = "Mod fonte nao carregado, ja aplicado ou com tipos incompatíveis";
        return false;
    }
    std::istringstream lines(runtimeValues);
    std::string header;
    if (!std::getline(lines, header) || header != "BUNNY_MANAGED_DEFAULTS_V1") {
        *error = "Formato do resultado gerenciado nao suportado";
        return false;
    }
    ManagedProjectileDefaults projectile;
    ManagedWeaponDefaults weapon;
    std::string projectileName, weaponName, shootName;
    bool seenProjectile = false;
    bool seenWeapon = false;
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty()) continue;
        std::istringstream parts(line);
        std::vector<std::string> fields;
        std::string part;
        while (std::getline(parts, part, '\t')) fields.push_back(part);
        if (fields.size() == 8 && fields[0] == "PROJECTILE" && !seenProjectile) {
            int friendly = 0;
            if (!integer(fields[2], &projectile.width) ||
                !integer(fields[3], &projectile.height) ||
                !integer(fields[4], &projectile.aiStyle) ||
                !integer(fields[5], &projectile.penetrate) ||
                !integer(fields[6], &projectile.timeLeft) ||
                !integer(fields[7], &friendly) || friendly != 1) {
                *error = "Valores gerenciados invalidos para projetil";
                return false;
            }
            projectileName = fields[1];
            projectile.friendly = true;
            seenProjectile = true;
        } else if (fields.size() == 6 && fields[0] == "WEAPON" && !seenWeapon) {
            if (!integer(fields[2], &weapon.damage) ||
                !integer(fields[3], &weapon.useTime) ||
                !real(fields[4], &weapon.shootSpeed)) {
                *error = "Valores gerenciados invalidos para arma";
                return false;
            }
            weaponName = fields[1];
            shootName = fields[5];
            seenWeapon = true;
        } else {
            *error = "Linha inesperada no resultado do C#";
            return false;
        }
    }
    if (!seenWeapon || !seenProjectile || projectileName != shootName) {
        *error = "Arma ou projetil ausente, referencia Shoot invalida";
        return false;
    }
    const auto projectileType = mod->registeredTypes.find(projectileName);
    const auto weaponType = mod->registeredTypes.find(weaponName);
    if (projectileType == mod->registeredTypes.end() ||
        weaponType == mod->registeredTypes.end() ||
        projectileType->second == weaponType->second ||
        !applyManagedNativeDefaults(modId, projectileName, projectileType->second,
                                    projectile, weaponName, weaponType->second, weapon)) {
        *error = "IDs do Terraria ou dados do SetDefaults nao correspondem ao mod";
        return false;
    }
    mod->runtimeApplied = true;
    return true;
}

std::vector<SourceModStatus> sourceModStatuses() { return g_statuses; }

}
