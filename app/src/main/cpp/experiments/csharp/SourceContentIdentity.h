#pragma once
#include <string>
#include <string_view>

namespace bl::csharp {

inline std::string_view sourceContentDomain(std::string_view base) {
    if (base == "BunnyItem" || base == "BunnyWeapon" ||
        base == "BunnyAccessory" || base == "BunnyBuffPotion") return "ITEM";
    if (base == "BunnyProjectile") return "PROJECTILE";
    if (base == "BunnyBuff") return "BUFF";
    if (base == "BunnyNPC") return "NPC";
    if (base == "BunnyPrefix") return "PREFIX";
    if (base == "BunnyRarity") return "RARITY";
    if (base == "BunnyPlayer") return "PLAYER";
    if (base == "BunnyTile") return "TILE";
    if (base == "BunnyWall") return "WALL";
    if (base == "BunnyDust") return "DUST";
    if (base == "BunnyTree") return "TREE";
    if (base == "BunnyGore") return "GORE";
    if (base == "BunnyScreenEffect") return "SCREEN_EFFECT";
    if (base == "BunnyCommand") return "COMMAND";
    if (base == "BunnyWorldSystem") return "SYSTEM";
    return {};
}


inline bool sourceRequiresNativeRegistration(std::string_view base) {
    return base != "BunnyPlayer" && base != "BunnyScreenEffect" &&
           base != "BunnyCommand" && base != "BunnyWorldSystem" && base != "BunnyTree" && base != "BunnyGore";
}

inline std::string_view sourceWireDomain(std::string_view kind) {
    if (kind == "WEAPON" || kind == "ACCESSORY" || kind == "POTION") return "ITEM";
    if (kind == "ITEM" || kind == "PROJECTILE" || kind == "BUFF" ||
        kind == "NPC" || kind == "PREFIX" || kind == "RARITY" || kind == "PLAYER" ||
        kind == "TILE" || kind == "WALL" || kind == "DUST" || kind == "TREE" || kind == "GORE") return kind;
    return {};
}

inline std::string sourceContentKey(std::string_view domain, std::string_view name) {
    if (domain.empty() || name.empty()) return {};
    std::string key(domain);
    key += ':';
    key += name;
    return key;
}

inline std::string sourceIdentityForBase(std::string_view base, std::string_view name) {
    return sourceContentKey(sourceContentDomain(base), name);
}

inline std::string sourceIdentityForWire(std::string_view kind, std::string_view name) {
    return sourceContentKey(sourceWireDomain(kind), name);
}

}
