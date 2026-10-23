# Projectile tile collision: native KeepAlive

The source JS `samples/ExampleMod/content/Content/Projectiles/ExampleSentry.js` overrides `OnTileCollide` to return `false`, preventing its destruction. The JS loader handles this in `Projectile.HandleMovement(Vector2)`, detecting changes to the original velocity, postponing collision-related `Kill()`, and moving the projectile along the collided velocity.

This first C# feature offers a native deterministic subset:

```csharp
public override void SetDefaults()
{
    TileCollide = true;
    TileCollisionMode = BunnyProjectileTileCollisionMode.KeepAlive;
}
```

`Vanilla` is the default. `KeepAlive` installs paired selective native hooks for `HandleMovement` and `Kill`. The collision response is made synchronously on the game thread without waiting on MonoVM; all other projectile types are forwarded to the unmodified original methods. Only collision-associated kills (with changed velocity during movement) are deferred. The existing projectile AI and event bridge remain unchanged. Install fails closed when Terraria signatures cannot be found.

This does **not** implement dynamic `OnTileCollide` override return values, `TileCollideStyle`, multiplayer synchronization, or per-collision C# event notification; those need additional native/managed contracts and device QA. Do not interpret this as full ExampleSentry parity or full ProjectileLoader compatibility. The policy is currently scoped to managed source projectile types, with validation before native defaults become published.

The compiler uses a 19-field extended `PROJECTILE` wire row only for non-Vanilla collision mode; existing rows and 18-field movement rows remain accepted unchanged. A zero contact-damage flag is valid on non-minion projectiles. The native decision is tested independently, but full Android/IL2CPP gameplay testing has not yet been performed.

The end-to-end metadata probe `TileCollisionMetadataProbe` compiles and executes `CompiledAssemblyProbe.Verify` against actual C# subclasses, confirming that Vanilla keeps the existing 8-field wire row and KeepAlive produces the 19-field extension. This test does not assert in-game native detouring.

## Asynchronous C# OnTileCollide notification

For native `KeepAlive` projectiles, the game thread detects the velocity delta during `Projectile.HandleMovement` and enqueues a non-blocking, session-scoped, generation-bound MonoVM event. `BunnyProjectile.OnTileCollide(ref BunnyProjectileAiState state, float oldVelocityX, float oldVelocityY, float newVelocityX, float newVelocityY)` executes on the pinned managed worker. The host rejects stale slot generations, killed projectiles, invalid vectors and a full queue. This callback is a **notification** whose return type is `void`. Mutating the supplied `state` does not currently override collision physics. Game-thread collision decisions continue using the declared native `KeepAlive` policy. This preserves game-thread performance and avoids waiting for MonoVM but is not full synchronous JS `OnTileCollide` parity.
