from pathlib import Path
root = Path(__file__).resolve().parents[1] / "app/src/main/cpp"
items = (root / "content/items/ModItems.cpp").read_text()
basic = (root / "experiments/csharp/NativeBasicItems.cpp").read_text()
header = (root / "experiments/csharp/NativeBasicItems.h").read_text()
assert "std::vector<int> g_pendingTooltipTypes;" in items
setter = items.split("void setModItemTooltip(", 1)[1].split("bool modItemsSettled()", 1)[0]
assert "g_pendingTooltipTypes.push_back(type);" in setter
assert "applyTooltip(" not in setter
assert "bool syncPendingTooltipsOnGameThread(int size)" in items
assert "applyTooltip(type, g_regs[index]);" in items
assert "if (syncPendingTooltipsOnGameThread(from))" in items
assert "bl::csharp::requestBasicItemInstanceRefresh();" in items
assert "ItemTooltip.Lines" in items
assert "tem texto mas 0 linhas" in items
assert "void requestBasicItemInstanceRefresh();" in header
assert "void requestBasicItemInstanceRefresh() {" in basic
print("BUNNY_TOOLTIP_GAME_THREAD_SYNC=PASS")
