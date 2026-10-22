#pragma once

#include "experiments/csharp/NativeCloudRegistry.h"
#include "experiments/csharp/NativeCloudSelection.h"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace bl::csharp {

// MonoVM submits immutable weights in batches instead of being invoked from
// Cloud.addCloud or from the render loop. Only the Terraria game thread reads
// these pools. The host owns synchronization before calling publish().
struct NativeCloudWeight {
    int type = -1;
    double chance = 0;
};

class NativeCloudRollCoordinator {
public:
    bool publish(const NativeCloudRegistry& registry,
                 const std::vector<NativeCloudWeight>& weights) {
        if (registry.firstType() <= 0 ||
            registry.count() > NativeCloudWeightedPool::kMaxModClouds ||
            weights.size() > NativeCloudWeightedPool::kMaxModClouds)
            return false;

        NativeCloudWeightedPool common, rare, anniversary;
        if (!common.reset(22) || !rare.reset(18) || !anniversary.reset(15))
            return false;
        for (const auto& entry : weights) {
            const auto* record = registry.find(entry.type);
            if (!record || !std::isfinite(entry.chance) || entry.chance < 0)
                return false;
            if (entry.chance == 0) continue;
            if (!record->ready) return false;
            if (record->rare) {
                if (!registry.addWeight(rare, entry.type, true, entry.chance) ||
                    !registry.addWeight(anniversary, entry.type, true, entry.chance))
                    return false;
            } else if (!registry.addWeight(common, entry.type, false, entry.chance)) {
                return false;
            }
        }

        common_ = std::move(common);
        rare_ = std::move(rare);
        anniversary_ = std::move(anniversary);
        refresh_.sampled();
        loaded_ = true;
        return true;
    }

    // Called once per Terraria addCloud attempt. No allocation, MonoVM,
    // JNI, locks or texture queries on this hot path.
    int chooseCommon(double random01) {
        if (!loaded_) return 0;
        const int type = common_.choose(random01);
        refresh_.nextAttempt();
        return type;
    }

    // The original Bunny JS uses 18 rare vanilla choices, or 15 in a
    // tenth-anniversary world. The host supplies the world snapshot.
    int chooseRare(double random01, bool anniversaryWorld) const {
        if (!loaded_) return 0;
        return (anniversaryWorld ? anniversary_ : rare_).choose(random01);
    }

    bool refreshRequested() const {
        return !loaded_ || refresh_.needsRefresh();
    }

    void clear() {
        loaded_ = false;
        common_.reset(22);
        rare_.reset(18);
        anniversary_.reset(15);
        refresh_.reset();
    }

    bool loaded() const { return loaded_; }

private:
    NativeCloudWeightedPool common_;
    NativeCloudWeightedPool rare_;
    NativeCloudWeightedPool anniversary_;
    NativeCloudCommonRefresh refresh_;
    bool loaded_ = false;
};

} // namespace bl::csharp
