#include "experiments/csharp/NativeDefaults.h"
#include <cassert>
#include <iostream>
#include <fstream>
struct Il2CppClass {const char* ns;const char* name;uint32_t size;bool value;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;size_t offset;uint32_t flags=0;};
bl::il2cpp::Api fixture;
Il2CppClass byteClass{"System","Byte",1,true},boolClass{"System","Boolean",1,true},floatClass{"System","Single",4,true},intClass{"System","Int32",4,true},objectClass{"Terraria","Item",64,false};
std::map<std::string,FieldInfo> fields;
namespace bl::il2cpp {
Api& api(){return fixture;}
FieldInfo* findField(Il2CppClass*,std::string_view name){auto it=fields.find(std::string(name));return it==fields.end()?nullptr:&it->second;}
Il2CppClass* findClass(const TypeRef&){return &objectClass;}
}
int resolve(const std::string& key){return key=="independent/ExampleBulletProjectile"?1200:-1;}
int main(int argc,char** argv) {
 using namespace bl::csharp;
 fixture.class_from_il2cpp_type=[](const Il2CppType* type){return type->cls;};fixture.field_get_type=[](FieldInfo* field)->const Il2CppType*{return &field->type;};
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};fixture.class_instance_size=[](Il2CppClass* c){return c->size;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t* align){*align=4;return static_cast<int32_t>(c->size);};
 fixture.field_static_get_value=[](FieldInfo*,void* output){*static_cast<int*>(output)=97;};
 fields={{"useStyle",{{&byteClass},16}},{"ranged",{{&boolClass},18}},{"shootSpeed",{{&floatClass},20}},
   {"shoot",{{&intClass},24}},{"ammo",{{&byteClass},28,0}},{"Bullet",{{&intClass},0,0x10}}};
 NativeDefaultsRequest request{"ThirdPartyBullet","consumer","Ammo.cs","useStyle:i:5;ranged:b:1;shootSpeed:f:4.5;shoot:p:independent/ExampleBulletProjectile;ammo:d:AmmoID:Bullet",6230,false};
 std::string error;assert(publishNativeDefaults({request},&error,resolve));
 alignas(8) unsigned char bytes[64];std::memset(bytes,0xAA,sizeof bytes);applyNativeDefaults(reinterpret_cast<Il2CppObject*>(bytes),false,6230);
 assert(bytes[16]==5&&bytes[17]==0xAA&&bytes[18]==1&&bytes[19]==0xAA&&bytes[28]==97&&bytes[29]==0xAA);
 float speed=0;int shoot=0;std::memcpy(&speed,bytes+20,4);std::memcpy(&shoot,bytes+24,4);assert(speed==4.5f&&shoot==1200);
 const auto good=std::atomic_load(&g_nativeDefaults);
 auto reject=[&](const std::string& descriptor,const std::string& reason){auto bad=request;bad.descriptor=descriptor;bad.type=6231;
   assert(!publishNativeDefaults({bad},&error,resolve)&&error.find(reason)!=std::string::npos);
   assert(std::atomic_load(&g_nativeDefaults)==good);
   assert(error.find("Class=ThirdPartyBullet")!=std::string::npos&&error.find("File=Ammo.cs")!=std::string::npos);
 };
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
 std::cout<<"NATIVE_DEFAULTS_PRODUCTION_TYPED_WRITES_BYTE_SENTINELS_IDS_CROSS_MOD_TRANSACTIONAL_REJECTION_AND_COMPILED_EXAMPLE_BULLET_PASS\n";
}
