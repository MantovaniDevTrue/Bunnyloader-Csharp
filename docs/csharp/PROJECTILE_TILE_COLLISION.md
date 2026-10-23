# Projectile tile collision: native deterministic policies

The reference `samples/ExampleMod/content/Content/Projectiles/ExampleSentry.js` returns `false` in `OnTileCollide` to keep its sentry alive. The JavaScript `ProjectileLoader.js` detects tile collisions inside `Projectile.HandleMovement(Vector2)`, postpones collision-related `Kill()`, and uses the collision-adjusted velocity.

Bunny C# exposes three fixed native policies through `BunnyProjectile.SetDefaults()`:

```csharp
TileCollide = true;
TileCollisionMode = BunnyProjectileTileCollisionMode.KeepAlive;
```

| Value | Native decision |
|---|---|
| `Vanilla = 0` | Original Terraria movement and collision behavior |
| `KeepAlive = 1` | Cancel collision-related death and retain Terraria's post-collision velocity |
| `Bounce = 2` | Cancel collision-related death and reverse the original velocity on collided axes |
| `Stop = 3` | Cancel collision-related death and set collided velocity axes to zero |

Only axes whose velocities differ between pre-movement and post-collision are affected. Uncollided axes retain the Terraria result; corner collisions affect both axes. Non-finite velocities, disabled `tileCollide`, and invalid modes do not cause native collision policy changes.

The synchronous decisions run on the game thread, with no MonoVM wait. Two selective hooks cover `Projectile.HandleMovement(Vector2)` and `Projectile.Kill()`. All other projectile types delegate to Terraria's original behavior. The current implementation supports singleplayer only and uses validated IL2CPP field/method signatures, with no hard-coded offsets.

The `PROJECTILE` metadata continues using the existing optional 19-field extension for all three modes. Previously supported rows are unchanged. The .NET metadata probe checks a real C# `BunnyProjectile` for every policy, and native tests cover unchanged trajectories, separate axes, corner impacts, disabled collisions, invalid modes, and non-finite inputs.

## C# notification after the decision

`BunnyProjectile.OnTileCollide(ref BunnyProjectileAiState state, float oldVelocityX, float oldVelocityY, float newVelocityX, float newVelocityY)` runs on the pinned MonoVM worker after the native response. It carries the velocity before movement and the final post-policy velocity. Generation, owner, slot, and finite-value validation reject stale or invalid events; a full queue drops a notification rather than blocking the game thread.

The callback has return type `void`. It cannot synchronously decide survival, change Terraria's collision decision, or immediately rewrite physics. The policy is selected in `SetDefaults()`, not dynamically per collision. These modes are reusable native behavior extensions, **not** a claim of full JavaScript `OnTileCollide` or `TileCollideStyle` parity. Tile style, arbitrary synchronous callback returns, multiplayer synchronization, live Android gameplay behavior, visual fidelity, and device FPS still require implementation or QA.

## Speed-sensitive bounce (21-field optional extension)

C# projectile classes can customize `Bounce` in `SetDefaults()`:

```csharp
TileCollisionMode = BunnyProjectileTileCollisionMode.Bounce;
BounceRestitution = 0.75f;
BounceMinimumSpeed = 3f;
```

`BounceRestitution` (0–2, default 1) multiplies the reversed velocity component only on collided axes. `BounceMinimumSpeed` (0–64 pixels/tick, default 0) uses the incoming velocity magnitude to select behavior at each collision: below the threshold, the host leaves the original Terraria collision/death behavior in place instead of preserving the projectile. This is a deterministic game-thread decision; no synchronous MonoVM call or periodic world scan is added.

Only mode `Bounce` accepts nondefault settings. The C# compiler validates both finite ranges and the native batch validates them again. An optional 21-field `PROJECTILE` record transmits nondefault bounce tuning. The original 8/19-field formats remain unchanged for other mods. The native tests cover restitution, thresholds and compatibility while .NET probes exercise the actual metadata serialization.

The threshold cannot force Terraria to kill a projectile whose original collision path would not kill it. The changes do not implement a custom arbitrary C# return value in the collision frame, and FPS/gameplay still require Android validation.

## Native descriptor-size compatibility fix

`parseProjectileTuning` previously rejected the already-shipped 19-field collision descriptor because it recognized only lengths 16, 17 and 18. The parser now accepts 16, 17, 18, 19 and 21, while rejecting 20 or other unsupported lengths. A host-native C++ smoke test validates the exact length matrix plus a rejected non-finite tuning value. These checks are necessary because compiling the Android library alone does not exercise the runtime source loader.

## Per-instance collision survival budget (22-field descriptor)

`BunnyProjectile.TileCollisionSurvivalLimit` is an optional `SetDefaults()` value from 0 to 64. Zero (default) means unlimited native policy applications. A positive limit means exactly that many eligible tile impacts can use `KeepAlive`, `Bounce`, or `Stop`; after the limit, the projectile uses Terraria's original collision behavior on subsequent impacts.

```csharp
TileCollisionMode = BunnyProjectileTileCollisionMode.Bounce;
BounceRestitution = 0.65f;
TileCollisionSurvivalLimit = 3;
```

The counter resides in the already-existing native slot record for the live projectile. It resets on first observation, Terraria pool reuse (`SetDefaults` notification), and world reset. Only valid, retained native collision events consume the budget; rejected/slow `BounceMinimumSpeed` impacts do not. If a limited projectile has no confirmed active generation/owner/type, it falls back to vanilla rather than inheriting a stale counter. No MonoVM call or world scan is required for the physics decision.

The optional 22-field descriptor carries the existing bounce parameters (or their defaults) followed by the positive survival limit. Legacy 8/12/13/14/16/17/18/19/21-field descriptors remain unchanged. The SDK/compiler, native decoder and native validator all verify bounds. Native budget tests cover exhaustion and reset, while the compiled .NET probe verifies the actual 22-field serialization. These checks are source/native contracts, **not** device-tested Terraria behavior.

## Native platform fall-through policy (23-field optional extension)

The original ExampleModBunny `ExampleSentry.js` uses `TileCollideStyle` to set `fallThrough.value = false` so the sentry rests on platforms. The C# subset now supports that specific platform decision without running MonoVM on the game thread:

```csharp
TileCollide = true;
TilePlatformCollision = BunnyProjectilePlatformCollision.SolidPlatforms;
```

`Vanilla = 0` preserves the existing Terraria calculation, `SolidPlatforms = 1` sets `decidesManualFallThrough=true` and `shouldFallThrough=false` for the scope of `Projectile.HandleMovement`, and `PassThroughPlatforms = 2` sets `shouldFallThrough=true`. The native hook restores both original fields immediately after the original movement call, including when a collision policy is also applied. This avoids modifying platform state outside the relevant movement. The two fields are verified by typed IL2CPP field contracts; failure prevents enabling the mod rather than guessing offsets.

Platform overrides also work with `TileCollisionMode=Vanilla` and do not consume collision survival counts. The source descriptor includes optional field 23 for the platform style and fills legacy policy defaults. Older descriptors are unchanged. The .NET probe covers both combinations while the native C++ smoke checks parser lengths.

**Scope:** this is the no-fall-through/fall-through subset of `TileCollideStyle`, not support for custom collision width/height, hitbox anchor, `return false` collision bypass, or arbitrary scripted logic; multiplayer/gameplay/FPS are not yet device validated.

The hook installer supports subsequent mods enabling platform controls even when an earlier mod already installed the shared collision hooks without platform fields. The later install request must resolve and validate the additional typed IL2CPP fields before activation; existing shared detours remain active and are not installed twice.

## Tile collision hitbox overrides (27-field descriptor)

`TileCollideStyle` in the ExampleModBunny JS loader can change the collision `width`, `height` and `resizeAnchor` returned by `Projectile.GetCollisionParams`. Bunny C# adds native fixed per-type settings:

```csharp
TileCollide = true;
// Only the tile collision box changes; rendering and NPC damage sizes stay.
TileCollisionBoxWidth = 18;
TileCollisionBoxHeight = 12;
// World pixels after resizing: right by 2.5px and up by 3px.
// These are not Terraria's normalized resizeAnchor components.
TileCollisionAnchorOffsetX = 2.5f;
TileCollisionAnchorOffsetY = -3f;
```

Each width/height is 0 to preserve Terraria's original value or 1–512 to override it. Each anchor offset must be finite and within -256 to 256 pixels. Offsets translate the resized tile collision rectangle in world pixels: positive X moves right and positive Y moves down. Terraria's `resizeAnchor` is a fraction, normally `(0.5, 0.5)`, and its consumer computes `Center - colSize * resizeAnchor`. The hook therefore subtracts `offsetPixels / effectiveCollisionSize` from each native anchor component after choosing width/height. Adding pixel offsets directly to the fraction would scale the translation by the box dimensions and reverse its direction. With an 18px width, `OffsetX=2.5` now moves right by 2.5px; the original implementation moved left by 45px. A zero size override uses the original dimension as the divisor; zero translation preserves the native anchor. The native `GetCollisionParams` hook only modifies output parameters inside the currently scoped managed projectile `HandleMovement`, after delegating to the original Terraria method. It does not alter the projectile render size or NPC damage hitbox, and has no MonoVM waiting or per-tile scans.

An optional 27-field `PROJECTILE` descriptor carries the four numbers following existing platform settings. Previous descriptor variants (including 23-field platform and 22-field limit) remain unchanged. Native validation and C# serialization tests cover this encoding. Since the original JS `TileCollideStyle` is arbitrary runtime logic, these bounded static parameters constitute partial, not complete, hook parity.

The `GetCollisionParams` detour has a no-override fast path for collision policies that use native movement without changing box dimensions. Because each invocation begins with Terraria's original output parameters, repeated calls during the same movement do not repeatedly accumulate anchor offsets.


## Geometry regression and callback execution boundary

`tile_collision_geometry_smoke.cpp` checks the rectangle produced by Terraria's
consumer formula rather than only checking helper output anchors. Its 1,600
cases cover positive/negative/fractional offsets, preserved and overridden
dimensions, 1px/512px bounds, noncentral vanilla anchors and repeated native
parameter reads. Non-finite rejected offsets preserve all output parameters.
The formula and default anchor were inspected in the Android build 301720 native
library; this inspection is not Android execution or a gameplay test.

An exact tModLoader `OnTileCollide` return value affects the collision that is
currently running. An event sent to the MonoVM worker cannot supply that return
without waiting. The current `void OnTileCollide` is explicitly a notification;
its `ref` state does not write physics back to Terraria. Similarly, a worker
response from a previous tick is not an exact dynamic `TileCollideStyle` decision
for the current impact. These contracts must remain distinct.

Before exposing synchronous dynamic signatures, the shared executor needs a
validated game-thread execution path, mod-state ownership shared with AI and
lifecycle callbacks, reentrancy rules, fault handling and native integration tests
covering the actual movement/kill order. This change does not invent an async
boolean result, add a game-thread wait or claim that a stale cached result closes
that gap. Dynamic synchronous callbacks, multiplayer, device physics and FPS
remain pending. The ExampleMod APK readiness gate stays unchanged.

## Deferred per-instance mode updates from OnTileCollide

In the C# `OnTileCollide(ref BunnyProjectileAiState state, oldX, oldY, newX, newY)` callback, mods may set `state.NextTileCollisionMode` to `KeepAlive`, `Bounce`, or `Stop`. Leaving it null does not change the native mode. The update changes the policy **only for subsequent impacts**, not the collision that triggered the callback. `Vanilla` is not supported as a deferred request yet; unsupported requests are rejected without modifying the native mode.

```csharp
public override void OnTileCollide(ref BunnyProjectileAiState state,
    float oldX,float oldY,float newX,float newY)
{
    state.NextTileCollisionMode = BunnyProjectileTileCollisionMode.Stop;
}
```

The managed worker returns one integer: 0=rejected, 1=delivered/no change, 2=KeepAlive, 3=Bounce, 4=Stop. The native worker stores a reply by projectile slot together with type/generation/owner/object identity; the gameplay thread reads it on the next `HandleMovement` only after an atomic ready flag and with a nonblocking `try_to_lock`. Stale replies are discarded on mismatch, world exit resets the mailbox, and `SetDefaults`/new projectile generation resets its effective mode. No synchronous MonoVM call is added to the physics thread. The impact limit, restitution, hitbox, and platform settings continue to apply to that instance according to the existing static defaults.

This does **not** implement the JavaScript `OnTileCollide` synchronous return value or let the callback prevent death in its own frame. The callback only executes for valid delivered events and a projectile must remain alive to observe the subsequent policy. Physical/gameplay behavior still requires Android QA.

The native smoke test also verifies rejection of deferred messages belonging to another projectile type, generation, owner or pooled object identity, as well as zero/invalid identifiers. The managed lifecycle probe confirms that an unsupported request for `Vanilla` does not become a native mode update and that omitting an update preserves the normal event acknowledgement.
