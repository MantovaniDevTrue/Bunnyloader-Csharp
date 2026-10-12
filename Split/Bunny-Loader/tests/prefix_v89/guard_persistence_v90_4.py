from pathlib import Path

source = Path("app/src/main/cpp/content/items/ModItemSave.cpp").read_text()
gameplay = Path("app/src/main/cpp/experiments/csharp/ManagedPrefixGameplay.cpp").read_text()
assert "std::map<Il2CppObject*, std::pair<int, int>> before" in source
assert source.index("std::map<Il2CppObject*, std::pair<int, int>> before") < source.index("g_origSave(fileData, m)")
assert "prefix == 0" in source and "before.find(item)" in source
assert "previous->second.first == type" in source
assert "void* arguments[1] = {&prefix}" in source
assert "api.runtime_invoke(r.applyPrefix, item, arguments, &error)" in source
assert "api.runtime_invoke(r.resetPrefix, item, nullptr, &error)" in source
assert "if (slotItem && slotType == type) {" in source
assert "if (restoreSavedPrefix(slotItem, s))" in source
assert "restore(target, type, s, &prefixRestored)" in source
assert "pending[placeKey(s.container, s.slot)] = {type, s.prefixKey}" in source
assert "prefixos de item C# repostos=%d pendentes=%d" in source
assert "void hookReforgeTooltip" in gameplay
assert "g_originalReforge(popup, item, method)" in gameplay
print("BUNNY_CSHARP_PREFIX_SAVE_RESTORE_V90_4=PASS")
