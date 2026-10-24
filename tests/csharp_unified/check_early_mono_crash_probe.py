from pathlib import Path

root = Path(__file__).resolve().parents[2]
auto = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
runtime = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
trace = (root / "app/src/main/cpp/experiments/csharp/NativeAbortProbe.cpp").read_text()

early = auto.index('installNativeAbortProbe(work, "native-game-abort-trace.txt")')
assert auto.count('installNativeAbortProbe(work, "native-game-abort-trace.txt")') == 1
assert auto.index('status.applied = true;') < early < auto.index('registerUniversalHookMod(mod.id, assembly)')
assert early < auto.index('requestManagedWorldOnMono(request, &reply, 1500)')
assert 'managedRuntimeStatus().methodExecuted &&' in auto[early-120:early]
assert 'rearmNativeAbortProbe("source-registration-complete")' in auto

for stage in ("210", "211", "220", "221", "310", "311", "410", "411"):
    assert stage in runtime, stage
assert 'setNativeMonoWorkerTid(g_pinnedTid)' in runtime
assert 'writeUniversalRegistrationStatus(next.id, "registering"' in runtime
for symbol in ("segv_pc=", "segv_lr=", "segv_mono_worker_tid=", "segv_mono_worker_stage="):
    assert symbol in trace, symbol

print("CSHARP_QA_EARLY_NATIVE_CRASH_CAPTURE_OK")
