#include "experiments/csharp/NativeDefaults.h"
#include <cassert>
#include <iostream>
#include <fstream>
struct Il2CppClass {const char* ns;const char* name;uint32_t size;bool value;int rank=0;Il2CppClass* element=nullptr;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;size_t offset;uint32_t flags=0;};
bl::il2cpp::Api fixture;
Il2CppClass byteClass{"System","Byte",1,true},boolClass{"System","Boolean",1,true},floatClass{"System","Single",4,true},intClass{"System","Int32",4,true},objectClass{"Terraria","Item",64,false};
Il2CppClass styleClass{"Terraria.Audio","LegacySoundStyle",16,false},boolArrayClass{"System","Boolean[]",16,false,1,&boolClass},floatArrayClass{"System","Int32[]",16,false,1,&intClass};
Il2CppObject soundStyle{&styleClass,nullptr};int barriers=0,rarityClassLookups=0;
struct BoolArray {Il2CppArray array;uint8_t cells[6400]{};} whole{{{&boolArrayClass,nullptr},nullptr,6400},{}},lockOn{{{&boolArrayClass,nullptr},nullptr,6400},{}};
struct FloatArray {Il2CppArray array;int cells[6400]{};} slots{{{&floatArrayClass,nullptr},nullptr,6400},{}};
std::map<std::string,FieldInfo> fields;
namespace bl::il2cpp {
Api& api(){return fixture;}
FieldInfo* findField(Il2CppClass*,std::string_view name){auto it=fields.find(std::string(name));return it==fields.end()?nullptr:&it->second;}
Il2CppClass* findClass(const TypeRef& ref){
 if(ref.ns=="Terraria.ID"&&ref.name=="ItemRarityID"){++rarityClassLookups;return nullptr;}
 return &objectClass;
}
}
int resolve(const std::string& key){return key=="independent/ExampleBulletProjectile"?1200:-1;}
int main(int argc,char** argv) {
 using namespace bl::csharp;
 fixture.class_from_il2cpp_type=[](const Il2CppType* type){return type->cls;};fixture.field_get_type=[](FieldInfo* field)->const Il2CppType*{return &field->type;};
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};fixture.class_instance_size=[](Il2CppClass* c){return c->size;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t* align){*align=4;return static_cast<int32_t>(c->size);};
 fixture.field_static_get_value=[](FieldInfo* f,void* output){
  if(f==&fields["Item44"])*static_cast<Il2CppObject**>(output)=&soundStyle;
  else if(f==&fields["GamepadWholeScreenUseRange"])*static_cast<Il2CppArray**>(output)=&whole.array;
  else if(f==&fields["LockOnIgnoresCollision"])*static_cast<Il2CppArray**>(output)=&lockOn.array;
  else if(f==&fields["StaffMinionSlotsRequired"])*static_cast<Il2CppArray**>(output)=&slots.array;
  else if(f==&fields["Swing"])*static_cast<int*>(output)=1;
  else *static_cast<int*>(output)=97;
 };
 fixture.class_get_rank=[](const Il2CppClass* cls){return cls->rank;};fixture.class_get_element_class=[](Il2CppClass* cls){return cls->element;};
 fixture.gc_wbarrier_set_field=[](Il2CppObject*,void** destination,void* value){*destination=value;++barriers;};
 fields={{"useStyle",{{&byteClass},16}},{"ranged",{{&boolClass},18}},{"shootSpeed",{{&floatClass},20}},
   {"shoot",{{&intClass},24}},{"ammo",{{&byteClass},28,0}},{"Bullet",{{&intClass},0,0x10}}};
 NativeDefaultsRequest request{"ThirdPartyBullet","consumer","Ammo.cs","useStyle:i:5;ranged:b:1;shootSpeed:f:4.5;shoot:p:independent/ExampleBulletProjectile;ammo:d:AmmoID:Bullet",6230,false};
 std::string error;assert(publishNativeDefaults({request},&error,resolve));
 alignas(8) unsigned char bytes[64];std::memset(bytes,0xAA,sizeof bytes);applyNativeDefaults(reinterpret_cast<Il2CppObject*>(bytes),false,6230);
 fields["questItem"]={{&boolClass},30};
 auto quest=request;quest.name="QuestFish";quest.descriptor="questItem:b:1";quest.type=6234;
 if(!publishNativeDefaults({quest},&error,resolve)){std::cerr<<error<<"\n";assert(false);}
 applyNativeDefaults(reinterpret_cast<Il2CppObject*>(bytes),false,6234);
 assert(bytes[30]==1&&bytes[31]==0xAA);
 assert(bytes[16]==5&&bytes[17]==0xAA&&bytes[18]==1&&bytes[19]==0xAA&&bytes[28]==97&&bytes[29]==0xAA);
 float speed=0;int shoot=0;std::memcpy(&speed,bytes+20,4);std::memcpy(&shoot,bytes+24,4);assert(speed==4.5f&&shoot==1200);
 const auto good=std::atomic_load(&g_nativeDefaults);
 auto reject=[&](const std::string& descriptor,const std::string& reason){auto bad=request;bad.descriptor=descriptor;bad.type=6231;
   assert(!publishNativeDefaults({bad},&error,resolve)&&error.find(reason)!=std::string::npos);
   assert(std::atomic_load(&g_nativeDefaults)==good);
   assert(error.find("Class=ThirdPartyBullet")!=std::string::npos&&error.find("File=Ammo.cs")!=std::string::npos);
 };
 reject("questItem:i:1","NativeIntegerTypeOrRangeMismatch");
 reject("identity:i:1","UnsupportedField");reject("useStyle:i:256","NativeIntegerTypeOrRangeMismatch");
 reject("ranged:i:1","NativeIntegerTypeOrRangeMismatch");reject("shootSpeed:f:NaN","NativeTypeOrValueMismatch");
 reject("shoot:p:absent/Projectile","ProjectileDependencyMissing");reject("useStyle:i:1;useStyle:i:2","UnsupportedField");
 fields["useStyle"].offset=64;reject("useStyle:i:1","InvalidFieldBounds");fields["useStyle"].offset=16;
 fields["ranged"].flags=0x20;reject("ranged:b:1","NativeTypeOrStorageMismatch");fields["ranged"].flags=0;
 // Apply the real compiled ExampleBulletProjectile descriptor through the production binder.
 fields={{"extraUpdates",{{&byteClass},16}},{"light",{{&floatClass},20}},{"localNPCHitCooldown",{{&intClass},24}},
   {"ranged",{{&boolClass},28}},{"hostile",{{&boolClass},29}},{"scale",{{&floatClass},32}},{"usesLocalNPCImmunity",{{&boolClass},36}}};
 assert(argc==2);std::ifstream file(argv[1]);std::string row,payload;
 while(std::getline(file,row))if(row.rfind("NATIVE_PROJECTILE\tExampleBulletProjectile\t",0)==0)payload=row.substr(row.find_last_of('\t')+1);
 assert(!payload.empty());request.projectile=true;request.type=1200;request.descriptor=payload;
 assert(publishNativeDefaults({request},&error,resolve));std::memset(bytes,0,sizeof bytes);
 applyNativeDefaults(reinterpret_cast<Il2CppObject*>(bytes),true,1200);
 int cooldown=0;std::memcpy(&cooldown,bytes+24,4);assert(bytes[16]==1&&bytes[28]&&bytes[36]&&cooldown==-1);
 fields["UseSound"]={{&styleClass},40,0};fields["Item44"]={{&styleClass},0,0x30};
 fields["GamepadWholeScreenUseRange"]={{&boolArrayClass},0,0x30};fields["LockOnIgnoresCollision"]={{&boolArrayClass},0,0x30};fields["StaffMinionSlotsRequired"]={{&floatArrayClass},0,0x30};
 NativeDefaultsRequest staff{"IndependentStaff","consumer","Staff.cs","UseSound:s:Item44;ItemSet_GamepadWholeScreenUseRange:b:1;ItemSet_LockOnIgnoresCollision:b:1;ItemSet_StaffMinionSlotsRequired:i:1",6231,false};
 assert(publishNativeDefaults({staff},&error,resolve));std::memset(bytes,0,sizeof bytes);applyNativeDefaults(reinterpret_cast<Il2CppObject*>(bytes),false,6231);
 Il2CppObject* actual=nullptr;std::memcpy(&actual,bytes+40,sizeof actual);assert(actual==&soundStyle&&barriers==1);
 assert(whole.cells[6231]&&lockOn.cells[6231]&&slots.cells[6231]==1&&whole.cells[6230]==0&&slots.cells[6230]==0);
 auto stable=std::atomic_load(&g_nativeDefaults);whole.array.length=10;assert(!publishNativeDefaults({staff},&error,resolve)&&stable==std::atomic_load(&g_nativeDefaults));whole.array.length=6400;
 fields["Item44"].type.cls=&intClass;assert(!publishNativeDefaults({staff},&error,resolve));fields["Item44"].type.cls=&styleClass;
 fields["UseSound"].flags=0x20;assert(!publishNativeDefaults({staff},&error,resolve));fields["UseSound"].flags=0;
 // Bind EVERY field from the actual compiled staff, with ItemRarityID absent
 // and the real Android primitive types (rare:Int32, useStyle:Byte).
 std::ifstream staffWire(argv[1]);std::string staffPayload;
 while(std::getline(staffWire,row))if(row.rfind("NATIVE_ITEM\tExampleMinionItem\t",0)==0)staffPayload=row.substr(row.find_last_of('\t')+1);
 assert(!staffPayload.empty());
 fields["rare"]={{&intClass},20};fields["useStyle"]={{&byteClass},16};
 fields["autoReuse"]={{&boolClass},17};fields["height"]={{&intClass},24};
 fields["knockBack"]={{&floatClass},28};fields["mana"]={{&intClass},32};
 fields["useAnimation"]={{&intClass},36};fields["value"]={{&intClass},48};
 fields["width"]={{&intClass},52};fields["Swing"]={{&intClass},0,0x50};
 staff.name="ExampleMinionItem";staff.descriptor=staffPayload;
 assert(publishNativeDefaults({staff},&error,resolve));
 std::memset(bytes,0xAA,sizeof bytes);applyNativeDefaults(reinterpret_cast<Il2CppObject*>(bytes),false,6231);
 auto integerAt=[&](int offset){int n=0;std::memcpy(&n,bytes+offset,4);return n;};
 float knockBack=0;std::memcpy(&knockBack,bytes+28,4);
 assert(bytes[16]==1&&bytes[17]==0&&bytes[18]==0xAA&&bytes[19]==0xAA);
 assert(integerAt(20)==9&&integerAt(24)==32&&knockBack==3&&integerAt(32)==10);
 assert(integerAt(36)==36&&integerAt(48)==1500000&&integerAt(52)==32);
 std::memcpy(&actual,bytes+40,sizeof actual);assert(actual==&soundStyle);
 assert(whole.cells[6231]&&lockOn.cells[6231]&&slots.cells[6231]==1&&rarityClassLookups==0);
 // All named public constants, including valid negative rarities. Unknown names
 // must reject atomically; they must not fall back to White or another class.
 const std::pair<const char*,int> rarities[]={{"Master",-13},{"Expert",-12},{"Quest",-11},{"Gray",-1},{"White",0},{"Blue",1},{"Green",2},{"Orange",3},{"LightRed",4},{"Pink",5},{"LightPurple",6},{"Lime",7},{"Yellow",8},{"Cyan",9},{"Red",10},{"Purple",11},{"Count",12}};
 for(const auto& [name,value]:rarities){
   staff.descriptor=std::string("rare:d:ItemRarityID:")+name;
   assert(publishNativeDefaults({staff},&error,resolve));
   applyNativeDefaults(reinterpret_cast<Il2CppObject*>(bytes),false,6231);assert(integerAt(20)==value);
 }
 stable=std::atomic_load(&g_nativeDefaults);
 for(const auto& bad:{"rare:d:ItemRarityID:Cyan9","rare:d:ItemRarityID:cyan","useStyle:d:ItemUseStyleID:MissingStyle","rare:d:UnknownFamily:Cyan"}){
   staff.descriptor=bad;assert(!publishNativeDefaults({staff},&error,resolve));
   assert(stable==std::atomic_load(&g_nativeDefaults));
 }
 assert(error.find("UnsupportedIdFamily")!=std::string::npos&&rarityClassLookups==0);
 staff.descriptor="useStyle:d:ItemUseStyleID:Swing";
 fields["Swing"].type.cls=&floatClass;
 assert(!publishNativeDefaults({staff},&error,resolve)&&stable==std::atomic_load(&g_nativeDefaults));
 assert(error.find("NativeIdTypeMismatch")!=std::string::npos);fields["Swing"].type.cls=&intClass;
 std::cout<<"NATIVE_DEFAULTS_FULL_COMPILED_STAFF_ANDROID_WITHOUT_ITEMRARITYID_ALL_RARITIES_AND_ATOMIC_REJECTION_PASS\n";
 std::cout<<"NATIVE_DEFAULTS_PRODUCTION_TYPED_WRITES_BYTE_SENTINELS_IDS_CROSS_MOD_TRANSACTIONAL_REJECTION_AND_COMPILED_EXAMPLE_BULLET_STATIC_SETS_AND_ROOTED_VANILLA_SOUND_PASS\n";
}
