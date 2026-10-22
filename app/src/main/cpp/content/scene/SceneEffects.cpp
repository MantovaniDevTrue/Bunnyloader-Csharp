#include "content/scene/SceneEffects.h"
#include <algorithm>
#include <cmath>

namespace bl::runtime::scene {
namespace {
struct Ranked {
    int index;
    double rank;
};

double clampWeight(double weight) {
    // Same as Math.max(0, Math.min(1, Number(weight) || 0)): NaN is 0, and
    // the infinities clamp to the ends.
    if (std::isnan(weight)) return 0.0;
    return std::max(0.0, std::min(1.0, weight));
}
}  // namespace

Result rank(const std::vector<Candidate>& candidates) {
    Result result;
    std::vector<Ranked> ranked;
    ranked.reserve(candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i) {
        const Candidate& c = candidates[i];
        if (!c.active) continue;
        ranked.push_back({static_cast<int>(i), clampWeight(c.weight) + static_cast<double>(c.priority)});
    }
    if (ranked.empty()) return result;

    std::stable_sort(ranked.begin(), ranked.end(), [&](const Ranked& a, const Ranked& b) {
        if (a.rank != b.rank) return a.rank > b.rank;
        return candidates[a.index].key < candidates[b.index].key;
    });

    result.anyActive = true;
    result.active.reserve(ranked.size());
    for (const Ranked& r : ranked) result.active.push_back(r.index);

    for (const Ranked& r : ranked) {
        const Candidate& c = candidates[r.index];
        for (int channel = 0; channel < kValueChannels; ++channel) {
            ChannelPick& pick = result.channels[channel];
            // The original loader only skips a music of exactly -1 (use the
            // game's music); other negatives still claim the channel. Style
            // and water slots are never negative when an effect fills them.
            const bool fills = channel == Music ? c.values[channel] != -1 : c.values[channel] >= 0;
            if (pick.from >= 0 || !fills) continue;
            pick.value = c.values[channel];
            pick.priority = c.priority;
            pick.from = r.index;
            pick.weight = r.rank - static_cast<double>(c.priority);
        }
        if (result.map.from < 0 && !c.map.empty()) {
            result.map.value = c.map;
            result.map.from = r.index;
        }
    }
    return result;
}

}  // namespace bl::runtime::scene
