# Bunny C# Visual API — v90.27 development

## Current status: .90.27.6

The earlier sections below are historical integration notes. The Mystic Ward
overlay is connected and has user-confirmed device rendering. Shield/Shoes use
native equipment textures; this patch installs them at content-ready even when
there are no JS ModItems, and also installs late descriptors from the draw poll.
Saved functional/vanity items refresh their native slot before the original
equipment method. Trailblazer boots cancel conflicting boot visuals, set
hellfireTreads and call DoBootsEffect_PlaceFlamesOnTile while visible, grounded,
moving faster than one pixel/tick, alive and unmounted. Stats are unchanged.

Host tests cover C#-only startup, late install, deduplication, missing textures,
native assignment rejection/recovery and boot tile geometry. Android rendering
and frame rate of this fix are pending device verification. No custom shader API
is introduced. Logs identify missing texture paths, rejected slots and missing
native methods/fields; the first eligible flame attempts log tile/result.

Base: `csharp-composite-accessories-v90-26`.

## Implemented in branch

- Reused original ExampleModBunny shield/boots inventory and equipment sprites under ExampleModCSharp.
- Updated shield and boots inventory texture paths; all gameplay callbacks unchanged.
- Added `BunnyLoader.Source.BunnyVisual` to BunnyCsSdk as a bounded managed command producer.
- Commands: `SpawnParticle`, `SetPlayerOverlay`, `ClearPlayerOverlay`, `SetEquipmentSprite`.
- Queue capacity: 64 commands, guarded against invalid players, effect IDs, opacity, scale and oversized paths.
- `TryTake` and `ClearPending` are available for a future host consumer.

## Not implemented yet

This is **NOT an operational render API**. No MonoVM-to-native command drain, Terraria draw-layer hook, asset resolution, animation renderer, or equipped-sprite placement is wired. The new methods can enqueue commands in a managed assembly but cannot display effects in-game. Do not ship a release advertising visual effects as functional.

## Next integration

1. Confirm the Source compiler links BunnyCsSdk's BunnyVisual definition into playable source mods.
2. Implement a native command consumer with thread-safe bounded transport and lifecycle reset.
3. Resolve textures from the originating mod, render player-following overlays and pooled particles.
4. Wire the ExampleMysticWard overlay on equip and pulse on protection; visual cleanup on unequip/world exit.
5. Emit Trailblazer particles only while moving; add equipped shield and boots layers.
6. Run compiler tests and Android 60 FPS QA before signing/publishing an APK.

Keep the existing passive Mystic Ward protection and the existing Guardian/Trailblazer gameplay stats intact.

## Runtime entry points (commit 3159746)

- `pollManagedVisualCommands(assemblyPath, budget)` resolves `BunnyLoader.Source.BunnyVisual.TakeWire()` through the existing Mono embedding runtime, attaches the current thread when needed, and pumps bounded commands to a shared native queue.
- `takeManagedVisualCommand()` reads from that queue; `clearManagedVisualCommands()` resets it.
- These are *on-demand native API entry points*, not yet scheduled by the game loop. No renderer or texture draw has been connected. Full Android compilation and in-game validation are still required.
- Caveat: the source assembly must include `BunnyVisual` in namespace `BunnyLoader.Source`. Confirm package/source compiler inclusion before using this path.

## Native state integration (commit 786a294)

The native ManagedRuntime now owns a persistent ManagedVisualState and offers `advanceManagedVisualState(tick, budget)` plus `activeManagedVisualCount()`. Clearing commands also clears overlay state. This implements a bounded way to transfer already-ingested commands into persistent visual state. The game update loop does NOT yet schedule it, and no drawing layer consumes it. The bridge therefore remains non-visual until frame integration and renderer implementation. Full Android compilation is still outstanding.

## Game update lifecycle (commit 684c563)

`AutoSourceRuntime::noteAutoGameUpdate()` now advances the native visual state once per game update while a world session is ready, with a maximum budget of 32 queue messages. World loading and world exit clear pending and active visual effects and reset the update counter. This is update-state integration only: the source assembly visual producer is not yet scheduled automatically, and no render hook draws active overlays or particles. No Android APK has been built or performance measured for this branch.

## SDK class resolution correction (7c703f4)

The original Mono polling entry point incorrectly resolved `BunnyLoader.Source.BunnyVisual` in each compiled mod assembly. Because the Roslyn compiler references `BunnySourceSdk.dll`, the class belongs to the SDK. The polling entry point now resolves the class from an SDK DLL adjacent to the provided mod assembly. **This filesystem layout is an assumption that still requires verification against real APK extraction paths**. Do not enable per-frame Mono polling until assembly location, lifecycle and threading are verified in-game.

## Automatic polling (commit 21cff14)

During an active world session, AutoSourceRuntime polls up to eight pending SDK commands once every twelve game updates, then advances native visual state with a budget of 32. The Mono entry point uses a try-lock to avoid waiting for the Mono runtime mutex. Polling is experimental and requires an Android APK build plus device FPS/crash verification. Effects still cannot be seen: no active visual draw-layer consumer is installed, and the texture resolver is not wired. The sample accessories emit commands but their player index and graphics bindings remain placeholders.

