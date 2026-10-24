#pragma once

namespace bl::csharp {
inline constexpr bool worldTickUnsupported(int result) {
    return result == -2;
}
inline constexpr bool worldTickFailed(int result) {
    return result < 0 && !worldTickUnsupported(result);
}
}
