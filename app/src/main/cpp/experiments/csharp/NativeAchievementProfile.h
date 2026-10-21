#pragma once

#include "experiments/csharp/PortableSha256.h"

#include <string>
#include <string_view>

namespace bl::csharp {

inline std::string achievementProfileKey(std::string_view path, bool cloud) {
    if (path.empty() || path.size() > 2048) return {};
    PortableSha256 hash;
    hash.field("BUNNY_CSHARP_ACHIEVEMENT_PROFILE_V1");
    hash.field(cloud ? "cloud" : "local");
    hash.field(path);
    return hash.finish();
}

// Apenas na thread do jogo: Main.get_ActivePlayerFileData(),
// FileData._path e FileData._isCloudSave. Nenhum objeto IL2CPP sai da thread.
bool activeAchievementProfile(std::string* profileKey, std::string* error);

}
