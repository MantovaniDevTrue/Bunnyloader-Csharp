#include "script/api/SurfaceBackgrounds.h"
#include "content/backgrounds/SurfaceBackgrounds.h"
#include "script/bridge/ScriptEngine.h"
#include "core/Log.h"
#include <cmath>
#if BL_HAVE_QUICKJS
#include "quickjs.h"
namespace {
namespace surface=bl::runtime::surface;
JSContext* surfaceContext=nullptr;
JSValue surfaceChoose=JS_UNDEFINED,surfaceMenu=JS_UNDEFINED,surfaceCallback=JS_UNDEFINED;
bool surfaceException(JSContext* ctx,std::string* error,const char* method) {
    JSValue ex=JS_GetException(ctx),stack=JS_IsObject(ex)?JS_GetPropertyStr(ctx,ex,"stack"):JS_UNDEFINED;
    if(JS_IsException(stack)){auto nested=JS_GetException(ctx);JS_FreeValue(ctx,nested);stack=JS_UNDEFINED;}
    const char* message=JS_ToCString(ctx,ex),*trace=JS_IsUndefined(stack)?nullptr:JS_ToCString(ctx,stack);
    *error="[JavaScript][ERROR][SurfaceBackground] Method="+std::string(method)+" Stage=Invoke Result=CallbackRejected Exception="+
        (message?message:"UnprintableException")+(trace?" Stack="+std::string(trace):"");
    if(!message || (!JS_IsUndefined(stack) && !trace)){auto nested=JS_GetException(ctx);JS_FreeValue(ctx,nested);}
    if(trace)JS_FreeCString(ctx,trace);
    if(message)JS_FreeCString(ctx,message);
    JS_FreeValue(ctx,stack);JS_FreeValue(ctx,ex);return false;
}
void surfaceReport(const std::string& error) {
    static unsigned failures=0;++failures;
    if(!(failures&(failures-1)))BL_ERROR("%s SuppressedTotal=%u",error.c_str(),failures-1);
}
bool surfaceNumber(JSContext* ctx,JSValueConst value,double* number) {
    return JS_IsNumber(value) && JS_ToFloat64(ctx,number,value)==0 && std::isfinite(*number);
}
bool surfaceInteger(double number,int low,int high){return number>=low && number<=high && std::floor(number)==number;}
surface::Candidate chooseSurface() {
    bl::script::JsLock lock;if(!surfaceContext)return {};
    auto* ctx=surfaceContext;auto value=JS_Call(ctx,surfaceChoose,JS_UNDEFINED,0,nullptr);
    if(JS_IsException(value)){std::string error;surfaceException(ctx,&error,"ChooseStyle");surfaceReport(error);JS_FreeValue(ctx,value);return {};}
    double numbers[3]{};bool valid=JS_IsArray(value);
    for(int i=0;valid && i<3;++i) {
        auto item=JS_GetPropertyUint32(ctx,value,i);
        if(JS_IsException(item)){std::string error;surfaceException(ctx,&error,"ChooseStyle");surfaceReport(error);valid=false;}
        else valid=surfaceNumber(ctx,item,&numbers[i]);
        JS_FreeValue(ctx,item);
    }
    JS_FreeValue(ctx,value);
    if(!valid || !surfaceInteger(numbers[0],0,4095) || !surfaceInteger(numbers[1],0,8) || numbers[2]<0 || numbers[2]>1)return {};
    return {int(numbers[0]),int(numbers[1]),numbers[2]};
}
int menuSurface() {
    bl::script::JsLock lock;if(!surfaceContext)return -1;
    auto* ctx=surfaceContext;auto value=JS_Call(ctx,surfaceMenu,JS_UNDEFINED,0,nullptr);double number=-1;
    if(JS_IsException(value)){std::string error;surfaceException(ctx,&error,"MenuBackground");surfaceReport(error);JS_FreeValue(ctx,value);return -1;}
    const bool valid=surfaceNumber(ctx,value,&number) && surfaceInteger(number,0,4095);JS_FreeValue(ctx,value);return valid?int(number):-1;
}
bool surfaceProperty(JSContext* ctx,JSValueConst object,const char* name,double* number,std::string* error) {
    auto value=JS_GetPropertyStr(ctx,object,name);
    if(JS_IsException(value)){JS_FreeValue(ctx,value);return surfaceException(ctx,error,name);}
    const bool valid=surfaceNumber(ctx,value,number);JS_FreeValue(ctx,value);
    if(!valid)*error="[JavaScript][ERROR][SurfaceBackground] Stage=ReadReply Field="+std::string(name)+" Expected=FiniteNumber Actual=NonNumberOrNonFinite Result=Rejected Reason=CallbackContractMismatch";
    return valid;
}
bool invokeSurface(const surface::Request& request,surface::Reply* reply,std::string* error) {
    bl::script::JsLock lock;
    if(!surfaceContext){*error="[JavaScript][ERROR][SurfaceBackground] Stage=Invoke Result=Rejected Reason=ProviderUnloaded";return false;}
    auto* ctx=surfaceContext;
    JSValue args[]={JS_NewInt32(ctx,request.slot),JS_NewInt32(ctx,int(request.operation)),JS_NewFloat64(ctx,request.speed),JS_NewArray(ctx)};
    for(size_t i=0;i<request.fades.size();++i)JS_SetPropertyUint32(ctx,args[3],uint32_t(i),JS_NewFloat64(ctx,request.fades[i]));
    auto value=JS_Call(ctx,surfaceCallback,JS_UNDEFINED,4,args);
    for(auto arg:args)JS_FreeValue(ctx,arg);
    if(JS_IsException(value)) {
        const char* method=request.operation==surface::Operation::Fades?"ModifyFarFades":request.operation==surface::Operation::Far?"ChooseFarTexture":request.operation==surface::Operation::Middle?"ChooseMiddleTexture":"PreDrawCloseBackground/ChooseCloseTexture";
        JS_FreeValue(ctx,value);return surfaceException(ctx,error,method);
    }
    bool valid=JS_IsObject(value);
    if(request.operation==surface::Operation::Fades) {
        auto fades=JS_GetPropertyStr(ctx,value,"fades");
        if(JS_IsException(fades)){JS_FreeValue(ctx,fades);JS_FreeValue(ctx,value);return surfaceException(ctx,error,"ModifyFarFades");}
        auto length=JS_GetPropertyStr(ctx,fades,"length");double size=0;
        if(JS_IsException(length)){JS_FreeValue(ctx,length);JS_FreeValue(ctx,fades);JS_FreeValue(ctx,value);return surfaceException(ctx,error,"ModifyFarFades.length");}
        valid=valid && JS_IsArray(fades) && surfaceNumber(ctx,length,&size) && size==double(request.fades.size());JS_FreeValue(ctx,length);
        if(valid)reply->fades.resize(request.fades.size());
        for(size_t i=0;valid && i<request.fades.size();++i) {
            auto item=JS_GetPropertyUint32(ctx,fades,uint32_t(i));double number=0;
            if(JS_IsException(item)){surfaceException(ctx,error,"ModifyFarFades[index]");valid=false;}
            else valid=surfaceNumber(ctx,item,&number) && std::abs(number)<=double(__FLT_MAX__);
            JS_FreeValue(ctx,item);if(valid)reply->fades[i]=float(number);
        }
        JS_FreeValue(ctx,fades);
    } else {
        double texture=-1;valid=valid && surfaceProperty(ctx,value,"texture",&texture,error) && surfaceInteger(texture,-1,16383);
        if(valid)reply->texture=int(texture);
        if(valid && request.operation==surface::Operation::Close) {
            auto draw=JS_GetPropertyStr(ctx,value,"defaultClose");
            if(JS_IsException(draw)){surfaceException(ctx,error,"PreDrawCloseBackground");valid=false;}
            else {valid=JS_IsBool(draw);if(valid)reply->defaultClose=JS_ToBool(ctx,draw)!=0;}
            JS_FreeValue(ctx,draw);
            valid=valid && surfaceProperty(ctx,value,"scale",&reply->close.scale,error) && surfaceProperty(ctx,value,"parallax",&reply->close.parallax,error) &&
                surfaceProperty(ctx,value,"a",&reply->close.a,error) && surfaceProperty(ctx,value,"b",&reply->close.b,error);
        }
    }
    JS_FreeValue(ctx,value);
    if(!valid && error->empty())*error="[JavaScript][ERROR][SurfaceBackground] Stage=ReadReply ContentId="+std::to_string(request.slot)+" Expected=SurfaceReply Actual=InvalidFields Result=Rejected Reason=CallbackContractMismatch";
    return valid;
}
JSValue reserveSurface(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv) {
    if(argc!=2)return JS_ThrowTypeError(ctx,"reserveSurfaceStyle(modUid,className): Expected=2Arguments Actual=%d",argc);
    const char* mod=JS_ToCString(ctx,argv[0]);if(!mod)return JS_EXCEPTION;
    const char* name=JS_ToCString(ctx,argv[1]);if(!name){JS_FreeCString(ctx,mod);return JS_EXCEPTION;}
    std::string error;int slot=surface::reserve(mod,name,surface::Language::JavaScript,&error);JS_FreeCString(ctx,name);JS_FreeCString(ctx,mod);
    return slot>=0?JS_NewInt32(ctx,slot):JS_ThrowInternalError(ctx,"%s",error.c_str());
}
JSValue installSurface(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv) {
    if(argc!=3 || !JS_IsFunction(ctx,argv[0]) || !JS_IsFunction(ctx,argv[1]) || !JS_IsFunction(ctx,argv[2]))return JS_ThrowTypeError(ctx,"installSurfaceBackgrounds(choose,menu,invoke): Expected=ThreeFunctions");
    if(surfaceContext && surfaceContext!=ctx)return JS_ThrowInternalError(ctx,"[JavaScript][ERROR][SurfaceBackground] Stage=Install Result=Rejected Reason=ProviderAlreadyOwnedByAnotherContext");
    std::string error;if(!surface::install(&error))return JS_ThrowInternalError(ctx,"%s",error.c_str());
    if(surfaceContext){JS_FreeValue(ctx,surfaceChoose);JS_FreeValue(ctx,surfaceMenu);JS_FreeValue(ctx,surfaceCallback);}
    surfaceContext=ctx;surfaceChoose=JS_DupValue(ctx,argv[0]);surfaceMenu=JS_DupValue(ctx,argv[1]);surfaceCallback=JS_DupValue(ctx,argv[2]);
    surface::provider(surface::Language::JavaScript,chooseSurface,menuSurface,invokeSurface);return JS_UNDEFINED;
}
JSValue wakeSurface(JSContext*,JSValueConst,int,JSValueConst*){surface::wake();return JS_UNDEFINED;}
}
namespace bl::script {
void installSurfaceBackgroundBindings(void* opaque) {
    auto* ctx=static_cast<JSContext*>(opaque);auto global=JS_GetGlobalObject(ctx),api=JS_GetPropertyStr(ctx,global,"bl");
    JS_SetPropertyStr(ctx,api,"reserveSurfaceStyle",JS_NewCFunction(ctx,reserveSurface,"reserveSurfaceStyle",2));
    JS_SetPropertyStr(ctx,api,"installSurfaceBackgrounds",JS_NewCFunction(ctx,installSurface,"installSurfaceBackgrounds",3));
    JS_SetPropertyStr(ctx,api,"wakeSurfaceBackgrounds",JS_NewCFunction(ctx,wakeSurface,"wakeSurfaceBackgrounds",0));
    JS_FreeValue(ctx,api);JS_FreeValue(ctx,global);
}
void releaseSurfaceBackgroundBindings(void* opaque) {
    if(surfaceContext!=opaque)return;
    surface::provider(surface::Language::JavaScript,nullptr,nullptr,nullptr);surface::retireLanguage(surface::Language::JavaScript);
    JS_FreeValue(surfaceContext,surfaceChoose);JS_FreeValue(surfaceContext,surfaceMenu);JS_FreeValue(surfaceContext,surfaceCallback);
    surfaceContext=nullptr;surfaceChoose=surfaceMenu=surfaceCallback=JS_UNDEFINED;
}
}
#else
namespace bl::script {void installSurfaceBackgroundBindings(void*){}void releaseSurfaceBackgroundBindings(void*){}}
#endif
