# Five Mobile loaders QA batch

The user selected PRs #133, #126, #121, #122 and #120 and explicitly authorized
one QA APK plus a test mod for this batch. This branch imports each pinned PR's
feature diff onto `ddcea5ce`; it does not merge those PRs or import their old
SDK binaries, workflows or unrelated commit ancestry. PR #122's Split paths
are mapped onto this base's root layout. Pins are in the adjacent JSON file.

Integration preserves the base's command, localization, dust, invasion, rarity,
world-entry and early compiler paths. It adds the missing Android SDK touch
sources, native-free world/hook classification, standalone update module
discovery/verification, and serialized Mono calls for touch callbacks. Gore
examples now use their manifest UID. Optional Gore requests return false
with a host diagnostic when the native visual ABI is absent, so an NPC death
callback does not lose invasion scoring in a host-only environment. Tests reference the net8 Android SDK,
not a different auto-glob net10 facade. The Gore test includes its audio helper.

Mobile review used the extracted 301720 ELF and metadata, not a PC Terraria
assembly: `dayRate` and `desiredWorldTilesUpdateRate` are `System.Int32`.
The time hook handles Int32 explicitly and retains fractional accumulators.
The WorldNetwork sender passes the 16-byte NetPacket payload to value-type
methods and arguments, keeps the objects rooted across IL2CPP invocations, and
checks error cleanup. Passing the boxed header would corrupt memory.
The workflow verifies the exact ELF SHA-256 and all eight instruction anchors
before it builds the APK. Static/instance checks protect the root, UI and time
hook entrypoints. The touch signatures follow Mobile GUIVirtualInputController.

Validation: `bash tools/five_loaders/check.sh`, the QA Roslyn/defaults/save
roundtrip probe, the full editable ExampleMod probe and the existing integrated
host regressions. The dedicated workflow then rebuilds the Android SDK/compiler,
verifies private runtime inputs, compiles ARM64, runs Android JVM unit tests,
checks the V3 signature, ZIP alignment, SDK/framework hashes and bundled game
data, and publishes only to the existing private builds repository.

The APK has its own `com.bunnyloader.fiveqa` package. It does not replace the
previous CSharp Test installation. Import the supplied FiveLoadersCSharp `.bl`
and follow its Portuguese `TESTE_NO_MOBILE.md`. Gameplay, FPS and real network
sessions remain device QA, never inferred from host test success. Gore hot
unload is not supported; native types live until process restart. WorldSave QA
uses local worlds, not cloud storage. StatInheritanceData supplies factors,
not the PC DamageClass runtime.

The global loader completion inventory is unchanged. This scoped, authorized
QA workflow does not declare other loaders finished or publish a stable release.
