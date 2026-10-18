#pragma once
#include <string>

namespace bl::csharp {

struct NativeTileGameplaySpec {
    std::string mod;
    std::string name;
    std::string dustName;
    int type = -1;
    int shape = 0;
    int failDust = 1;
    int normalDust = 3;
};

bool registerNativeTileGameplay(const NativeTileGameplaySpec& spec);

}
