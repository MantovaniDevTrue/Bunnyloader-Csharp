from pathlib import Path

root = Path(__file__).resolve().parents[1] / "app/src/main/cpp"
probe = (root / "boot/Probe.cpp").read_text()
loader = (root / "mods/ModLoader.cpp").read_text()
items = (root / "experiments/csharp/NativeBasicItems.cpp").read_text()

probe_load = probe.index("mods::loadAll(config().modsDir, config().enabledMods);")
probe_csharp = probe.index("csharp::startAutoSourceRuntime();")
assert probe_load < probe_csharp
assert "script::engine().init()" not in probe

registry_end = loader.index("// 2a passada:")
loader_csharp = loader.index("bl::csharp::loadCSharpSourceMod(", registry_end)
js_guard = loader.index("if (!script::engine().ready() && !script::engine().init())", loader_csharp)
js_load = loader.index("script::engine().loadMod(", js_guard)
assert registry_end < loader_csharp < js_guard < js_load
assert "if (!script::engine().ready()) return;" not in loader[registry_end:loader_csharp]
assert "registry()[i].enabled = false;" in loader[js_guard:js_load]
assert 'registry()[i].entry.compare(registry()[i].entry.size() - 3, 3, ".cs") == 0' in loader
assert 'void loadBuiltins()' in loader
assert 'script::engine().loadBuiltinMod(' in loader
assert "behaviorsPending" in items
assert "__blSyncCSharpItemBehaviors" not in items
assert "pumpNativeItemPoliciesOnGameThread();" in items
print("BUNNY_CSHARP_INDEPENDENT_MOD_LOADING=PASS")
