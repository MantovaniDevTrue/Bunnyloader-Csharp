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
assert "panel.addView(hookCounters, hl);" in editor
assert "inner.addView(hookCounters, hl);" not in editor
assert 'csharpMode = prefs(a).getBoolean("editorCsharpMode", true);' in editor
assert 'prefs(act).edit().putBoolean("editorCsharpMode", csharpMode).apply();' in editor
assert "hookCounters.setMinHeight(px(a, 44));" in editor
assert 'if (csharpMode) pullHookCounters();' in editor
assert editor.index("panel.addView(hookCounters, hl);") < editor.index("panel.addView(inner, ")
assert 'HookCounters=VisiblePathConnected' in jni
assert "pullHookCounters();" in editor
assert "g_api.invoke(g_managedHurtSnapshotMethod" in runtime
assert 'g_hurtSnapshotText = "Hooks C#: aguardando evento de dano..."' in runtime
assert "mono_runtime_invoke" in runtime
assert "g_api.invoke" not in jni
print("BUNNY_CSHARP_EDITOR_HOOK_COUNTERS_BRIDGE=PASS")

# C# help must not inherit the JS thread/global/let/const instructions.
help_method = editor.split("void updateEditorHelp()", 1)[1].split("void switchEditorLanguage()", 1)[0]
csharp_help, js_help = help_method.split("?", 1)[1].split(": tr(", 1)
assert "Print(1 + 2);" in csharp_help and "arquivo .cs" in csharp_help
for word in ("let/const", "globalThis", "bl.log", "thread do jogo"):
    assert word not in csharp_help, word
assert "globalThis" in js_help and "bl.log" in js_help
assert "appendEntry('h'" not in editor
assert editor.count("updateEditorHelp();") == 2
print("BUNNY_EDITOR_LANGUAGE_HELP_AND_SHARED_LOG_SEPARATION=PASS")
