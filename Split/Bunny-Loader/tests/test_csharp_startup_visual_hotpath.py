from pathlib import Path
base = Path(__file__).resolve().parents[1] / "app/src/main/cpp/experiments/csharp"
runtime = (base / "ManagedRuntime.cpp").read_text()
startup = (base / "AutoSourceRuntime.cpp").read_text()
assert "static void* cachedDomain = nullptr;" in runtime
assert "if (cachedDomain != g_domain)" in runtime
assert "cachedMethod = method;" in runtime
assert "api.takeWireMethod = cachedMethod;" in runtime
assert "if (!cachedMethod && !validAssembly(assemblyPath)) return 0;" in runtime
assert "static const std::string sdk = folderOfCheckpoint()" in startup
run = startup.split("void run() {", 1)[1]
assert run.index("const auto scanStarted") < run.index('note(&status, "waiting-content");')
assert run.index("readCache(cacheFile, mod.id, key, assembly,") < run.index('note(&status, "waiting-content");')
assert "status.cached = !runtimeIdentity.empty() &&" in run
assert "if(!runtimeChecked)" in run
assert "const auto cacheMs = fingerprintMs + elapsedMilliseconds(readStarted);" in run
assert "const auto tablesMs = elapsedMilliseconds(tablesStarted);" in run
print("BUNNY_CSHARP_STARTUP_VISUAL_HOTPATH=PASS")
