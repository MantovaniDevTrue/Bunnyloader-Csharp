from pathlib import Path

source = (Path(__file__).resolve().parents[2] / "app/src/main/cpp/experiments/csharp/NativeBasicItems.cpp").read_text()
start = source.index("void refreshLiveBasicItems()")
end = source.index("bool checkBasicItemContract(", start)
refresh = source[start:end]
assert 'getStatic("HoverItem")' in refresh
assert 'getStatic("tooltipPrefixComparisonItem")' in refresh
assert 'ContentSamples' in refresh and 'ItemsByType' in refresh
assert 'class_get_method_from_name(api.object_get_class(dictionary),"get_Item",1)' in refresh
assert 'runtime::field<int32_t>(sample,typeOffset)==type' in refresh
assert 'refresh(sample);' in refresh
assert 'runtime::finishModItem(object,type)' in refresh
assert 'std::lock_guard lock(mutex)' not in refresh
assert 'if(!entry.tooltip.empty()||!entry.localizedTooltips.empty())' in source
items = (Path(__file__).resolve().parents[2] / "app/src/main/cpp/content/items/ModItems.cpp").read_text()
start = items.index("void applyTooltip(int type, const Entry& reg)")
end = items.index("void applyName(", start)
native_tooltip = items[start:end]
assert 'reg.def.tooltips.empty() ||' not in native_tooltip
assert 'content::textForCulture(reg.def.tooltips, "")' in native_tooltip
assert 'content::makeLocalizedText(key, text)' in native_tooltip
assert 'r.tooltipFromKey' in native_tooltip
assert 'content::setTableElement(r.tooltipCache, type, tip)' in native_tooltip
assert 'if (f == r.tooltipCache) applyTooltip(type, g_regs[i]);' in items
assert items.index('applyTooltip(type, reg);') < items.index('registerSample(n.type, n.mod, n.name);')
print("CSHARP_NATIVE_TOOLTIP_SAMPLE_REFRESH_GUARD=PASS")
print("CSHARP_NATIVE_EMPTY_TOOLTIP_CACHE_GUARD=PASS")
start = items.index("void ensureArmorSetLookups(int size)")
end = items.index("void tickModItems()", start)
lookup = items[start:end]
assert 'il2cpp::findField(cls, "SetsContaining")' in lookup
assert 'a.class_get_element_class(rowsClass)' in lookup
assert 'a.array_new(entryClass, 0)' in lookup
assert 'if (rows[type]) continue;' in lookup
assert 'content::setTableElement(field, type, empty)' in lookup
assert 'if (last == outer && checked >= size) return;' in lookup
assert items.count('ensureArmorSetLookups(from);') == 1
assert items.index('ensureArmorSetLookups(to);') < items.index('registerSample(n.type, n.mod, n.name);')
assert 'for (const set of Array.from(ArmorSetBonuses.SetsContaining[item.type]))' in (Path(__file__).resolve().parents[2] / "app/src/main/cpp/script/js/mod/Loaders/VanillaTooltips.js").read_text()
print("CSHARP_NATIVE_ARMOR_LOOKUP_EMPTY_ROWS_GUARD=PASS")
