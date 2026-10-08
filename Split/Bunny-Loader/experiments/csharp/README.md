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
