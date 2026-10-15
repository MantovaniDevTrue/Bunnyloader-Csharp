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
print("CSHARP_NATIVE_TOOLTIP_SAMPLE_REFRESH_GUARD=PASS")
