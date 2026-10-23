#pragma once

#include "experiments/csharp/NativeAchievementManifest.h"
#include "il2cpp/Types.h"

#include <map>
#include <string>
#include <vector>

namespace bl::csharp::achievement {

class NativeAchievementAtlases {
    struct Atlas {
        uint32_t root = 0;
    };
    struct Owner {
        std::string revision;
        std::map<std::string, Atlas> textures;
    };
    std::map<std::string, Owner> owners_;

public:
    bool prepare(const std::string& mod, const std::string& revision,
                 const std::vector<Descriptor>& achievements, std::string* error);
    Il2CppObject* asset(const std::string& mod, const std::string& texture) const;
    void retire(const std::string& mod);
    void reset();
};

NativeAchievementAtlases& nativeAchievementAtlases();

}
