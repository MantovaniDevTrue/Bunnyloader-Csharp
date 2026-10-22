#pragma once
#include "content/backgrounds/BackgroundTextures.h"
#include "experiments/csharp/SourceContentIdentity.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace bl::csharp {
inline bool validateBackgroundTextureWire(const std::vector<std::string>& fields,
    const std::string& mod, const std::map<std::string,int>& registered,
    std::map<std::string,bool>* seen, std::set<std::string>* backgroundRows, std::string* error) {
    const std::string name = fields.size() > 1 ? fields[1] : "<missing>";
    const auto key = sourceIdentityForWire("BACKGROUND_TEXTURE", name);
    const auto type = registered.find(key);
    const int found = fields.size() == 3 ? runtime::backgrounds::find(mod, fields[2], false) : -1;
    if (fields.size() != 3 || type == registered.end() || found < 0 || found != type->second ||
        seen->contains(key) || backgroundRows->contains(name) || backgroundRows->size() >= 4096) {
        *error = "[CSharp][ERROR][BackgroundTexture] Mod=" + mod + " Class=" + name +
            " Stage=ValidateManagedDefaults Expected=UniqueReservedPathMatchingNativeSlot Actual=" + std::to_string(found) +
            " Result=Rejected Reason=DescriptorIdentityOrSlotMismatch";
        return false;
    }
    // This is a content row, not auxiliary metadata. Account for it in the
    // same strict identity set used by the final native/managed class gate.
    seen->emplace(key, true);
    backgroundRows->insert(name);
    return true;
}
}
