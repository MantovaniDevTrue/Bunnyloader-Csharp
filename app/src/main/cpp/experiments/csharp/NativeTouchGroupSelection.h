#pragma once

namespace bl::csharp::touch {
inline constexpr int kGroupReadError = -2;

inline bool nativeTouchGroupSelected(int previous, int current, int count) {
    return previous >= 0 && previous != 9 && previous < count &&
           current == -1;
}
}
