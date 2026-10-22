#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace bl::csharp::banners {

struct NearbyBanner {
    int x = 0, y = 0, tileType = -1;
    int style = -1, banner = -1;
    uint64_t lastSeen = 0;
};

class NearbyBannerCache final {
public:
    static constexpr size_t kMaximumEntries = 512;
    static constexpr uint64_t kExpirationTicks = 120;

    void reset() {
        entries_.clear();
        epoch_ = 0;
    }

    void beginWorld(uint64_t epoch) {
        if (epoch_ == epoch) return;
        entries_.clear();
        epoch_ = epoch;
    }

    bool observe(uint64_t world, uint64_t tick, int x, int y,
                 int tileType, int style, int banner) {
        if (!world || world != epoch_ || x < 0 || y < 0 ||
            x > 100000 || y > 100000 ||
            tileType < 754 || style < 0 || style > 1024 ||
            banner < 697 || banner >= 262144) return false;
        const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
                             static_cast<uint32_t>(y);
        if (!entries_.contains(key) && entries_.size() >= kMaximumEntries) {
            prune(tick);
            if (entries_.size() >= kMaximumEntries) entries_.erase(entries_.begin());
        }
        entries_[key] = {x, y, tileType, style, banner, tick};
        return true;
    }

    void invalidate(int x, int y) {
        const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
                             static_cast<uint32_t>(y);
        entries_.erase(key);
    }

    void prune(uint64_t tick) {
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second.lastSeen > tick ||
                tick - it->second.lastSeen > kExpirationTicks) it = entries_.erase(it);
            else ++it;
        }
    }

    std::vector<NearbyBanner> near(uint64_t world, uint64_t tick, int left,
                                   int top, int width, int height) const {
        std::vector<NearbyBanner> result;
        if (!world || epoch_ != world || left < 0 || top < 0 ||
            width <= 0 || height <= 0 || width > 256 || height > 256)
            return result;
        for (const auto& [key, entry] : entries_) {
            if (entry.lastSeen > tick ||
                tick - entry.lastSeen > kExpirationTicks ||
                entry.x < left || entry.y < top ||
                static_cast<int64_t>(entry.x) >= static_cast<int64_t>(left) + width ||
                static_cast<int64_t>(entry.y) >= static_cast<int64_t>(top) + height)
                continue;
            result.push_back(entry);
        }
        return result;
    }

    size_t size() const { return entries_.size(); }

private:
    uint64_t epoch_ = 0;
    std::map<uint64_t, NearbyBanner> entries_;
};

inline NearbyBannerCache& nearbyBannerCache() {
    static NearbyBannerCache cache;
    return cache;
}

} // namespace bl::csharp::banners
