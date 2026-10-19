#pragma once
#include <cstdint>

namespace bl::csharp {

inline uint32_t tintNativeRarityText(uint32_t argb, uint8_t opacity) {
    const uint32_t red = ((argb >> 16) & 255u) * opacity / 255u;
    const uint32_t green = ((argb >> 8) & 255u) * opacity / 255u;
    const uint32_t blue = (argb & 255u) * opacity / 255u;
    const uint32_t alpha = ((argb >> 24) & 255u) * opacity / 255u;
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

inline bool shouldTintNativeRarityText(uint32_t color, uint8_t opacity) {
    const uint32_t red = (color >> 16) & 255u;
    const uint32_t green = (color >> 8) & 255u;
    const uint32_t blue = color & 255u;
    return opacity > 0 && (red > 25u || green > 25u || blue > 25u);
}

}
