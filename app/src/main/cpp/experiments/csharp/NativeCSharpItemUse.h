#pragma once
#include <cstdint>
#include <string>

namespace bl::csharp {

void setNativeItemUseWorldState(bool ready, uint64_t session);
void pumpNativeItemUseBindingsOnGameThread();
bool enqueueVerifiedRemoteItemUse(int playerIndex, const std::string& mod,
                                  const std::string& itemName);

}
