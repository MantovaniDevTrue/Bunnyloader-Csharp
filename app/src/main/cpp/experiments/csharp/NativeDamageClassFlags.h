#pragma once

#include <string_view>

namespace bl::csharp {

struct NativeDamageClassFlags {
    bool resolved = false;
    bool melee = false;
    bool ranged = false;
    bool magic = false;
    bool summon = false;
};

inline NativeDamageClassFlags nativeDamageClassFlags(std::string_view key) {
    if (key == "V2" || key == "V3") return {true, true, false, false, false};
    if (key == "V4") return {true, false, true, false, false};
    if (key == "V5") return {true, false, false, true, false};
    if (key == "V6" || key == "V7") return {true, false, false, false, true};
    if (key == "V1" || key.starts_with("C:")) return {true, false, false, false, false};
    return {};
}

}
