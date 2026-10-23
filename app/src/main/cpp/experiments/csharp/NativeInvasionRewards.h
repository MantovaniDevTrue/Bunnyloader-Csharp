#pragma once
#include <cstdint>

namespace bl::csharp {
void setNativeInvasionRewardWorldState(bool ready, uint64_t session);
void pumpNativeInvasionRewardsOnGameThread();
}
