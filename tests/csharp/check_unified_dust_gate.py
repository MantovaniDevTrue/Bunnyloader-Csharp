from pathlib import Path

root = Path(__file__).resolve().parents[2]
gradle = (root / "app/build.gradle.kts").read_text()
cmake = (root / "app/src/main/cpp/CMakeLists.txt").read_text()
dust = (root / "app/src/main/cpp/experiments/csharp/ManagedDustContract.h").read_text()
auto = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
source = (root / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text()
worker = (root / "app/src/main/cpp/experiments/csharp/ManagedRuntime.cpp").read_text()
checks = {
    "QA flag derives from unified app variant":
        'val dustPreview = (project.findProperty("bl.dustPreview") as String?)?.toBoolean() ?: unifiedCSharpQa' in gradle,
    "Native cmake flag enabled only when requested":
        'if (dustPreview) arguments += "-DBUNNY_CSHARP_DUST_PREVIEW=ON"' in gradle,
    "Source-only preview defaults off for ordinary Bunny":
        'option(BUNNY_CSHARP_DUST_PREVIEW "Enable experimental managed Dust callbacks in source-only preview" OFF)' in cmake,
    "Native gate matches CMake definition":
        "BUNNY_CSHARP_DUST_PREVIEW == 1" in dust,
    "Reject unconnected Mono before callback registration":
        "if (hooks != 0 && !managedDustCallbacksConnected())" in source,
    "Managed Dust binding planned after successful defaults":
        "if (mod.dusts > 0)" in auto and "selectManagedDustBindings(mod)" in auto,
    "Managed binding scheduled to MonoVM":
        "registerManagedDustSourceMod(mod.id, assembly, dustTypes)" in auto,
    "MonoVM invokes actual CSharp Dust callbacks":
        'g_api.findMethod(cls, "Invoke", 3)' in worker,
    "World reset still invalidates Dust sessions":
        "g_dustMailbox.resetWorld();" in worker,
}
for name, passed in checks.items():
    if not passed:
        raise SystemExit("DUST_UNIFIED_QA_GATE_FAILED: " + name)
print("DUST_UNIFIED_QA_GATE_PASS")
