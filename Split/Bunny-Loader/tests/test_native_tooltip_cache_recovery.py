from pathlib import Path

root = Path(__file__).resolve().parents[1] / "app/src/main/cpp"
header = (root / "content/common/TypeTables.h").read_text()
tables = (root / "content/common/TypeTables.cpp").read_text()
items = (root / "content/items/ModItems.cpp").read_text()

assert "void checkPending(int size, Regrown onRegrown = nullptr)" in header
assert "void TypeTables::checkPending(int size, Regrown onRegrown)" in tables
assert "if (onRegrown) onRegrown(f, size);" in tables
assert "g_tables.checkPending(from, onTableRegrown);" in items
assert "void repairMissingModTooltips(int size)" in items
assert "repairMissingModTooltips(from);" in items
assert "if (entries[type]) continue;" in items
assert "if (repaired)" in items
assert "ItemTooltip indisponivel" in items
print("BUNNY_TOOLTIP_LATE_CACHE_RECOVERY=PASS")
