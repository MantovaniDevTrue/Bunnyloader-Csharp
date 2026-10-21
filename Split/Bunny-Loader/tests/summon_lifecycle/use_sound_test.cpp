#include "experiments/csharp/NativeUseSounds.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <set>
#include <cstring>
#include <sstream>
#include <chrono>
struct Il2CppClass {const char* ns;const char* name;uint32_t size;bool value;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;size_t offset;uint32_t flags=0;};
struct MethodInfo {bool instance=true;};
bl::il2cpp::Api fixture;
Il2CppClass itemClass{"Terraria","Item",128,false},legacy{"Terraria.Audio","LegacySoundStyle",64,false},soundType{"Terraria.Audio","SoundType",4,true};
FieldInfo soundField{{&legacy},32};MethodInfo constructor;
std::map<uint32_t,Il2CppObject*> roots;
std::set<int> liveStyles;
uint32_t nextRoot=1;int nextStyle=0,loads=0,writes=0,invocations=0;
bool failRouter=false,failLoad=false,failState=false,failCtor=false,missingCtor=false,incremental=true;
float lastVolume=0,lastPitch=0,lastVariance=0;int lastMax=-1,lastLimit=-1;
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& ref){return ref.name=="Item"?&itemClass:ref.name=="LegacySoundStyle"?&legacy:&soundType;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){return cls==&itemClass&&name=="UseSound"?&soundField:nullptr;}
Signature parseSignature(std::string_view text){Signature s;s.name=text;return s;}
const MethodInfo* findMethodBySignature(Il2CppClass* cls,const Signature& sig,bool*,const MethodInfo**) {
 assert(cls==&legacy&&sig.name=="void .ctor(int soundId, int style, SoundType type, int maxTrackedInstances)");return missingCtor?nullptr:&constructor;
}
}
namespace bl::runtime::content {
bool installNativeSoundRouter(){return !failRouter;}
int loadSound(const std::string& path){assert(std::filesystem::is_regular_file(path));++loads;return failLoad?0:7;}
int soundState(int id){assert(id==7);return failState?-1:0;}
int registerSoundStyle(int sound,float volume,float pitch,float variance,int max,int limit){assert(sound==7);lastVolume=volume;lastPitch=pitch;lastVariance=variance;lastMax=max;lastLimit=limit;int id=nextStyle++;liveStyles.insert(id);return id;}
void releaseSoundStyle(int style){assert(liveStyles.erase(style)==1);}
}
int main(int argc,char** argv) {
 using namespace bl::csharp::useSounds;
 assert(argc==3);
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};fixture.field_get_offset=[](FieldInfo* f){return f->offset;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};fixture.class_instance_size=[](Il2CppClass* c){return c->size;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t* a){*a=4;return static_cast<int32_t>(c->size);};
 fixture.class_is_enum=[](const Il2CppClass* c){return c==&soundType;};fixture.method_is_instance=[](const MethodInfo* m){return m->instance;};
 fixture.gc_is_incremental=[](){return incremental;};
 fixture.object_new=[](Il2CppClass* c)->Il2CppObject*{assert(c==&legacy);return reinterpret_cast<Il2CppObject*>(new char[64]{});};
 fixture.gchandle_new=[](Il2CppObject* o,bool pinned){assert(!pinned);uint32_t id=nextRoot++;roots[id]=o;return id;};
 fixture.gchandle_get_target=[](uint32_t id){return roots.at(id);};
 fixture.gchandle_free=[](uint32_t id){auto it=roots.find(id);assert(it!=roots.end());delete[] reinterpret_cast<char*>(it->second);roots.erase(it);};
 fixture.gc_wbarrier_set_field=[](Il2CppObject*,void** target,void* value){++writes;*target=value;};
 fixture.runtime_invoke=[](const MethodInfo* method,void* object,void** args,Il2CppObject** exception)->Il2CppObject* {
  assert(method==&constructor&&object);assert(*static_cast<int*>(args[0])==1000&&*static_cast<int*>(args[2])==0&&*static_cast<int*>(args[3])==0);
  int style=*static_cast<int*>(args[1]);assert(liveStyles.contains(style));++invocations;
  if(failCtor)*exception=reinterpret_cast<Il2CppObject*>(1);
  return nullptr;
 };
 Entry e{"independent","ExampleGun","Independent/Gun.cs",argv[1],"Assets/Sounds/Items/Guns/ExampleGun.ogg",6240,.9f,0,.2f,3,1};
 std::ifstream wire(argv[2]);std::string row;bool found=false;
 while(std::getline(wire,row))if(row.starts_with("USE_SOUND\tExampleGun\t")) {
  std::istringstream stream(row);std::vector<std::string> fields;std::string field;
  while(std::getline(stream,field,'\t'))fields.push_back(field);
  std::string decodeError;assert(decode(fields,e,&e,&decodeError));found=true;
  auto malformed=fields;malformed[2]="00";assert(!decode(malformed,e,&e,&decodeError));
  malformed=fields;malformed[3]="nan";assert(!decode(malformed,e,&e,&decodeError));
 }
 assert(found);
 std::string error;assert(publish({e},&error));assert(lastVolume==.9f&&lastVariance==.2f&&lastPitch==0&&lastMax==3&&lastLimit==1);
 assert(roots.size()==1&&liveStyles.size()==1&&invocations==1);
 const int firstLoads=loads;assert(publish({e},&error)&&loads==firstLoads&&invocations==1);
 Entry shared=e;shared.type++;shared.name="AlternateGun";
 assert(publish({shared},&error)&&loads==firstLoads&&invocations==1&&roots.size()==1&&liveStyles.size()==1);
 Entry changed=e;changed.volume=.8f;assert(!publish({changed},&error)&&error.find("ItemSoundAlreadyConfigured")!=std::string::npos&&loads==firstLoads);
 alignas(8) char bytes[128];std::memset(bytes,0x5a,sizeof(bytes));auto* item=reinterpret_cast<Il2CppObject*>(bytes);
 apply(item,e.type);void* marker=nullptr;std::memcpy(&marker,bytes+32,sizeof(marker));assert(marker==roots.begin()->second&&writes==1);
 assert(static_cast<unsigned char>(bytes[31])==0x5a&&static_cast<unsigned char>(bytes[40])==0x5a);
 apply(item,1);assert(writes==1);
 auto unchanged=std::atomic_load(&table);const int before=loads;
 Entry bad=e;bad.path="missing.ogg";assert(!publish({e,bad},&error)&&loads==before&&std::atomic_load(&table)==unchanged);
 bad.type++;assert(!publish({e,bad},&error)&&error.find("AssetMissing")!=std::string::npos&&loads==before);
 bad=e;bad.path="../outside.ogg";assert(!publish({bad},&error)&&error.find("InvalidAssetPath")!=std::string::npos);
 const auto temporary=std::filesystem::temp_directory_path()/("bunny-sound-boundary-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 std::filesystem::create_directories(temporary/"content");
 std::filesystem::create_symlink(std::filesystem::canonical(std::filesystem::path(e.directory)/"content"/e.path),temporary/"content"/"escape.ogg");
 bad=e;bad.directory=temporary.string();bad.path="escape.ogg";
 assert(!publish({bad},&error)&&error.find("AssetOutsideModContent")!=std::string::npos&&loads==before);
 std::filesystem::remove_all(temporary);
 bad=e;bad.variance=std::numeric_limits<float>::quiet_NaN();assert(!publish({bad},&error)&&loads==before);
 soundField.offset=125;assert(!publish({e},&error)&&error.find("NativeSoundFieldMismatch")!=std::string::npos);soundField.offset=33;assert(!publish({e},&error)&&error.find("NativeFieldAlignmentMismatch")!=std::string::npos);soundField.offset=32;
 soundField.flags=0x10;assert(!publish({e},&error));soundField.flags=0;
 soundField.flags=0x20;assert(!publish({e},&error)&&error.find("InitOnly")!=std::string::npos);soundField.flags=0;
 missingCtor=true;assert(!publish({e},&error)&&error.find("NativeConstructorMismatch")!=std::string::npos);missingCtor=false;
 failRouter=true;assert(!publish({e},&error)&&loads==before);failRouter=false;
 Entry independent=e;independent.type+=2;independent.name="IndependentGun";independent.pitch=.25f;
 Entry failedShared=shared;failedShared.type+=10;failedShared.name="FailedShared";
 failState=true;assert(!publish({failedShared},&error)&&error.find("NativeAudioStateFailed")!=std::string::npos&&roots.size()==1);failState=false;
 failCtor=true;assert(!publish({independent},&error)&&roots.size()==1&&liveStyles.size()==1&&std::atomic_load(&table)==unchanged);failCtor=false;
 const auto freshRoot=std::filesystem::temp_directory_path()/("bunny-sound-load-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 std::filesystem::create_directories(freshRoot/"content");
 std::filesystem::copy_file(std::filesystem::path(e.directory)/"content"/e.path,freshRoot/"content"/"fresh.ogg");
 Entry fresh=independent;fresh.directory=freshRoot.string();fresh.path="fresh.ogg";
 failLoad=true;assert(!publish({fresh},&error)&&error.find("NativeAudioLoadRejected")!=std::string::npos);failLoad=false;
 std::filesystem::remove_all(freshRoot);
 auto barrier=fixture.gc_wbarrier_set_field;fixture.gc_wbarrier_set_field=nullptr;
 assert(!publish({e},&error)&&error.find("RequiredExportMissing")!=std::string::npos);
 incremental=false;const int previousLoads=loads;assert(publish({independent},&error)&&loads==previousLoads);apply(item,independent.type);fixture.gc_wbarrier_set_field=barrier;
 unchanged.reset();std::atomic_store(&table,std::make_shared<const Table>());
 assert(roots.empty()&&liveStyles.empty());
 std::cout<<"CSHARP_NATIVE_USE_SOUND_PRODUCTION_ASSET_CONFINEMENT_TYPED_FIELD_CTOR_GC_ROOTS_BARRIER_SHARED_AUDIO_ROLLBACK_AND_COMPILED_GUN_PASS\n";
}
