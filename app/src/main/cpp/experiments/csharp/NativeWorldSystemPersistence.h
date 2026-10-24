#pragma once
#include <cstdint>
#include <string>

namespace bl::csharp {

void setNativeWorldSystemSaveWorld(bool ready, uint64_t session,
                                  const std::string& path, bool cloud);

}
