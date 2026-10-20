#pragma once

#include <cstdint>

namespace bl::csharp {

void setNativeInvasionWorldState(bool ready, uint64_t session);
void pumpNativeInvasionSpawnsOnGameThread();
void clearNativeInvasionMailbox();

}
