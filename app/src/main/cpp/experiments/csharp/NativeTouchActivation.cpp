#include "experiments/csharp/NativeTouchActivation.h"
#include "experiments/csharp/NativeTouchInput.h"
#include "experiments/csharp/NativeTouchGameAbi.h"
#include "experiments/csharp/NativeTouchController.h"
#include "experiments/csharp/NativeTouchPointer.h"
#include "experiments/csharp/NativeTouchGroups.h"
#include "experiments/csharp/NativeTouchVisibility.h"
#include "experiments/csharp/NativeTouchEditor.h"
#include "experiments/csharp/NativeTouchEditorOptions.h"
#include "experiments/csharp/NativeTouchIconHooks.h"
#include "experiments/csharp/NativeTouchLayoutHooks.h"
#include "experiments/csharp/NativeTouchStaticMappings.h"
#include "core/Log.h"
#include <cstdint>

namespace bl::csharp::touch {

void pumpNativeTouchActivationOnGameThread() {
    static uint64_t tick = 0;
    if (registry().count() == 0) {
        if (hostReady()) setHostReady(false);
        return;
    }
    if (hostReady()) {
        if (++tick % 60 != 0) return;
        if (!ensureNativeTouchEditorOptionsOnGameThread()) return;
        reapplyNativeTouchLayoutsOnGameThread();
        return;
    }
    if (++tick != 1 && tick % 30 != 0) return;
    if (tick >= 600 && tick % 600 == 0) {
        const auto abi = inspectNativeTouchAbi();
        BL_WARN("[CSharp][Touch] ActivationPending ABI=%s Observer=%d Pointer=%d Groups=%d Visibility=%d EditorLabels=%d IconFrames=%d QuickDraw=%d LayoutHooks=%d",
                abi.diagnostic.c_str(),
                nativeTouchObserverInstalled(), nativeTouchPointerInstalled(),
                nativeTouchGroupBridgeReady(), nativeTouchVisibilityHookInstalled(),
                nativeTouchEditorLabelsInstalled(), nativeTouchIconFrameHooksInstalled(),
                nativeTouchQuickDrawHookInstalled(), nativeTouchLayoutHooksInstalled());
    }

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
    BL_INFO("[CSharp][Touch] NativeHostReady Controls=%d", registry().count());
}

} // namespace bl::csharp::touch
