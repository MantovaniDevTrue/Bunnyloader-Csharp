#include "script/api/CSharpPrototypeApi.h"

#include "experiments/csharp/CSharpPrototype.h"

#if BL_HAVE_QUICKJS
#include "quickjs.h"

namespace bl::script {

namespace {

JSValue jsAvailable(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    return JS_NewBool(ctx, bl::csharp::available());
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

JSValue jsClockMs(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    return JS_NewFloat64(ctx, bl::csharp::clockMs());
}

JSValue jsMathBatch(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int32_t iterations = 1000000;
    int32_t seed = 0x12345678;
    if (argc > 0 && JS_ToInt32(ctx, &iterations, argv[0]) < 0) return JS_EXCEPTION;
    if (argc > 1 && JS_ToInt32(ctx, &seed, argv[1]) < 0) return JS_EXCEPTION;
    return JS_NewInt64(ctx, static_cast<int64_t>(
        bl::csharp::mathBatch(iterations, static_cast<uint32_t>(seed))));
}

JSValue jsProjectileBatch(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int32_t count = 4096;
    int32_t steps = 180;
    if (argc > 0 && JS_ToInt32(ctx, &count, argv[0]) < 0) return JS_EXCEPTION;
    if (argc > 1 && JS_ToInt32(ctx, &steps, argv[1]) < 0) return JS_EXCEPTION;
    return JS_NewFloat64(ctx, bl::csharp::projectileBatch(count, steps));
}

constexpr char kSuiteSource[] = R"JS(
(function(iterations, count, steps) {
    iterations = iterations === undefined ? 1000000 : Math.max(1000, Math.min(20000000, iterations | 0));
    count = count === undefined ? 4096 : Math.max(1, Math.min(16384, count | 0));
    steps = steps === undefined ? 180 : Math.max(1, Math.min(2000, steps | 0));

    const seed = 0x12345678;

    function mathJs(n, start) {
        let x = (start ^ 0xA5A5A5A5) >>> 0;
        for (let i = 0; i < n; i++) {
            x = (x + Math.imul(i, 0x9E3779B9)) >>> 0;
            x = ((x << 7) | (x >>> 25)) >>> 0;
            x = (x ^ (x >>> 13)) >>> 0;
            x = Math.imul(x, 0x85EBCA6B) >>> 0;
            x = (x ^ (x << 5)) >>> 0;
        }
        return x >>> 0;
    }

    function projectilesJs(n, ticks) {
        const x = new Float64Array(n);
        const y = new Float64Array(n);
        const vx = new Float64Array(n);
        const vy = new Float64Array(n);

        for (let i = 0; i < n; i++) {
            x[i] = (i % 64) * 3.125 - 100.0;
            y[i] = (i % 37) * 1.75 + 40.0;
            vx[i] = ((i % 17) - 8) * 0.03125;
            vy[i] = ((i % 13) - 6) * 0.0275;
        }

        let checksum = 0.0;
        for (let tick = 0; tick < ticks; tick++) {
            for (let i = 0; i < n; i++) {
                let px = x[i] + vx[i];
                let py = y[i] + vy[i];
                let dx = vx[i] * 0.9995;
                let dy = vy[i] + 0.015;

                if (py > 900.0) {
                    py = 900.0;
                    dy = -Math.abs(dy) * 0.72;
                }
                if (px < -1200.0 || px > 1200.0)
                    dx = -dx;

                x[i] = px;
                y[i] = py;
                vx[i] = dx;
                vy[i] = dy;

                checksum += px * 0.0001 + py * 0.0002 + dx * 0.3 + dy * 0.2;
            }
        }
        return checksum;
    }

    mathJs(1000, seed);
    bl.csharp.mathBatch(1000, seed);
    projectilesJs(Math.min(count, 64), Math.min(steps, 4));
    bl.csharp.projectileBatch(Math.min(count, 64), Math.min(steps, 4));

    let t0 = bl.csharp.clockMs();
    const jsMathValue = mathJs(iterations, seed);
    let t1 = bl.csharp.clockMs();

    let t2 = bl.csharp.clockMs();
    const csharpMathValue = bl.csharp.mathBatch(iterations, seed);
    let t3 = bl.csharp.clockMs();

    let t4 = bl.csharp.clockMs();
    const jsProjectileValue = projectilesJs(count, steps);
    let t5 = bl.csharp.clockMs();

    let t6 = bl.csharp.clockMs();
    const csharpProjectileValue = bl.csharp.projectileBatch(count, steps);
    let t7 = bl.csharp.clockMs();

    const jsMathMs = t1 - t0;
    const csharpMathMs = t3 - t2;
    const jsProjectileMs = t5 - t4;
    const csharpProjectileMs = t7 - t6;

    return {
        abi: bl.csharp.version(),
        math: {
            iterations,
            jsMs: jsMathMs,
            csharpMs: csharpMathMs,
            speedup: csharpMathMs > 0 ? jsMathMs / csharpMathMs : 0,
            jsValue: jsMathValue,
            csharpValue: csharpMathValue,
            match: jsMathValue === csharpMathValue
        },
        projectiles: {
            count,
            steps,
            updates: count * steps,
            jsMs: jsProjectileMs,
            csharpMs: csharpProjectileMs,
            speedup: csharpProjectileMs > 0 ? jsProjectileMs / csharpProjectileMs : 0,
            jsValue: jsProjectileValue,
            csharpValue: csharpProjectileValue,
            delta: Math.abs(jsProjectileValue - csharpProjectileValue)
        },
        bridge: bl.csharp.benchmark(1000000)
    };
})
)JS";

JSValue makeSuite(JSContext* ctx) {
    return JS_Eval(ctx, kSuiteSource, sizeof(kSuiteSource) - 1,
                   "csharp-benchmark-suite", JS_EVAL_TYPE_GLOBAL);
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
    JS_SetPropertyStr(ctx, api, "clockMs", JS_NewCFunction(ctx, jsClockMs, "clockMs", 0));
    JS_SetPropertyStr(ctx, api, "mathBatch", JS_NewCFunction(ctx, jsMathBatch, "mathBatch", 2));
    JS_SetPropertyStr(ctx, api, "projectileBatch", JS_NewCFunction(ctx, jsProjectileBatch, "projectileBatch", 2));

    JSValue suite = makeSuite(ctx);
    if (JS_IsException(suite)) {
        JS_FreeValue(ctx, suite);
    } else {
        JS_SetPropertyStr(ctx, api, "suite", suite);
    }

    JS_SetPropertyStr(ctx, bl, "csharp", api);
}

}

#endif
