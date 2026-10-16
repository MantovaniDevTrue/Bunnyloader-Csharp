from pathlib import Path

root=Path(__file__).resolve().parents[1]
sdk=(root/"experiments/csharp/BunnyCsSdk/BunnyItemBehavior.cs").read_text()
writer=(root/"experiments/csharp/BunnySourceCompiler/BasicItemWriter.cs").read_text()
basic=(root/"app/src/main/cpp/experiments/csharp/BasicItemDefaults.h").read_text()
source=(root/"app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text()
native=(root/"app/src/main/cpp/experiments/csharp/NativeBasicItems.cpp").read_text()
auto=(root/"app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
assert 'kCacheVersion = "BUNNY_AUTO_SOURCE_' in auto
assert '&&defense==0&&' not in sdk
assert 'NativeArmorDefinition' in sdk
assert 'int defense=Get<int>("Defense");' in writer
assert 'if(defense>0)' in writer
assert 'while(parts.Count<23)parts.Add("-");' in writer
assert 'parts.Add(defense.ToString(CultureInfo.InvariantCulture));' in writer
assert 'int type=-1; int defense=0;' in basic
assert 'fields.size()>26' in basic
assert 'fields.size()>=23 && fields[22]!="-"' in basic
assert 'if(fields.size()>=24 && fields[23]!="-" &&' in basic
assert 'basicInteger(fields[23],&out.defense)' in basic
assert 'if (item.defense>0 && !setBasicItemDefense(item.type,item.defense,error)) return false;' in source
assert 'instancesPending.store(true,std::memory_order_release);' in native
assert '__blSyncCSharpItemBehaviors' not in native
assert 'pumpNativeItemPoliciesOnGameThread();' in native
print("BUNNY_CSHARP_NATIVE_DEFENSE_POLICY=PASS")
