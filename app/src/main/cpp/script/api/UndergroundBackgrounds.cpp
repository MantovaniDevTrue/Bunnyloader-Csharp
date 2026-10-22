#include "script/api/UndergroundBackgrounds.h"
#include "content/backgrounds/UndergroundBackgrounds.h"
#include "script/bridge/ScriptEngine.h"
#include "core/Log.h"
#include <cmath>
#if BL_HAVE_QUICKJS
#include "quickjs.h"
namespace {
JSContext* context=nullptr;
JSValue chooseCallback=JS_UNDEFINED,fillCallback=JS_UNDEFINED;
namespace bg=bl::runtime::underground;
bool exception(JSContext* ctx,std::string* error,const char* method) {
    JSValue ex=JS_GetException(ctx),stack=JS_IsObject(ex)?JS_GetPropertyStr(ctx,ex,"stack"):JS_UNDEFINED;
    if(JS_IsException(stack)){JSValue nested=JS_GetException(ctx);JS_FreeValue(ctx,nested);stack=JS_UNDEFINED;}
    const char* message=JS_ToCString(ctx,ex),*trace=JS_IsUndefined(stack)?nullptr:JS_ToCString(ctx,stack);
    *error="[JavaScript][ERROR][UndergroundBackground] Method="+std::string(method)+" Stage=Invoke Result=UsingLastValidFill Exception="+
        (message?message:"UnprintableException")+(trace?" Stack="+std::string(trace):"");
    if(!message || (!JS_IsUndefined(stack) && !trace)){JSValue nested=JS_GetException(ctx);JS_FreeValue(ctx,nested);}
    if(trace)JS_FreeCString(ctx,trace);
    if(message)JS_FreeCString(ctx,message);
    JS_FreeValue(ctx,stack);JS_FreeValue(ctx,ex);return false;
}
void report(const std::string& error) {
    static unsigned failures=0;++failures;
    if(failures && !(failures&(failures-1)))BL_ERROR("%s SuppressedTotal=%u",error.c_str(),failures-1);
}
bg::Candidate choose() {
    bl::script::JsLock lock;if(!context)return {};
    JSValue value=JS_Call(context,chooseCallback,JS_UNDEFINED,0,nullptr);
    if(JS_IsException(value)){std::string error;exception(context,&error,"ChooseStyle");report(error);JS_FreeValue(context,value);return {};}
    bg::Candidate candidate;
    for(int i=0;i<3;++i) {
        JSValue item=JS_GetPropertyUint32(context,value,i);double number=0;
        const int converted=JS_ToFloat64(context,&number,item);
        if(converted || !std::isfinite(number)) {
            JS_FreeValue(context,item);JS_FreeValue(context,value);
            if(converted){std::string error;exception(context,&error,"ChooseStyle");report(error);}
            return {};
        }
        if(i==0)candidate.slot=number>=0 && number<=16384 && std::floor(number)==number?int(number):-1;
        else if(i==1)candidate.priority=number>=0 && number<=8 && std::floor(number)==number?int(number):-1;
        else candidate.weight=number;
        JS_FreeValue(context,item);
    }
    JS_FreeValue(context,value);return candidate;
}
bool fill(int slot,bg::Slots* slots,std::string* error) {
    bl::script::JsLock lock;
    if(!context){*error="[JavaScript][ERROR][UndergroundBackground] Stage=FillTextureArray Result=Rejected Reason=ProviderUnloaded";return false;}
    JSValue arg=JS_NewInt32(context,slot),value=JS_Call(context,fillCallback,JS_UNDEFINED,1,&arg);
    JS_FreeValue(context,arg);
    if(JS_IsException(value)){exception(context,error,"FillTextureArray");JS_FreeValue(context,value);return false;}
    if(!JS_IsArray(value)){JS_FreeValue(context,value);*error="[JavaScript][ERROR][UndergroundBackground] Stage=FillTextureArray Expected=ArrayOfTextureSlots Actual=OtherType Result=Rejected Reason=CallbackContractMismatch";return false;}
    for(size_t i=0;i<5;++i) {
        JSValue item=JS_GetPropertyUint32(context,value,i);double number=0;
        if(JS_IsException(item)){JS_FreeValue(context,value);return exception(context,error,"FillTextureArray");}
        // Match Number.isInteger in the original loader: null/string/Symbol
        // preserve a layer, rather than coercing to texture 0 or throwing.
        if(!JS_IsNumber(item)){(*slots)[i]=-1;JS_FreeValue(context,item);continue;}
        const int converted=JS_ToFloat64(context,&number,item);JS_FreeValue(context,item);
        if(converted){JS_FreeValue(context,value);return exception(context,error,"FillTextureArray");}
        (*slots)[i]=std::isfinite(number) && number>=0 && number<16384 && std::floor(number)==number?int(number):-1;
    }
    JS_FreeValue(context,value);return true;
}
JSValue reserve(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv) {
    if(argc!=2)return JS_ThrowTypeError(ctx,"reserveUndergroundStyle(modUid,className): Expected=2Arguments Actual=%d",argc);
    const char* mod=JS_ToCString(ctx,argv[0]);if(!mod)return JS_EXCEPTION;
    const char* name=JS_ToCString(ctx,argv[1]);if(!name){JS_FreeCString(ctx,mod);return JS_EXCEPTION;}
    std::string error;const int slot=bg::reserve(mod,name,bg::Language::JavaScript,&error);
    JS_FreeCString(ctx,name);JS_FreeCString(ctx,mod);
    return slot>=0?JS_NewInt32(ctx,slot):JS_ThrowInternalError(ctx,"%s",error.c_str());
}
JSValue install(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv) {
    if(argc!=2 || !JS_IsFunction(ctx,argv[0]) || !JS_IsFunction(ctx,argv[1]))return JS_ThrowTypeError(ctx,"installUndergroundBackgrounds(choose,fill): Expected=TwoFunctions");
    if(context && context!=ctx)return JS_ThrowInternalError(ctx,"[JavaScript][ERROR][UndergroundBackground] Stage=Install Result=Rejected Reason=ProviderAlreadyOwnedByAnotherContext");
    std::string error;if(!bg::install(&error))return JS_ThrowInternalError(ctx,"%s",error.c_str());
    if(context){JS_FreeValue(ctx,chooseCallback);JS_FreeValue(ctx,fillCallback);}
    chooseCallback=JS_DupValue(ctx,argv[0]);fillCallback=JS_DupValue(ctx,argv[1]);context=ctx;
    bg::provider(bg::Language::JavaScript,choose,fill);return JS_UNDEFINED;
}
}
namespace bl::script {
void installUndergroundBackgroundBindings(void* opaque) {
    auto* ctx=static_cast<JSContext*>(opaque);JSValue global=JS_GetGlobalObject(ctx),api=JS_GetPropertyStr(ctx,global,"bl");
    JS_SetPropertyStr(ctx,api,"reserveUndergroundStyle",JS_NewCFunction(ctx,reserve,"reserveUndergroundStyle",2));
    JS_SetPropertyStr(ctx,api,"installUndergroundBackgrounds",JS_NewCFunction(ctx,install,"installUndergroundBackgrounds",2));
    JS_FreeValue(ctx,api);JS_FreeValue(ctx,global);
}
void releaseUndergroundBackgroundBindings(void* opaque) {
    if(context!=opaque)return;
    bg::provider(bg::Language::JavaScript,nullptr,nullptr);
    bg::retireLanguage(bg::Language::JavaScript);
    JS_FreeValue(context,chooseCallback);JS_FreeValue(context,fillCallback);context=nullptr;
    chooseCallback=fillCallback=JS_UNDEFINED;
}
}
#else
namespace bl::script {void installUndergroundBackgroundBindings(void*){}void releaseUndergroundBackgroundBindings(void*){}}
#endif
