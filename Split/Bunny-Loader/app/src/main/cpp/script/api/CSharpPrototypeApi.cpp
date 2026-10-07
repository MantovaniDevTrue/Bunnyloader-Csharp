#include "script/api/CSharpPrototypeApi.h"

#include "experiments/csharp/CSharpPrototype.h"

#if BL_HAVE_QUICKJS
#include "quickjs.h"

namespace bl::script {

namespace {

JSValue jsAvailable(JSContext*, JSValueConst, int, JSValueConst*) {
    return JS_NewBool(nullptr, bl::csharp::available());
}

JSValue jsVersion(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    return JS_NewInt32(ctx, bl::csharp::version());
}

JSValue jsAdd(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "bl.csharp.add(a, b)");
    int32_t a = 0;
    int32_t b = 0;
    if (JS_ToInt32(ctx, &a, argv[0]) < 0 || JS_ToInt32(ctx, &b, argv[1]) < 0) return JS_EXCEPTION;
    return JS_NewInt32(ctx, bl::csharp::add(a, b));
}

JSValue jsChecksum(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int32_t iterations = 100000;
    if (argc > 0 && JS_ToInt32(ctx, &iterations, argv[0]) < 0) return JS_EXCEPTION;
    if (iterations < 0) iterations = 0;
    return JS_NewInt64(ctx, bl::csharp::checksum(iterations));
}

JSValue jsPlayerSample(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 4) return JS_ThrowTypeError(ctx, "bl.csharp.playerSample(x, y, life, maxLife)");
    double x = 0.0;
    double y = 0.0;
    int32_t life = 0;
    int32_t maxLife = 0;
    if (JS_ToFloat64(ctx, &x, argv[0]) < 0 ||
        JS_ToFloat64(ctx, &y, argv[1]) < 0 ||
        JS_ToInt32(ctx, &life, argv[2]) < 0 ||
        JS_ToInt32(ctx, &maxLife, argv[3]) < 0) return JS_EXCEPTION;
    return JS_NewInt64(ctx, bl::csharp::playerSample(
        static_cast<float>(x), static_cast<float>(y), life, maxLife));
}

JSValue jsBenchmark(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int32_t iterations = 250000;
    if (argc > 0 && JS_ToInt32(ctx, &iterations, argv[0]) < 0) return JS_EXCEPTION;
    const auto result = bl::csharp::benchmark(iterations);
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "cppNsPerCall", JS_NewFloat64(ctx, result.cppNsPerCall));
    JS_SetPropertyStr(ctx, out, "csharpNsPerCall", JS_NewFloat64(ctx, result.csharpNsPerCall));
    JS_SetPropertyStr(ctx, out, "ratio", JS_NewFloat64(ctx, result.ratio));
    JS_SetPropertyStr(ctx, out, "cppValue", JS_NewInt64(ctx, result.cppValue));
    JS_SetPropertyStr(ctx, out, "csharpValue", JS_NewInt64(ctx, result.csharpValue));
    return out;
}

}

void installCSharpPrototypeApi(JSContext* ctx, JSValueConst bl) {
    JSValue api = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, api, "available", JS_NewCFunction(ctx, jsAvailable, "available", 0));
    JS_SetPropertyStr(ctx, api, "version", JS_NewCFunction(ctx, jsVersion, "version", 0));
    JS_SetPropertyStr(ctx, api, "add", JS_NewCFunction(ctx, jsAdd, "add", 2));
    JS_SetPropertyStr(ctx, api, "checksum", JS_NewCFunction(ctx, jsChecksum, "checksum", 1));
    JS_SetPropertyStr(ctx, api, "playerSample", JS_NewCFunction(ctx, jsPlayerSample, "playerSample", 4));
    JS_SetPropertyStr(ctx, api, "benchmark", JS_NewCFunction(ctx, jsBenchmark, "benchmark", 1));
    JS_SetPropertyStr(ctx, const_cast<JSValue&>(bl), "csharp", api);
}

}

#endif
