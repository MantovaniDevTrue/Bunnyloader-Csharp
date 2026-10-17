# Screen FX Probe CSharp

Visual-only Bunny Loader source mod. It does not register fake items or require
QuickJS. Two ordinary C# classes inherit `BunnyScreenEffect`, and the native
host loads two GLSL ES 3.0 shaders from `content/Effects` before rendering.

**Experimental, not device-validated.** Requires the APK from this feature
branch, not the validated `.62` release.

Use `Playable/` to package a `.bmod` and import it in the Bunny Loader.
On entering a world, the color channels should visibly shift and the world
corners should be darker, while the mobile HUD stays unchanged.

For a useful A/B test, disable the mod, restart the game and compare the
same location. Then enable it alongside a JS effect: neither effect should
erase the other. Validate resolution/zoom, world exit, shader compile logs,
frame time, and stability on Android.

If `ShaderFile` is missing, points outside the mod, or contains an invalid
GLSL program, the mod must fail cleanly or the shader must report a controlled
compile failure. No placeholder fallback shader should silently replace it.
