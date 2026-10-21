#include "il2cpp/Api.h"
struct Il2CppClass { const char* name;int size;bool value=true;const char* ns="System"; };
struct Il2CppType { Il2CppClass* cls; };
struct FieldInfo { const char* name;Il2CppType type;uint32_t flags=0x10;double number=1; };
#include "content/systems/SystemTimeRates.cpp"
#include "script/api/SystemTimeRates.h"
#include "quickjs.h"
#include <cassert>
#include <cstring>
#include <cstdarg>
#include <fstream>
#include <sstream>
#include <iostream>
namespace {
bl::il2cpp::Api fixture;
Il2CppClass single{"Single",4},integer{"Int32",4},owner{"Main",0,false,"Terraria"};
FieldInfo day{"dayRate",{&single}},tile{"desiredWorldTilesUpdateRate",{&integer}};
int tid=0,writes=0,vanillaCalls=0;
}
namespace bl::il2cpp {
Api& api(){return fixture;}
Il2CppClass* findClassQuiet(const std::string& ns,const std::string& name){return ns=="Terraria" && name=="Main"?&owner:nullptr;}
FieldInfo* findField(Il2CppClass* cls,std::string_view name){return cls!=&owner?nullptr:name==day.name?&day:name==tile.name?&tile:nullptr;}
}
namespace bl::runtime {int gameThreadId(){return tid;}}
namespace bl::log {void write(int,const char*,...) {}}
namespace rates=bl::runtime::systems;
JSValue vanilla(JSContext*,JSValueConst,int,JSValueConst*) {++vanillaCalls;day.number=tile.number=1;return JS_UNDEFINED;}
JSValue nativeCsHook(JSContext* ctx,JSValueConst,int,JSValueConst*) {
 std::string error;assert(rates::beginTimeRates(&error));vanilla(ctx,JS_UNDEFINED,0,nullptr);
 rates::TimeRates r;assert(rates::readTimeRates(&r,&error));assert(r.time==1 && r.tiles==1 && r.events==1);
 assert(rates::publishTimeRates({.5,.25,.25},&error));int before=writes;assert(rates::endTimeRates(&error));assert(writes==before);return JS_UNDEFINED;
}
void eval(JSContext* ctx,const std::string& source,bool expectFailure=false) {
 JSValue result=JS_Eval(ctx,source.data(),source.size(),"real-system-loader-test",JS_EVAL_TYPE_GLOBAL);
 const bool failed=JS_IsException(result);
 if(failed){auto exception=JS_GetException(ctx);const char* message=JS_ToCString(ctx,exception);
  if(!expectFailure)std::cerr<<(message?message:"JS exception without message")<<"\n";
  else assert(message && std::string(message).find("original-failure")!=std::string::npos);
  JS_FreeCString(ctx,message);JS_FreeValue(ctx,exception);
 }
 JS_FreeValue(ctx,result);assert(failed==expectFailure);
}
int main(){
 fixture.field_get_flags=[](FieldInfo* f){return f->flags;};
 fixture.field_get_type=[](FieldInfo* f)->const Il2CppType*{return &f->type;};
 fixture.class_from_il2cpp_type=[](const Il2CppType* t){return t->cls;};
 fixture.class_get_name=[](Il2CppClass* c){return c->name;};
 fixture.class_get_namespace=[](Il2CppClass* c){return c->ns;};
 fixture.class_is_valuetype=[](const Il2CppClass* c){return c->value;};
 fixture.class_value_size=[](Il2CppClass* c,uint32_t*){return int32_t(c->size);};
 fixture.field_static_get_value=[](FieldInfo* f,void* out){
  if(f->type.cls==&single){float v=f->number;std::memcpy(out,&v,4);}else{int32_t v=f->number;std::memcpy(out,&v,4);}
 };
 fixture.field_static_set_value=[](FieldInfo* f,void* in){
  ++writes;if(f->type.cls==&single){float v;std::memcpy(&v,in,4);f->number=v;}else{int32_t v;std::memcpy(&v,in,4);f->number=v;}
 };
 auto* runtime=JS_NewRuntime();assert(runtime);auto* ctx=JS_NewContext(runtime);assert(ctx);
 eval(ctx,"globalThis.bl={};");
 bl::script::installSystemTimeRateBindings(ctx);
 JSValue global=JS_GetGlobalObject(ctx);
 JS_SetPropertyStr(ctx,global,"fixtureVanilla",JS_NewCFunction(ctx,vanilla,"fixtureVanilla",0));
 JS_SetPropertyStr(ctx,global,"fixtureCSharpNativeHook",JS_NewCFunction(ctx,nativeCsHook,"fixtureCSharpNativeHook",0));
 JS_FreeValue(ctx,global);
 eval(ctx,R"JS(
 class Ref {constructor(value){this.value=value;}}
 class ModSystem {ModifyTimeRate() {}}
 let timeHook,clockHook,modify;
 const Hooks={Overrides:(cls,base,name)=>cls.prototype[name]!==base.prototype[name],Once:(name,fn)=>fn()};
 const SystemLoader={Call:(name,...args)=>modify(...args)};
 const Terraria={Main:{'void UpdateTimeRate()':{hook:fn=>{timeHook=fn;}}}};
 bl.installSystemStage=(name,fn)=>{if(name!=='ClockAdvance')throw Error('unexpected stage');clockHook=fn;};
 )JS");
 std::ifstream file("app/src/main/cpp/script/js/mod/Loaders/SystemUpdateHooks.js");assert(file);
 std::stringstream buffer;buffer<<file.rdbuf();eval(ctx,buffer.str());
 // Metadata-only preparation occurs at registration, off the game thread.
 eval(ctx,"class ExampleRates extends ModSystem {ModifyTimeRate(){}}; SystemUpdateHooks.Install(ExampleRates);");
 assert(!writes && !vanillaCalls);tid=gettid();std::string error;rates::TimeRates r;
 for(int order=0;order<2;++order){
  rates::observeTimeRateMenu(true);rates::observeTimeRateMenu(false);
  for(int frame=0;frame<4;++frame){
   if(order==0){
    assert(rates::beginTimeRates(&error));
    eval(ctx,"modify=(time,tiles,events)=>{time.value=.25;tiles.value=.5;events.value=.75;};timeHook(fixtureVanilla);");
    assert(rates::readTimeRates(&r,&error));assert(r.time==.25 && r.tiles==.5 && r.events==.75);
    assert(rates::publishTimeRates({r.time+.5,r.tiles+.25,r.events+.25},&error));assert(rates::endTimeRates(&error));
   }else{
    eval(ctx,R"JS(modify=(time,tiles,events)=>{
     if(time.value!==.5 || tiles.value!==.25 || events.value!==.25)throw Error('inner native fractions lost');
     time.value+=.25;tiles.value+=.5;events.value+=.75;
    };timeHook(fixtureCSharpNativeHook);)JS");
   }
   assert(rates::clockRate()==.75 && day.number==1 && tile.number==(frame==0?0:1));
   eval(ctx,"if(clockHook()!==.75)throw Error('clock hook differs');");
  }
 }
 assert(vanillaCalls==8 && writes==16);
 rates::observeTimeRateMenu(true);rates::observeTimeRateMenu(false);
 eval(ctx,"timeHook(()=>{throw Error('original-failure');});",true);
 assert(!rates::readTimeRates(&r,&error) && error.find("NoActiveTransaction")!=error.npos);
 assert(writes==16 && std::isnan(rates::clockRate()));
 eval(ctx,"modify=(time,tiles,events)=>{time.value=NaN;tiles.value=-1;events.value=Infinity;};timeHook(fixtureVanilla);");
 assert(rates::clockRate()==1 && day.number==1 && tile.number==1);
 JS_FreeContext(ctx);JS_FreeRuntime(runtime);
 std::cout<<"REAL_QUICKJS_ORIGINAL_SYSTEM_LOADER_PRODUCTION_NATIVE_BINDINGS_NESTED_RATES_BOTH_ORDERS=PASS\n";
}
