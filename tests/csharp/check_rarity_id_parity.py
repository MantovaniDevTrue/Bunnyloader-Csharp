from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
js = (root / "app/src/main/cpp/script/js/ModHelpers.js").read_text()
header = (root / "app/src/main/cpp/experiments/csharp/BunnyItemRarityId.h").read_text()
native = (root / "app/src/main/cpp/experiments/csharp/NativeDefaults.h").read_text()
match = re.search(r"const ItemRarityID\s*=\s*Object\.freeze\(\{(.*?)\}\);", js, re.S)
if not match:
    raise SystemExit("ItemRarityID missing in Bunny ExampleMod JS reference")
js_values = dict((key, int(value)) for key, value in re.findall(r"\b([A-Za-z][A-Za-z0-9]*)\s*:\s*(-?\d+)", match.group(1)) if key != "Count")
native_values = dict((key, int(value)) for key, value in re.findall(r'\{"([A-Za-z][A-Za-z0-9]*)",\s*(-?\d+)\}', header))
if not js_values or native_values != js_values:
    raise SystemExit(f"Rarity map mismatch: JS={js_values} CSharpNative={native_values}")
if not ("family == \"ItemRarityID\"" in native and "resolveBunnyItemRarity(parts[3], &value)" in native):
    raise SystemExit("NativeDefaults must resolve Bunny-provided rarity IDs without requiring Terraria.ID.ItemRarityID")
if '"UnsupportedBunnyRarityName"' in native:
    raise SystemExit("Stale rarity mapping fallback")
print(f"BUNNY_RARITY_ID_PARITY_PASS names={len(js_values)} Cyan={js_values['Cyan']}")
