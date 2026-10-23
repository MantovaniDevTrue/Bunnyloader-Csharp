#pragma once
#include <array>
#include <string_view>

namespace bl::csharp {

inline constexpr std::array<const char*, 10> kSourceContentKinds = {{
    "Item",
    "Tile",
    "Weapon",
    "Projectile",
    "Buff",
    "BuffPotion",
    "Accessory",
    "Player",
    "NPC",
    "Prefix",
}};

inline constexpr std::array<const char*, 10> kSourceContentBases = {{
    "BunnyItem",
    "BunnyTile",
    "BunnyWeapon",
    "BunnyProjectile",
    "BunnyBuff",
    "BunnyBuffPotion",
    "BunnyAccessory",
    "BunnyPlayer",
    "BunnyNPC",
    "BunnyPrefix",
}};

inline constexpr std::array<const char*, 9> kSourceNativeRegistrationOrder = {{
    "BunnyTile",
    "BunnyItem",
    "BunnyBuff",
    "BunnyProjectile",
    "BunnyAccessory",
    "BunnyNPC",
    "BunnyBuffPotion",
    "BunnyWeapon",
    "BunnyPrefix",
}};

inline bool isKnownSourceKind(std::string_view kind) {
    for (const char* value : kSourceContentKinds)
        if (kind == value) return true;
    return false;
}

inline bool isRecognizedSourceBase(std::string_view base) {
    for (const char* value : kSourceContentBases)
        if (base == value) return true;
    return false;
}

}
