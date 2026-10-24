# Five loaders QA.3: Mobile controls, chat and C# diagnostics

Device QA.2 confirmed Gore rendering and projectile callbacks. It did not
validate touch controls or chat: the first touch observer required a static
`Main.gameMenu` field that does not exist on Mobile 301720. The command bridge
required the same field plus a four-byte `Color` constructor absent from this
build. Compilation and signature comparison against the JS reference missed
these Mobile differences.

The touch observer and chat now resolve the static `Main.get_gameMenu()`
property, invoke it on the game thread, and treat invocation failures as menu
state. Touch focus resolves `get_drawingPlayerChat()` and `get_myPlayer()`;
`Main.player` remains an actual static array. Chat selects the exact four-Int32
Color constructor and passes unboxed value-type `this` to runtime_invoke.
The installed-hook messages identify these Mobile bindings in C# diagnostics.

Native C# logs now enter a bounded 256-entry queue consumed by the C# editor,
including messages produced before the editor opens. They no longer go through
the generic JS console subscriber. File/logcat logging and accumulated errors
keep their existing behavior.

Validation: tools/five_loaders/check.sh, including production chat color ABI,
menu property success/failure, C# diagnostic routing/bounds, and all five loader
contracts. Android ARM64/signing are validated by the authorized QA workflow.
Gameplay, touch profile editing/persistence and command replies require another
device run; no claim of complete device parity follows from these tests.

QA APK versionCode 3 / 1.0.13-five-loaders-qa.3 updates com.bunnyloader.fiveqa.
The existing fiveloaderscsharp 0.1.1 package remains compatible. Retest `/lote`,
assign TestarLote in the Terraria controls editor, press/hold/double-tap, then
check `/lote` counters. Open the C# editor to view loader diagnostics.
