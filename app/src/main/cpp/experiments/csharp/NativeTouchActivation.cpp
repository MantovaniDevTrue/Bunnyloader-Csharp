#include "experiments/csharp/NativeTouchActivation.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchPointer.h"
#include "experiments/csharp/NativeTouchGroups.h"
#include "experiments/csharp/NativeTouchVisibility.h"
#include "experiments/csharp/NativeTouchEditor.h"
#include "experiments/csharp/NativeTouchEditorOptions.h"
#include "experiments/csharp/NativeTouchIconHooks.h"
#include "experiments/csharp/NativeTouchLayoutHooks.h"
#include "experiments/csharp/NativeTouchStaticMappings.h"
#include <cstdint>

namespace bl::csharp::touch {

void pumpNativeTouchActivationOnGameThread() {
    static uint64_t tick = 0;
    static uint64_t appliedVersion = 0;
    if (registry().count() == 0) {
        if (hostReady()) setHostReady(false);
        return;
    }
    const uint64_t version = registry().registrationVersion();
    if (hostReady()) {
        if (++tick % 60 != 0) return;
        if (!ensureNativeTouchEditorOptionsOnGameThread()) return;
        if (version != appliedVersion &&
            reapplyNativeTouchLayoutsOnGameThread())
            appliedVersion = version;
        return;
    }
    if (++tick != 1 && tick % 30 != 0) return;

    // Nenhuma ação chega a ProcessTriggers enquanto faltar qualquer peça.
    // O editor não anuncia controles acionáveis se o hook de toque, o
    // salvamento seguro e a representação de grupos não estiverem prontos.
    if (!nativeTouchObserverInstalled() ||
        !nativeTouchPointerInstalled() ||
        !nativeTouchGroupBridgeReady() ||
        !nativeTouchVisibilityHookInstalled() ||
        !nativeTouchEditorLabelsInstalled() ||
        !nativeTouchIconFrameHooksInstalled() ||
        !nativeTouchQuickDrawHookInstalled() ||
        !nativeTouchLayoutHooksInstalled())
        return;
    if (!reapplyNativeTouchLayoutsOnGameThread()) return;
    if (!ensureNativeTouchStaticMappingsOnGameThread()) return;
    if (!ensureNativeTouchEditorOptionsOnGameThread()) return;
    setHostReady(true);
    appliedVersion = version;
}

} // namespace bl::csharp::touch
