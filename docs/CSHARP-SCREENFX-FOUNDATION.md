# Native screen-frame composition for C# mods

This change introduces an isolated, reusable graphics seam. The old
`submitEffects(frame)` entry point remains the JS producer and still replaces
only its own latest frame. Native/C# producers may call:

```cpp
graphics::submitModEffects(modId, std::move(frame));
```

Call this **once per active mod and game update**, from the game thread,
not the Mono worker and not Unity's GL render thread. The renderer consumes
contributions after world-camera rendering. Missing a contribution for a frame
does not repeat stale draws. `submitModEffects(modId, {})` clears a pending
contribution, not a currently displayed frame.

The compositor namespaces target names as `modId:targetName`, remaps
positive target IDs and their references (source, dest and sampler bindings),
and validates geometry/uniforms/target bindings before changing the destination.
A rejected mod contribution does not erase JS or other mod contributions.
Contributors are combined in deterministic mod-ID order, after the JS frame.
Shader handles and mod PNG texture handles are registry-global already; this
system only remaps **positive frame-local render-target IDs**. No JS adapter or
tModLoader code is used.

The new seam does **not** yet execute managed C# callbacks, discover shaders
from .bmod manifests, or collect Terraria time/biome snapshots. Those require
a subsequent typed MonoVM-to-host service and integration with the Lights port.
It is deliberately not advertised as working Lights And Shadows gameplay.

## Local unit test

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Werror \
  -Iapp/src/main/cpp \
  app/src/main/cpp/graphics/FrameComposer.cpp \
  tests/frame_composer_multi_mod_test.cpp \
  -o /tmp/frame_composer_multi_mod_test
/tmp/frame_composer_multi_mod_test
```

## Device acceptance

1. JS screen effect alone remains unchanged
2. Two independent native producers may both reference target ID 51001
3. Target surfaces are separate, and no UI lighting regression occurs
4. Disabled C# mod stops submitting, and effect disappears next frame
5. Shader compiler errors do not crash the GL render thread
6. Verify frame-times and GPU timers under real 60 FPS gameplay

The renderer must be tested on the target Android device before shipping
an APK as validated.
