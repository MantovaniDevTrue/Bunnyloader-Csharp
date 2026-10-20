#pragma once
#include <cstdint>

namespace bl::csharp {
void setNativeInvasionNoticeWorld(bool ready, uint64_t session);
void pumpNativeInvasionNoticesOnGameThread();
}
