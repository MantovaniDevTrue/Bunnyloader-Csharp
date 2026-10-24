# QA.4 diagnostic: world-entry SIGABRT

QA.3 on Xiaomi Android 14 / Mobile 301720 completed Roslyn and registered
QaUpdates (15 native hooks), then exited with signal 6 around world entry.
No current tombstone, abort trace or native module maps were available. This
does not establish whether SystemUpdate, touch, Mono or another native path
caused the abort. QA.4 is an evidence build, not a claimed crash fix.

The authorized five-loader APK enables BUNNY_FIVE_LOADERS_QA. It arms the existing
SIGABRT probe before source initialization, rearms after Mono/source registration
and persists game module maps. It records lock-free stage markers and the active
managed hook slot; there is no per-frame file logging. Mono fatal/critical log
messages are forwarded to the normal C# diagnostics and session log. JS is not
needed to enable capture. Other APK variants leave this instrumentation off.

The signal handler now unblocks SIGABRT after restoring the prior handler before
re-raising it. Previously its immediate exit fallback could run before the blocked
fatal signal was delivered. A real child-process test checks captured evidence
and termination by SIGABRT rather than a successful/ordinary exit.

Stage values:
- 100: automatic game update entry.
- 101: command hook installation; 102: command replies.
- 103: universal hook installation.
- 104..113: touch observer, editor labels, static mappings, icons, icon frame
  hooks, quick drawing, pointer hook, layouts, visibility and activation.
- 200: touch controller entry; 201: controller arrays; 202: original controller
  update; 203: controller snapshot; 204: chat/player focus properties.
- 1000 + stage index: SystemUpdate synchronous dispatch (indices in
  UniversalSystemStages.cpp). `managed_slot_plus_one=0` means no active slot.
- `game_stage` is active scope state; `last_game_stage` is the last nonzero marker.

The production ARM64 bridge macros are now executed under Unicorn, checking
saved general registers, SP, flags, SIMD, continuation and clock-rate frame
mutation. IP0/x16 is the indirect continuation scratch register. This test does
not emulate the ShadowHook patch, Android, Mono GC or device gameplay, and did
not reproduce the user's abort. All five-loader host tests also pass.

Install QA.4 over QA.3, keep test mod 0.1.1, enter the same world once and export
the Bunny report after the failure. The report should now contain
native-game-abort-trace.txt and native-game-module-maps.txt for the new process.
No loader was disabled to make the test appear successful.
