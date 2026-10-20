#pragma once

#include <cstdint>

namespace bl::csharp::invasion_peer {

constexpr uint64_t expiryInterval = 1800;
constexpr uint64_t sweepInterval = 180;

inline bool expired(uint64_t now, uint64_t lastSeen) {
    return now > lastSeen && now - lastSeen > expiryInterval;
}

}
