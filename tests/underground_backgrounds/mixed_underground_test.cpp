#define BUNNY_QJS_NO_MAIN
#include "quickjs_underground_test.cpp"
namespace {JSRuntime* mixedRuntime=nullptr;JSContext* mixedContext=nullptr;}
extern "C" __attribute__((visibility("default"))) int bunny_test_underground_js_start(){
 mixedRuntime=JS_NewRuntime();mixedContext=JS_NewContext(mixedRuntime);assert(mixedRuntime && mixedContext);
 eval(mixedContext,R"JS(
 globalThis.bl={mod:{uuid:'js-mixed-uid'}};
 const Hooks={keys:new Set(),Once(key,fn){if(!this.keys.has(key)){this.keys.add(key);fn();}}};
 const Ready={work:[],Add(fn){this.work.push(fn);}};
 const Templates={Adopt(cls,inst){inst.Mod=bl.mod;}};
 const Safe={Run(label,fn){return fn();}};
 const Terraria={Main:{LocalPlayer:{}}};
 let selected={value:-1,priority:3,weight:.5};
 const SceneEffectLoader={Of(player){return {undergroundBackground:selected};}};
 const BackgroundTextureLoader={GetBackgroundSlot(mod,path){return 10+Number(path.at(-1));}};
 )JS");
 bl::script::installUndergroundBackgroundBindings(mixedContext);
 eval(mixedContext,read("app/src/main/cpp/script/js/mod/Loaders/UndergroundBackgroundLoader.js"));
 eval(mixedContext,read("app/src/main/cpp/script/js/mod/ModUndergroundBackgroundStyle.js"));
 auto original=read("tests/underground_backgrounds/reference/ExampleUndergroundBackgroundStyle.js");
 auto keyword=original.find("export class");assert(keyword!=original.npos);original.replace(keyword,12,"class");eval(mixedContext,original);
 eval(mixedContext,"const first=ModUndergroundBackgroundStyle.register(ExampleUndergroundBackgroundStyle);for(const fn of Ready.work)fn();selected.value=first.Slot;");
 return bl::runtime::underground::find("js-mixed-uid","ExampleUndergroundBackgroundStyle");
}
extern "C" __attribute__((visibility("default"))) void bunny_test_underground_js_priority(int priority){
 assert(priority>=0 && priority<=8);eval(mixedContext,"selected.priority="+std::to_string(priority)+";selected.weight=.75;");
}
extern "C" __attribute__((visibility("default"))) void bunny_test_underground_js_stop(){
 bl::script::releaseUndergroundBackgroundBindings(mixedContext);JS_FreeContext(mixedContext);JS_FreeRuntime(mixedRuntime);
 mixedContext=nullptr;mixedRuntime=nullptr;
}
