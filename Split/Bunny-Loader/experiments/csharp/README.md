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
