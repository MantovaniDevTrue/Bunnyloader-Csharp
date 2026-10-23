#pragma once
#include <array>

namespace bl::csharp {

inline constexpr int kExtendedHookMin = 1;
inline constexpr int kExtendedHookMax = 5;
inline constexpr int kExtendedHookInt32Zero = 1;
inline constexpr int kExtendedHookVoidInt32 = 2;
inline constexpr int kExtendedHookVoidInt32Pair = 3;
inline constexpr int kExtendedHookInt32Triple = 4;
inline constexpr int kExtendedHookVoidInt32Triple = 5;

struct ExtendedHookSignature {
    int parameterCount;
    bool returnsInt32;
};

inline constexpr std::array<ExtendedHookSignature, kExtendedHookMax + 1> kExtendedHookSignatures = {{
    {0, false},
    {0, true},
    {1, false},
    {2, false},
    {3, true},
    {3, false},
}};

}
