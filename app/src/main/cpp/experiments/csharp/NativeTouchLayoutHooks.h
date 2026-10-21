#pragma once
namespace bl::csharp::touch {
void pumpNativeTouchLayoutHooksOnGameThread();
bool nativeTouchLayoutHooksInstalled();
bool nativeTouchLayoutsRestored();
} // namespace bl::csharp::touch
