#pragma once
namespace bl::csharp::touch {
void pumpNativeTouchLayoutHooksOnGameThread();
void invalidateNativeTouchLayouts();
bool nativeTouchLayoutHooksInstalled();
bool nativeTouchLayoutsRestored();
bool reapplyNativeTouchLayoutsOnGameThread();
} // namespace bl::csharp::touch
