#include "experiments/csharp/NativeHoldout.h"
#include <cassert>
#include <iostream>
#include <fstream>
struct Il2CppClass {const char* ns;const char* name;uint32_t size;bool value;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;size_t offset;uint32_t flags=0;};
struct MethodInfo {bool instance;};
bl::il2cpp::Api fixture;
Il2CppClass player{"Terraria","Player",128,false},item{"Terraria","Item",128,false},rectangle{"Microsoft.Xna.Framework","Rectangle",16,true},integer{"System","Int32",4,true},single{"System","Single",4,true},vector{"Microsoft.Xna.Framework","Vector2",8,true};
std::map<Il2CppClass*,std::map<std::string,FieldInfo>> members;
MethodInfo style{true};bool methodsPresent=true;
using bl::csharp::holdout::Vector;using bl::csharp::holdout::Rectangle;
int calls=0;
void original(Il2CppObject* p,float,Il2CppObject*,Rectangle frame,const MethodInfo*) {
    assert(frame.width==32&&frame.height==16);++calls;Vector position{100,200};std::memcpy(reinterpret_cast<char*>(p)+24,&position,8);
}
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& type){return type.name=="Item"?&item:type.name=="Player"?&player:&rectangle;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){auto& m=members[cls];auto it=m.find(std::string(name));return it==m.end()?nullptr:&it->second;}
Signature parseSignature(std::string_view text){Signature signature;signature.name=text;return signature;}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature& sig,bool*,const MethodInfo**) {
    assert(sig.name=="void ItemCheck_ApplyUseStyle(float mountOffset, Item sItem, Rectangle heldItemFrame)"||sig.name=="void ItemCheck_ApplyHoldStyle(float mountOffset, Item sItem, Rectangle heldItemFrame)");return methodsPresent?&style:nullptr;
}
}
namespace bl::hook {bool install(const MethodInfo*,void*,void** output){*output=reinterpret_cast<void*>(original);return true;}}
int main(int argc,char** argv) {
 using namespace bl::csharp::holdout;
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};fixture.class_instance_size=[](Il2CppClass* c){return c->size;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t* a){*a=4;return static_cast<int32_t>(c->size);};
 fixture.method_is_instance=[](const MethodInfo* m){return m->instance;};
 fixture.class_get_fields=[](Il2CppClass* c,void** cursor)->FieldInfo* {
    const size_t index=reinterpret_cast<uintptr_t>(*cursor);auto& fields=members[c];if(index>=fields.size())return nullptr;
    auto it=fields.begin();std::advance(it,static_cast<long>(index));*cursor=reinterpret_cast<void*>(index+1);return &it->second;
 };
 members[&item]={{"type",{{&integer},16}}};
 members[&player]={{"direction",{{&integer},16}},{"gravDir",{{&single},20}},{"itemLocation",{{&vector},24}},{"itemRotation",{{&single},32}}};
 members[&rectangle]={{"X",{{&integer},0}},{"Y",{{&integer},4}},{"Width",{{&integer},8}},{"Height",{{&integer},12}}};
 std::string error;assert(publish({{"independent","Rocket","Rocket.cs",6230,-2,-2}},&error));
 alignas(8) char playerBytes[128]{},itemBytes[128]{};auto* p=reinterpret_cast<Il2CppObject*>(playerBytes);auto* i=reinterpret_cast<Il2CppObject*>(itemBytes);
 int type=6230,direction=1;float gravity=1,angle=0;std::memcpy(itemBytes+16,&type,4);std::memcpy(playerBytes+16,&direction,4);std::memcpy(playerBytes+20,&gravity,4);std::memcpy(playerBytes+32,&angle,4);
 use(p,0,i,{0,0,32,16},&style);Vector result{};std::memcpy(&result,playerBytes+24,8);assert(calls==1&&result.x==98&&result.y==198);
 direction=-1;gravity=-1;angle=1.57079632679f;std::memcpy(playerBytes+16,&direction,4);std::memcpy(playerBytes+20,&gravity,4);std::memcpy(playerBytes+32,&angle,4);
 hold(p,0,i,{0,0,32,16},&style);std::memcpy(&result,playerBytes+24,8);assert(calls==2&&std::fabs(result.x-98)<.001&&std::fabs(result.y-202)<.001);
 type=1;std::memcpy(itemBytes+16,&type,4);use(p,0,i,{0,0,32,16},&style);std::memcpy(&result,playerBytes+24,8);assert(result.x==100&&result.y==200);
 // Validate both real IL2CPP conventions for Rectangle field offsets.
 originalUse=originalHold=nullptr;for(auto& field:members[&rectangle])field.second.offset+=16;assert(prepare(&error));
 originalUse=originalHold=nullptr;members[&rectangle]["Width"].offset=28;
 assert(!prepare(&error)&&error.find("AbiMismatch")!=std::string::npos);members[&rectangle]["Width"].offset=24;
 style.instance=false;assert(!prepare(&error)&&error.find("NativeMethodMismatch")!=std::string::npos);style.instance=true;
 members[&player]["itemLocation"].flags=0x20;assert(!prepare(&error)&&error.find("InitOnly")!=std::string::npos);members[&player]["itemLocation"].flags=0;
 assert(argc==2);std::ifstream wire(argv[1]);std::string row;bool found=false;
 while(std::getline(wire,row))if(row=="HOLDOUT\tExampleRocketLauncher\t-2\t-2")found=true;
 assert(found);
 std::cout<<"HOLDOUT_PRODUCTION_USE_HOLD_STYLES_DIRECTION_GRAVITY_ROTATION_VANILLA_ISOLATION_RECTANGLE_ABI_AND_COMPILED_ROCKET_PASS\n";
}
