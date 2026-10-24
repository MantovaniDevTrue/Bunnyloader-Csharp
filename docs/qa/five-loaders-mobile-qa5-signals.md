# QA.5 — native signal chaining with Unity/IL2CPP

Two QA.4 device runs (fresh source compilation and cache hit) reach world entry,
then stop. Both traces contain NATIVE_PROCESS_ATEXIT_BEGIN without an abort
snapshot. This is not sufficient to identify the original fault, or even to
exclude DSO finalization: atexit callbacks also run during library unloading.

A confirmed bootstrap defect was found: ManagedRuntime initialized Mono without
setting mono_set_signal_chaining. The APK's runtime is Mono 8.0.23 and exports
both policy setters (verified against libmonosgen-2.0.so from the exact APK).
In that version:

- mini-posix.c saves prior signal handlers only when mono_do_signal_chaining
  is enabled: https://github.com/dotnet/runtime/blob/v8.0.23/src/mono/mono/mini/mini-posix.c#L382
- With crash chaining disabled, an unrecognized native fault first gets passed
  to the saved handler: https://github.com/dotnet/runtime/blob/v8.0.23/src/mono/mono/mini/mini-runtime.c#L3892
- Mono's Android native crash fallback calls exit(-1), consistent with atexit
  evidence but not proof of the cause here: https://github.com/dotnet/runtime/blob/v8.0.23/src/mono/mono/mini/mini-posix.c#L903

Before any domain initialization, bootstrap now requires and sets signal
chaining=1, crash chaining=0. This gives Unity/IL2CPP's saved native handlers
first opportunity to process native signals, including faults native runtimes
may deliberately use. It preserves Mono's handling of managed JIT code. Missing
policy APIs reject initialization explicitly. This does not suppress genuine
native faults, nor does it prove that this policy defect caused these crashes.

The QA probe is installed after Mono saves Unity's handlers; this prevents
creating an abort-handler chain cycle. QA additionally records handled SIGSEGV
before forwarding to the original handler, intercepts exit while preserving
its code, and records atexit frame addresses plus gameplay/managed slot markers.
No pthread mutex interception is enabled. Mono stderr is forwarded into C# logs.
Module maps are compacted to one representative executable mapping per image;
IL2CPP's many mprotect regions no longer crowd other modules out of the report.

Validation:
- The real ManagedRuntime bootstrap is exercised against a controlled Mono ABI
  in all 48 signal-ownership modes; setters are checked before domain init.
- Missing setters cause no partial policy configuration.
- Child-process tests preserve fatal SIGABRT, forward a handled SIGSEGV exactly
  once, preserve exit(17), and verify atexit evidence and compact map headers.
- Five-loader contracts and executed ARM64 bridge regressions remain required.
These host tests do not reproduce Android/Unity/Mono signal behavior end to end.

Install versionCode 5 / 1.0.13-five-loaders-qa.5-signal-chain over QA.4, retain
fiveloaderscsharp 0.1.1 and test world entry once. If it closes again, the next
report should identify the preceding native signal or exit caller. If entry
works, continue the existing /lote, touch-control and save/load tests. Confirmation
of the world-entry fix and of loader behavior still requires device testing.
