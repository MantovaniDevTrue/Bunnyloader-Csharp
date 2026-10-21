#pragma once

#include "il2cpp/Types.h"

#include <string>

namespace bl::csharp {

struct NativeCloudHookBindings {
    const MethodInfo* addCloud = nullptr;
    const MethodInfo* rollRareCloud = nullptr;
    FieldInfo* mainCloudArray = nullptr;
    FieldInfo* tenthAnniversaryWorld = nullptr;
    int active = -1;
    int type = -1;
    int scale = -1;
    int width = -1;
    int height = -1;

    // Terraria stores SpriteEffects as an XNA enum; its underlying type must
    // be inspected rather than treating the enum like a managed object.
    int spriteEffects = -1;
};

// Resolve *exactly* the signatures used by the original ExampleModBunny JS.
// This does not install hooks or expose new IDs to the game. Calls are made
// only during game-thread initialization, never inside Cloud.addCloud itself.
bool resolveNativeCloudHookBindings(NativeCloudHookBindings* bindings,
                                    std::string* error);

} // namespace bl::csharp
