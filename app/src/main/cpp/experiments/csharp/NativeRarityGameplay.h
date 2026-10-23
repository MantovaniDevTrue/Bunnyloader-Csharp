#pragma once
#include <string>
#include <cstdint>

namespace bl::csharp {

bool registerNativeRaritySampling(const std::string& mod, const std::string& assembly);
void pumpNativeRarityGameplay();
void pumpNativeTooltipGameplay();
bool nativeTooltipReadyForItem(int type);
bool nativeTooltipJavascriptOwner(bool active);
void retireNativeRaritySampling(const std::string& mod);
bool nativeManagedRarityColor(int type, uint32_t* argb);

}
