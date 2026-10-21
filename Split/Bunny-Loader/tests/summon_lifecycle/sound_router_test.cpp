#include "content/sounds/NativeSoundRouter.cpp"
#include <cassert>
#include <iostream>
#include <map>
struct Il2CppClass {const char* ns;const char* name;int size;bool value;};
struct Il2CppType {Il2CppClass* cls;};
struct FieldInfo {Il2CppType type;uint32_t flags=0x10;};
struct MethodInfo {bool instance;Il2CppType type;};
bl::il2cpp::Api fixture;
Il2CppClass mainClass{"Terraria","Main",64,false},playerClass{"Terraria.Audio","LegacySoundPlayer",64,false},
 integerClass{"System","Int32",4,true},floatClass{"System","Single",4,true},vectorClass{"Microsoft.Xna.Framework","Vector2",8,true};
FieldInfo position{{&vectorClass}},width{{&integerClass}},height{{&integerClass}},volume{{&floatClass}},attenuation{{&floatClass}};
MethodInfo playMethod{true,{&floatClass}},widthGetter{false,{&integerClass}};
struct Boxed {Il2CppObject obj{};int value=1000;} boxed;
int getterInvocations=0;
bool allowGetter=false;int getterParams=0,nextHandle=1,forcedHandle=0,enqueueCalls=0,stopCalls=0,originalCalls=0;
std::map<int,int> states;float leftValue=0,rightValue=0,rateValue=0;
Il2CppObject* original(Il2CppObject*,int32_t,int32_t,int32_t,int32_t,float,float,const MethodInfo*) {++originalCalls;return nullptr;}
namespace bl::log {void write(int,const char*,...) {}}
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClass(const TypeRef& type){return type.name=="Main"?&mainClass:&playerClass;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){
 if(cls==&playerClass)return name=="SoundAttenuationDistance"?&attenuation:nullptr;
 return name=="screenPosition"?&position:name=="screenWidth"?&width:name=="screenHeight"?&height:name=="soundVolume"?&volume:nullptr;
}
Signature parseSignature(std::string_view text){Signature s;s.name=text;return s;}
const MethodInfo* findMethodBySignature(Il2CppClass*,const Signature& sig,bool*,const MethodInfo**) {
 assert(sig.name=="SoundEffectInstance PlaySound(int type, int x, int y, int Style, float volumeScale, float pitchOffset)");return &playMethod;
}
}
namespace bl::hook {bool install(const MethodInfo*,void*,void** out){*out=reinterpret_cast<void*>(original);return true;}}
namespace bl::runtime::content {
int enqueueSound(int group,int id,float left,float right,float rate,int max,bool ignore) {
 assert(group==-id&&id>0&&max==0&&!ignore);++enqueueCalls;leftValue=left;rightValue=right;rateValue=rate;
 int handle=forcedHandle?forcedHandle:nextHandle++;if(handle>0)states[handle]=1;return handle;
}
void cancelSound(int handle){++stopCalls;states[handle]=4;}
int playbackState(int handle){return states[handle];}
}
int main() {
 using namespace bl::runtime::content;
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};fixture.class_get_name=[](Il2CppClass* c){return c->name;};fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};fixture.class_value_size=[](Il2CppClass* c,uint32_t* a){*a=4;return c->size;};
 fixture.runtime_class_init=[](Il2CppClass*){};
 fixture.field_static_get_value=[](FieldInfo* f,void* out){
  if(f==&position){float xy[2]={0,0};std::memcpy(out,xy,8);}
  else if(f==&width||f==&height){int x=f==&width?1000:600;std::memcpy(out,&x,4);}
  else {float x=f==&volume?.5f:2500;std::memcpy(out,&x,4);}
 };
 fixture.method_is_instance=[](const MethodInfo* m){return m->instance;};fixture.method_get_param_count=[](const MethodInfo*){return static_cast<uint32_t>(getterParams);};
 fixture.method_get_return_type=[](const MethodInfo* m)->const Il2CppType*{return &m->type;};
 fixture.class_get_method_from_name=[](Il2CppClass*,const char* name,int)->const MethodInfo*{return allowGetter&&std::string(name)=="get_screenWidth"?&widthGetter:nullptr;};
 fixture.runtime_invoke=[](const MethodInfo* method,void*,void**,Il2CppObject**)->Il2CppObject*{assert(method==&widthGetter);++getterInvocations;return &boxed.obj;};
 assert(installNativeSoundRouter());
 int style=registerSoundStyle(7,.9f,0,0,1,0);assert(style==0);
 int first=playSoundStyle(style,-1,-1,1,0);assert(first==kAsyncSoundTokenBase+1&&enqueueCalls==1&&std::fabs(leftValue-.45f)<.001&&leftValue==rightValue&&rateValue==1);
 assert(playSoundStyle(style,-1,-1,1,0)==0&&enqueueCalls==1); // IgnoreNew preserves live instance.
 states[1]=3;assert(findActiveSoundStyle(style)==first); // Paused is live, not a wall-clock timeout.
 stopSoundStyleToken(first);assert(states[1]==4&&findActiveSoundStyle(style)==0);
 int replace=registerSoundStyle(8,1,0,0,1,1);
 int a=playSoundStyle(replace,-1,-1,1,0);int b=playSoundStyle(replace,-1,-1,1,0);
 assert(a!=b&&states[a-kAsyncSoundTokenBase]==4&&findActiveSoundStyle(replace)==b);
 playSoundHook(nullptr,1,0,0,0,1,0,&playMethod);assert(originalCalls==1);
 playSoundHook(nullptr,1000,-1,-1,replace,1,0,&playMethod);assert(originalCalls==1);
 int placeholder=registerSoundStyle(0,1,0,0,1,1);releaseSoundStyle(style);
 assert(registerSoundStyle(9,1,0,0,1,1)==style);assert(g_styles[placeholder].soundId==0&&!g_styles[placeholder].released);
 forcedHandle=-1;assert(playSoundStyle(style,-1,-1,1,0)==0);forcedHandle=kAsyncSoundTokenBase;
 assert(playSoundStyle(style,-1,-1,1,0)==0&&states[kAsyncSoundTokenBase]==4);forcedHandle=0;
 for(int n=0;n<10000;++n){int token=playSoundStyle(style,-1,-1,1,0);states[token-kAsyncSoundTokenBase]=0;}
 assert(findActiveSoundStyle(style)==0&&g_styles[style].active.empty());
 g_hooked=false;width.type.cls=&floatClass;assert(registerSoundStyle(10,1,0,0,1,1)==-1); // No undersized/wrong-type static read.
 allowGetter=true;assert(installNativeSoundRouter());assert(playSoundStyle(style,500,300,1,0)>0&&getterInvocations==1&&leftValue==.5f&&rightValue==.5f);g_hooked=false;widthGetter.instance=true;assert(!installNativeSoundRouter());widthGetter.instance=false;
 widthGetter.type.cls=&floatClass;assert(!installNativeSoundRouter());widthGetter.type.cls=&integerClass;getterParams=1;assert(!installNativeSoundRouter());getterParams=0;
 width.type.cls=&integerClass;allowGetter=false;playMethod.instance=false;assert(!installNativeSoundRouter());playMethod.instance=true;
 assert(installNativeSoundRouter());
 std::cout<<"NATIVE_SOUND_ROUTER_PRODUCTION_TYPED_AUDIO_FIELDS_GETTERS_LEGACY_JS_TOKENS_NATIVE_BOUNDED_QUEUE_LIMITS_PAUSE_REJECTION_RELEASE_REUSE_AND_10000_COMPLETIONS_PASS\n";
}
