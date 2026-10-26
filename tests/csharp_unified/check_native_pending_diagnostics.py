from pathlib import Path

item = Path("app/src/main/cpp/experiments/csharp/NativeItemPolicies.cpp").read_text()
shape = Path("app/src/main/cpp/content/tiles/ModTiles.cpp").read_text()
angler = Path("app/src/main/cpp/experiments/csharp/NativeAnglerQuestGameplay.cpp").read_text()
costume = Path("app/src/main/cpp/experiments/csharp/NativeContent.cpp").read_text()

for field in ("Group=", "Flag=", "Variant=", "Slot=", "Detail=", "ArmorGroupUnavailable",
              "ArmorSetsUnavailable", "NativeSkinFallbackUnavailable", "ArmorFlagNotBooleanArray",
              "ArmorFlagTableUnavailable", "ArmorFlagResizeUnavailable", "ArmorFlagOwnedWriteRejected"):
    assert field in item, field
assert 'if(!ready)return false;' in item
read_shape = shape[shape.index("DrawShape readDrawShape("):shape.index("void hookDrawData()")]
assert "CoordinateHeightsUnreconstructable Rows=%d FullHeight=%d Padding=%d" in read_shape
assert "arrayData(heights)" not in read_shape
for reason in ("InvalidQuestItemArrayClassPointer", "InvalidQuestItemArrayLength",
               "QuestItemArrayTypeMismatch", "QuestItemArrayEmpty"):
    assert reason in angler, reason
quest = angler[angler.index("bool updateQuests("):angler.index("void pumpOnGameThread()")]
assert quest.index("nativeManagedPointerPlausible(original)") < quest.index("original->obj.klass")
assert quest.index("nativeManagedPointerPlausible(original->obj.klass)") < quest.index("original->length")
assert "Stage=BindParticles Result=Unavailable Reason=%s" in costume
assert "NewDustMethodUnavailable" in costume
assert "GoldFlameDustIdUnavailable" in costume
assert "BlueFlareDustIdUnavailable" in costume
print("COSTUME_TILE_ANGLER_NATIVE_DIAGNOSTICS=PASS")
