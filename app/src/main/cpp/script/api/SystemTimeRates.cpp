#include "script/api/SystemTimeRates.h"
#include "content/systems/SystemTimeRates.h"
#include "core/Log.h"
#if BL_HAVE_QUICKJS
#include "quickjs.h"
namespace {
namespace timeRates = bl::runtime::systems;
JSValue timeRatePrepare(JSContext* ctx,JSValueConst,int,JSValueConst*) {
    std::string error;
    return timeRates::prepareTimeRates(&error) ? JS_UNDEFINED : JS_ThrowInternalError(ctx,"%s",error.c_str());
}
JSValue timeRateBegin(JSContext* ctx,JSValueConst,int,JSValueConst*) {
    std::string error;
    const bool entered=timeRates::beginTimeRates(&error);
    // Returning false keeps the native original running even on a rejected
    // transaction. Repeated native failures are bounded in the session log.
    if(!entered) {
        static unsigned failures=0;
        ++failures;
    if((failures & (failures-1))==0) BL_ERROR("%s SuppressedTotal=%u",error.c_str(),failures-1);
    }
    return JS_NewBool(ctx,entered);
}
JSValue timeRateRead(JSContext* ctx,JSValueConst,int,JSValueConst*) {
    timeRates::TimeRates rates;std::string error;
    if(!timeRates::readTimeRates(&rates,&error)) return JS_ThrowInternalError(ctx,"%s",error.c_str());
    JSValue values=JS_NewArray(ctx);
    if(JS_IsException(values)) return values;
    if(JS_SetPropertyUint32(ctx,values,0,JS_NewFloat64(ctx,rates.time))<0 ||
       JS_SetPropertyUint32(ctx,values,1,JS_NewFloat64(ctx,rates.tiles))<0 ||
       JS_SetPropertyUint32(ctx,values,2,JS_NewFloat64(ctx,rates.events))<0) {
        JS_FreeValue(ctx,values);return JS_EXCEPTION;
    }
    return values;
}
JSValue timeRatePublish(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv) {
    timeRates::TimeRates rates;std::string error;
    if(argc!=3) return JS_ThrowTypeError(ctx,"systemTimeRatePublish(time,tiles,events): Expected=3Numbers Actual=%dArguments",argc);
    if(JS_ToFloat64(ctx,&rates.time,argv[0]) || JS_ToFloat64(ctx,&rates.tiles,argv[1]) || JS_ToFloat64(ctx,&rates.events,argv[2])) return JS_EXCEPTION;
    return timeRates::publishTimeRates(rates,&error) ? JS_UNDEFINED : JS_ThrowInternalError(ctx,"%s",error.c_str());
}
JSValue timeRateEnd(JSContext* ctx,JSValueConst,int,JSValueConst*) {
    std::string error;
    return timeRates::endTimeRates(&error) ? JS_UNDEFINED : JS_ThrowInternalError(ctx,"%s",error.c_str());
}
JSValue timeRateClock(JSContext* ctx,JSValueConst,int,JSValueConst*) { return JS_NewFloat64(ctx,timeRates::clockRate()); }
}
namespace bl::script {
void installSystemTimeRateBindings(void* context) {
    auto* ctx=static_cast<JSContext*>(context);
    JSValue global=JS_GetGlobalObject(ctx);
    JSValue api=JS_GetPropertyStr(ctx,global,"bl");
    JS_SetPropertyStr(ctx,api,"systemTimeRatePrepare",JS_NewCFunction(ctx,timeRatePrepare,"systemTimeRatePrepare",0));
    JS_SetPropertyStr(ctx,api,"systemTimeRateBegin",JS_NewCFunction(ctx,timeRateBegin,"systemTimeRateBegin",0));
    JS_SetPropertyStr(ctx,api,"systemTimeRateRead",JS_NewCFunction(ctx,timeRateRead,"systemTimeRateRead",0));
    JS_SetPropertyStr(ctx,api,"systemTimeRatePublish",JS_NewCFunction(ctx,timeRatePublish,"systemTimeRatePublish",3));
    JS_SetPropertyStr(ctx,api,"systemTimeRateEnd",JS_NewCFunction(ctx,timeRateEnd,"systemTimeRateEnd",0));
    JS_SetPropertyStr(ctx,api,"systemTimeRateClock",JS_NewCFunction(ctx,timeRateClock,"systemTimeRateClock",0));
    JS_FreeValue(ctx,api);JS_FreeValue(ctx,global);
}
}
#else
namespace bl::script { void installSystemTimeRateBindings(void*) {} }
#endif
