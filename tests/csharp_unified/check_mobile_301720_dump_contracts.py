import json
from pathlib import Path

base = Path(__file__).resolve().parents[2]
facts = json.loads((base / "tools/csharp_unified/contracts/terraria-mobile-301720.json").read_text())
def fact(cls, key, value):
    assert value in facts["facts"][cls][key], (cls, key, value)

content = (base / "app/src/main/cpp/experiments/csharp/NativeContent.cpp").read_text()
item = (base / "app/src/main/cpp/experiments/csharp/NativeItemPolicies.cpp").read_text()
tile = (base / "app/src/main/cpp/content/tiles/ModTiles.cpp").read_text()
angler = (base / "app/src/main/cpp/experiments/csharp/NativeAnglerQuestGameplay.cpp").read_text()
array = (base / "app/src/main/cpp/il2cpp/FieldContract.h").read_text()

for name, value in facts["facts"]["Terraria.ID.DustID"]["constant_short"].items():
    assert f'!std::strcmp(name,"{name}")?{value}' in content
for group, expected, actual in (
    ("Head", "DrawHead", "HidesHead"),
    ("Body", "HidesArms", "MissingHand")
):
    cls = f"Terraria.ID.ArmorIDs.{group}.Sets"
    fact(cls, "absent", expected)
    fact(cls, "present", actual)
    assert f'nativeFlag="{actual}"' in item
for group, missing in (
    ("Body", "HidesTopSkin"), ("Legs", "HidesBottomSkin")
):
    fact(f"Terraria.ID.ArmorIDs.{group}.Sets", "absent", missing)
assert "Result=MobileDefault" in item
assert "Result=MobileFallback" in item
assert "writeOwnedStaticCell(entry.source.type" in item
assert "nativeValue=!value" in item
assert "nativeArrayContractIssue" in array
assert array.index('nativeManagedPointerPlausible(array)') < array.index('a.class_get_rank(array->obj.klass)')
assert "CoordinateHeightsNativeTypeMismatch Rows=%d Length=%zu Detail=%s" in tile
assert 'nativeArrayContractIssue(heights,"Int32"' in tile
assert "QuestItemArrayTypeMismatch Detail=" in angler
assert 'nativeArrayContractIssue(original,"Int32"' in angler
assert facts["metadata_sha256"] == "4c904c49d168913d28ef547c4f461e539d926d2ed27c0253ed53a2954e9585f7"
print("TERRARIA_MOBILE_301720_DUMP_CONTRACTS=PASS")
