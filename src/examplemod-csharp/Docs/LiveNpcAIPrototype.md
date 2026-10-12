# Bunny Loader C# — NPC.AI dynamic callback (v0.1.10.16.46)

This is the **first actual Terraria NPC.AI hook that invokes C# from an independent source mod in the game's MonoVM process**. It is a strictly opt-in experimental stage. It does not yet represent a complete ModNPC implementation or JavaScript parity.

The .45 Android test proved persistent, live, managed source execution. This .46 build extends that mechanism to the already existing Bunny Loader `NpcAiHost.cpp` hook.

## Scope and boundaries

- Only the registered `ExampleSlimeNPC` of one explicitly selected C# mod is eligible.
- `NPC.AI` calls the original Terraria AI first. It then invokes `BunnyLoader.Source.BunnyLiveNpc.AI(int playerDeltaX, int npcLife)` inside the game's MonoVM.
- Inputs are read from the **current** Terraria NPC and player, not a precomputed table. The returned integer represents bounded horizontal velocity in tenths of a pixel per game update.
- The native hook blends that horizontal velocity with the vanilla result, preserving the original vertical movement/jumping and animation.
- The method is called on the game's main update thread, not in the separate Roslyn compiler worker. The mod DLL stays loaded and the `MonoMethod` is cached.
- The compiler worker remains separate, compiles C# automatically and produces a separate `compiled/<uid>.dll` for each mod.
- Opt-in only; at most 240 actual NPC hook calls per session. Automatically disables on three errors or a callback taking over 20 ms after warmup.
- No automatic global managed NPC AI, no support for `PreAI`/`FindFrame`, multiplayer, town NPCs, bosses, arbitrary C# mod APIs or hot-unloading yet.

## Controlled device test

Install APK `v0.1.10.16.46` and ExampleModCSharp `v0.1.6`. Test singleplayer, with world backup.

1. Check `bl.csharp.sourceAutoStatus()` is **ready** for the C# mod and `bl.csharp.sourceMods()` contains `ExampleSlimeNPC`.
2. Confirm game MonoVM initialized. The `.45` process state does not persist across application restarts: run `bl.csharp.managedProbe()`, wait until worker verified, then run `bl.csharp.managedGameProbe("CONFIRM_IN_PROCESS")`.
3. Initialize the C# mod in the game process with:

```js
JSON.stringify(bl.csharp.sourceLiveTick("68c3115f-62f4-49f2-a56b-388256781d5f", 5, 10), null, 2)
```

4. Enable the native NPC hook explicitly:

```js
JSON.stringify(bl.csharp.sourceLiveNpcEnable(
  "68c3115f-62f4-49f2-a56b-388256781d5f",
  "CONFIRM_LIVE_NPC_AI"
), null, 2)
```

5. Spawn the ExampleSlimeNPC (type available via `bl.csharp.sourceMods()[0].registeredTypes.ExampleSlimeNPC`; do not assume a fixed ID) and move left/right around it.
6. Read runtime counters:

```js
JSON.stringify({
    npc: bl.csharp.sourceLiveNpcStatus(),
    managed: bl.csharp.sourceLiveStatus("68c3115f-62f4-49f2-a56b-388256781d5f")
}, null, 2)
```

Look for `npc.installed=true`, `npc.events>0`, `npc.writes>0`, `npc.errors=0`, `managed.npcCallbacks>0`. The NPC should change horizontal direction in response to player movement while retaining its normal jumping.

A disabled `npc.enabled=false` with `phase=completed-limit` and `limitReached=true` is expected after the 240-call safety cap.

To stop at any time:

```js
bl.csharp.sourceLiveNpcPause()
```

If performance drops, the game closes or `maxCallbackMs` spikes, stop and send the relevant diagnostic screenshot/log. Do not automatically enable this hook at startup in production mods.

## What is actually proven

The build-time .NET smoke verifies `BunnyLiveNpc.AI` handles right, left, low-life and close-distance cases, and preserves C# state. Only device testing can verify live Terraria NPC hooks, stable MonoVM/game-thread operation and sustained framerate.

The complete target is a C# class lifecycle equivalent to what ExampleModBunny actually supports, with live `SetDefaults`, `AI`, `HitEffect`, drops, spawn conditions, sound, draw, networking and mod dependency resolution. This prototype is the first bridge, not a full API.
