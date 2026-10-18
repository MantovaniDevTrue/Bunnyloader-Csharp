# Bunny Loader C# .90.1 — prefix autoload count hotfix

Android .90 reported Timeout de 180 segundos compilando o mod C# for ExampleModCSharp 0.1.15 while SecondExampleModCSharp loaded normally. The completion predicate in AutoSourceRuntime counted items, projectiles, buffs, players, and NPCs, but omitted prefixes. The managed verifier returns 11 types for the sample (9 earlier classes plus 2 BunnyPrefix classes). AutoSourceRuntime expected 9 and kept polling for 180 seconds despite a completed worker.

The .90.1 predicate now includes mod.prefixes, with an explicit early failure if a finished worker reports an inconsistent descriptor count rather than misleading users with a timeout. Host regression tests cover 11 types for ExampleModCSharp, 2 for SecondExampleCSharp, wrong mod IDs and the previously failing 9-count case.

This does not prove all prefix gameplay hooks work on Android. The next on-device check must confirm both prefixes load, reforging, damage/defense, names, persistence and FPS. Previous historical SIGKILL/low-memory entries in the Android report do not establish a new .90 crash.

No JavaScript is introduced in C# mods; .86 remains preserved.
