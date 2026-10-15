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
