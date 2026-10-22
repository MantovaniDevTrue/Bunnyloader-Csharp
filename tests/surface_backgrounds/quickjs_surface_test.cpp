#include "managed_surface_fixture.cpp"
#include "script/api/SurfaceBackgrounds.h"
#include "script/bridge/ScriptEngine.h"
#include "quickjs.h"
#include <fstream>
#include <sstream>
namespace bl::script {JsLock::JsLock(){held_=true;}JsLock::~JsLock(){}}
namespace {
JSRuntime* surfaceJsRuntime=nullptr;
JSContext* surfaceJsContext=nullptr;
void evalSurface(const std::string& source) {
    auto value=JS_Eval(surfaceJsContext,source.data(),source.size(),"example-surface-loader",JS_EVAL_TYPE_GLOBAL);
    if(JS_IsException(value)){auto ex=JS_GetException(surfaceJsContext);const char* message=JS_ToCString(surfaceJsContext,ex);std::cerr<<(message?message:"JS exception")<<'\n';JS_FreeCString(surfaceJsContext,message);JS_FreeValue(surfaceJsContext,ex);CHECK(false);}
    JS_FreeValue(surfaceJsContext,value);
}
std::string surfaceSource(const char* path){std::ifstream in(path);CHECK(in);std::stringstream result;result<<in.rdbuf();return result.str();}
}
extern "C" __attribute__((visibility("default"))) int bunny_test_surface_js_start() {
    surfaceJsRuntime=JS_NewRuntime();surfaceJsContext=JS_NewContext(surfaceJsRuntime);CHECK(surfaceJsContext);
    evalSurface(R"JS(
globalThis.bl={mod:{uuid:'js-surface-uid'}};
const Hooks={keys:new Set(),Once(key,fn){if(!this.keys.has(key)){this.keys.add(key);fn();}}};
const Ready={work:[],Add(fn){this.work.push(fn);}};
const Templates={Adopt(cls,inst){inst.Mod=bl.mod;}};
const Safe={Run(label,fn){return fn();}};
const Terraria={Main:{LocalPlayer:{},spriteBatch:{}},ID:{SurfaceBackgroundID:{Count:19}}};
class Ref {value=null;}
let selected={value:-1,priority:3,weight:.5},menuStyle=-1;
const SceneEffectLoader={Of(player){return {surfaceBackground:selected};}};
const MenuLoader={BackgroundSlot(){return menuStyle;}};
const BackgroundTextureLoader={GetBackgroundSlot(mod,path){
 if(path===undefined)path=mod;
 if(path.endsWith('ExampleBiomeSurfaceFar'))return 10;
 if(path.endsWith('ExampleBiomeSurfaceClose'))return 15;
 for(let i=0;i<4;i++)if(path.endsWith('ExampleBiomeSurfaceMid'+i))return 11+i;
 return -1;
}};
)JS");
    bl::script::installSurfaceBackgroundBindings(surfaceJsContext);
    evalSurface(surfaceSource("app/src/main/cpp/script/js/mod/Loaders/SurfaceBackgroundLoader.js"));
    evalSurface(surfaceSource("app/src/main/cpp/script/js/mod/ModSurfaceBackgroundStyle.js"));
    auto original=surfaceSource("tests/surface_backgrounds/reference/ExampleSurfaceBackgroundStyle.js");
    auto keyword=original.find("export class");CHECK(keyword!=original.npos);original.replace(keyword,12,"class");evalSurface(original);
    evalSurface("const first=ModSurfaceBackgroundStyle.register(ExampleSurfaceBackgroundStyle);for(const fn of Ready.work)fn();selected.value=first.Slot;SurfaceBackgroundLoader.Wake();");
    return surface::find("js-surface-uid","ExampleSurfaceBackgroundStyle");
}
extern "C" __attribute__((visibility("default"))) void bunny_test_surface_js_priority(int priority) {
    CHECK(priority>=0 && priority<=8);evalSurface("selected.priority="+std::to_string(priority)+";selected.weight=.75;SurfaceBackgroundLoader.Wake();");
}
extern "C" __attribute__((visibility("default"))) int bunny_test_surface_js_second() {
    evalSurface("bl.mod={uuid:'second-js-surface-uid'};class ExampleOtherSurface extends ExampleSurfaceBackgroundStyle{};const second=ModSurfaceBackgroundStyle.register(ExampleOtherSurface);if(SurfaceBackgroundLoader.Get(second.Slot)!==second)throw Error('shared ID gap lost');selected.value=second.Slot;SurfaceBackgroundLoader.Wake();");
    return surface::find("second-js-surface-uid","ExampleOtherSurface");
}
extern "C" __attribute__((visibility("default"))) void bunny_test_surface_js_stop() {
    bl::script::releaseSurfaceBackgroundBindings(surfaceJsContext);JS_FreeContext(surfaceJsContext);JS_FreeRuntime(surfaceJsRuntime);
    surfaceJsContext=nullptr;surfaceJsRuntime=nullptr;
}
#ifndef BUNNY_SURFACE_QJS_LIBRARY_ONLY
int main() {
    CHECK(bunny_test_surface_initialize());const int js=bunny_test_surface_js_start();CHECK(js==19 && runPreferred(0)==js);
    bunny_test_surface_scene(js,0);CHECK(bunny_test_surface_fades(js,0)==.4f);bunny_test_surface_scene(js,0);
    CHECK(bunny_test_surface_layer(0)==10);
    for(int i=1;i<=53;++i)CHECK(bunny_test_surface_layer(1)==11+(i/13)%4);
    CHECK(bunny_test_surface_layer(2)==15);
    std::string error;const int managed=surface::reserve("a-managed-gap","ExampleManaged",surface::Language::Managed,&error);CHECK(managed==20);
    const int second=bunny_test_surface_js_second();CHECK(second==21 && runPreferred(4)==second);
    bunny_test_surface_scene(second,0);CHECK(bunny_test_surface_layer(0)==10);
    evalSurface("second.ChooseFarTexture=()=>{throw Error('original-js-surface-exception');};");
    CHECK(bunny_test_surface_layer(0)==10); // the independent outgoing first style still renders
    bool reported=false;for(auto& log:logs)reported|=log.find("original-js-surface-exception")!=log.npos;
    CHECK(reported);
    evalSurface("second.ModifyFarFades=values=>Object.defineProperty(values,1,{get(){throw Error('surface-fade-getter-exception');}});");
    foreground(nullable(true,second),nullable(true,.1f));
    reported=false;for(auto& log:logs)reported|=log.find("surface-fade-getter-exception")!=log.npos;CHECK(reported);
    evalSurface("second.PreDrawCloseBackground=()=>false;second.ChooseCloseTexture=()=>{throw Error('suppressed-close-must-not-run');};");
    // Remove other visible styles before checking close suppression.
    alpha(js,true,0);alpha(js,false,0);CHECK(bunny_test_surface_layer(2)==-1);
    evalSurface("selected.priority=Symbol('bad');SurfaceBackgroundLoader.Wake();");CHECK(runPreferred(0)==0);
    bunny_test_surface_js_stop();CHECK(runPreferred(0)==0);
    for(auto& [id,e]:surface::entries){(void)id;for(auto& layer:e.layers)surface::release(layer);}
    for(int slot=10;slot<=15;++slot)bl::runtime::backgroundAssets::retire(slot);
    bl::runtime::backgroundAssets::sweep();CHECK(handles.empty());
    for(void* value:allocations)std::free(value);
    std::cout<<"SURFACE_ORIGINAL_JS_REAL_QUICKJS_SIX_HOOKS_SHARED_ID_GAPS_53_FRAME_CALLS_FADES_ERRORS_AND_SUPPRESSION=PASS checks="<<checked<<'\n';
}
#endif
