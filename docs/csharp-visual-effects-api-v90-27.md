# Bunny C# Visual API — v90.27 development

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
