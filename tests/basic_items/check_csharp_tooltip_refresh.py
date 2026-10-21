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
assert 'content::setTableElement(table, type, empty)' in lookup
assert 'if (last == outer && checked >= size && outer->length >= static_cast<uintptr_t>(size))' in lookup
assert items.count('ensureArmorSetLookups(from);') == 1
assert items.index('ensureArmorSetLookups(to);') < items.index('registerSample(n.type, n.mod, n.name);')
assert 'for (const set of Array.from(ArmorSetBonuses.SetsContaining[item.type]))' in (Path(__file__).resolve().parents[2] / "app/src/main/cpp/script/js/mod/Loaders/VanillaTooltips.js").read_text()
print("CSHARP_NATIVE_ARMOR_LOOKUP_EMPTY_ROWS_GUARD=PASS")

native = (Path(__file__).resolve().parents[2] / "app/src/main/cpp/experiments/csharp/NativeBasicItems.cpp").read_text()
saved = (Path(__file__).resolve().parents[2] / "app/src/main/cpp/content/items/ModItemSave.cpp").read_text()
auto = (Path(__file__).resolve().parents[2] / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
install_slot = native[native.index("bool setBasicItemEquipSlot("):native.index("bool applyBasicItemToObject(")]
assert 'equipSlots[type][kind]={binding,slot};' in install_slot
assert 'instancesPending.store(true,std::memory_order_release);' in install_slot
assert install_slot.index('equipSlots[type][kind]={binding,slot};') < install_slot.index('instancesPending.store(true,std::memory_order_release);')
rehydrate = native[native.index("void refreshLiveBasicItems()"):native.index("bool checkBasicItemContract(")]
assert 'runtime::field<int32_t>(object,typeOffset)' in rehydrate
assert 'applyBasicItemToObject(object,type)' in rehydrate
assert 'const int savedPrefix=runtime::field<uint8_t>(object,prefixOffset);' in rehydrate
assert 'getStatic("player")' in rehydrate
assert 'fieldOffset(player,"Loadouts")' in rehydrate
assert 'findClass({"Terraria","EquipmentLoadout",{}' in rehydrate
assert 'fieldOffset(loadoutClass,"Armor")' in rehydrate
assert 'fieldOffset(loadoutClass,"Dye")' in rehydrate
load = saved[saved.index("Il2CppObject* hkLoadPlayer("):saved.index("bool rehydrateSavedManagedItem(", saved.index("Il2CppObject* hkLoadPlayer("))]
assert 'if (slotItem && slotType == type)' in load
assert 'if (restored > 0)' in load and 'bl::csharp::requestBasicItemInstanceRefresh();' in load
world = auto[auto.index("void noteAutoGameUpdate()"):auto.index("bool preWorldCSharpContentReady()")]
ready = world.index('g_worldSessionReady.store(true, std::memory_order_release);')
assert world.index('requestBasicItemInstanceRefresh("WorldLoaded");',ready)>ready
assert 'bool finishPreWorldCSharpPlayer() {\n    requestBasicItemInstanceRefresh("BeforeWorld");' in auto
print("CSHARP_SAVED_WINGS_AND_INACTIVE_LOADOUT_REHYDRATION_GUARD=PASS")

exports = (Path(__file__).resolve().parents[2] / "app/src/main/cpp/script/js/mod/Exports.js").read_text()
tooltip_loader = (Path(__file__).resolve().parents[2] / "app/src/main/cpp/script/js/mod/Loaders/TooltipLoader.js").read_text()
native_api = (Path(__file__).resolve().parents[2] / "app/src/main/cpp/script/api/CSharpPrototypeApi.cpp").read_text()
writer = (Path(__file__).resolve().parents[2] / "experiments/csharp/BunnySourceCompiler/BasicItemWriter.cs").read_text()
sample = (Path(__file__).resolve().parents[2] / "samples/ExampleModCSharp/Playable/content/Content/Items/ExampleTooltipItem.cs").read_text()
assert "typeof globalThis.bl?.csharp?.tooltipPlan === 'function'" in exports
assert "Hooks.Once('item.Tooltips', TooltipLoader.Install);" in exports
assert 'JS_SetPropertyStr(ctx, api, "tooltipPlan"' in native_api
assert "bl::csharp::basicItemTooltipPlan(type)" in native_api
assert "TooltipLoader.#ModifyManaged(item, lines);" in tooltip_loader
assert "TooltipLoader.#DrawManagedWave(line, def);" in tooltip_loader
assert "batch[TooltipLoader.#DRAW_STRING](font, ch," in tooltip_loader
assert "const draw = batch[TooltipLoader.#DRAW_STRING];" not in tooltip_loader
assert "rainbow=draw.RainbowWave" in writer
assert 'line.OverrideColor = "FFD75A"' in sample
print("CSHARP_TOOLTIP_PLAN_NATIVE_ACTIVATION_GUARD=PASS")
