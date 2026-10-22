#define BUNNY_UNDERGROUND_LIBRARY_ONLY
#include "native_underground_test.cpp"
#include "script/api/UndergroundBackgrounds.h"
#include "script/bridge/ScriptEngine.h"
#include "quickjs.h"
#include <fstream>
#include <sstream>
namespace bl::script {
JsLock::JsLock(){held_=true;}
JsLock::~JsLock(){}
}
static void eval(JSContext* ctx,const std::string& source){
 auto value=JS_Eval(ctx,source.data(),source.size(),"original-underground-loader",JS_EVAL_TYPE_GLOBAL);
 if(JS_IsException(value)){auto ex=JS_GetException(ctx);const char* message=JS_ToCString(ctx,ex);std::cerr<<(message?message:"JS error")<<"\n";JS_FreeCString(ctx,message);JS_FreeValue(ctx,ex);assert(false);}
 JS_FreeValue(ctx,value);
}
static std::string read(const char* name){std::ifstream in(name);assert(in);std::stringstream text;text<<in.rdbuf();return text.str();}
#ifndef BUNNY_QJS_NO_MAIN
int main(){
 using namespace bl::runtime::underground;initialize();std::string error;
 auto* rt=JS_NewRuntime();auto* ctx=JS_NewContext(rt);assert(rt && ctx);
 eval(ctx,R"JS(
 globalThis.bl={mod:{uuid:'z-js-mod'}};
 const Hooks={keys:new Set(),Once(key,fn){if(!this.keys.has(key)){this.keys.add(key);fn();}}};
 const Ready={work:[],Add(fn){this.work.push(fn);}};
 const Templates={Adopt(cls,inst){inst.Mod=bl.mod;}};
 const Safe={Run(label,fn){return fn();}};
 const Terraria={Main:{LocalPlayer:{}}};
 let selected={value:-1,priority:0,weight:0};
 const SceneEffectLoader={Of(player){return {undergroundBackground:selected};}};
 const BackgroundTextureLoader={GetBackgroundSlot(mod,path){return 10+Number(path.at(-1));}};
 )JS");
 bl::script::installUndergroundBackgroundBindings(ctx);
 eval(ctx,read("app/src/main/cpp/script/js/mod/Loaders/UndergroundBackgroundLoader.js"));
 eval(ctx,read("app/src/main/cpp/script/js/mod/ModUndergroundBackgroundStyle.js"));
 std::string original=read("tests/underground_backgrounds/reference/ExampleUndergroundBackgroundStyle.js");
 auto keyword=original.find("export class");assert(keyword!=original.npos);original.replace(keyword,12,"class");eval(ctx,original);
 eval(ctx,"const first=ModUndergroundBackgroundStyle.register(ExampleUndergroundBackgroundStyle);for(const fn of Ready.work)fn();selected={value:first.Slot,priority:3,weight:.5};");
 const int cs=reserve("a-cs-mod","ExampleCSharp",Language::Managed,&error);configured[cs]={10,11,12,13,6};provider(Language::Managed,nullptr,fixtureFill);
 assert(cs==23); // JS first, C# in the gap before the next JS registration.
 eval(ctx,"class ExampleOtherUnderground extends ExampleUndergroundBackgroundStyle{};bl.mod={uuid:'other-js-mod'};const second=ModUndergroundBackgroundStyle.register(ExampleOtherUnderground);if(second.Slot!==24 || UndergroundBackgroundLoader.Get(24)!==second)throw Error('shared ID gap lost');");
 assert(bunny_test_underground_draw(3)==22);assert(bunny_test_underground_patch(22,0)==5);
 assert(select(cs,3,.5,true,&error));assert(bunny_test_underground_draw(3)==cs); // UID tie-break, not install order.
 eval(ctx,"selected={value:second.Slot,priority:4,weight:0};");assert(bunny_test_underground_draw(9)==24);assert(bunny_test_underground_patch(24,1)==5);
 // Source classes remain normal JS classes; exceptions preserve original detail.
 eval(ctx,"second.FillTextureArray=()=>{throw Error('example-fill-failure');};");
 assert(bunny_test_underground_draw(3)==24);bool reported=false;for(auto& line:logs)reported|=line.find("example-fill-failure")!=line.npos;assert(reported);
 eval(ctx,"selected={value:second.Slot,priority:Symbol('bad'),weight:.5};");assert(bunny_test_underground_draw(3)==cs);
 eval(ctx,"selected={value:first.Slot,priority:3,weight:1};");assert(bunny_test_underground_draw(3)==22);
 bl::script::releaseUndergroundBackgroundBindings(ctx);assert(bunny_test_underground_draw(3)==cs);
 JS_FreeContext(ctx);JS_FreeRuntime(rt);
 for(auto& [slot,own]:entries){(void)slot;release(own);}for(void* p:allocations)std::free(p);
 std::cout<<"ORIGINAL_EXAMPLE_JS_REAL_QUICKJS_NATIVE_UNDERGROUND_SHARED_IDS_PRIORITY_CURRENT_PREVIOUS_AND_ISOLATED_ERRORS=PASS\n";
}
#endif
