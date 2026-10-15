from pathlib import Path

root = Path(__file__).resolve().parents[1]
sdk = (root / "experiments/csharp/BunnyCsSdk/BunnyHooks.cs").read_text()
dispatcher = (root / "experiments/csharp/BunnyCsSdk/BunnyPlayerHurtDispatcher.cs").read_text()
runtime = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
header = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.h").read_text()
jni = (root / "app/src/main/cpp/menu/CheatButton.cpp").read_text()
bridge = (root / "tools/cheatbridge/bunny/CheatBridge.java").read_text()
editor = (root / "tools/cheatbridge/bunny/DevTools.java").read_text()

for name in ["BeforeCalls", "DodgeChecks", "SuccessfulDodges", "AfterCalls",
             "UnhookCalls", "CancelledByHook", "LastDamageBefore", "LastDamageAfter"]:
    assert name in sdk, name
assert "public static string GetHookSnapshot()" in dispatcher
assert 'g_api.findMethod(cls, "GetHookSnapshot", 0)' in runtime
assert 'std::chrono::milliseconds(650)' in runtime
assert "std::lock_guard<std::mutex> lock(g_hurtSnapshotMutex)" in runtime
assert "std::string managedHurtHookSnapshot()" in runtime
assert "std::string managedHurtHookSnapshot();" in header
assert "jni_csharpHookCounters" in jni
assert 'bl::csharp::managedHurtHookSnapshot()' in jni
assert '{"nCSharpHookCounters", "()Ljava/lang/String;"' in jni
assert "native String nCSharpHookCounters()" in bridge
assert 'hookCounters.setVisibility(csharpMode ? View.VISIBLE : View.GONE)' in editor
assert "now - lastHookCountersPoll < 850" in editor
assert "CheatBridge.nCSharpHookCounters()" in editor
assert "hookCounters.setText(current)" in editor
assert "if (!showing || !csharpMode) return;" in editor
assert "hookCounters.setVisibility(View.GONE);" in editor
assert "pullHookCounters();" in editor
assert "g_api.invoke(g_managedHurtSnapshotMethod" in runtime
assert 'g_hurtSnapshotText = "Hooks C#: aguardando evento de dano..."' in runtime
assert "mono_runtime_invoke" in runtime
assert "g_api.invoke" not in jni
print("BUNNY_CSHARP_EDITOR_HOOK_COUNTERS_BRIDGE=PASS")
