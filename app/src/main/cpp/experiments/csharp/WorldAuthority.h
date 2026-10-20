#pragma once

namespace bl::csharp {

constexpr bool worldIsAuthoritative(int netMode) noexcept {
    return netMode == 0 || netMode == 2 || netMode == 3;
}

}
