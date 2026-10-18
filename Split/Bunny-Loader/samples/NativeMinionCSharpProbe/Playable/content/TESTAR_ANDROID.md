# Native Minion CSharp Probe 0.1.0

Standalone source-only .bmod for the Bunny C# MonoVM .90.4. No JavaScript and no tModLoader API.

1. Install .90.4 APK and enable only this probe and optionally Lights And Shadows CSharp.
2. Start a disposable test world or back up the main world.
3. Give yourself "Native C# Summon Staff" using the loader's native item browser.
4. Use it once: a 1-slot minion projectile should appear and the matching native minion buff should apply.
5. Move left and right, jump and explore to check native projectile ownership and managed follow AI. Spawn a normal enemy to observe contact damage.
6. Test 60 FPS, saving/re-entering the world, and disabling/re-enabling the mod.
7. Return the Bunny Loader report, especially CSharp startup and projectile AI/buff logs.

Native IL2CPP fields are checked for presence when the minion contract is used. An unsupported field rejects this mod rather than writing to an unknown offset.

The underlying .90.3.1 ExampleWorkbench recipe and existing Lights effects are intentionally left untouched. Do not claim full summon fidelity until device verification.
