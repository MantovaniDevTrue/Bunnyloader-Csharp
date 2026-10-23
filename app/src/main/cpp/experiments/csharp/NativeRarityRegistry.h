#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bl::csharp {

struct NativeRarityIdentity {
    int type = -1;
    std::string mod;
    std::string name;
    bool managed = true;
    bool active = true; // IDs remain reserved for stable saves after unload.
};

class NativeRarityRegistry {
public:
    static constexpr int kVanillaCount = 12;
    static constexpr int kMaximumCustom = 4096;

    NativeRarityRegistry() : published_(std::make_shared<const std::vector<NativeRarityIdentity>>()) {}

    int registerType(std::string_view mod, std::string_view name) {
        if (!validMod(mod) || !validName(name)) return -1;
        std::string key(mod);
        key.push_back('/');
        key.append(name);
        std::lock_guard<std::mutex> guard(mutex_);
        const auto current = std::atomic_load_explicit(&published_, std::memory_order_acquire);
        if (const auto old=keys_.find(key);old!=keys_.end()) {
            const size_t slot=static_cast<size_t>(old->second-kVanillaCount);
            if((*current)[slot].active)return -1;
            auto next=std::make_shared<std::vector<NativeRarityIdentity>>(*current);
            (*next)[slot].active=true;
            std::shared_ptr<const std::vector<NativeRarityIdentity>> immutable=next;
            std::atomic_store_explicit(&published_,std::move(immutable),std::memory_order_release);
            return old->second;
        }
        if (current->size() >= kMaximumCustom) return -1;
        const int type = kVanillaCount + static_cast<int>(current->size());
        auto next = std::make_shared<std::vector<NativeRarityIdentity>>(*current);
        next->push_back({type, std::string(mod), std::string(name)});
        keys_.emplace(std::move(key), type);
        std::shared_ptr<const std::vector<NativeRarityIdentity>> immutable = next;
        std::atomic_store_explicit(&published_, std::move(immutable), std::memory_order_release);
        return type;
    }

    // Revokes lookup/callback eligibility without reassigning a saved numeric ID.
    void retireMod(std::string_view mod) {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto current=snapshot();
        auto next=std::make_shared<std::vector<NativeRarityIdentity>>(*current);
        bool changed=false;
        for(auto& entry:*next)if(entry.managed&&entry.mod==mod&&entry.active) {
            entry.active=false;changed=true;
        }
        if(changed) {
            std::shared_ptr<const std::vector<NativeRarityIdentity>> immutable=next;
            std::atomic_store_explicit(&published_,std::move(immutable),std::memory_order_release);
        }
    }

    int reserveExternalType() {
        std::lock_guard<std::mutex> guard(mutex_);
        const auto current = std::atomic_load_explicit(&published_, std::memory_order_acquire);
        if (current->size() >= kMaximumCustom) return -1;
        const int type = kVanillaCount + static_cast<int>(current->size());
        auto next = std::make_shared<std::vector<NativeRarityIdentity>>(*current);
        next->push_back({type, "", "", false});
        std::shared_ptr<const std::vector<NativeRarityIdentity>> immutable = next;
        std::atomic_store_explicit(&published_, std::move(immutable), std::memory_order_release);
        return type;
    }

    int findType(std::string_view mod, std::string_view name) const {
        if (!validMod(mod) || !validName(name)) return -1;
        std::string key(mod);
        key.push_back('/');
        key.append(name);
        std::lock_guard<std::mutex> guard(mutex_);
        const auto it = keys_.find(key);
        if(it==keys_.end())return -1;
        const auto current=std::atomic_load_explicit(&published_,std::memory_order_acquire);
        return (*current)[static_cast<size_t>(it->second-kVanillaCount)].active?it->second:-1;
    }

    NativeRarityIdentity identityForType(int type) const {
        const auto current = snapshot();
        const int index = type - kVanillaCount;
        if (index < 0 || static_cast<size_t>(index) >= current->size()) return {};
        return (*current)[static_cast<size_t>(index)];
    }

    std::string stableKeyForType(int type) const {
        const auto identity = identityForType(type);
        return identity.managed && !identity.mod.empty()
            ? identity.mod + "/" + identity.name : std::string();
    }

    int typeForStableKey(std::string_view stableKey) const {
        const size_t slash = stableKey.find('/');
        if (slash == std::string_view::npos ||
            stableKey.find('/', slash + 1) != std::string_view::npos) return -1;
        return findType(stableKey.substr(0, slash), stableKey.substr(slash + 1));
    }

    int nextType() const {
        const auto current = std::atomic_load_explicit(&published_, std::memory_order_acquire);
        return kVanillaCount + static_cast<int>(current->size());
    }

    std::shared_ptr<const std::vector<NativeRarityIdentity>> snapshot() const {
        return std::atomic_load_explicit(&published_, std::memory_order_acquire);
    }

private:
    static bool validMod(std::string_view value) {
        if (value.empty() || value.size() > 96) return false;
        for (unsigned char c : value)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
        return true;
    }

    static bool validName(std::string_view value) {
        if (value.empty() || value.size() > 72) return false;
        for (unsigned char c : value)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_')) return false;
        return true;
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, int> keys_;
    std::shared_ptr<const std::vector<NativeRarityIdentity>> published_;
};

inline NativeRarityRegistry& nativeRarities() {
    static NativeRarityRegistry registry;
    return registry;
}

}
