#pragma once

#include "experiments/csharp/NativeAchievementManifest.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp::achievement {

struct AtlasEntry {
    std::string owner;
    std::string texture;
    std::string filename;
};

inline bool achievementTexturePath(std::string_view texture) {
    if (texture.size() < 9 || texture.size() > 200 ||
        !texture.starts_with("Content/") || texture.find("..") != std::string_view::npos ||
        texture.find('\\') != std::string_view::npos ||
        texture.find("//") != std::string_view::npos ||
        texture.back() == '/' || texture.find(':') != std::string_view::npos)
        return false;
    for (unsigned char c : texture)
        if (c < 33 || c > 126) return false;
    return true;
}

inline bool planAchievementAtlases(std::string_view owner,
                                   const std::vector<Descriptor>& descriptors,
                                   std::vector<AtlasEntry>* entries) {
    if (!entries || owner.empty() || owner.size() > 96 ||
        descriptors.empty() || descriptors.size() > 128) return false;
    std::vector<AtlasEntry> staged;
    for (const Descriptor& definition : descriptors) {
        if (definition.name.substr(0, owner.size() + 1) != std::string(owner) + "/" ||
            !achievementTexturePath(definition.texture) ||
            definition.index < 0 || definition.index > 7) return false;
        if (std::any_of(staged.begin(), staged.end(), [&](const AtlasEntry& item) {
                return item.texture == definition.texture;
            })) continue;
        if (staged.size() >= 32) return false;
        staged.push_back({std::string(owner), definition.texture,
                          "content/" + definition.texture + ".png"});
    }
    *entries = std::move(staged);
    return true;
}

}
