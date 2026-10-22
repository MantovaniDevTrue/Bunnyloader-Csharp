#pragma once

#include <cstdint>
#include <string>

namespace bl::csharp {

bool installNativeBiomeUpdateHookOnGameThread(
    bool nativeBiomeRegistrationConfirmed, std::string* error);
uint32_t takeNativeBiomePlayerUpdatesOnGameThread();
void pauseNativeBiomeUpdateHookOnGameThread();
bool nativeBiomeUpdateHookEnabled();

} // namespace bl::csharp
