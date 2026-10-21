#include "il2cpp/Api.h"
#include <vector>
struct FieldInfo;
struct Il2CppClass {
 const char* ns;const char* name;int width;bool value;int rank=0;Il2CppClass* element=nullptr;
 std::vector<FieldInfo*> fields{};
};
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { const char* name;Il2CppType type;uint32_t flags=0x10;Il2CppArray* array=nullptr;size_t offset=0; };
struct MethodInfo { bool instance=false; };
#include "experiments/csharp/ManagedGoreRuntime.cpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {
bl::il2cpp::Api fixture;
Il2CppClass integer{"System","Int32",4,true}, byteType{"System","Byte",1,true},
 real{"System","Single",4,true}, boolean{"System","Boolean",1,true};
Il2CppClass assetClass{"ReLogic.Content","Asset<Texture2D>",sizeof(void*),false};
Il2CppClass vectorClass{"Microsoft.Xna.Framework","Vector2",8,true};
Il2CppClass textureArray{"ReLogic.Content","Asset<Texture2D>[]",sizeof(void*),false,1,&assetClass};
Il2CppClass integerArray{"System","Int32[]",sizeof(void*),false,1,&integer};
Il2CppClass realArray{"System","Single[]",sizeof(void*),false,1,&real};
Il2CppClass byteArray{"System","Byte[]",sizeof(void*),false,1,&byteType};
Il2CppClass boolArray{"System","Boolean[]",sizeof(void*),false,1,&boolean};
Il2CppClass assets{"Terraria.GameContent","TextureAssets",0,false},
 goreId{"Terraria.ID","GoreID",0,false}, sets{"Terraria.ID","Sets",0,false},
 safety{"Terraria.GameContent","ChildSafety",0,false}, gore{"Terraria","Gore",0,false},
 arrayClass{"System","Array",0,false};
FieldInfo xField{"X",{&real},0,nullptr,20}, yField{"Y",{&real},0,nullptr,16};
FieldInfo goreFields[]={ {"Gore",{&textureArray}}, {"SpecialAI",{&byteArray}},
 {"DisappearSpeed",{&integerArray}}, {"DisappearSpeedAlpha",{&realArray}},
 {"IsDrip",{&boolArray}}, {"SafeGore",{&boolArray}} };
MethodInfo setValue{true}, newGore{};
Il2CppType parameters[]={{&vectorClass},{&vectorClass},{&integer},{&real}}, returned{&integer};
struct Box {Il2CppObject header;int value; } result{{&integer,nullptr},1};
int gameTid=gettid(),writes=0,reads=0,resizes=0,failResize=-1,loads=0,destroys=0,failLoad=-1,spawns=0;
int lastType=0;float lastX=0,lastY=0;
std::map<uint32_t,Il2CppObject*> roots;
uint32_t nextRoot=1;
std::vector<void*> buffers;
Il2CppArray* makeArray(Il2CppClass* cls,size_t length) {
 auto* p=static_cast<Il2CppArray*>(std::calloc(1,sizeof(Il2CppArray)+length*cls->element->width));
 p->obj.klass=cls;p->length=length;buffers.push_back(p);return p;
}
}
namespace bl::log { void write(int,const char*,...){} }
namespace bl::runtime { int gameThreadId(){return gameTid;} }
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& t){
 for(auto* c:{&assets,&goreId,&safety,&gore,&arrayClass}) if(t.name==c->name)return c;
 return nullptr;
}
Il2CppClass* findNested(Il2CppClass* c,std::string_view name){return c==&goreId && name=="Sets" ? &sets : nullptr;}
FieldInfo* findField(Il2CppClass* c,std::string_view name){
 if(c==&vectorClass) return name=="X" ? &xField : name=="Y" ? &yField : nullptr;
 for(auto& f:goreFields) if(name==f.name)return &f;
 return nullptr;
}
Signature parseSignature(std::string_view text){Signature sig;sig.name=text.find("NewGore")!=text.npos ? "NewGore" : "SetValue";return sig;}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature& sig,bool*,const MethodInfo**){return sig.name=="NewGore" ? &newGore : &setValue;}
}
namespace bl::runtime {
Il2CppArray* TypeTables::resizedCopy(Il2CppArray* old,uintptr_t length){
 if(++resizes==failResize)return nullptr;
 auto* p=makeArray(old->obj.klass,length);
 std::memcpy(arrayData(p),arrayData(old),std::min(old->length,length)*old->obj.klass->element->width);return p;
}
namespace content {
Il2CppClass* textureAssetClass(){return &assetClass;}
Il2CppObject* loadTextureAsset(const std::string&,const unsigned char* data,size_t size,const std::string&,int* w,int* h){
 ++loads;if(loads==failLoad)return nullptr;assert(data && size>=24);
 *w=(int(data[16])<<24)|(int(data[17])<<16)|(int(data[18])<<8)|data[19];
 *h=(int(data[20])<<24)|(int(data[21])<<16)|(int(data[22])<<8)|data[23];
 return new Il2CppObject{&assetClass,nullptr};
}
void destroyTextureAsset(Il2CppObject* p){++destroys;delete p;}
}
}
void initialize(){
 vectorClass.fields={&xField,&yField};
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};
 fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 fixture.object_get_class=[](Il2CppObject* p){return p->klass;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};
 fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_get_rank=[](const Il2CppClass* c){return c->rank;};
 fixture.class_get_element_class=[](Il2CppClass* c){return c->element;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t*){return int32_t(c->width);};
 fixture.class_get_fields=[](Il2CppClass* c,void** it)->FieldInfo*{
  auto index=reinterpret_cast<uintptr_t>(*it);if(index>=c->fields.size())return nullptr;
  *it=reinterpret_cast<void*>(index+1);return c->fields[index];
 };
 fixture.field_static_get_value=[](FieldInfo* f,void* out){++reads;*static_cast<Il2CppArray**>(out)=f->array;};
 fixture.field_static_set_value=[](FieldInfo* f,void* in){
  auto* array=static_cast<Il2CppArray*>(in);assert(array && array->obj.klass==f->type.cls);
  ++writes;f->array=array;
 };
 fixture.gchandle_new=[](Il2CppObject* p,bool){auto id=nextRoot++;roots[id]=p;return id;};
 fixture.gchandle_get_target=[](uint32_t n){auto at=roots.find(n);return at==roots.end()?nullptr:at->second;};
 fixture.gchandle_free=[](uint32_t n){assert(roots.erase(n)==1);};
 fixture.method_is_instance=[](const MethodInfo* m){return m->instance;};
 fixture.method_get_param=[](const MethodInfo*,uint32_t n)->const Il2CppType*{return &parameters[n];};
 fixture.method_get_return_type=[](const MethodInfo*)->const Il2CppType*{return &returned;};
 fixture.runtime_invoke=[](const MethodInfo* m,void* self,void** args,Il2CppObject** error)->Il2CppObject*{
  *error=nullptr;
  if(m==&setValue){
   auto* arr=static_cast<Il2CppArray*>(self);int index=*static_cast<int*>(args[1]);
   assert(index>=0 && uintptr_t(index)<arr->length);
   static_cast<Il2CppObject**>(arrayData(arr))[index]=static_cast<Il2CppObject*>(args[0]);return nullptr;
  }
  assert(m==&newGore && self==nullptr && gettid()==gameTid);
  ++spawns;lastType=*static_cast<int*>(args[2]);
  std::memcpy(&lastX,static_cast<unsigned char*>(args[0])+4,4);
  std::memcpy(&lastY,args[0],4);
  return &result.header;
 };
 for(auto& f:goreFields)f.array=makeArray(f.type.cls,20);
}

extern "C" __attribute__((visibility("default"))) int bunny_gore_fixture_initialize(const char* folder){
 initialize();std::vector<bl::csharp::gore::Source> sources;std::string error;
 if(!bl::csharp::gore::discover(folder,&sources,&error)) return 0;
 return bl::csharp::gore::install("independent-a",sources,&error) &&
        bl::csharp::gore::install("independent-b",sources,&error) ? int(sources.size()) : 0;
}
extern "C" __attribute__((visibility("default"))) void bunny_gore_fixture_pump(){bl::csharp::gore::pumpOnGameThread();}
extern "C" __attribute__((visibility("default"))) void bunny_gore_fixture_ready(){bl::csharp::gore::worldReady();}
extern "C" __attribute__((visibility("default"))) void bunny_gore_fixture_boundary(){bl::csharp::gore::worldBoundary();}
extern "C" __attribute__((visibility("default"))) int bunny_gore_fixture_spawns(){return spawns;}
extern "C" __attribute__((visibility("default"))) int bunny_gore_fixture_last_type(){return lastType;}
extern "C" __attribute__((visibility("default"))) float bunny_gore_fixture_last_x(){return lastX;}

#if !defined(BUNNY_GORE_LIBRARY_ONLY)
int main(){
 using namespace bl::csharp::gore;
 initialize();std::vector<Source> sources;std::string error;
 assert(discover("samples/ExampleModCSharp/Playable",&sources,&error) && sources.size()==3);
 goreFields[0].flags=0;assert(!install("a",sources,&error) && reads==0 && writes==0);
 assert(error.find("Field=TextureAssets.Gore")!=error.npos);goreFields[0].flags=0x10;
 goreFields[0].type.cls=&integer;assert(!install("a",sources,&error) && reads==0);goreFields[0].type.cls=&textureArray;
 byteType.width=2;assert(!install("a",sources,&error));assert(error.find("NativeElementTypeOrSizeMismatch")!=error.npos);byteType.width=1;
 xField.offset=16;assert(!install("a",sources,&error));assert(error.find("NativeFieldOverlap")!=error.npos);xField.offset=20;
 newGore.instance=true;assert(!install("a",sources,&error));newGore.instance=false;
 int oldTid=gameTid;gameTid=-1;assert(!install("a",sources,&error));assert(error.find("WrongThread")!=error.npos);gameTid=oldTid;
 auto* first=goreFields[0].array;
 failResize=resizes+2;assert(!install("a",sources,&error) && goreFields[0].array==first && writes==0 && roots.empty());failResize=-1;
 failLoad=loads+2;assert(!install("a",sources,&error) && goreFields[0].array==first && writes==0 && roots.empty() && destroys==1);failLoad=-1;
 assert(install("a",sources,&error) && writes==6 && roots.size()==9);
 assert(install("b",sources,&error) && writes==12 && roots.size()==12);
 assert(install("a",sources,&error) && writes==12 && roots.size()==12);
 const auto& name=sources[0].name;int type=typeOf("a",name), other=typeOf("b",name);
 assert(type==20 && other==23 && typeOf("",name)==type && typeOf("missing",name)==0);
 assert(static_cast<int32_t*>(arrayData(goreFields[2].array))[type]==1);
 assert(!bunny_csharp_gore_spawn("a",name.c_str(),1,2,0,0,1));worldReady();
 assert(bunny_csharp_gore_configure("a",name.c_str(),9,3,5,1,1)==1);
 auto* prior=goreFields[2].array;goreFields[2].array=makeArray(&boolArray,prior->length);
 pumpOnGameThread();assert(!g_configs.empty());goreFields[2].array=prior;pumpOnGameThread();assert(g_configs.empty());
 assert(static_cast<uint8_t*>(arrayData(goreFields[1].array))[type]==9);
 assert(static_cast<int32_t*>(arrayData(goreFields[2].array))[type]==3);
 assert(static_cast<float*>(arrayData(goreFields[3].array))[type]==5);
 assert(static_cast<uint8_t*>(arrayData(goreFields[4].array))[type]==1);
 // Recreated arrays of equal length must restore all owned settings/assets.
 for(auto& f:goreFields)f.array=makeArray(f.type.cls,f.array->length);
 for(int i=0;i<20;++i)pumpOnGameThread();
 assert(static_cast<int32_t*>(arrayData(goreFields[2].array))[type]==3);
 assert(static_cast<Il2CppObject**>(arrayData(goreFields[0].array))[other]);
 // The original JS loader can grow all six vectors after C# registration.
 // A C# repair must preserve that later mod's native entry too.
 auto* externalAsset=new Il2CppObject{&assetClass,nullptr};
 const int externalType=int(goreFields[0].array->length);
 for(auto& f:goreFields) f.array=bl::runtime::TypeTables::resizedCopy(f.array,f.array->length+1);
 static_cast<Il2CppObject**>(arrayData(goreFields[0].array))[externalType]=externalAsset;
 static_cast<int32_t*>(arrayData(goreFields[2].array))[externalType]=7;
 for(int i=0;i<20;++i)pumpOnGameThread();
 assert(static_cast<Il2CppObject**>(arrayData(goreFields[0].array))[externalType]==externalAsset);
 assert(static_cast<int32_t*>(arrayData(goreFields[2].array))[externalType]==7);
 assert(bunny_csharp_gore_spawn("a",name.c_str(),120,240,2,-1,1)==1);pumpOnGameThread();
 assert(spawns==1 && lastType==type && lastX==120 && lastY==240);
 assert(bunny_csharp_gore_spawn("b",name.c_str(),1,2,0,0,1));worldBoundary();worldReady();pumpOnGameThread();assert(spawns==1);
 for(int i=0;i<128;++i)assert(bunny_csharp_gore_spawn("b",name.c_str(),1,2,0,0,1));
 assert(!bunny_csharp_gore_spawn("b",name.c_str(),1,2,0,0,1));pumpOnGameThread();assert(spawns==33);
 worldBoundary();
 std::cout<<"GORE_PRODUCTION_REGISTRATION_GC_TRANSACTION_METADATA_LAYOUT_QUEUE_WORLD_SESSION_REPAIR_NO_QUICKJS=PASS\n";
}
#endif
