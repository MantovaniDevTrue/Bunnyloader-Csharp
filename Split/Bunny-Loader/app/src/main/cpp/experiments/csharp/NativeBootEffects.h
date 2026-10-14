#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

namespace bl::csharp {
// Mesma condicao do ExampleBoots: correr no chao. A API nativa decide se
// o tile aceita fogo; aqui nao criamos tiles nem alteramos atributos.
inline bool nativeBootFlameTile(float x, float y, int width, int height,
                                float vx, float vy, bool dead, bool mounted,
                                int32_t* tileX, int32_t* tileY) {
    if (!tileX || !tileY || dead || mounted || width <= 0 || height <= 0 ||
        !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(vx) ||
        !std::isfinite(vy) || vy != 0.f || std::abs(vx) <= 1.f) return false;
    const double tx = std::floor((static_cast<double>(x) + width / 2.0) / 16.0);
    const double ty = std::floor((static_cast<double>(y) + height) / 16.0);
    if (tx < 0 || ty < 0 || tx > std::numeric_limits<int32_t>::max() ||
        ty > std::numeric_limits<int32_t>::max()) return false;
    *tileX = static_cast<int32_t>(tx);
    *tileY = static_cast<int32_t>(ty);
    return true;
}
}
