#pragma once

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace bl::csharp {

struct NativeDamageTypeBinding {
    std::string mod;
    int contentType = -1;
    bool projectile = false;
    std::string damageClass;
};

class NativeDamageTypeRegistry {
    using Key = std::pair<bool, int>;
    using Bindings = std::map<Key, NativeDamageTypeBinding>;
    std::mutex mutex_;
    std::shared_ptr<const Bindings> active_ = std::make_shared<const Bindings>();

public:
    bool publish(const std::string& mod, const std::vector<NativeDamageTypeBinding>& staged) {
        if (mod.empty() || staged.size() > 8192) return false;
        std::lock_guard guard(mutex_);
        auto next = std::make_shared<Bindings>(
            *std::atomic_load_explicit(&active_, std::memory_order_acquire));
        for (const auto& entry : staged) {
            if (entry.mod != mod || entry.contentType < 0 ||
                entry.contentType >= 32768 || entry.damageClass.empty() ||
                !next->emplace(Key{entry.projectile, entry.contentType}, entry).second)
                return false;
        }
        std::atomic_store_explicit(&active_, std::shared_ptr<const Bindings>(std::move(next)),
                                   std::memory_order_release);
        return true;
    }

    bool lookup(bool projectile, int type, NativeDamageTypeBinding* out) const {
        if (!out || type < 0) return false;
        auto snapshot = std::atomic_load_explicit(&active_, std::memory_order_acquire);
        auto it = snapshot->find(Key{projectile, type});
        if (it == snapshot->end()) return false;
        *out = it->second;
        return true;
    }

    void unload(const std::string& mod) {
        std::lock_guard guard(mutex_);
        auto next = std::make_shared<Bindings>(
            *std::atomic_load_explicit(&active_, std::memory_order_acquire));
        for (auto it = next->begin(); it != next->end();) {
            if (it->second.mod == mod) it = next->erase(it);
            else ++it;
        }
        std::atomic_store_explicit(&active_, std::shared_ptr<const Bindings>(std::move(next)),
                                   std::memory_order_release);
    }

    void reset() {
        std::lock_guard guard(mutex_);
        std::atomic_store_explicit(&active_, std::make_shared<const Bindings>(),
                                   std::memory_order_release);
    }
};

inline NativeDamageTypeRegistry& nativeDamageTypes() {
    static NativeDamageTypeRegistry registry;
    return registry;
}

}
