from pathlib import Path
src = (Path(__file__).resolve().parents[1] / "app/src/main/cpp/content/items/ModItems.cpp").read_text()
start = src.index("void ensureArmorSetLookups(int size) {")
end = src.index("} // namespace",start)
part = src[start:end]

assert 'static FieldInfo* table = nullptr;' in part
assert '"SetsContaining"' in part
assert 'if (last == outer && checked >= size && outer->length >= static_cast<uintptr_t>(size))' in part
assert 'if (outer->length < static_cast<uintptr_t>(size))' in part
assert 'TypeTables::resizedCopy(outer, static_cast<uintptr_t>(size))' in part
assert 'a.field_static_set_value(table, grown)' in part
assert 'outer = readStatic(table);' in part
assert 'if (rows[type]) continue;' in part
assert 'content::setTableElement(table, type, empty)' in part
assert 'last = outer;' in part
assert 'checked = size;' in part
assert 'ArmorSetBonuses.SetsContaining ampliada de %zu para %d' in part
assert 'installModTooltipProbe' not in src
assert 'hkDrawModTooltip' not in src
assert 'tooltip native probe' not in src
assert src.count("ensureArmorSetLookups(from)") == 1
assert src.count("ensureArmorSetLookups(to)") == 1
print("BUNNY_ARMOR_TABLE_RESIZE_AND_HOT_PATH=PASS")
