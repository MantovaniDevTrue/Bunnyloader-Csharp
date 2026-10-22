#pragma once

#include <string_view>

namespace bl::csharp {

inline bool validAchievementModUid(std::string_view uid) {
    if (uid.empty() || uid.size() > 96) return false;
    for (unsigned char ch : uid)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'))
            return false;
    return true;
}

inline bool achievementOwnedByMod(std::string_view identity,
                                  std::string_view modUid) {
    return validAchievementModUid(modUid) &&
           identity.size() > modUid.size() + 1 &&
           identity.substr(0, modUid.size()) == modUid &&
           identity[modUid.size()] == '/';
}

}
