#include "script/api/CSharpPrototypeApi.h"

#include "experiments/csharp/CSharpPrototype.h"
#include "experiments/csharp/TerrariaHost.h"
#include "experiments/csharp/ProjectileWriteTest.h"
#include "experiments/csharp/CSharpModLoader.h"
#include "experiments/csharp/SourceLoader.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/MonoServiceDispatch.h"
#include "experiments/csharp/ManagedProjectileHook.h"
#include "experiments/csharp/MonoServiceDispatch.h"
#include "core/Config.h"
#include "mods/ModLoader.h"
#include "experiments/csharp/NpcAiHost.h"
#include "il2cpp/Api.h"
#include "il2cpp/Resolver.h"
#include "il2cpp/Signature.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <csignal>
#include <fstream>
#include <string>
#include <string_view>
#ifdef __ANDROID__
#include <android/log.h>
#endif

#if BL_HAVE_QUICKJS
#include "quickjs.h"

namespace bl::script {

namespace {

JSValue makeManagedStatus(JSContext* ctx, const bl::csharp::ManagedRuntimeStatus& s) {
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "attempted", JS_NewBool(ctx, s.attempted));
    JS_SetPropertyStr(ctx, out, "libraryLoaded", JS_NewBool(ctx, s.libraryLoaded));
    JS_SetPropertyStr(ctx, out, "symbolsReady", JS_NewBool(ctx, s.symbolsReady));
    JS_SetPropertyStr(ctx, out, "domainInitialized", JS_NewBool(ctx, s.domainInitialized));
    JS_SetPropertyStr(ctx, out, "methodExecuted", JS_NewBool(ctx, s.methodExecuted));
    JS_SetPropertyStr(ctx, out, "result", JS_NewInt32(ctx, s.result));
    JS_SetPropertyStr(ctx, out, "callbacksExecuted", JS_NewInt32(ctx, s.callbacksExecuted));
    JS_SetPropertyStr(ctx, out, "phase", JS_NewString(ctx, s.phase.c_str()));
    JS_SetPropertyStr(ctx, out, "error", JS_NewString(ctx, s.error.c_str()));
    return out;
}

JSValue jsManagedRuntime(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    return makeManagedStatus(ctx, bl::csharp::managedRuntimeStatus());
}

JSValue jsManagedInspect(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    return makeManagedStatus(ctx, bl::csharp::inspectManagedRuntime());
}

JSValue jsManagedProbe(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "bl.csharp.managedProbe()");
    std::string error;
    const bool queued = bl::csharp::requestIsolatedMonoProbe(error);
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "queued", JS_NewBool(ctx, queued));
    JS_SetPropertyStr(ctx, out, "isolated", JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, out, "error", JS_NewString(ctx, error.c_str()));
    JS_SetPropertyStr(ctx, out, "details", JS_NewString(ctx, "Consultar managedWorkerStatus() e relatorio"));
    return out;
}

std::string readManagedWorkerFile(const char* filename) {
    const auto& path = bl::config().managedCheckpointPath;
    const auto slash = path.find_last_of('/');
    if (slash == std::string::npos) return {};
    std::ifstream stream(path.substr(0, slash + 1) + filename, std::ios::binary);
    if (!stream) return {};
    char buffer[2049]{};
    stream.read(buffer, 2048);
    return std::string(buffer, static_cast<size_t>(stream.gcount()));
}

JSValue jsManagedGameProbe(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "managedGameProbe('CONFIRM_IN_PROCESS')");
    const char* token = JS_ToCString(ctx, argv[0]);
    if (!token) return JS_EXCEPTION;
    const bool confirmed = std::string_view(token) == "CONFIRM_IN_PROCESS";
    JS_FreeCString(ctx, token);
    if (!confirmed) return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_IN_PROCESS");

    const auto workerState = readManagedWorkerFile("isolated-worker-status.txt");
    const auto workerCheckpoint = readManagedWorkerFile("last-isolated-probe.txt");
    if (workerState.find("state=finished\n") == std::string::npos ||
        workerState.find("methodExecuted=1\n") == std::string::npos ||
        workerState.find("result=42\n") == std::string::npos ||
        workerCheckpoint.find("phase=verified\n") == std::string::npos)
        return JS_ThrowInternalError(ctx, "Executar managedProbe() no worker e aguardar verified antes do teste no jogo");

    const auto& checkpoint = bl::config().managedCheckpointPath;
    const auto slash = checkpoint.find_last_of('/');
    if (slash == std::string::npos)
        return JS_ThrowInternalError(ctx, "Diretorio do Mono nao configurado");
    const auto assembly = checkpoint.substr(0, slash + 1) + "BunnyManagedProbe.dll";
    std::ifstream probeFile(assembly, std::ios::binary);
    if (!probeFile)
        return JS_ThrowInternalError(ctx, "BunnyManagedProbe.dll ausente; executar worker antes");

    return makeManagedStatus(ctx, bl::csharp::probeManagedRuntime(assembly));
}

JSValue jsManagedCallbackProbe(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1) return JS_ThrowTypeError(ctx, "managedCallbackProbe(count)");
    int32_t count = 0;
    if (JS_ToInt32(ctx, &count, argv[0]) < 0) return JS_EXCEPTION;
    if (count < 1 || count > 120)
        return JS_ThrowRangeError(ctx, "Quantidade de callbacks precisa estar entre 1 e 120");
    const auto state = bl::csharp::managedRuntimeStatus();
    if (!state.domainInitialized || !state.methodExecuted)
        return JS_ThrowInternalError(ctx, "Executar managedGameProbe antes do teste AI");
    const auto& checkpoint = bl::config().managedCheckpointPath;
    const auto slash = checkpoint.find_last_of('/');
    if (slash == std::string::npos)
        return JS_ThrowInternalError(ctx, "Diretorio CSharp nao configurado");
    const auto assembly = checkpoint.substr(0, slash + 1) + "BunnyManagedProbe.dll";
    return makeManagedStatus(ctx, bl::csharp::probeManagedCallbacks(assembly, count));
}

JSValue makeSourceBoltHookStatus(JSContext* ctx) {
    const auto status = bl::csharp::managedSourceBoltHookStatus();
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "installed", JS_NewBool(ctx, status.installed));
    JS_SetPropertyStr(ctx, result, "enabled", JS_NewBool(ctx, status.enabled));
    JS_SetPropertyStr(ctx, result, "type", JS_NewInt32(ctx, status.type));
    JS_SetPropertyStr(ctx, result, "callbacks", JS_NewInt32(ctx, status.callbacks));
    JS_SetPropertyStr(ctx, result, "velocityWrites", JS_NewInt32(ctx, status.velocityWrites));
    JS_SetPropertyStr(ctx, result, "spawns", JS_NewInt32(ctx, status.spawns));
    JS_SetPropertyStr(ctx, result, "errors", JS_NewInt32(ctx, status.errors));
    JS_SetPropertyStr(ctx, result, "lastSlot", JS_NewInt32(ctx, status.lastSlot));
    JS_SetPropertyStr(ctx, result, "lastState", JS_NewInt32(ctx, status.lastState));
    return result;
}

JSValue jsManagedProjectileEnable(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "managedProjectileEnable('CONFIRM_SOURCE_BOLT')");
    const char* confirm = JS_ToCString(ctx, argv[0]);
    if (!confirm) return JS_EXCEPTION;
    const bool valid = std::string_view(confirm) == "CONFIRM_SOURCE_BOLT";
    JS_FreeCString(ctx, confirm);
    if (!valid)
        return JS_ThrowTypeError(ctx, "Confirmar explicitamente: CONFIRM_SOURCE_BOLT");

    std::string error;
    if (!bl::csharp::enableManagedSourceBoltHook(&error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    return makeSourceBoltHookStatus(ctx);
}

JSValue jsManagedProjectileStatus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_WARN, "BunnyCSharp", "sourcebolt-status enter");
#endif
    const bool wasEnabled = bl::csharp::pauseManagedSourceBoltHook();
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_WARN, "BunnyCSharp",
                        "sourcebolt-status paused previousEnabled=%d", wasEnabled ? 1 : 0);
#endif
    JSValue out = makeSourceBoltHookStatus(ctx);
    JS_SetPropertyStr(ctx, out, "pausedForSnapshot", JS_NewBool(ctx, wasEnabled));
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_WARN, "BunnyCSharp", "sourcebolt-status completed");
#endif
    return out;
}

JSValue jsManagedSourceCompile(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0)
        return JS_ThrowTypeError(ctx, "bl.csharp.sourceCompile()");
    constexpr const char* uid = "57a98f20-7a35-4d19-8ce8-079e25bb36a1";
    bool loaded = false;
    for (const auto& status : bl::csharp::sourceModStatuses()) {
        if (status.id == uid && status.loaded && status.sourceFiles > 0) {
            loaded = true;
            break;
        }
    }
    if (!loaded)
        return JS_ThrowInternalError(ctx, "Source C# Test nao carregado");
    const std::string root = bl::mods::rootOf(uid);
    if (root.empty())
        return JS_ThrowInternalError(ctx, "Pasta do mod fonte indisponivel");
    std::string error;
    const bool queued = bl::csharp::requestIsolatedMonoCompile(root, error);
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "queued", JS_NewBool(ctx, queued));
    JS_SetPropertyStr(ctx, out, "isolated", JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, out, "engine", JS_NewString(ctx, "Roslyn-net8"));
    JS_SetPropertyStr(ctx, out, "error", JS_NewString(ctx, error.c_str()));
    return out;
}

JSValue jsManagedSourceCompileStatus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    JSValue out = JS_NewObject(ctx);
    const auto state = readManagedWorkerFile("source-compile-status.txt");
    const auto diagnostics = readManagedWorkerFile("source-compile-diagnostics.txt");
    const auto checkpoint = readManagedWorkerFile("last-isolated-probe.txt");
    const auto health = readManagedWorkerFile("source-compile-health.txt");
    JS_SetPropertyStr(ctx, out, "status", JS_NewString(ctx, state.c_str()));
    JS_SetPropertyStr(ctx, out, "diagnostics", JS_NewString(ctx, diagnostics.c_str()));
    JS_SetPropertyStr(ctx, out, "checkpoint", JS_NewString(ctx, checkpoint.c_str()));
    JS_SetPropertyStr(ctx, out, "health", JS_NewString(ctx, health.c_str()));
    const auto pidAt = state.find("pid=");
    long workerPid = 0;
    if (pidAt != std::string::npos) {
        const char* begin = state.c_str() + pidAt + 4;
        char* end = nullptr;
        const long value = std::strtol(begin, &end, 10);
        if (end != begin && value > 1 && value <= 4194304 &&
            (*end == '\n' || *end == '\r' || *end == '\0'))
            workerPid = value;
    }
    const bool workerKnown = workerPid > 0;
    bool workerAlive = false;
    if (workerKnown) {
        const int result = kill(static_cast<pid_t>(workerPid), 0);
        workerAlive = result == 0 || (result < 0 && errno == EPERM);
    }
    JS_SetPropertyStr(ctx, out, "workerPid", JS_NewInt32(ctx, static_cast<int>(workerPid)));
    JS_SetPropertyStr(ctx, out, "workerKnown", JS_NewBool(ctx, workerKnown));
    JS_SetPropertyStr(ctx, out, "workerAlive", JS_NewBool(ctx, workerAlive));
    JS_SetPropertyStr(ctx, out, "staleCompilingState",
        JS_NewBool(ctx, workerKnown && !workerAlive &&
            state.find("state=compiling") != std::string::npos));
    std::string exitInfo;
    if (workerKnown && !workerAlive) {
        const auto timestampAt = checkpoint.find("timestampUnix=");
        long long minimumTimestampMs = 0;
        if (timestampAt != std::string::npos) {
            const char* start = checkpoint.c_str() + timestampAt + 14;
            char* end = nullptr;
            const long long stamp = std::strtoll(start, &end, 10);
            if (end != start && stamp > 0 && stamp < 100000000000LL)
                minimumTimestampMs = stamp * 1000;
        }
        exitInfo = bl::csharp::historicalMonoWorkerExit(
            static_cast<int>(workerPid), minimumTimestampMs);
    }
    JS_SetPropertyStr(ctx, out, "exitInfo", JS_NewString(ctx, exitInfo.c_str()));
    return out;
}

JSValue jsManagedWorkerStatus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    JSValue out = JS_NewObject(ctx);
    const auto state = readManagedWorkerFile("isolated-worker-status.txt");
    const auto checkpoint = readManagedWorkerFile("last-isolated-probe.txt");
    JS_SetPropertyStr(ctx, out, "status", JS_NewString(ctx, state.c_str()));
    JS_SetPropertyStr(ctx, out, "checkpoint", JS_NewString(ctx, checkpoint.c_str()));
    return out;
}

JSValue jsSourceMods(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    JSValue out = JS_NewArray(ctx);
    const auto mods = bl::csharp::sourceModStatuses();
    for (uint32_t i = 0; i < mods.size(); ++i) {
        JSValue row = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, row, "id", JS_NewString(ctx, mods[i].id.c_str()));
        JS_SetPropertyStr(ctx, row, "loaded", JS_NewBool(ctx, mods[i].loaded));
        JS_SetPropertyStr(ctx, row, "files", JS_NewInt32(ctx, mods[i].sourceFiles));
        JS_SetPropertyStr(ctx, row, "items", JS_NewInt32(ctx, mods[i].items));
        JS_SetPropertyStr(ctx, row, "projectiles", JS_NewInt32(ctx, mods[i].projectiles));
        JS_SetPropertyStr(ctx, row, "engine", JS_NewString(ctx, "SetDefaults-literals-v1"));
        JS_SetPropertyStr(ctx, row, "error", JS_NewString(ctx, mods[i].error.c_str()));
        JS_SetPropertyUint32(ctx, out, i, row);
    }
    return out;
}

JSValue jsExternalMods(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    JSValue out = JS_NewArray(ctx);
    const auto mods = bl::csharpmods::snapshot();
    uint32_t at = 0;
    for (const auto& mod : mods) {
        JSValue row = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, row, "id", JS_NewString(ctx, mod.id.c_str()));
        JS_SetPropertyStr(ctx, row, "abi", JS_NewInt32(ctx, mod.abiVersion));
        JS_SetPropertyStr(ctx, row, "hostApi", JS_NewInt32(ctx, mod.hostApiVersion));
        JS_SetPropertyStr(ctx, row, "loaded", JS_NewBool(ctx, mod.loaded));
        JS_SetPropertyStr(ctx, row, "projectileBehavior", JS_NewBool(ctx, mod.projectileBehavior));
        JS_SetPropertyStr(ctx, row, "selectiveProjectiles", JS_NewBool(ctx, mod.selectiveProjectiles));
        JS_SetPropertyStr(ctx, row, "projectileBindings", JS_NewInt32(ctx, mod.projectileBindings));
        JS_SetPropertyStr(ctx, row, "npcBehavior", JS_NewBool(ctx, mod.npcBehavior));
        JS_SetPropertyStr(ctx, row, "npcBindings", JS_NewInt32(ctx, mod.npcBindings));
        JS_SetPropertyStr(ctx, row, "npcCombat", JS_NewBool(ctx, mod.npcCombat));
        JS_SetPropertyStr(ctx, row, "nativeContent", JS_NewBool(ctx, mod.nativeContent));
        JS_SetPropertyStr(ctx, row, "nativeItems", JS_NewInt32(ctx, mod.nativeItems));
        JS_SetPropertyStr(ctx, row, "nativeProjectiles", JS_NewInt32(ctx, mod.nativeProjectiles));
        JS_SetPropertyStr(ctx, row, "nativeNpcs", JS_NewInt32(ctx, mod.nativeNpcs));
        JS_SetPropertyStr(ctx, row, "nativeAccessories", JS_NewInt32(ctx, mod.nativeAccessories));
        JS_SetPropertyStr(ctx, row, "nativeBuffs", JS_NewInt32(ctx, mod.nativeBuffs));
        JS_SetPropertyStr(ctx, row, "nativePotions", JS_NewInt32(ctx, mod.nativePotions));
        JS_SetPropertyStr(ctx, row, "nativeNpcType", JS_NewInt32(ctx, mod.nativeFirstNpcType));
        JS_SetPropertyStr(ctx, row, "nativeAccessoryType", JS_NewInt32(ctx, mod.nativeFirstAccessoryType));
        JS_SetPropertyStr(ctx, row, "nativeBuffType", JS_NewInt32(ctx, mod.nativeFirstBuffType));
        JS_SetPropertyUint32(ctx, out, at++, row);
    }
    return out;
}

JSValue jsHostInfo(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "available", JS_NewBool(ctx, bl::csharp::available() || bl::csharpmods::loadedCount() > 0));
    JS_SetPropertyStr(ctx, out, "runtimeAbi", JS_NewInt32(ctx, bl::csharp::version()));
    JS_SetPropertyStr(ctx, out, "hostApi", JS_NewInt32(ctx, bl::csharp::hostApiVersion()));
    JS_SetPropertyStr(ctx, out, "projectileStage", JS_NewString(ctx, "Projectile.UpdatePosition"));
    JS_SetPropertyStr(ctx, out, "safeClaim", JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, out, "automaticOwnedProjectiles", JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, out, "playerWrite", JS_NewBool(ctx, false));
    JS_SetPropertyStr(ctx, out, "npcWrite", JS_NewBool(ctx, bl::csharp::npcAiStatus().installed));
    JS_SetPropertyStr(ctx, out, "projectileVelocityWrite", JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, out, "externalModCount",
                      JS_NewInt32(ctx, static_cast<int32_t>(bl::csharpmods::loadedCount())));
    return out;
}

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


JSValue makeFlatNpcArray(JSContext* ctx, const std::vector<bl::csharp::NpcSample>& samples) {
    JSValue out = JS_NewArray(ctx);
    uint32_t at = 0;
    for (const auto& s : samples) {
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.index));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.type));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.life));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.lifeMax));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewFloat64(ctx, s.x));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewFloat64(ctx, s.y));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewFloat64(ctx, s.vx));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewFloat64(ctx, s.vy));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.flags));
    }
    return out;
}

JSValue makeFlatProjectileArray(JSContext* ctx, const std::vector<bl::csharp::ProjectileSample>& samples) {
    JSValue out = JS_NewArray(ctx);
    uint32_t at = 0;
    for (const auto& s : samples) {
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.index));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.type));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.owner));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.timeLeft));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.damage));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewFloat64(ctx, s.x));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewFloat64(ctx, s.y));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewFloat64(ctx, s.vx));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewFloat64(ctx, s.vy));
        JS_SetPropertyUint32(ctx, out, at++, JS_NewInt32(ctx, s.flags));
    }
    return out;
}

constexpr char kTerrariaSuiteSource[] = R"JS(
(function(npcs, projectiles, playerX, playerY, repeats) {
    const npcStride = 9;
    const projectileStride = 10;

    function npcScanJs() {
        let checksum = 0.0;
        for (let repeat = 0; repeat < repeats; repeat++) {
            let bestIndex = -1;
            let bestDistance = Number.MAX_VALUE;
            for (let at = 0; at < npcs.length; at += npcStride) {
                const flags = npcs[at + 8] | 0;
                const life = npcs[at + 2] | 0;
                if ((flags & 1) === 0 || (flags & 6) !== 0 || life <= 0) continue;

                const dx = npcs[at + 4] - playerX;
                const dy = npcs[at + 5] - playerY;
                const distance = dx * dx + dy * dy;
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestIndex = npcs[at] | 0;
                }
            }
            checksum += bestIndex;
            if (bestIndex >= 0) checksum += bestDistance * 0.000000001;
        }
        return checksum;
    }

    function projectileScanJs() {
        let checksum = 0.0;
        for (let repeat = 0; repeat < repeats; repeat++) {
            for (let at = 0; at < projectiles.length; at += projectileStride) {
                const flags = projectiles[at + 9] | 0;
                if ((flags & 1) === 0) continue;

                checksum += (projectiles[at] | 0) * 0.00001;
                checksum += (projectiles[at + 1] | 0) * 0.00002;
                checksum += (projectiles[at + 2] | 0) * 0.00003;
                checksum += (projectiles[at + 3] | 0) * 0.00004;
                checksum += (projectiles[at + 4] | 0) * 0.00005;
                checksum += projectiles[at + 5] * 0.000001;
                checksum += projectiles[at + 6] * 0.000002;
                checksum += projectiles[at + 7] * 0.0003;
                checksum += projectiles[at + 8] * 0.0004;
            }
        }
        return checksum;
    }

    let t0 = bl.csharp.clockMs();
    const npcValue = npcScanJs();
    let t1 = bl.csharp.clockMs();

    let t2 = bl.csharp.clockMs();
    const projectileValue = projectileScanJs();
    let t3 = bl.csharp.clockMs();

    return {
        npcMs: t1 - t0,
        npcValue,
        projectileMs: t3 - t2,
        projectileValue
    };
})
)JS";

JSValue makeTerrariaSuite(JSContext* ctx) {
    return JS_Eval(ctx, kTerrariaSuiteSource, sizeof(kTerrariaSuiteSource) - 1,
                   "csharp-terraria-suite", JS_EVAL_TYPE_GLOBAL);
}

JSValue jsTerrariaSuite(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int32_t repeats = 1000;
    if (argc > 0 && JS_ToInt32(ctx, &repeats, argv[0]) < 0) return JS_EXCEPTION;
    repeats = std::clamp(repeats, 1, 1000000);

    bl::csharp::TerrariaSnapshot snapshot = bl::csharp::captureTerrariaSnapshot();
    if (!snapshot.ready) {
        return JS_ThrowInternalError(ctx, "bl.csharp.terrariaSuite: Terraria ainda nao esta pronto");
    }

    JSValue npcData = makeFlatNpcArray(ctx, snapshot.npcs);
    JSValue projectileData = makeFlatProjectileArray(ctx, snapshot.projectiles);
    JSValue suite = makeTerrariaSuite(ctx);
    if (JS_IsException(suite)) {
        JS_FreeValue(ctx, npcData);
        JS_FreeValue(ctx, projectileData);
        return suite;
    }

    JSValue args[] = {
        npcData,
        projectileData,
        JS_NewFloat64(ctx, snapshot.player.x),
        JS_NewFloat64(ctx, snapshot.player.y),
        JS_NewInt32(ctx, repeats)
    };

    JSValue jsResult = JS_Call(ctx, suite, JS_UNDEFINED, 5, args);
    JS_FreeValue(ctx, suite);
    for (JSValue arg : args) JS_FreeValue(ctx, arg);
    if (JS_IsException(jsResult)) return jsResult;

    const double cNpcStart = bl::csharp::clockMs();
    const double cNpcValue = bl::csharp::npcScan(
        snapshot.npcs.data(), static_cast<int>(snapshot.npcs.size()),
        snapshot.player.x, snapshot.player.y, repeats);
    const double cNpcMs = bl::csharp::clockMs() - cNpcStart;

    const double cProjectileStart = bl::csharp::clockMs();
    const double cProjectileValue = bl::csharp::projectileScan(
        snapshot.projectiles.data(), static_cast<int>(snapshot.projectiles.size()), repeats);
    const double cProjectileMs = bl::csharp::clockMs() - cProjectileStart;

    double jsNpcMs = 0.0;
    double jsNpcValue = 0.0;
    double jsProjectileMs = 0.0;
    double jsProjectileValue = 0.0;
    JSValue v = JS_GetPropertyStr(ctx, jsResult, "npcMs");
    JS_ToFloat64(ctx, &jsNpcMs, v);
    JS_FreeValue(ctx, v);
    v = JS_GetPropertyStr(ctx, jsResult, "npcValue");
    JS_ToFloat64(ctx, &jsNpcValue, v);
    JS_FreeValue(ctx, v);
    v = JS_GetPropertyStr(ctx, jsResult, "projectileMs");
    JS_ToFloat64(ctx, &jsProjectileMs, v);
    JS_FreeValue(ctx, v);
    v = JS_GetPropertyStr(ctx, jsResult, "projectileValue");
    JS_ToFloat64(ctx, &jsProjectileValue, v);
    JS_FreeValue(ctx, v);
    JS_FreeValue(ctx, jsResult);

    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "abi", JS_NewInt32(ctx, bl::csharp::version()));
    JS_SetPropertyStr(ctx, out, "captureMs", JS_NewFloat64(ctx, snapshot.captureMs));
    JS_SetPropertyStr(ctx, out, "repeats", JS_NewInt32(ctx, repeats));

    JSValue player = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, player, "index", JS_NewInt32(ctx, snapshot.player.index));
    JS_SetPropertyStr(ctx, player, "life", JS_NewInt32(ctx, snapshot.player.life));
    JS_SetPropertyStr(ctx, player, "lifeMax", JS_NewInt32(ctx, snapshot.player.lifeMax));
    JS_SetPropertyStr(ctx, player, "x", JS_NewFloat64(ctx, snapshot.player.x));
    JS_SetPropertyStr(ctx, player, "y", JS_NewFloat64(ctx, snapshot.player.y));
    JS_SetPropertyStr(ctx, out, "player", player);

    JSValue npc = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, npc, "slots", JS_NewInt32(ctx, static_cast<int32_t>(snapshot.npcs.size())));
    JS_SetPropertyStr(ctx, npc, "active", JS_NewInt32(ctx, snapshot.activeNpcs));
    JS_SetPropertyStr(ctx, npc, "jsMs", JS_NewFloat64(ctx, jsNpcMs));
    JS_SetPropertyStr(ctx, npc, "csharpMs", JS_NewFloat64(ctx, cNpcMs));
    JS_SetPropertyStr(ctx, npc, "speedup", JS_NewFloat64(ctx, cNpcMs > 0.0 ? jsNpcMs / cNpcMs : 0.0));
    JS_SetPropertyStr(ctx, npc, "jsValue", JS_NewFloat64(ctx, jsNpcValue));
    JS_SetPropertyStr(ctx, npc, "csharpValue", JS_NewFloat64(ctx, cNpcValue));
    JS_SetPropertyStr(ctx, npc, "delta", JS_NewFloat64(ctx, std::abs(jsNpcValue - cNpcValue)));
    JS_SetPropertyStr(ctx, out, "npcScan", npc);

    JSValue projectile = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, projectile, "slots", JS_NewInt32(ctx, static_cast<int32_t>(snapshot.projectiles.size())));
    JS_SetPropertyStr(ctx, projectile, "active", JS_NewInt32(ctx, snapshot.activeProjectiles));
    JS_SetPropertyStr(ctx, projectile, "jsMs", JS_NewFloat64(ctx, jsProjectileMs));
    JS_SetPropertyStr(ctx, projectile, "csharpMs", JS_NewFloat64(ctx, cProjectileMs));
    JS_SetPropertyStr(ctx, projectile, "speedup",
                      JS_NewFloat64(ctx, cProjectileMs > 0.0 ? jsProjectileMs / cProjectileMs : 0.0));
    JS_SetPropertyStr(ctx, projectile, "jsValue", JS_NewFloat64(ctx, jsProjectileValue));
    JS_SetPropertyStr(ctx, projectile, "csharpValue", JS_NewFloat64(ctx, cProjectileValue));
    JS_SetPropertyStr(ctx, projectile, "delta",
                      JS_NewFloat64(ctx, std::abs(jsProjectileValue - cProjectileValue)));
    JS_SetPropertyStr(ctx, out, "projectileScan", projectile);
    return out;
}



bool containsNoCase(std::string_view text, std::string_view needle) {
    if (needle.empty()) return true;
    if (text.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= text.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            const unsigned char a = static_cast<unsigned char>(text[i + j]);
            const unsigned char b = static_cast<unsigned char>(needle[j]);
            if (std::tolower(a) != std::tolower(b)) {
                ok = false;
                break;
            }
        }
        if (ok) return true;
    }
    return false;
}

std::string pointerHex(const void* p) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%llx",
                  static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(p)));
    return buf;
}

JSValue methodEntry(JSContext* ctx, const MethodInfo* method) {
    auto& api = bl::il2cpp::api();
    JSValue out = JS_NewObject(ctx);
    const char* name = api.method_get_name(method);
    JS_SetPropertyStr(ctx, out, "name", JS_NewString(ctx, name ? name : ""));
    const std::string signature = bl::il2cpp::describeMethod(method);
    JS_SetPropertyStr(ctx, out, "signature", JS_NewString(ctx, signature.c_str()));
    const std::string address = pointerHex(bl::il2cpp::methodPointer(method));
    JS_SetPropertyStr(ctx, out, "address", JS_NewString(ctx, address.c_str()));
    JS_SetPropertyStr(ctx, out, "params",
                      JS_NewInt32(ctx, static_cast<int32_t>(api.method_get_param_count(method))));
    JS_SetPropertyStr(ctx, out, "instance",
                      JS_NewBool(ctx, api.method_is_instance ? api.method_is_instance(method) : true));
    return out;
}

JSValue collectMethods(JSContext* ctx, Il2CppClass* cls,
                       bool (*keep)(std::string_view, const std::string&),
                       int* total) {
    JSValue out = JS_NewArray(ctx);
    uint32_t at = 0;
    *total = 0;
    auto& api = bl::il2cpp::api();
    if (!cls) return out;

    for (Il2CppClass* c = cls; c; c = api.class_get_parent(c)) {
        void* iter = nullptr;
        while (const MethodInfo* method = api.class_get_methods(c, &iter)) {
            ++*total;
            const char* rawName = api.method_get_name(method);
            const std::string signature = bl::il2cpp::describeMethod(method);
            if (!keep(rawName ? std::string_view(rawName) : std::string_view{}, signature)) continue;
            JS_SetPropertyUint32(ctx, out, at++, methodEntry(ctx, method));
        }
    }
    return out;
}

bool keepProjectileProbe(std::string_view name, const std::string& signature) {
    static constexpr std::string_view words[] = {
        "update", "ai", "kill", "setdefaults", "collision", "position",
        "movement", "active", "timeleft", "damage", "proj", "step"
    };
    for (std::string_view word : words) {
        if (containsNoCase(name, word) || containsNoCase(signature, word)) return true;
    }
    return false;
}

bool keepMainProbe(std::string_view name, const std::string& signature) {
    static constexpr std::string_view words[] = {
        "projectile", "proj", "doupdate", "update", "game"
    };
    for (std::string_view word : words) {
        if (containsNoCase(name, word) || containsNoCase(signature, word)) return true;
    }
    return false;
}

JSValue jsProjectileProbe(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    using namespace bl::il2cpp;
    Il2CppClass* projectile = findClass({"Terraria", "Projectile", {}});
    Il2CppClass* main = findClass({"Terraria", "Main", {}});

    JSValue out = JS_NewObject(ctx);
    int projectileTotal = 0;
    int mainTotal = 0;
    JSValue projectileMethods = collectMethods(ctx, projectile, keepProjectileProbe, &projectileTotal);
    JSValue mainMethods = collectMethods(ctx, main, keepMainProbe, &mainTotal);

    JS_SetPropertyStr(ctx, out, "projectileMethodCount", JS_NewInt32(ctx, projectileTotal));
    JS_SetPropertyStr(ctx, out, "mainMethodCount", JS_NewInt32(ctx, mainTotal));
    JS_SetPropertyStr(ctx, out, "projectileCandidates", projectileMethods);
    JS_SetPropertyStr(ctx, out, "mainCandidates", mainMethods);
    return out;
}

JSValue jsProjectileMethods(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    std::string filter;
    if (argc > 0 && JS_IsString(argv[0])) {
        const char* raw = JS_ToCString(ctx, argv[0]);
        if (raw) {
            filter = raw;
            JS_FreeCString(ctx, raw);
        }
    }

    Il2CppClass* projectile = bl::il2cpp::findClass({"Terraria", "Projectile", {}});
    JSValue out = JS_NewArray(ctx);
    uint32_t at = 0;
    auto& api = bl::il2cpp::api();
    if (!projectile) return out;

    void* iter = nullptr;
    while (const MethodInfo* method = api.class_get_methods(projectile, &iter)) {
        const char* rawName = api.method_get_name(method);
        const std::string signature = bl::il2cpp::describeMethod(method);
        if (!filter.empty() &&
            !containsNoCase(rawName ? std::string_view(rawName) : std::string_view{}, filter) &&
            !containsNoCase(signature, filter)) {
            continue;
        }
        JS_SetPropertyUint32(ctx, out, at++, methodEntry(ctx, method));
    }
    return out;
}

JSValue jsNpcAttack(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int32_t slot = -1;
    int32_t type = -1;
    if (argc < 2 || JS_ToInt32(ctx, &slot, argv[0]) < 0 ||
        JS_ToInt32(ctx, &type, argv[1]) < 0) return JS_EXCEPTION;

    bl::csharp::HostNpcAttack attack{};
    if (!bl::csharp::takeNpcAttack(slot, type, &attack)) return JS_NULL;

    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "count", JS_NewInt32(ctx, attack.count));
    JS_SetPropertyStr(ctx, out, "damage", JS_NewInt32(ctx, attack.damage));
    JS_SetPropertyStr(ctx, out, "x", JS_NewFloat64(ctx, attack.x));
    JS_SetPropertyStr(ctx, out, "y", JS_NewFloat64(ctx, attack.y));
    JS_SetPropertyStr(ctx, out, "vx0", JS_NewFloat64(ctx, attack.vx0));
    JS_SetPropertyStr(ctx, out, "vy0", JS_NewFloat64(ctx, attack.vy0));
    JS_SetPropertyStr(ctx, out, "vx1", JS_NewFloat64(ctx, attack.vx1));
    JS_SetPropertyStr(ctx, out, "vy1", JS_NewFloat64(ctx, attack.vy1));
    return out;
}

JSValue jsNpcStatus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    const auto status = bl::csharp::npcAiStatus();
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "installed", JS_NewBool(ctx, status.installed));
    JS_SetPropertyStr(ctx, out, "calls", JS_NewInt32(ctx, status.calls));
    JS_SetPropertyStr(ctx, out, "writes", JS_NewInt32(ctx, status.writes));
    JS_SetPropertyStr(ctx, out, "spawns", JS_NewInt32(ctx, status.spawns));
    JS_SetPropertyStr(ctx, out, "releases", JS_NewInt32(ctx, status.releases));
    JS_SetPropertyStr(ctx, out, "attacksQueued", JS_NewInt32(ctx, status.attacksQueued));
    JS_SetPropertyStr(ctx, out, "attacksConsumed", JS_NewInt32(ctx, status.attacksConsumed));
    JS_SetPropertyStr(ctx, out, "attacksRejected", JS_NewInt32(ctx, status.attacksRejected));
    return out;
}

JSValue writeStatusToJs(JSContext* ctx, const bl::csharp::ProjectileWriteStatus& status) {
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "installed", JS_NewBool(ctx, status.installed));
    JS_SetPropertyStr(ctx, out, "automaticCalls", JS_NewInt32(ctx, status.automaticCalls));
    JS_SetPropertyStr(ctx, out, "automaticWrites", JS_NewInt32(ctx, status.automaticWrites));
    JS_SetPropertyStr(ctx, out, "automaticSpawns", JS_NewInt32(ctx, status.automaticSpawns));
    JS_SetPropertyStr(ctx, out, "automaticReleases", JS_NewInt32(ctx, status.automaticReleases));
    JS_SetPropertyStr(ctx, out, "claimed", JS_NewBool(ctx, status.claimed));
    JS_SetPropertyStr(ctx, out, "valid", JS_NewBool(ctx, status.valid));
    JS_SetPropertyStr(ctx, out, "slot", JS_NewInt32(ctx, status.slot));
    JS_SetPropertyStr(ctx, out, "type", JS_NewInt32(ctx, status.type));
    JS_SetPropertyStr(ctx, out, "owner", JS_NewInt32(ctx, status.owner));
    JS_SetPropertyStr(ctx, out, "generation", JS_NewInt32(ctx, status.generation));
    JS_SetPropertyStr(ctx, out, "targetIndex", JS_NewInt32(ctx, status.targetIndex));
    JS_SetPropertyStr(ctx, out, "writes", JS_NewInt32(ctx, status.writes));
    JS_SetPropertyStr(ctx, out, "loopCalls", JS_NewInt32(ctx, status.loopCalls));
    JS_SetPropertyStr(ctx, out, "lastSlot", JS_NewInt32(ctx, status.lastSlot));
    JS_SetPropertyStr(ctx, out, "lastType", JS_NewInt32(ctx, status.lastType));
    JS_SetPropertyStr(ctx, out, "lastTargetIndex", JS_NewInt32(ctx, status.lastTargetIndex));
    JS_SetPropertyStr(ctx, out, "lastWrites", JS_NewInt32(ctx, status.lastWrites));
    JS_SetPropertyStr(ctx, out, "lastLoopCalls", JS_NewInt32(ctx, status.lastLoopCalls));
    return out;
}

JSValue jsClaimProjectile(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int32_t slot = -1;
    if (argc > 0 && !JS_IsUndefined(argv[0]) && JS_ToInt32(ctx, &slot, argv[0]) < 0) {
        return JS_EXCEPTION;
    }
    return writeStatusToJs(ctx, bl::csharp::claimProjectile(slot));
}

JSValue jsReleaseProjectile(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    bl::csharp::releaseProjectile();
    return writeStatusToJs(ctx, bl::csharp::projectileWriteStatus());
}

JSValue jsProjectileWriteStatus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    return writeStatusToJs(ctx, bl::csharp::projectileWriteStatus());
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
    JS_SetPropertyStr(ctx, api, "hostInfo", JS_NewCFunction(ctx, jsHostInfo, "hostInfo", 0));
    JS_SetPropertyStr(ctx, api, "sourceMods", JS_NewCFunction(ctx, jsSourceMods, "sourceMods", 0));
    JS_SetPropertyStr(ctx, api, "managedRuntime",
                      JS_NewCFunction(ctx, jsManagedRuntime, "managedRuntime", 0));
    JS_SetPropertyStr(ctx, api, "managedInspect",
                      JS_NewCFunction(ctx, jsManagedInspect, "managedInspect", 0));
    JS_SetPropertyStr(ctx, api, "managedProbe",
                      JS_NewCFunction(ctx, jsManagedProbe, "managedProbe", 0));
    JS_SetPropertyStr(ctx, api, "managedWorkerStatus",
                      JS_NewCFunction(ctx, jsManagedWorkerStatus, "managedWorkerStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourceCompile",
                      JS_NewCFunction(ctx, jsManagedSourceCompile, "sourceCompile", 0));
    JS_SetPropertyStr(ctx, api, "sourceCompileStatus",
                      JS_NewCFunction(ctx, jsManagedSourceCompileStatus, "sourceCompileStatus", 0));
    JS_SetPropertyStr(ctx, api, "managedGameProbe",
                      JS_NewCFunction(ctx, jsManagedGameProbe, "managedGameProbe", 1));
    JS_SetPropertyStr(ctx, api, "managedCallbackProbe",
                      JS_NewCFunction(ctx, jsManagedCallbackProbe, "managedCallbackProbe", 1));
    JS_SetPropertyStr(ctx, api, "managedProjectileEnable",
                      JS_NewCFunction(ctx, jsManagedProjectileEnable, "managedProjectileEnable", 1));
    JS_SetPropertyStr(ctx, api, "managedProjectileStatus",
                      JS_NewCFunction(ctx, jsManagedProjectileStatus, "managedProjectileStatus", 0));
    JS_SetPropertyStr(ctx, api, "externalMods",
                      JS_NewCFunction(ctx, jsExternalMods, "externalMods", 0));
    JS_SetPropertyStr(ctx, api, "version", JS_NewCFunction(ctx, jsVersion, "version", 0));
    JS_SetPropertyStr(ctx, api, "add", JS_NewCFunction(ctx, jsAdd, "add", 2));
    JS_SetPropertyStr(ctx, api, "checksum", JS_NewCFunction(ctx, jsChecksum, "checksum", 1));
    JS_SetPropertyStr(ctx, api, "playerSample", JS_NewCFunction(ctx, jsPlayerSample, "playerSample", 4));
    JS_SetPropertyStr(ctx, api, "benchmark", JS_NewCFunction(ctx, jsBenchmark, "benchmark", 1));
    JS_SetPropertyStr(ctx, api, "clockMs", JS_NewCFunction(ctx, jsClockMs, "clockMs", 0));
    JS_SetPropertyStr(ctx, api, "mathBatch", JS_NewCFunction(ctx, jsMathBatch, "mathBatch", 2));
    JS_SetPropertyStr(ctx, api, "projectileBatch", JS_NewCFunction(ctx, jsProjectileBatch, "projectileBatch", 2));
    JS_SetPropertyStr(ctx, api, "terrariaSuite", JS_NewCFunction(ctx, jsTerrariaSuite, "terrariaSuite", 1));
    JS_SetPropertyStr(ctx, api, "claimProjectile", JS_NewCFunction(ctx, jsClaimProjectile, "claimProjectile", 1));
    JS_SetPropertyStr(ctx, api, "releaseProjectile", JS_NewCFunction(ctx, jsReleaseProjectile, "releaseProjectile", 0));
    JS_SetPropertyStr(ctx, api, "projectileWriteStatus",
                      JS_NewCFunction(ctx, jsProjectileWriteStatus, "projectileWriteStatus", 0));
    JS_SetPropertyStr(ctx, api, "npcStatus", JS_NewCFunction(ctx, jsNpcStatus, "npcStatus", 0));
    JS_SetPropertyStr(ctx, api, "npcAttack", JS_NewCFunction(ctx, jsNpcAttack, "npcAttack", 2));
    JS_SetPropertyStr(ctx, api, "projectileProbe",
                      JS_NewCFunction(ctx, jsProjectileProbe, "projectileProbe", 0));
    JS_SetPropertyStr(ctx, api, "projectileMethods",
                      JS_NewCFunction(ctx, jsProjectileMethods, "projectileMethods", 1));

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
