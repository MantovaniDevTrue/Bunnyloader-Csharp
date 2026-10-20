#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace bl::csharp {
class ManagedAssemblyBindings {
public:
    void bind(const std::string& mod, const std::string& path) {
        if (mod.empty() || path.empty()) return;
        std::lock_guard lock(mutex_);
        uint64_t& revision = revisions_[mod];
        ++revision;
        if (revision == 0) revision = 1;
        active_[mod] = path + "|bunny-revision=" + std::to_string(revision);
    }

    void unbind(const std::string& mod) {
        std::lock_guard lock(mutex_);
        active_.erase(mod);
    }

    bool unbindIfCurrent(const std::string& mod, const std::string& captured) {
        if (captured.empty()) return false;
        std::lock_guard lock(mutex_);
        const auto it = active_.find(mod);
        if (it == active_.end() || it->second != captured) return false;
        active_.erase(it);
        return true;
    }

    std::string current(const std::string& mod) const {
        std::lock_guard lock(mutex_);
        const auto it = active_.find(mod);
        return it == active_.end() ? std::string{} : it->second;
    }

    bool isCurrent(const std::string& mod, const std::string& captured) const {
        if (captured.empty()) return false;
        std::lock_guard lock(mutex_);
        const auto it = active_.find(mod);
        return it != active_.end() && it->second == captured;
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::string> active_;
    std::map<std::string, uint64_t> revisions_;
};
}
