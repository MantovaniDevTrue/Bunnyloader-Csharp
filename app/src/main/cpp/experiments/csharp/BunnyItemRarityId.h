#pragma once
#include <array>
#include <string_view>
#include <utility>

namespace bl::csharp {

inline constexpr std::array<std::pair<std::string_view, int>, 16> kBunnyItemRarityIds{{
    {"Master", -13}, {"Expert", -12}, {"Quest", -11}, {"Gray", -1},
    {"White", 0}, {"Blue", 1}, {"Green", 2}, {"Orange", 3},
    {"LightRed", 4}, {"Pink", 5}, {"LightPurple", 6}, {"Lime", 7},
    {"Yellow", 8}, {"Cyan", 9}, {"Red", 10}, {"Purple", 11}
}};

inline constexpr bool resolveBunnyItemRarity(std::string_view name, int* result) {
    if (!result) return false;
    for (const auto& entry : kBunnyItemRarityIds) {
        if (entry.first == name) {
            *result = entry.second;
            return true;
        }
    }
    return false;
}

}
