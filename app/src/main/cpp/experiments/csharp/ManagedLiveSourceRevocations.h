#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <string>

namespace bl::csharp {

class ManagedLiveSourceRevocations {
public:
    bool revoked(const std::string& modId) const {
        return blocked_.contains(modId);
    }

    template <typename Value>
    size_t revoke(const std::string& modId, std::map<std::string, Value>& cached) {
        if (modId.empty()) return 0;
        blocked_.insert(modId);
        size_t removed = 0;
        const std::string prefix = modId + ":";
        for (auto it = cached.begin(); it != cached.end();) {
            if (it->first == modId || it->first.starts_with(prefix)) {
                it = cached.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

private:
    std::set<std::string> blocked_;
};

}
