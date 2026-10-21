#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

namespace bl::csharp {

// CloudLoader C#: deterministic source-only policy. Never touch IL2CPP here:
// texture installation, cloud hooks and managed callbacks belong to the game thread.
// No C# cloud may be advertised as playable until those integrations exist.
class NativeCloudWeightedPool {
public:
    static constexpr std::size_t kMaxModClouds = 128;
    static constexpr double kMaxTotalWeight = 1000000000.0;

    // The original Bunny Loader uses 22 common vanilla clouds, 18 rare vanilla
    // clouds, or 15 rare vanilla clouds in tenth-anniversary worlds.
    bool reset(double vanillaWeight) {
        entries_.clear();
        if (!std::isfinite(vanillaWeight) || vanillaWeight <= 0 ||
            vanillaWeight > kMaxTotalWeight) {
            total_ = 0;
            return false;
        }
        total_ = vanillaWeight;
        return true;
    }

    // Reject duplicate type IDs, missing textures and invalid weights:
    // otherwise Terraria may select a type with no TextureAssets.Cloud entry.
    bool add(int type, int firstModType, double weight, bool textureReady) {
        if (type < firstModType || firstModType <= 0 || !textureReady ||
            !std::isfinite(weight) || weight <= 0 || total_ <= 0 ||
            weight > kMaxTotalWeight - total_ || entries_.size() >= kMaxModClouds)
            return false;
        for (const Entry& e : entries_)
            if (e.type == type) return false;
        entries_.push_back({type, total_, total_ + weight});
        total_ += weight;
        return true;
    }

    // Returns 0 for vanilla. Random sample must be finite in [0,1), matching
    // Math.random() in ExampleModBunny; no allocation occurs on the hot path.
    int choose(double random01) const {
        if (total_ <= 0 || !std::isfinite(random01) ||
            random01 < 0 || random01 >= 1) return 0;
        const double selection = random01 * total_;
        for (const Entry& e : entries_)
            if (selection >= e.begin && selection < e.end) return e.type;
        return 0;
    }

    double totalWeight() const { return total_; }
    std::size_t modCount() const { return entries_.size(); }

private:
    struct Entry { int type; double begin; double end; };
    std::vector<Entry> entries_;
    double total_ = 0;
};

// Common SpawnChance() is refreshed every 60 attempts in the original JS.
// This counter never invokes MonoVM from the frame/render loop.
class NativeCloudCommonRefresh {
public:
    bool needsRefresh() const { return remaining_ == 0; }
    void sampled() { remaining_ = 60; }
    void nextAttempt() { if (remaining_ > 0) --remaining_; }
    void reset() { remaining_ = 0; }
private:
    int remaining_ = 0;
};

// A rare roll is nested in Cloud.addCloud: it always supersedes the prior
// common candidate. Keep slots local to the attempt to prevent stale OnSpawn.
class NativeCloudSpawnAttempt {
public:
    void begin(int commonType, int freeCommonSlot) {
        rareRolled_ = false;
        commonType_ = commonType > 0 ? commonType : 0;
        commonSlot_ = freeCommonSlot;
        rareType_ = 0;
        rareSlot_ = -1;
    }
    void rareRolled(int selectedModType, int freeRareSlot) {
        rareRolled_ = true;
        rareType_ = selectedModType > 0 ? selectedModType : 0;
        rareSlot_ = freeRareSlot;
    }
    int candidateSlot() const { return rareRolled_ ? rareSlot_ : commonSlot_; }
    bool wasRareRolled() const { return rareRolled_; }
    struct Spawn { int type; int slot; bool replaceType; };
    // The hook must additionally ensure Main.cloud[slot] is active before
    // calling this method, and must verify actualRareType after the original.
    Spawn result(bool active, int actualRareType) const {
        if (!active) return {0, -1, false};
        if (rareRolled_) {
            if (rareType_ <= 0 || rareSlot_ < 0 || actualRareType != rareType_)
                return {0, -1, false};
            return {rareType_, rareSlot_, false};
        }
        if (commonType_ <= 0 || commonSlot_ < 0) return {0, -1, false};
        return {commonType_, commonSlot_, true};
    }
private:
    int commonType_ = 0, commonSlot_ = -1;
    int rareType_ = 0, rareSlot_ = -1;
    bool rareRolled_ = false;
};

// The mobile horizon renderer indexes TextureMaskManager.CloudMasks[type]
// WITHOUT a bounds check. Before exposing a new type grow both arrays and
// install a non-null mask. outputWidth=0 avoids incorrectly hiding the sun.
struct NativeCloudRenderGate {
    static bool safe(int type, int firstModType, int textureLength,
                     int maskLength, bool texturePresent, bool maskPresent,
                     int sourceWidth, int sourceHeight, int maskOutputWidth) {
        return firstModType > 0 && type >= firstModType &&
               type < textureLength && type < maskLength &&
               texturePresent && maskPresent && sourceWidth > 0 &&
               sourceHeight > 0 && maskOutputWidth == 0;
    }
};

} // namespace bl::csharp
