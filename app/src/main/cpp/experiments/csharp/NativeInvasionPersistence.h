#pragma once
#include <cstdint>
#include <string>

namespace bl::csharp {

void setNativeInvasionSaveWorld(bool ready, uint64_t session,
                                const std::string& path, bool cloud);
void updateNativeInvasionWorldPath(bool ready, uint64_t session);

}
