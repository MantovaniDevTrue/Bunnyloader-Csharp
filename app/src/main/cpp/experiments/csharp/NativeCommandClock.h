#pragma once

#include <cmath>

namespace bl::csharp {

// Mesmo ciclo usado em ExampleTimeCommand.js no ExampleModBunny.
// Matematica pura para poder testar limites sem instanciar Terraria.
inline bool advanceNativeCommandClock(double time, bool day, int ticks,
                                      double* nextTime, bool* nextDay) {
    constexpr double dayLength = 54000.0;
    constexpr double nightLength = 32400.0;
    constexpr double cycle = dayLength + nightLength;
    if (!nextTime || !nextDay || !std::isfinite(time) ||
        time < 0 || time > cycle ||
        ticks < -86400000 || ticks > 86400000) return false;
    double absolute = std::fmod(time + (day ? 0.0 : dayLength) +
                                static_cast<double>(ticks), cycle);
    if (absolute < 0) absolute += cycle;
    *nextDay = absolute < dayLength;
    *nextTime = *nextDay ? absolute : absolute - dayLength;
    return std::isfinite(*nextTime);
}

}
