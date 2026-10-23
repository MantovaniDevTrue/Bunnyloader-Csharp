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
