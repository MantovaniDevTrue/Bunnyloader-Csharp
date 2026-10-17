#pragma once

#include "graphics/EffectTypes.h"
#include <cstddef>
#include <string>

namespace bl::graphics {

struct FrameComposeLimits {
    static constexpr size_t MaxTargets = 64;
    static constexpr size_t MaxDraws = 256;
};

bool appendNamespacedFrame(EffectFrame& destination, EffectFrame&& contribution,
                           const std::string& modId, std::string* error = nullptr);

}
