#pragma once

namespace bl::csharp::touch {

// Chamados antes/depois de GUIVirtualInputController.Update, na mesma thread.
void nativeTouchGroupBefore(void* controller, bool gameplay);
void nativeTouchGroupAfter(void* controller);
void resetNativeTouchGroupSession();
bool nativeTouchGroupBridgeReady();

} // namespace bl::csharp::touch
