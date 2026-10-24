#pragma once
#include <cstdint>
#include <string_view>

namespace bl::csharp {
inline constexpr uint64_t kRarityPendingBackoffFrames = 30;
inline constexpr bool rarityRetryPending(std::string_view result) {
    return result.starts_with("PENDING|");
}
inline constexpr uint64_t rarityRetryAt(uint64_t now) {
    return now + kRarityPendingBackoffFrames;
}
inline constexpr bool raritySampleReady(uint64_t now, uint64_t next) {
    return now >= next;
}
}
