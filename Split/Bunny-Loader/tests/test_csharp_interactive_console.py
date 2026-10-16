from pathlib import Path

root = Path(__file__).resolve().parents[1]
native = root / "app/src/main/cpp"
ui = (root / "tools/cheatbridge/bunny/DevTools.java").read_text()
bridge = (root / "tools/cheatbridge/bunny/CheatBridge.java").read_text()
jni = (native / "menu/CheatButton.cpp").read_text()
runtime = (native / "experiments/csharp/AutoSourceRuntime.cpp").read_text()
mono = (native / "experiments/csharp/ManagedRuntime.cpp").read_text()
probe = (root / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text()

assert 'nCSharpConsoleRun' in ui and 'nCSharpConsoleTake' in ui
assert 'new File(new File(work, "interactive"), id)' in ui
assert 'ConsoleEntry.cs' in ui
assert 'namespace BunnyLoader.Interactive' in ui
assert 'Print(1 + 2);' in ui
assert 'runCSharp(); return;' in ui
assert 'saveCSharpSource()' not in ui
assert 'chooseCSharpSource()' not in ui
assert 'nCSharpConsoleRun(String id, String sourceRoot, String workDir)' in bridge
assert 'nCSharpConsoleTake()' in bridge
assert 'jni_csharpConsoleRun' in jni and 'jni_csharpConsoleTake' in jni
assert 'bl::csharp::submitInteractiveConsoleSource' in jni
assert 'g_consoleRunCount>=24' in runtime
assert 'requestIsolatedMonoCompile(root,id,*error)' in runtime
assert 'pumpInteractiveConsoleOnGameThread();' in runtime
assert 'invokeInteractiveCSharp(dir+"/compiled/"+id+".dll"' in runtime
assert 'g_consolePending.active=false;' in runtime
assert 'mono_runtime_invoke' in mono
assert 'g_api.findClass(image,"BunnyLoader.Interactive","ConsoleEntry")' in mono
assert 'g_api.findMethod(klass,"Run",0)' in mono
assert 'mono_string_to_utf8' in mono
assert 'mono_object_to_string' in mono
assert 'BunnyLoader.Interactive.ConsoleEntry' in probe
assert 'INTERACTIVE_CONSOLE_VERIFIED classes=1' in probe
assert 'throw new InvalidOperationException("CSharp console assembly contract invalid")' in probe
print("BUNNY_CSHARP_INTERACTIVE_CONSOLE_JS_INDEPENDENT=PASS")
