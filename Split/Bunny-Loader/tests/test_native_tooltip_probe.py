from pathlib import Path
source = (Path(__file__).resolve().parents[1] / "app/src/main/cpp/content/items/ModItems.cpp").read_text()
assert "void installModTooltipProbe()" in source
assert '"MouseText_DrawItemTooltip_GetLinesInfo", 12' in source
assert "a.method_is_instance(target)" in source
assert "hook::install(target, hkDrawModTooltip, &g_origDrawModTooltip)" in source
assert "g_origDrawModTooltip(item, yoyo, research, materials, oldKB, count," in source
assert "const int type = field<int32_t>(item, r.offType);" in source
assert "static bool observed[384] = {};" in source
assert 'a.class_get_method_from_name(itemClass, "get_ToolTip", 0)' in source
assert '"SetsContaining"' in source
assert "BL_INFO(\"tooltip native probe: type=%d" in source
assert "installModTooltipProbe();" in source
print("BUNNY_NATIVE_TOOLTIP_PROBE=PASS")
