from pathlib import Path

source = Path("app/src/main/cpp/experiments/csharp/NativeContent.cpp").read_text()
header = Path("app/src/main/cpp/experiments/csharp/NativeContent.h").read_text()
policy = Path("app/src/main/cpp/experiments/csharp/NativeItemPolicies.cpp").read_text()
original = Path("samples/ExampleMod/content/Common/Players/ExampleCostumePlayer.js").read_text()
original_item = Path("samples/ExampleMod/content/Content/Items/Armor/ExampleCostume.js").read_text()
original_buff = Path("samples/ExampleMod/content/Content/Buffs/Blocky.js").read_text()

assert "CostumePlayerHooksUnavailable Detail=" in policy
assert "nativeCostumeRuntimeDiagnostic()" in policy
assert "const char* nativeCostumeRuntimeDiagnostic();" in header
assert "g_costumeReady=addBuffReady&&removeBuffReady&&hideReady&&identityReady&&fieldsReady;" in source
for reason in (
    "AddBuffMethodMissingOrSignatureMismatch",
    "DelBuffMethodMissingOrRuntimeInvokeUnavailable",
    "HideVisibleAccessoryFieldMissing",
    "PlayerWhoAmIFieldMissing",
    "BindPlayerHooks Result=Pending Reason=%s",
):
    assert reason in source, reason
for name in ("townNPCs", "jumpSpeedBoost", "extraFall", "wet", "head", "body", "legs"):
    assert f'costumeField("{name}"' in source
assert source.count("nativeManagedPointerPlausible(armor)") >= 4
assert source.count("nativeManagedPointerPlausible(armor->obj.klass)") >= 4
equipped = source[source.index("bool costumeEquipped("):source.index("bool costumeHidden(")]
assert equipped.index("nativeManagedPointerPlausible(armor)") < equipped.index("armor->length")
hidden = source[source.index("bool costumeHidden("):source.index("int costumeTownCount(")]
assert hidden.index("nativeManagedPointerPlausible(armor)") < hidden.index("armor->length")
assert "GoldFlame" in original_item and "BlueFlare" in original_item
assert "player.townNPCs >= 1" in original
assert "p.BlockyAccessoryPrevious" in original_buff
assert "g_costumeNormalDust=costumeDustId(\"GoldFlame\")" in source
assert "g_costumeWetDust=costumeDustId(\"BlueFlare\")" in source
assert "cycle->power=true;" in source
assert "cycle->previousType=cycle->currentType;" in source
assert "duration=60" in source
print("COSTUME_NATIVE_BIND_GUARD=PASS")
