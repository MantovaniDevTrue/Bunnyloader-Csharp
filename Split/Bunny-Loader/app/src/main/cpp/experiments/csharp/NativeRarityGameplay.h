#pragma once
#include <string>
#include <cstdint>

namespace bl::csharp {

bool registerNativeRaritySampling(const std::string& mod, const std::string& assembly);
void pumpNativeRarityGameplay();
bool nativeManagedRarityColor(int type, uint32_t* argb);

}
