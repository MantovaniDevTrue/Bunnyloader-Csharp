#pragma once
// Declaration-only test header. The host binder test never calls QuickJS;
// its pump is discarded by --gc-sections. The real JS recipe bridge is tested
// independently with Node, and the real header/library are used in APK builds.
#include <cstdint>
struct JSContext;
using JSValue=uint64_t;
using JSValueConst=JSValue;
JSValue JS_GetGlobalObject(JSContext*);
JSValue JS_GetPropertyStr(JSContext*,JSValueConst,const char*);
int JS_IsFunction(JSContext*,JSValueConst);
JSValue JS_Call(JSContext*,JSValueConst,JSValueConst,int,JSValueConst*);
int JS_IsException(JSValueConst);
JSValue JS_GetException(JSContext*);
const char* JS_ToCString(JSContext*,JSValueConst);
void JS_FreeCString(JSContext*,const char*);
void JS_FreeValue(JSContext*,JSValue);
