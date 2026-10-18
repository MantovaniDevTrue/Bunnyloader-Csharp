from pathlib import Path

root = Path(__file__).resolve().parents[1]
bridge = (root / "tools/cheatbridge/bunny/CheatBridge.java").read_text()
editor = (root / "tools/cheatbridge/bunny/DevTools.java").read_text()
native = (root / "app/src/main/cpp/menu/CheatButton.cpp").read_text()
config = (root / "app/src/main/cpp/core/Config.h").read_text()
sdk = (root / "experiments/csharp/BunnyCsSdk/BunnyUniversalHooks.cs").read_text()
runtime = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
source = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()

assert "public static native String nCSharpRuntimeStatus()" in bridge
assert '"nCSharpRuntimeStatus", "()Ljava/lang/String;"' in native
assert "managedRuntimeStatus()" in native
assert "bunny_universal_hook_stat(0)" in native
assert "bl::script::engine().ready()" in native
assert "config().disableQuickJs" in native
assert "bool disableQuickJs = false" in config
assert "nCSharpRuntimeStatus()" in editor
assert "sCSharpOutput = new SpannableStringBuilder()" in editor
assert "activeOutput()" in editor
assert "sCSharpOutputLines = 0" in editor
assert "if (csharpMode) {" in editor.split("void pullOutput()", 1)[1]
cs_output = editor.split("void pullOutput()", 1)[1].split("void refreshOutput()", 1)[0]
assert cs_output.index("nCSharpConsoleTake()") < cs_output.index("nConsoleTake()")
assert "if (csharpMode)" in cs_output and "} else {" in cs_output
assert "nCSharpConsoleTake()" not in cs_output.split("} else {", 1)[1]
assert 'output.setText(csharpMode ? sCSharpOutput : sOutput)' in editor
assert "Universal C# void()" in sdk
assert "prepareUniversalHookSdk" in runtime
assert "pumpUniversalHookInstallsOnGameThread" in source
print("CSHARP_UNIVERSAL_EDITOR_TELEMETRY=PASS")
