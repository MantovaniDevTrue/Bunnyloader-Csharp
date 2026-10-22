#include "il2cpp/Api.h"
#include <vector>
#include <functional>
struct FieldInfo;
struct MethodInfo;
struct Il2CppClass {const char* ns;const char* name;int size;bool value;int rank=0;Il2CppClass* element=nullptr;std::vector<FieldInfo*> fields{};std::vector<MethodInfo*> methods{};};
struct Il2CppType {Il2CppClass* cls;bool byref=false;};
struct FieldInfo {const char* name;Il2CppType type;uint32_t flags=0x10;size_t offset=0;std::vector<unsigned char> data{};};
struct MethodInfo {void* pointer;const char* name;Il2CppClass* owner;uint32_t flags;Il2CppType returned;std::vector<Il2CppType> parameters{};std::vector<const char*> parameterNames{};};
#include "content/backgrounds/UndergroundBackgrounds.cpp"
#include "experiments/csharp/ManagedUndergroundBackgrounds.cpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <cstdarg>
namespace {
bl::il2cpp::Api exports;
Il2CppClass integer{"System","Int32",4,true},real{"System","Single",4,true},dbl{"System","Double",8,true},boolean{"System","Boolean",1,true};
Il2CppClass vector{"Microsoft.Xna.Framework","Vector2",8,true},texture{"Microsoft.Xna.Framework.Graphics","Texture2D",0,false},asset{"ReLogic.Content","Asset<Texture2D>",0,false};
Il2CppClass intArray{"System","Int32[]",8,false,1,&integer},texArray{"Microsoft.Xna.Framework.Graphics","Texture2D[]",8,false,1,&texture},assetArray{"ReLogic.Content","Asset<Texture2D>[]",8,false,1,&asset};
Il2CppClass mainClass{"Terraria","Main",64,false},localClass{"","LocalUserGameState",64,false},assetOwner{"Terraria.GameContent","TextureAssets",0,false};
FieldInfo menu{"gameMenu",{&boolean}},enabled{"BackgroundEnabled",{&boolean}},position{"screenPosition",{&vector}},width{"screenWidth",{&integer}},
 caveX{"caveBackX",{&intArray}},caves{"caveBackStyle",{&intArray}},current{"undergroundBackground",{&integer}},previous{"oldUndergroundBackground",{&integer}},bgAssets{"Background",{&assetArray}},
 localInstance{"Instance",{&localClass}},back{"backTexture",{&intArray},0,sizeof(Il2CppObject)},oldBack{"oldBackTexture",{&intArray},0,sizeof(Il2CppObject)+sizeof(void*)},
 xField{"X",{&real},0,sizeof(Il2CppObject)+4},yField{"Y",{&real},0,sizeof(Il2CppObject)};
MethodInfo pick{nullptr,"DrawBackground_PickUndergroundBackgroundStyle",&mainClass,0x10,{&integer},{{&dbl}},{"magmaLayer"}},
 getValues{nullptr,"get_backTextureValues",&mainClass,0,{&texArray}},getOldValues{nullptr,"get_oldBackTextureValues",&mainClass,0,{&texArray}},
 getMenu{nullptr,"get_gameMenu",&mainClass,0x10,{&boolean}};
std::map<uint32_t,Il2CppObject*> handles;uint32_t nextHandle=1;
std::map<Il2CppObject*,Il2CppObject*> images;
std::map<const MethodInfo*,void*> installed;
std::vector<void*> allocations;
std::vector<std::string> logs;
int gameTid=0,destroyed=0,nativeVanilla=3,vanillaCalls=0,fillCalls=0,writes=0,failRoot=-1;
Il2CppArray *values=nullptr,*oldValues=nullptr;
Il2CppObject* localObject=nullptr;
int (*managedCallback)(int,int64_t)=nullptr;int managedSlot=-1;int64_t ticket=0;
std::map<int,bl::runtime::underground::Slots> configured;
std::function<void()> duringFill;
template<class T>void set(FieldInfo& field,T value){field.data.resize(sizeof(T));std::memcpy(field.data.data(),&value,sizeof(T));}
Il2CppArray* array(Il2CppClass* cls,int length){auto* p=static_cast<Il2CppArray*>(std::calloc(1,sizeof(Il2CppArray)+size_t(length)*size_t(cls->element->value?cls->element->size:8)));p->obj.klass=cls;p->length=length;allocations.push_back(p);return p;}
Il2CppObject* object(Il2CppClass* cls){auto* p=static_cast<Il2CppObject*>(std::calloc(1,std::max(size_t(64),sizeof(Il2CppObject)+size_t(cls->size))));p->klass=cls;allocations.push_back(p);return p;}
int nativePick(double,const MethodInfo*){++vanillaCalls;return nativeVanilla;}
Il2CppArray* nativeValues(void*,const MethodInfo*){return values;}
Il2CppArray* nativeOldValues(void*,const MethodInfo*){return oldValues;}
bool fixtureFill(int slot,bl::runtime::underground::Slots* result,std::string*){++fillCalls;if(duringFill)duringFill();auto it=configured.find(slot);if(it==configured.end())return false;*result=it->second;return true;}
}
namespace bl::log {void write(int,const char* format,...){char buffer[2048];va_list ap;va_start(ap,format);vsnprintf(buffer,sizeof(buffer),format,ap);va_end(ap);logs.emplace_back(buffer);}}
namespace bl::runtime {int gameThreadId(){return gameTid;}}
namespace bl::il2cpp {
Api& api(){return exports;}
Il2CppClass* findClassQuiet(const std::string& ns,const std::string& name){for(auto* c:{&mainClass,&localClass,&texture,&assetOwner})if(ns==c->ns && name==c->name)return c;return nullptr;}
FieldInfo* findField(Il2CppClass* c,std::string_view name){for(auto* f:c->fields)if(name==f->name)return f;return nullptr;}
}
namespace bl::runtime::content {
Il2CppClass* textureAssetClass(){return &asset;}
void destroyTextureAsset(Il2CppObject*){++destroyed;}
Il2CppObject* textureAssetValue(Il2CppObject* a){auto it=images.find(a);return it==images.end()?nullptr:it->second;}
}
namespace bl::hook {
bool install(const MethodInfo* method,void* replacement,void** original){if(installed.contains(method))return false;installed[method]=replacement;*original=method->pointer;return true;}
}
void initialize(){
 exports.class_get_name=[](Il2CppClass* c){return c->name;};exports.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 exports.class_is_valuetype=[](const Il2CppClass* c){return c->value;};exports.class_value_size=[](Il2CppClass* c,uint32_t*){return int32_t(c->size);};
 exports.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};exports.class_get_element_class=[](Il2CppClass* c){return c->element;};exports.class_get_rank=[](const Il2CppClass* c){return c->rank;};
 exports.class_is_assignable_from=[](Il2CppClass* a,Il2CppClass* b){return a==b;};exports.object_get_class=[](Il2CppObject* o){return o->klass;};exports.class_instance_size=[](Il2CppClass* c){return uint32_t(c->size);};
 exports.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};exports.field_get_flags=[](FieldInfo* f){return f->flags;};exports.field_get_offset=[](FieldInfo* f){return f->offset;};
 exports.field_static_get_value=[](FieldInfo* f,void* out){std::memcpy(out,f->data.data(),f->data.size());};
 exports.class_get_fields=[](Il2CppClass* c,void** it)->FieldInfo*{size_t i=reinterpret_cast<size_t>(*it);if(i>=c->fields.size())return nullptr;*it=reinterpret_cast<void*>(i+1);return c->fields[i];};
 exports.class_get_field_from_name=[](Il2CppClass* c,const char* name)->FieldInfo*{for(auto* f:c->fields)if(std::strcmp(name,f->name)==0)return f;return nullptr;};
 exports.class_get_method_from_name=[](Il2CppClass* c,const char* name,int count)->const MethodInfo*{for(auto* m:c->methods)if(std::strcmp(name,m->name)==0 && int(m->parameters.size())==count)return m;return nullptr;};
 exports.class_get_methods=[](Il2CppClass* c,void** it)->const MethodInfo*{size_t i=reinterpret_cast<size_t>(*it);if(i>=c->methods.size())return nullptr;*it=reinterpret_cast<void*>(i+1);return c->methods[i];};exports.class_get_parent=[](Il2CppClass*)->Il2CppClass*{return nullptr;};
 exports.method_get_name=[](const MethodInfo* m){return m->name;};exports.method_get_flags=[](const MethodInfo* m,uint32_t*){return m->flags;};exports.method_get_param_count=[](const MethodInfo* m){return uint32_t(m->parameters.size());};
 exports.method_get_param=[](const MethodInfo* m,uint32_t i)->const Il2CppType*{return &m->parameters[i];};exports.method_get_param_name=[](const MethodInfo* m,uint32_t i){return m->parameterNames[i];};exports.method_get_return_type=[](const MethodInfo* m)->const Il2CppType*{return &m->returned;};exports.method_is_generic=[](const MethodInfo*){return false;};
 exports.type_get_name=[](const Il2CppType* t){return strdup((std::string(t->cls->ns)+"."+t->cls->name+(t->byref?"&":"")).c_str());};exports.il2cpp_free=[](void* p){std::free(p);};
 exports.runtime_invoke=[](const MethodInfo* m,void*,void**,Il2CppObject**)->Il2CppObject*{if(m==&getMenu){auto* box=object(&boolean);std::memcpy(reinterpret_cast<char*>(box)+sizeof(Il2CppObject),menu.data.data(),1);return box;}return nullptr;};
 exports.gchandle_new=[](Il2CppObject* o,bool){if(failRoot==0)return 0u;if(failRoot>0)--failRoot;uint32_t id=nextHandle++;handles[id]=o;return id;};exports.gchandle_get_target=[](uint32_t id){auto it=handles.find(id);return it==handles.end()?nullptr:it->second;};exports.gchandle_free=[](uint32_t id){handles.erase(id);};
 exports.gc_wbarrier_set_field=[](Il2CppObject*,void** location,void* value){++writes;*location=value;};
 mainClass.fields={&menu,&enabled,&position,&width,&caveX,&caves,&current,&previous};localClass.fields={&localInstance,&back,&oldBack};vector.fields={&xField,&yField};assetOwner.fields={&bgAssets};
 pick.pointer=reinterpret_cast<void*>(nativePick);getValues.pointer=reinterpret_cast<void*>(nativeValues);getOldValues.pointer=reinterpret_cast<void*>(nativeOldValues);mainClass.methods={&pick,&getValues,&getOldValues,&getMenu};
 set(menu,false);set(enabled,true);set(width,int32_t(320));set(position,std::array<float,2>{0,800});set(current,int32_t(3));set(previous,int32_t(3));
 auto* limits=array(&intArray,3);auto* caveStyles=array(&intArray,4);auto* tx=array(&assetArray,16);
 auto* p=static_cast<int32_t*>(arrayData(limits));p[0]=100;p[1]=200;p[2]=300;
 for(int i=0;i<16;++i){auto* a=object(&asset);images[a]=object(&texture);static_cast<Il2CppObject**>(arrayData(tx))[i]=a;}
 set(caveX,limits);set(caves,caveStyles);set(bgAssets,tx);
 localObject=object(&localClass);auto* numbers=array(&intArray,7);auto* oldNumbers=array(&intArray,7);
 std::memcpy(reinterpret_cast<char*>(localObject)+back.offset,&numbers,sizeof(numbers));std::memcpy(reinterpret_cast<char*>(localObject)+oldBack.offset,&oldNumbers,sizeof(oldNumbers));set(localInstance,localObject);
 values=array(&texArray,7);oldValues=array(&texArray,7);gameTid=gettid();
}
extern "C" __attribute__((visibility("default"))) int bunny_test_underground_initialize(){initialize();std::string error;return bl::runtime::underground::install(&error)?1:0;}
extern "C" __attribute__((visibility("default"))) void bunny_test_underground_callback(int(*callback)(int,int64_t)){managedCallback=callback;}
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_install_stage(const char* name,int slot){if(std::strcmp(name,"UndergroundBackground"))return -3;managedSlot=slot;bl::csharp::managedUndergroundDispatch([]{assert(managedCallback);managedCallback(managedSlot,++ticket);});return 1012;}
extern "C" __attribute__((visibility("default"))) void bunny_universal_hook_remove(int handle){assert(handle==1012);bl::csharp::removeManagedUndergroundDispatch();}
extern "C" __attribute__((visibility("default"))) int bunny_universal_hook_original(int,int64_t){return 1;}
extern "C" __attribute__((visibility("default"))) int64_t bunny_universal_hook_stat(int){return 0;}
extern "C" __attribute__((visibility("default"))) int bunny_background_texture_slot(const char*,const char* path){const std::string p=path;const auto digit=p.back();return p.find("ExampleBiomeUnderground")!=p.npos && digit>='0' && digit<='3'?10+digit-'0':-1;}
extern "C" __attribute__((visibility("default"))) int bunny_test_underground_draw(int vanilla){nativeVanilla=vanilla;const int selected=reinterpret_cast<bl::runtime::underground::Picker>(installed.at(&pick))(500,&pick);
 set(current,int32_t(selected));
 if(selected>=22)reinterpret_cast<bl::runtime::underground::Getter>(installed.at(&getValues))(object(&mainClass),&getValues);
 int32_t old;std::memcpy(&old,previous.data.data(),4);
 if(old>=22)reinterpret_cast<bl::runtime::underground::Getter>(installed.at(&getOldValues))(object(&mainClass),&getOldValues);
 return selected;}
extern "C" __attribute__((visibility("default"))) int bunny_test_underground_patch(int slot,int old){set(old?previous:current,int32_t(slot));auto fn=reinterpret_cast<bl::runtime::underground::Getter>(installed.at(old?&getOldValues:&getValues));auto* array=fn(object(&mainClass),old?&getOldValues:&getValues);int matches=0;auto** data=static_cast<Il2CppObject**>(arrayData(array));for(size_t i=0;i<std::min(size_t(5),size_t(array->length));++i)matches+=data[i]!=nullptr;return matches;}
extern "C" __attribute__((visibility("default"))) void bunny_test_underground_reset(){bl::runtime::underground::resetWorld();}
#ifndef BUNNY_UNDERGROUND_LIBRARY_ONLY
int main(){
 using namespace bl::runtime::underground;initialize();std::string error;
 auto classNameExport=exports.class_get_name;exports.class_get_name=nullptr;
 assert(!install(&error) && error.find("MetadataUnavailable")!=error.npos && installed.empty());exports.class_get_name=classNameExport;
 auto parameterExport=exports.method_get_param;exports.method_get_param=nullptr;
 assert(!install(&error) && error.find("MetadataUnavailable")!=error.npos && installed.empty());exports.method_get_param=parameterExport;
 auto parentExport=exports.class_get_parent;exports.class_get_parent=nullptr;
 assert(!install(&error) && error.find("MetadataUnavailable")!=error.npos && installed.empty());exports.class_get_parent=parentExport;
 auto methodsExport=exports.class_get_methods;exports.class_get_methods=nullptr;
 assert(!install(&error) && error.find("Missing=class_get_methods")!=error.npos && installed.empty());exports.class_get_methods=methodsExport;
 dbl.size=4;
 assert(!install(&error) && error.find("NativeMethodValueSizeMismatch")!=error.npos && error.find("Double:4")!=error.npos && installed.empty());dbl.size=8;
 auto originalMenuType=menu.type;menu.type.cls=&integer;
 assert(!install(&error) && error.find("NativeTypeOrSizeMismatch")!=error.npos && installed.empty());menu.type=originalMenuType;
 // Field/getter contracts and malformed storage are tested without writes.
 mainClass.fields.erase(mainClass.fields.begin());
 bl::il2cpp::CheckedProperty property;
 assert(property.bind(&mainClass,"gameMenu",bl::il2cpp::CheckedProperty::Kind::Boolean,true,&error));
 bool readMenu=true;assert(property.read(nullptr,&readMenu,&error) && !readMenu);
 getMenu.flags=0;assert(!property.bind(&mainClass,"gameMenu",bl::il2cpp::CheckedProperty::Kind::Boolean,true,&error));getMenu.flags=0x10;
 mainClass.fields.insert(mainClass.fields.begin(),&menu);
 const auto oldOffset=back.offset;back.offset=65536;
 assert(!property.bind(&localClass,"backTexture",bl::il2cpp::CheckedProperty::Kind::IntArray,false,&error));back.offset=oldOffset;
 back.type.byref=true;assert(!property.bind(&localClass,"backTexture",bl::il2cpp::CheckedProperty::Kind::IntArray,false,&error));back.type.byref=false;
 // Name and parameter count alone must not select an incompatible overload.
 MethodInfo wrongPicker=pick;wrongPicker.parameters={{&integer}};
 mainClass.methods.insert(mainClass.methods.begin(),&wrongPicker);
 assert(install(&error) && installed.contains(&pick) && !installed.contains(&wrongPicker));provider(Language::Managed,nullptr,fixtureFill);
 auto* ownedArray=*reinterpret_cast<Il2CppArray**>(bgAssets.data.data());
 const auto assetRoot=exports.gchandle_new(static_cast<Il2CppObject**>(arrayData(ownedArray))[10],true);
 assert(bl::runtime::backgroundAssets::adopt(10,assetRoot));
 int a=reserve("z-mod","Example",Language::Managed,&error),b=reserve("a-mod","Example",Language::Managed,&error);
 assert(a==22 && b==23 && reserve("z-mod","Example",Language::Managed,&error)==a);
 configured[a]={10,11,12,13,6};configured[b]={10,11,12,13,6};
 auto* caveArray=*reinterpret_cast<Il2CppArray**>(caves.data.data());auto* caveData=static_cast<int32_t*>(arrayData(caveArray));
 for(int i=0;i<4;++i)caveData[i]=i;
 assert(select(a,1,.5,true,&error));
 for(auto [x,cave]:std::vector<std::pair<float,int>>{{1440,3},{1456,4},{3056,5},{4656,6}}){set(position,std::array<float,2>{0,x});assert(bunny_test_underground_draw(cave)==a);}
 set(position,std::array<float,2>{0,800});fillCalls=vanillaCalls=0;
 assert(select(a,1,.5,true,&error));assert(bunny_test_underground_draw(3)==a && fillCalls==1 && vanillaCalls==1);
 assert(bunny_test_underground_draw(1)==1);assert(select(a,2,.5,true,&error));assert(bunny_test_underground_draw(1)==a && bunny_test_underground_draw(11)==a && bunny_test_underground_draw(9)==9);
 assert(select(a,3,.5,true,&error));assert(bunny_test_underground_draw(9)==a);
 assert(select(b,3,.5,true,&error));assert(bunny_test_underground_draw(3)==b); // stable UID tie-break
 assert(bunny_test_underground_patch(b,0)==5 && bunny_test_underground_patch(b,1)==5);
 auto* numbers=*reinterpret_cast<Il2CppArray**>(reinterpret_cast<char*>(localObject)+back.offset);auto* ids=static_cast<int32_t*>(arrayData(numbers));
 assert(ids[0]==10 && ids[3]==13 && ids[4]==6 && ids[5]==0 && ids[6]==0);
 auto** data=static_cast<Il2CppObject**>(arrayData(values));data[1]=nullptr;ids[1]=0;
 assert(bunny_test_underground_patch(b,0)==5 && ids[1]==11); // repair earlier layer, not only sentinel 3
 values=array(&texArray,7);assert(bunny_test_underground_patch(b,0)==5); // rebuilt render buffers
 const int before=writes;values=array(&texArray,4);assert(bunny_test_underground_patch(b,0)==0 && writes==before);values=array(&texArray,7);
 assert(!select(a,9,.5,true,&error));assert(!select(a,3,NAN,true,&error));
 set(menu,true);assert(bunny_test_underground_draw(3)==3);set(menu,false);set(enabled,false);assert(bunny_test_underground_draw(3)==3);set(enabled,true);
 // Fill executes without registry lock: worker-side selection cannot deadlock.
 duringFill=[&]{std::thread thread([&]{std::string detail;assert(select(a,3,.75,true,&detail));});thread.join();};
 assert(bunny_test_underground_draw(3)==b);duringFill={};assert(bunny_test_underground_draw(3)==a);
 // Transactional GC failure leaves the previously valid five-layer cache intact.
 const auto rootsBefore=handles.size();configured[a]={11,12,13,10,6};failRoot=1;assert(bunny_test_underground_draw(3)==a);failRoot=-1;assert(handles.size()==rootsBefore);
 configured[a]={9999,11,12,13,6};assert(bunny_test_underground_draw(3)==a); // rejected, prior valid cache retained
 const auto beforeStale=handles.size();
 duringFill=[]{resetWorld();};assert(bunny_test_underground_draw(3)==a);duringFill={};
 assert(handles.size()==beforeStale && !entries.at(a).active);
 // A getter cannot change the picker result already returned to Terraria.
 // Its stale callback must leave the last valid cache and next draw vanilla.
 assert(bunny_test_underground_draw(3)==3);
 resetWorld();assert(bunny_test_underground_draw(3)==3);retire(a);assert(find("z-mod","Example")==-1);
 configured[a]={10,11,12,13,6};assert(select(a,3,.5,true,&error)==false); // retired style cannot be selected
 assert(select(b,3,.5,true,&error));assert(bunny_test_underground_draw(3)==b);
 set(current,int32_t(b));set(previous,int32_t(b));
 bl::runtime::backgroundAssets::retire(10);assert(destroyed==0);
 static_cast<Il2CppObject**>(arrayData(ownedArray))[10]=nullptr;
 const int beforeLostSelection=fillCalls;
 assert(select(b,1,.5,true,&error));nativeVanilla=9;
 assert(reinterpret_cast<Picker>(installed.at(&pick))(500,&pick)==9 && fillCalls==beforeLostSelection);
 assert(select(b,3,.5,true,&error));
 assert(bunny_test_underground_draw(3)==3 && destroyed==0 && bunny_test_underground_patch(b,1)==5);
 set(current,int32_t(3));set(previous,int32_t(3));assert(bunny_test_underground_draw(3)==3 && destroyed==1);
 assert(!logs.empty());for(auto& [slot,own]:entries){(void)slot;release(own);}assert(handles.empty());
 for(void* p:allocations)std::free(p);
 std::cout<<"UNDERGROUND_NATIVE_ORIGINAL_PICKER_CURRENT_OLD_FIVE_LAYERS_GC_CONTRACTS_PRIORITY_AND_WORLD=PASS\n";
}
#endif
