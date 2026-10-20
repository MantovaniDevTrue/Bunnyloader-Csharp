#pragma once
#include <cstdint>

namespace bl::csharp {

void setNativeItemUseWorldState(bool ready, uint64_t session);
void pumpNativeItemUseBindingsOnGameThread();

}
