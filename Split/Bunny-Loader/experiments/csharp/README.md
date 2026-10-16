# Bunny C# NativeAOT prototype

This experiment verifies that C# can execute inside the Bunny Loader process without embedding Mono/CoreCLR.

The C# project is compiled with .NET NativeAOT for Android Bionic ARM64 and packaged as `libbunnycs.so`. Bunny loads it with `dlopen` and calls exported C ABI methods.

This is intentionally not a C# mod loader yet. It measures the smallest viable bridge first.

Exports:

- `bunnycs_version`
- `bunnycs_add`
- `bunnycs_checksum`
- `bunnycs_player_sample`

The Bunny native side benchmarks direct C++ calls against C++ -> C# NativeAOT calls and publishes a small `bl.csharp` test API to QuickJS.

If this prototype proves stable on-device, the next stage is a host ABI for Terraria objects and content registration.


## Benchmark suite

The 0.1.9.6 prototype adds two batch workloads:

- `bunnycs_math_batch`: deterministic 32-bit integer workload.
- `bunnycs_projectile_batch`: position/velocity/gravity/bounce simulation over many projectile-like objects.

The QuickJS API exposes `bl.csharp.suite(iterations, count, steps)`. It runs the same algorithm in JavaScript and C# NativeAOT, measures both with the same native steady clock, checks the integer checksum and reports projectile checksum delta.

Recommended device test:

```js
JSON.stringify(bl.csharp.suite(1000000, 4096, 180), null, 2)
```


## Live Terraria host benchmark

Version 0.1.9.7 adds a read-only host snapshot of the current Terraria process.

The native Bunny side resolves `Main.player`, `Main.npc` and `Main.projectile` with the existing IL2CPP resolver. It copies player/NPC/projectile state into blittable structs and sends each batch to C# NativeAOT in one call.

No Terraria state is written by this prototype.

Run in-world:

```js
JSON.stringify(bl.csharp.terrariaSuite(1000), null, 2)
```

The result separates snapshot capture time from JavaScript and C# processing time and reports checksum deltas for both workloads.


## Controlled projectile write test

Version 0.1.9.8 adds the first controlled write path.

Fire a normal projectile owned by the local player, then run:

```js
JSON.stringify(bl.csharp.claimProjectile(), null, 2)
```

The host claims only one active, damaging projectile owned by the local player. It stores the projectile slot, object reference, type, owner and a Bunny generation token. The claim is also cleared when Projectile.SetDefaults runs on the same object, so slot reuse cannot inherit an old claim.

C# NativeAOT scans the current NPC snapshot, chooses the nearest valid hostile target and returns a new velocity/rotation. Bunny applies only those two fields to the claimed projectile.

Status:

```js
JSON.stringify(bl.csharp.projectileWriteStatus(), null, 2)
```

Release manually:

```js
bl.csharp.releaseProjectile()
```

The claim is released when the projectile receives `Kill()` or when `SetDefaults()` starts reusing that projectile object. No Player or NPC state is written by this test.


## 0.1.9.8.2 write-order fix

The claimed projectile is now processed by the C# host before vanilla `Projectile.AI`. This avoids losing the write when vanilla AI kills the projectile during the same call.

`projectileWriteStatus()` also reports `aiCalls` so device tests can distinguish "hook was not called" from "hook ran but no valid target was found".


## 0.1.9.8.3 Update hook fix

The write host now hooks `Projectile.Update(int i)` instead of `Projectile.AI()`.

Device testing showed `Kill()` firing while the claimed vanilla arrow never entered the resolved `AI()` hook (`aiCalls = 0`). `Update(int i)` is the lifecycle entry that owns AI/collision/kill for the projectile, so the C# steering write is now applied before vanilla Update runs.

`projectileWriteStatus()` reports `updateCalls`.


## 0.1.9.8.4 Main projectile loop hook

Device testing showed both resolved `Projectile.AI()` and `Projectile.Update(int)` receiving zero calls for a live vanilla arrow while `Projectile.Kill()` fired normally.

The write host now hooks `Main.UpdateProjectiles()` instead. Before Terraria runs the global projectile update loop, Bunny validates the single claimed projectile and applies the C# NativeAOT steering result to it.

Status now reports `loopCalls` and preserves the last released claim in `lastSlot`, `lastType`, `lastTargetIndex`, `lastWrites` and `lastLoopCalls`, so a short-lived projectile can still be diagnosed after `Kill()`.


## 0.1.9.8.5 IL2CPP method probe

Terraria mobile 1.4.5.8.6 does not expose `Main.UpdateProjectiles()`, and device tests showed the resolved `Projectile.AI()` and `Projectile.Update(int)` entry points were not called for the tested vanilla arrow.

This build adds a runtime method probe instead of guessing another lifecycle name:

```js
JSON.stringify(bl.csharp.projectileProbe(), null, 2)
```

It enumerates methods directly from the loaded IL2CPP metadata and returns exact signatures plus native addresses for likely Projectile/Main lifecycle candidates.

For a narrower search inside Terraria.Projectile:

```js
JSON.stringify(bl.csharp.projectileMethods("update"), null, 2)
```


## 0.1.9.8.6 UpdatePosition hook

The runtime IL2CPP probe confirmed this real Terraria.Projectile method on the mobile build:

```
void UpdatePosition(Vector2 wetVelocity)
```

The controlled C# steering test now hooks that exact method instead of guessing a higher-level update method. When the claimed projectile enters `UpdatePosition`, C# selects the nearest valid hostile NPC and computes a new velocity/rotation. Bunny writes the projectile velocity and also replaces the `wetVelocity` argument passed to vanilla `UpdatePosition` so the movement step uses the steered direction immediately.

Kill/SetDefaults invalidation and the identity-free claim token remain unchanged.


## 0.1.9.9 C# Host API v1

The validated UpdatePosition write path is now routed through a reusable C# layer instead of keeping the homing logic inside the unmanaged export.

Host API v1 adds:

- `PlayerView`
- `NpcView`
- `ProjectileView`
- `ProjectileContext`
- `IProjectileBehavior`
- `BunnyHost`

The validated homing behavior now lives in `ExampleHomingProjectile.cs` and implements `IProjectileBehavior`.

The C ABI remains the boundary between Bunny native code and NativeAOT. `bunnycs_host_projectile_tick` only creates a context and dispatches the registered C# behavior.

Runtime status:

```js
JSON.stringify(bl.csharp.hostInfo(), null, 2)
```

Expected values include `runtimeAbi: 5`, `hostApi: 1` and `projectileStage: "Projectile.UpdatePosition"`.

This is the first reusable Host API layer. It is still embedded in the separate C# test app and is not yet an external C# mod package loader.


## 0.1.10.0 External NativeAOT mod loader

The validated homing behavior is no longer dispatched from the Bunny runtime library.

An enabled Bunny package may now include:

```
native/arm64-v8a/libmod.so
```

After the normal package is loaded, Bunny detects that library, validates Native Mod ABI 1 and Host API 1, calls `bunny_mod_initialize`, and registers its optional projectile callback.

The first SDK lives in `experiments/csharp/BunnyCsSdk`. The first external mod lives in `experiments/csharp/ExampleCSharpMod` and is packaged as a normal Bunny mod with a minimal JavaScript entry only so the current launcher can import and enable it unchanged.

Required exports for Native Mod ABI 1:

```
bunny_mod_abi_version
bunny_mod_host_api_version
bunny_mod_initialize
```

Optional projectile callback:

```
bunny_mod_projectile_tick
```

The loader first tries the mod package path directly. If Android refuses executable mapping from external storage, it copies the library into the app's private `code_cache/bunny-csharp` directory and loads it there.

Editor diagnostics:

```js
JSON.stringify(bl.csharp.hostInfo(), null, 2)
JSON.stringify(bl.csharp.externalMods(), null, 2)
```

The controlled `claimProjectile()` test now dispatches only to external C# mods. Without an external mod exposing `bunny_mod_projectile_tick`, the steering hook refuses to install.

## 0.1.10.1 Automatic owned projectiles

The host no longer needs the embedded `libbunnycs.so` prototype to install the external projectile hook. The benchmark prototype remains optional and unchanged.

The native host skips the full projectile-array scan when evaluating a projectile behavior, capturing only the player and NPCs instead.

A mod's NativeAOT `bunny_mod_projectile_tick` now runs automatically on active, locally owned, damaging projectile types that the same Bunny package registered in JavaScript (the native package UID must match the projectile registration owner). Other mods' projectile types and vanilla projectile types are not automatically affected. Existing `claimProjectile()` remains available for the tested vanilla-arrow demonstration.

Run in the Editor:

```js
JSON.stringify(bl.csharp.projectileWriteStatus(), null, 2)
```

The additional `automaticCalls` and `automaticWrites` counters measure the new path. Neither counter should increase for projectiles owned by unrelated mods. The 0.1.10.0 ExampleCSharpMod only registers a minimal JavaScript entry, not a new projectile type; its existing vanilla homing test still requires `claimProjectile()`. Creating a new projectile in the example via the Bunny ExampleMod JavaScript API is the next test.

This branch is a source-level test; automatic mode requires a new Android build and on-device validation before it can be declared stable.

### Testando o homing automático

A versão 0.1.1 do exemplo inclui `NativeHomingWand` e `NativeHomingBolt` na pasta JavaScript `Content/`. Os dois seguem as classes do ExampleModBunny. Os PNGs são reutilizados dos exemplos originais do Bunny.

Crie a `NativeHomingWand` com 10 madeiras e uma bancada de trabalho. Atire próximo de um NPC hostil. A nova biblioteca C# deve receber automaticamente o projétil `NativeHomingBolt` registrado pelo mesmo pacote, sem precisar executar `claimProjectile()`.

```js
JSON.stringify(bl.csharp.projectileWriteStatus(), null, 2)
```

Verifique que `automaticCalls` e `automaticWrites` aumentam. Para comparação, projéteis de outros mods não devem aumentar estes contadores.

## 0.1.10.2 Selective projectile profiles

Native Mod ABI 1 and Host API 1 are unchanged. The C++ host optionally resolves `bunny_mod_bind_projectile(int type, const char* name)` during module startup, after JavaScript has registered the mod's projectile types.

When the optional function is present, the host only dispatches projectile updates for types explicitly accepted by that module. Existing NativeAOT mods without this export continue to receive their own projectile types as before.

The SDK has new helpers: `TryFindClosestHostileInRange(radius, out target)` and `SteerTowardsPredicted(target, minimumSpeed, maximumSpeed, turn, maxLeadTicks)`. The latter approximates interception using the NPC velocity, with a maximum prediction horizon.

The ExampleCSharpMod v0.1.2 offers `NativeHomingWand` (unchanged homing) and `NativePredictiveWand` (predictive homing). Both are craftable with 10 wood and a workbench. The new weapon's PNGs use existing original ExampleModBunny textures.

In the in-game editor:

```js
JSON.stringify(bl.csharp.externalMods(), null, 2)
JSON.stringify(bl.csharp.projectileWriteStatus(), null, 2)
```

Expected after module load: `selectiveProjectiles: true`, `projectileBindings: 2`. Fire both weapons near moving enemies. The previous automatic counters must still increase. This change still needs Android visual verification.

## 0.1.10.3 — Bunny C# projectile lifecycle

The C# organization is now class-based, without importing tModLoader runtime code. Registration, content defaults and textures remain in the original Bunny ExampleMod JavaScript API. The NativeAOT DLL handles fast projectile AI.

The `BunnyCsSdk/BunnyProjectileBehavior.cs` file introduces a bounded 4096-slot state array and hooks:
- `OnSpawn(ref ProjectileContext, ref ProjectileMemory)`
- `AI(ref ProjectileContext, ref ProjectileMemory)`
- `OnKill(ref ProjectileMemory)`

The native host assigns a new generation when a tracked projectile slot changes identity and retires that generation at `Kill()` or `SetDefaults()`. The optional `bunny_mod_projectile_release(int type, int slot, int owner, int generation)` export releases managed state; ABI 1 / Host API 1 are unchanged, and older mods may omit the export.

`ProjectileMotion.AccelerateTowards` and `ProjectileMotion.ApplyDrag` provide bounded movement without new allocations per tick. The `ExampleCSharpMod` uses `BunnyProjectileRegistry` only for `NativeWaveBolt`; the previous normal and predictive homing behaviors are unchanged. A third `NativeWaveWand` is craftable with 10 wood at a workbench.

Editor checks:
```js
JSON.stringify({
  mods: bl.csharp.externalMods(),
  state: bl.csharp.projectileWriteStatus()
}, null, 2)
```

Expected: `projectileBindings: 3`, `automaticWrites` increasing, and new `automaticSpawns` / `automaticReleases` counters. Compare the two previously validated wands against `NativeWaveWand`. This build is experimental until tested on the Android device.

The per-projectile memory is intentionally constrained to 4096 slots and only has native write access to `velocity` and `rotation`. It is not a full PC tModLoader API or an IL2CPP DLL loader.

## 0.1.10.4 — NativeAOT NPC AI

The project keeps Native Mod ABI 1 and Host API 1. NPC types, textures, defaults, and summon item are registered with the original Bunny ExampleMod JavaScript classes. No tModLoader runtime is required.

An optional set of three exports is supported by external NativeAOT mods:

- `bunny_mod_bind_npc(int type, const char* name)`
- `bunny_mod_npc_tick(HostNpcState* state, float playerX, float playerY)`
- `bunny_mod_npc_release(int type, int slot, int generation)`

Only NPC types belonging to that exact package and explicitly accepted by the C# mod are dispatched. Only the local singleplayer world is supported in this prototype. After the original Terraria `NPC.AI()` completes, the host validates slot, generation, active state, type, life, and finite/clamped velocity before writing. Hook-based `CheckActive()` and `NPCLoot()` support lifecycle release. Native player-only snapshots skip all NPC and projectile array scanning.

The C# SDK includes `NpcContext`, `NpcMemory`, `BunnyNpcBehavior`, and `BunnyNpcRegistry`, with `OnSpawn`, `AI`, and `OnDespawn` methods. The new `NativePhaseSlime` selects phase 1 at more than half life and phase 2 at half life or below. The `NativePhaseSlimeBeacon` is reusable, crafted with 10 wood at a workbench, and only spawns one test NPC in singleplayer.

Test after installing the APK and importing ExampleCSharpMod 0.1.4:

```js
JSON.stringify({
  mods: bl.csharp.externalMods(),
  npc: bl.csharp.npcStatus(),
  projectiles: bl.csharp.projectileWriteStatus()
}, null, 2)
```

Expected: `npcBindings: 1`, `projectileBindings: 3`, `npc.installed: true`, and NPC calls/writes increasing while the slime moves. Reduce its health below 50% to check the movement phase. After it despawns or dies, check the release counter. Existing three projectile weapons should still work at stable FPS. Visual behavior, gameplay safety, and multiplayer remain pending Android validation.

## 0.1.10.5 — NPC combat patterns

The experimental Bunny C# mod keeps Native Mod ABI 1 and Host API 1. Existing `bunny_mod_npc_tick` implementations continue to work. An optional `bunny_mod_npc_combat_tick(HostNpcState*, float playerX, float playerY, HostNpcAttack*)` export emits a bounded attack command, not a raw IL2CPP call.

C# handles target direction, phase-based cooldown, attack count, damage and projectile velocity. The Bunny ExampleMod JavaScript API is responsible for the actual projectile spawn using the tested `Terraria.Projectile.NewProjectile` overload. The host validates each request (active local NPC, type, generation, singleplayer mode, damage, finite positions, and maximum projectile speed); a pending request can only be consumed once and expires after several AI updates. No GC-heavy allocations or scans are added to the C# host update path.

ExampleCSharpMod 0.1.5 adds `NativeSlimeOrb` (hostile projectile), fired from `NativePhaseSlime`: phase 1 single shot approximately every 140 ticks, phase 2 two-shot spread approximately every 70 ticks. The original 3 C# projectile weapons and movement AI remain unchanged. The slime summon is still reusable and is craftable with 10 wood at a workbench.

Run in the game editor:

```js
JSON.stringify({
  mods: bl.csharp.externalMods(),
  npc: bl.csharp.npcStatus(),
  projectiles: bl.csharp.projectileWriteStatus()
}, null, 2)
```

Expect `npcCombat: true`, `npcBindings: 1`, `projectileBindings: 3`, `attacksQueued > 0`, and `attacksConsumed > 0` after the slime attacks. `attacksRejected` should remain at zero. Keep at least one slime alive long enough to trigger the attacks, then reduce its health below 50% to compare single and double shots.

NPC combat remains singleplayer-only in this test. Native C# multiplayer requires a separate server-authoritative state and projectile synchronization design, so do not claim multiplayer support on this branch. The Android build and visual test must succeed before marking 0.1.10.5 stable.


## 0.1.10.5.1 — JavaScript-only NPC projectile source hotfix

The Android APK remains 0.1.10.5 and the NativeAOT `libmod.so` is unchanged. The `NativePhaseSlime.AI()` sample previously called `npc.GetSource_FromAI()`, a member missing in the Android Terraria.NPC bridge. The mod-only patch now uses the existing ExampleModBunny API `player.GetProjectileSource_Item(player.HeldItem)`, already demonstrated by `ExampleLaserHoldout.js`. Because this is a test of NativeAOT NPC combat and not a final NPC source API, the spawn source is temporarily attributed to the local player's held item in singleplayer. Later NPC-specific source support should be developed and tested separately.

Download ExampleCSharpMod 0.1.5.1 and replace 0.1.5 using the existing APK 0.1.10.5. Confirm that hostile orbs spawn and damage the player without the missing member error, and check `bl.csharp.npcStatus()` for `attacksQueued` and `attacksConsumed`.
