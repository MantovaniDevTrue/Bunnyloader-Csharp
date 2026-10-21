#pragma once
namespace bl::csharp::touch {
void pumpNativeTouchLayoutHooksOnGameThread();
bool nativeTouchLayoutHooksInstalled();
bool nativeTouchLayoutsRestored();
bool reapplyNativeTouchLayoutsOnGameThread();
} // namespace bl::csharp::touch
