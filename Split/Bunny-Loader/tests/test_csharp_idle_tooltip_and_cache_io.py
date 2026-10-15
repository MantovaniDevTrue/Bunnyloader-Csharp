from pathlib import Path

root = Path(__file__).resolve().parents[1] / "app/src/main/cpp"
items = (root / "content/items/ModItems.cpp").read_text()
source = (root / "experiments/csharp/AutoSourceRuntime.cpp").read_text()
sync = items.split("bool syncPendingTooltipsOnGameThread(int size)", 1)[1].split("void repairMissingModTooltips", 1)[0]
setter = items.split("void setModItemTooltip(", 1)[1].split("bool modItemsSettled()", 1)[0]
armor = items.split("void ensureArmorSetLookups(int size)", 1)[1].split("} // namespace", 1)[0]
reader = source.split("std::string readText(", 1)[1].split("bool writeText(", 1)[0]

assert "std::atomic<bool> g_pendingTooltipSync{false};" in items
assert sync.index("if (!g_pendingTooltipSync.load(std::memory_order_acquire))") < sync.index("const Refs& r = refs();")
assert "g_pendingTooltipSync.store(false, std::memory_order_release);" in sync
assert "g_pendingTooltipSync.store(true, std::memory_order_release);" in setter
assert "g_pendingTooltipTypes.push_back(type);" in setter
assert "applyTooltip(type, g_regs[index]);" in sync
assert "TypeTables::resizedCopy(outer, static_cast<uintptr_t>(size))" in armor
assert "if (last == outer && checked >= size" in armor
assert "ensureArmorSetLookups(to);" in items
assert "ensureArmorSetLookups(from);" in items
assert "std::ios::binary | std::ios::ate" in reader
assert "file.tellg()" in reader
assert "static_cast<uintmax_t>(length) > limit" in reader
assert "std::string result(static_cast<size_t>(length), '\\0');" in reader
assert "file.read(result.data(), length)" in reader
assert "file.peek() != std::char_traits<char>::eof()" in reader
assert "result(limit + 1" not in reader
print("BUNNY_CSHARP_IDLE_TOOLTIP_CACHE_IO=PASS")
