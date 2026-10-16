from pathlib import Path
root=Path(__file__).resolve().parents[1]
sdk=(root/"experiments/csharp/BunnyCsSdk/BunnyItemBehavior.cs").read_text()
writer=(root/"experiments/csharp/BunnySourceCompiler/BasicItemWriter.cs").read_text()
parser=(root/"app/src/main/cpp/experiments/csharp/BasicItemDefaults.h").read_text()
native=(root/"app/src/main/cpp/experiments/csharp/NativeBasicItems.cpp").read_text()
content=(root/"app/src/main/cpp/experiments/csharp/NativeContent.cpp").read_text()
policies=(root/"app/src/main/cpp/experiments/csharp/NativeItemPolicies.cpp").read_text()
auto=(root/"app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
assert "NativeAccessoryEffectsDefinition" in sdk
assert 'nativeAccessory' in sdk
assert 'while(parts.Count<24)parts.Add("-");' in writer
assert "fields.size()>26" in parser
assert "parseBasicAccessoryEffects(fields[24]" in parser
assert "applyBasicItemAccessoryEffects(player,type);" in native
assert "applyBasicItemAccessoryEffects(self,type);" not in content
assert "prepareBasicAccessoryHook" in native
assert "JS_" not in native
assert "using ponte JS" not in native
assert "managedNativeEquipTextures()" in policies
assert "resolveBasicAccessoryPolicy" in native
assert "atomic_load_explicit" in native
assert "BUNNY_AUTO_SOURCE_V3_NATIVE_ITEM_POLICIES_20261015" in auto
print("BUNNY_CSHARP_NATIVE_ACCESSORY_POLICY=PASS")
