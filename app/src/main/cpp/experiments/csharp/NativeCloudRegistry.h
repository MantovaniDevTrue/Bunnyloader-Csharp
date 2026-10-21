#pragma once

#include "experiments/csharp/NativeCloudSelection.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace bl::csharp {

// One-session registry for native C# clouds. No QuickJS dependency and no
// Terraria calls: the caller owns game-thread-only asset/hook installation.
class NativeCloudRegistry {
public:
    struct Entry {
        int type;
        std::string modId;
        std::string name;
        std::string texture;
        bool rare;
        bool ready = false;
        int width = 0;
        int height = 0;
    };

    // Must read Terraria.ID.CloudID.Count at runtime. Never hardcode cloud IDs
    // or reuse IDs while a previous world still draws them.
    bool start(int firstModType) {
        if (firstModType <= 0 || firstModType > 10000) return false;
        firstType_ = firstModType;
        entries_.clear();
        return true;
    }

    // Names are unique inside a mod, not globally. Type allocation is stable
    // in registration order for the lifetime of this one source-mod session.
    int add(std::string_view modId, std::string_view name,
            std::string_view texture, bool rare) {
        if (firstType_ <= 0 ||
            entries_.size() >= NativeCloudWeightedPool::kMaxModClouds ||
            !validName(modId) || !validName(name) || !validTexture(texture))
            return -1;
        for (const auto& entry : entries_)
            if (entry.modId == modId && entry.name == name) return -1;
        const int type = firstType_ + static_cast<int>(entries_.size());
        entries_.push_back({type, std::string(modId), std::string(name),
                            std::string(texture), rare});
        return type;
    }

    // Publish only after TextureAssets.Cloud AND the Android CloudMasks arrays
    // have been grown and each entry is non-null. Failed rechecks disable spawn.
    bool setInstalled(int type, int textureArrayLength, int maskArrayLength,
                      bool hasTexture, bool hasMask, int width, int height,
                      int maskOutputWidth) {
        Entry* entry = mutableEntry(type);
        if (!entry) return false;
        entry->ready = NativeCloudRenderGate::safe(type, firstType_,
            textureArrayLength, maskArrayLength, hasTexture, hasMask,
            width, height, maskOutputWidth);
        entry->width = entry->ready ? width : 0;
        entry->height = entry->ready ? height : 0;
        return entry->ready;
    }

    const Entry* find(int type) const {
        if (type < firstType_ || firstType_ <= 0) return nullptr;
        const std::size_t index = static_cast<std::size_t>(type - firstType_);
        return index < entries_.size() ? &entries_[index] : nullptr;
    }

    bool addWeight(NativeCloudWeightedPool& pool, int type,
                   bool wantRare, double spawnChance) const {
        const Entry* entry = find(type);
        return entry && entry->rare == wantRare &&
               pool.add(type, firstType_, spawnChance, entry->ready);
    }

    void clear() { entries_.clear(); firstType_ = 0; }
    int firstType() const { return firstType_; }
    std::size_t count() const { return entries_.size(); }

private:
    Entry* mutableEntry(int type) {
        return const_cast<Entry*>(static_cast<const NativeCloudRegistry*>(this)->find(type));
    }
    static bool validName(std::string_view value) {
        if (value.empty() || value.size() > 72) return false;
        for (unsigned char c : value)
            if (!(std::isalnum(c) || c == '_')) return false;
        return true;
    }
    static bool validTexture(std::string_view value) {
        if (value.empty() || value.size() > 240 || value.front() == '/' ||
            value.find("..") != std::string_view::npos ||
            value.find('\\') != std::string_view::npos ||
            value.find(':') != std::string_view::npos) return false;
        for (unsigned char c : value)
            if (!(std::isalnum(c) || c == '/' || c == '_' || c == '-')) return false;
        return true;
    }

    std::vector<Entry> entries_;
    int firstType_ = 0;
};

} // namespace bl::csharp
