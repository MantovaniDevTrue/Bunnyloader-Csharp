#pragma once
#include <cstdint>

namespace bl::csharp {
void setNativeInvasionHudWorldState(bool ready, uint64_t session);
void pumpNativeInvasionHudOnGameThread();
}
