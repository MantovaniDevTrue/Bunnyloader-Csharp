from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / "app/src/main/cpp/content/items/ModItems.cpp").read_text()
examples = (root / "tools/tests/hooks/content/main.js").read_text()

assert 'ItemTooltip GetTooltip(int itemId)' in examples
assert 'ItemTooltip GetTooltip(int itemId)' in source
assert "void hookItemTooltips()" in source
assert "g_origGetItemTooltip = nullptr;" in source
assert "if (isModItem(id))" in source
assert "readStatic(r.tooltipCache)" in source
assert "if (Il2CppObject* tip = entries[id]) return tip;" in source
assert "return g_origGetItemTooltip(id, m);" in source
assert "hookItemTooltips();" in source
assert "void repairMissingModTooltips(int size)" in source
assert "g_tables.checkPending(from, onTableRegrown)" in source
assert re.search(r"hookItemNames\(\);\s*hookItemTooltips\(\);", source)
print("BUNNY_NATIVE_MOD_TOOLTIP_LOOKUP=PASS")
