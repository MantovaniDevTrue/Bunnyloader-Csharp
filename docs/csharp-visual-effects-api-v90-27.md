# Bunny C# visual effects — v90.27 work in progress

This branch is based on `csharp-composite-accessories-v90-26`.

## Already staged

- The original ExampleModBunny ExampleShield and ExampleBoots sprites were reused inside ExampleModCSharp.
- Guardian and Trailblazer icon paths are no longer aliases of the Focus accessory.
- Equipment sprites are preserved as separate assets for future equipment layer integration.
- No C# gameplay damage callbacks were modified.

## New API design (NOT IMPLEMENTED)

The proposed native-backed **BunnyVisual** API will have:

- `BunnyVisual.SpawnParticle(in BunnyParticleSpec particle)` — pooled particles, hard per-player budget.
- `BunnyVisual.SetPlayerOverlay(in BunnyOverlaySpec overlay)` — a player-following energy shield, fades/pulses based on activation.
- `BunnyVisual.ClearPlayerOverlay(int effectId)` — deterministic removal when an accessory is unequipped.
- `BunnyVisual.SetEquipmentSprite(in BunnyEquipmentVisualSpec sprite)` — boots and shield visual layers.

These are proposed contracts, **not callable APIs** until implemented in BunnyCsSdk, the native gameplay bridge, and the source compiler/packager.

The Mystic Ward remains a passive protective accessory; its existing low-life and nearby-projectile combat rules stay unchanged until visual bridge is validated. The shield overlay should be translucent blue, animate on successful protection, and expose customizable texture/color/scale/opacity to mod authors. The boots trail should be motion-gated and use a bounded number of particles per frame.

## Implementation / validation criteria

1. Dispatch visual commands from managed C# through an explicit bounded command queue.
2. Resolve custom mod sprite textures and layer ownership without recreating them each frame.
3. Apply visual state only to valid player instances, release on world leave and accessory removal.
4. Implement a real drawing hook and texture compositing; do not mistake `FrameEffects() => true` for rendering.
5. Validate both effect paths on-device and preserve 60 FPS.
6. Do not claim a compiled APK until CI builds and signs one.
