#include "content/items/ModItems.h"
#include "script/api/CSharpPrototypeApi.h"

#include "experiments/csharp/CSharpPrototype.h"
#include "experiments/csharp/TerrariaHost.h"
#include "experiments/csharp/ProjectileWriteTest.h"
#include "experiments/csharp/CSharpModLoader.h"
#include "experiments/csharp/SourceLoader.h"
#include "experiments/csharp/NativeRarityRegistry.h"
#include "experiments/csharp/NativeRarityGameplay.h"
#include "experiments/csharp/AutoSourceRuntime.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/NativeContent.h"
#include "experiments/csharp/NativeBasicItems.h"
#include "experiments/csharp/NativeArmorPolicies.h"
#include "experiments/csharp/NativeItemPolicies.h"
#include "experiments/csharp/NativeAbortProbe.h"
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
#include <array>
#include <sstream>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <cerrno>
#include <cstdlib>
#include <csignal>
#include <fstream>
#include <unistd.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

#if BL_HAVE_QUICKJS
#include "quickjs.h"

namespace bl::script {

namespace {

bool sourceAssemblyTypesVerified(
    const std::string& state,
    const std::string& expectedModId = "57a98f20-7a35-4d19-8ce8-079e25bb36a1");

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

JSValue jsManagedRarityColor(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1) return JS_ThrowTypeError(ctx, "rarityColor(type)");
    int32_t type = -1;
    if (JS_ToInt32(ctx, &type, argv[0]) < 0) return JS_EXCEPTION;
    uint32_t argb = 0;
    if (!bl::csharp::nativeManagedRarityColor(type, &argb)) return JS_NULL;
    JSValue channels = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, channels, 0, JS_NewInt32(ctx, (argb >> 16) & 255));
    JS_SetPropertyUint32(ctx, channels, 1, JS_NewInt32(ctx, (argb >> 8) & 255));
    JS_SetPropertyUint32(ctx, channels, 2, JS_NewInt32(ctx, argb & 255));
    return channels;
}

JSValue jsRarityCount(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "rarityCount()");
    return JS_NewInt32(ctx, bl::csharp::nativeRarities().nextType());
}

JSValue jsReserveRarityId(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "reserveRarityId()");
    const int id = bl::csharp::nativeRarities().reserveExternalType();
    if (id < 0) return JS_ThrowRangeError(ctx, "rarity ID capacity exhausted");
    return JS_NewInt32(ctx, id);
}

JSValue jsManagedNativeEquipTextures(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc) return JS_ThrowTypeError(ctx, "nativeEquipTextures()");
    const auto records = bl::csharp::managedNativeEquipTextures();
    JSValue array = JS_NewArray(ctx);
    uint32_t index = 0;
    for (const auto& entry : records) {
        JSValue record = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, record, "type", JS_NewInt32(ctx, entry.itemType));
        JS_SetPropertyStr(ctx, record, "kind", JS_NewInt32(ctx, entry.visualMode));
        JS_SetPropertyStr(ctx, record, "texture", JS_NewString(ctx, entry.texture.c_str()));
        JS_SetPropertyUint32(ctx, array, index++, record);
    }
    return array;
}

JSValue jsPendingItemRecipes(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if(argc) return JS_ThrowTypeError(ctx,"pendingItemRecipes()");
    JSValue array=JS_NewArray(ctx);uint32_t i=0;
    for(const auto& recipe:bl::csharp::pendingBasicRecipes()) {
        JSValue entry=JS_NewObject(ctx);
        JS_SetPropertyStr(ctx,entry,"id",JS_NewString(ctx,recipe.id.c_str()));
        JS_SetPropertyStr(ctx,entry,"mod",JS_NewString(ctx,recipe.modId.c_str()));
        JS_SetPropertyStr(ctx,entry,"source",JS_NewString(ctx,recipe.source.c_str()));
        JS_SetPropertyStr(ctx,entry,"error",JS_NewString(ctx,recipe.error.c_str()));
        JS_SetPropertyStr(ctx,entry,"result",JS_NewInt32(ctx,recipe.result));
        JS_SetPropertyStr(ctx,entry,"stack",JS_NewInt32(ctx,recipe.values.stack));
        JS_SetPropertyStr(ctx,entry,"station",JS_NewInt32(ctx,recipe.values.station));
        JS_SetPropertyStr(ctx,entry,"stationKey",JS_NewString(ctx,recipe.values.stationKey.c_str()));
        JSValue ingredients=JS_NewArray(ctx);uint32_t j=0;
        for(const auto& ingredient:recipe.values.ingredients) {
            JSValue value=JS_NewObject(ctx);
            JS_SetPropertyStr(ctx,value,"type",JS_NewInt32(ctx,ingredient.type));
            JS_SetPropertyStr(ctx,value,"count",JS_NewInt32(ctx,ingredient.count));
            JS_SetPropertyUint32(ctx,ingredients,j++,value);
        }
        JS_SetPropertyStr(ctx,entry,"ingredients",ingredients);
        JS_SetPropertyUint32(ctx,array,i++,entry);
    }
    return array;
}
JSValue jsNativeArmorPolicies(JSContext* ctx,JSValueConst,int argc,JSValueConst*) {
    if(argc)return JS_ThrowTypeError(ctx,"nativeArmorPolicies()");
    JSValue array=JS_NewArray(ctx);uint32_t i=0;
    for(const auto& status:bl::csharp::nativeArmorPolicyStatuses()) {
        JSValue value=JS_NewObject(ctx);
        JS_SetPropertyStr(ctx,value,"type",JS_NewInt32(ctx,status.type));
        JS_SetPropertyStr(ctx,value,"ready",JS_NewBool(ctx,status.ready));
        JS_SetPropertyStr(ctx,value,"mod",JS_NewString(ctx,status.mod.c_str()));
        JS_SetPropertyStr(ctx,value,"name",JS_NewString(ctx,status.name.c_str()));
        JS_SetPropertyStr(ctx,value,"phase",JS_NewString(ctx,status.phase.c_str()));
        JS_SetPropertyStr(ctx,value,"error",JS_NewString(ctx,status.error.c_str()));
        JS_SetPropertyUint32(ctx,array,i++,value);
    }
    return array;
}
JSValue jsNativeItemPolicies(JSContext* ctx,JSValueConst,int argc,JSValueConst*) {
    if(argc)return JS_ThrowTypeError(ctx,"nativeItemPolicies()");
    JSValue array=JS_NewArray(ctx);uint32_t i=0;
    for(const auto& status:bl::csharp::nativeItemPolicyStatus()) {
        JSValue value=JS_NewObject(ctx);
        JS_SetPropertyStr(ctx,value,"type",JS_NewInt32(ctx,status.type));
        JS_SetPropertyStr(ctx,value,"ready",JS_NewBool(ctx,status.ready));
        JS_SetPropertyStr(ctx,value,"mod",JS_NewString(ctx,status.mod.c_str()));
        JS_SetPropertyStr(ctx,value,"name",JS_NewString(ctx,status.name.c_str()));
        JS_SetPropertyStr(ctx,value,"phase",JS_NewString(ctx,status.phase.c_str()));
        JS_SetPropertyStr(ctx,value,"error",JS_NewString(ctx,status.error.c_str()));
        JS_SetPropertyUint32(ctx,array,i++,value);
    }
    return array;
}
JSValue jsPendingItemBehaviors(JSContext* ctx,JSValueConst,int argc,JSValueConst*) {
    if(argc)return JS_ThrowTypeError(ctx,"pendingItemBehaviors()");
    JSValue array=JS_NewArray(ctx);uint32_t i=0;
    for(const auto& item:bl::csharp::pendingBasicItemBehaviors()) {
        JSValue value=JS_NewObject(ctx);
        JS_SetPropertyStr(ctx,value,"type",JS_NewInt32(ctx,item.type));
        JS_SetPropertyStr(ctx,value,"name",JS_NewString(ctx,item.name.c_str()));
        JS_SetPropertyStr(ctx,value,"mod",JS_NewString(ctx,item.modId.c_str()));
        JS_SetPropertyStr(ctx,value,"source",JS_NewString(ctx,item.sourceFile.c_str()));
        JS_SetPropertyStr(ctx,value,"root",JS_NewString(ctx,(item.modDir+"/content/").c_str()));
        JS_SetPropertyStr(ctx,value,"definition",JS_NewString(ctx,item.behavior.c_str()));
        JS_SetPropertyStr(ctx,value,"error",JS_NewString(ctx,item.behaviorError.c_str()));
        JSValue defaults=JS_NewArray(ctx);
        for(uint32_t j=0;j<item.values.size();++j)JS_SetPropertyUint32(ctx,defaults,j,JS_NewInt32(ctx,item.values[j]));
        JS_SetPropertyStr(ctx,value,"defaults",defaults);
        JS_SetPropertyStr(ctx,value,"knockBack",JS_NewFloat64(ctx,item.knockBack));
        JS_SetPropertyUint32(ctx,array,i++,value);
    }
    return array;
}
JSValue jsAcknowledgeItemBehavior(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv) {
    if(argc!=2||!JS_IsString(argv[1]))return JS_ThrowTypeError(ctx,"acknowledgeItemBehavior(type,error)");
    int32_t type=-1;if(JS_ToInt32(ctx,&type,argv[0])<0)return JS_EXCEPTION;
    const char* raw=JS_ToCString(ctx,argv[1]);if(!raw)return JS_EXCEPTION;
    const std::string error(raw);JS_FreeCString(ctx,raw);
    return JS_NewBool(ctx,bl::csharp::acknowledgeBasicItemBehavior(type,error));
}

JSValue jsReportItemRecipeError(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv) {
    if(argc!=2||!JS_IsString(argv[0])||!JS_IsString(argv[1]))return JS_ThrowTypeError(ctx,"reportItemRecipeError(id,error)");
    const char* rawId=JS_ToCString(ctx,argv[0]);if(!rawId)return JS_EXCEPTION;
    const std::string id(rawId);JS_FreeCString(ctx,rawId);
    const char* rawError=JS_ToCString(ctx,argv[1]);if(!rawError)return JS_EXCEPTION;
    const std::string error(rawError);JS_FreeCString(ctx,rawError);
    return JS_NewBool(ctx,bl::csharp::reportBasicRecipeError(id,error));
}

JSValue jsAcknowledgeItemRecipe(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if(argc!=2||!JS_IsString(argv[0]))return JS_ThrowTypeError(ctx,"acknowledgeItemRecipe(id,index)");
    int32_t index=-1;if(JS_ToInt32(ctx,&index,argv[1])<0)return JS_EXCEPTION;
    const char* raw=JS_ToCString(ctx,argv[0]);if(!raw)return JS_EXCEPTION;
    const std::string id(raw);JS_FreeCString(ctx,raw);
    return JS_NewBool(ctx,bl::csharp::acknowledgeBasicRecipe(id,index));
}

JSValue jsSetNativeItemDefense(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv) {
    if(argc!=2)return JS_ThrowTypeError(ctx,"setNativeItemDefense(type,value)");
    int32_t type=-1,value=-1;
    if(JS_ToInt32(ctx,&type,argv[0])<0||JS_ToInt32(ctx,&value,argv[1])<0)return JS_EXCEPTION;
    std::string error;
    if(!bl::csharp::setBasicItemDefense(type,value,&error))return JS_ThrowInternalError(ctx,"%s",error.c_str());
    return JS_TRUE;
}
JSValue jsResolveNativeItemKey(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv) {
    if(argc!=1||!JS_IsString(argv[0]))return JS_ThrowTypeError(ctx,"resolveNativeItemKey('uid/name')");
    const char* raw=JS_ToCString(ctx,argv[0]);if(!raw)return JS_EXCEPTION;
    const std::string key(raw);JS_FreeCString(ctx,raw);
    return JS_NewInt32(ctx,key.size()<=160?bl::runtime::modItemTypeByKey(key):-1);
}

JSValue jsSetNativeItemEquipSlot(JSContext* ctx,JSValueConst,int argc,JSValueConst* argv) {
    if(argc!=3||!JS_IsString(argv[1]))return JS_ThrowTypeError(ctx,"setNativeItemEquipSlot(type,kind,slot)");
    int32_t type=-1,slot=-1;
    if(JS_ToInt32(ctx,&type,argv[0])<0||JS_ToInt32(ctx,&slot,argv[2])<0)return JS_EXCEPTION;
    const char* raw=JS_ToCString(ctx,argv[1]);if(!raw)return JS_EXCEPTION;
    const std::string kind(raw);JS_FreeCString(ctx,raw);
    std::string error;
    if(!bl::csharp::setBasicItemEquipSlot(type,kind,slot,&error))
        return JS_ThrowInternalError(ctx,"%s",error.c_str());
    return JS_TRUE;
}

JSValue jsManagedSetNativeEquipSlot(JSContext* ctx, JSValueConst, int argc,
                                   JSValueConst* argv) {
    if (argc != 3) return JS_ThrowTypeError(ctx, "setNativeEquipSlot(itemType, kind, slot)");
    int32_t type = -1, kind = -1, slot = -1;
    if (JS_ToInt32(ctx, &type, argv[0]) < 0 ||
        JS_ToInt32(ctx, &kind, argv[1]) < 0 ||
        JS_ToInt32(ctx, &slot, argv[2]) < 0) return JS_EXCEPTION;
    return JS_NewBool(ctx, bl::csharp::setManagedNativeEquipSlot(type, kind, slot));
}

JSValue jsManagedVisualTexturePath(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "visualTexturePath('modUid:Content/...png')");
    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw) return JS_EXCEPTION;
    const std::string qualified(raw);
    JS_FreeCString(ctx, raw);
    const size_t sep = qualified.find(':');
    if (sep != 36 || qualified.size() > 240 || qualified.find(':', sep + 1) != std::string::npos)
        return JS_NULL;
    const std::string modUid = qualified.substr(0, sep);
    const std::string relative = qualified.substr(sep + 1);
    if (relative.empty() || relative[0] == '/' ||
        relative.find("..") != std::string::npos ||
        relative.find(static_cast<char>(92)) != std::string::npos ||
        relative.find('|') != std::string::npos) return JS_NULL;
    for (size_t i = 0; i < modUid.size(); ++i) {
        const char ch = modUid[i];
        const bool dashed = i == 8 || i == 13 || i == 18 || i == 23;
        if ((dashed && ch != '-') ||
            (!dashed && !((ch >= '0' && ch <= '9') ||
                          (ch >= 'a' && ch <= 'f') ||
                          (ch >= 'A' && ch <= 'F')))) return JS_NULL;
    }
    const std::string modDir = bl::mods::dirOf(modUid);
    if (modDir.empty()) return JS_NULL;
    const std::string fullPath = modDir + "/" + relative;
    struct stat st{};
    if (stat(fullPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_size <= 0 || st.st_size > 1024 * 1024) return JS_NULL;
    return JS_NewString(ctx, fullPath.c_str());
}

JSValue jsManagedVisualActiveCount(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "visualActiveCount()");
    return JS_NewInt32(ctx, static_cast<int>(bl::csharp::activeManagedVisualCount()));
}

JSValue jsManagedVisualSnapshot(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1) return JS_ThrowTypeError(ctx, "visualSnapshot(playerIndex)");
    int32_t player = -1;
    if (JS_ToInt32(ctx, &player, argv[0]) < 0) return JS_EXCEPTION;
    if (player < 0 || player >= 256) return JS_ThrowRangeError(ctx, "playerIndex fora do limite");
    const auto active = bl::csharp::snapshotManagedVisualCommands(player, 8);
    JSValue list = JS_NewArray(ctx);
    uint32_t index = 0;
    for (const auto& effect : active) {
        JSValue record = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, record, "kind", JS_NewInt32(ctx, static_cast<int>(effect.kind)));
        JS_SetPropertyStr(ctx, record, "style", JS_NewInt32(ctx, effect.style));
        JS_SetPropertyStr(ctx, record, "effectId", JS_NewInt32(ctx, effect.effectId));
        JS_SetPropertyStr(ctx, record, "texture", JS_NewString(ctx, effect.texture.c_str()));
        JS_SetPropertyStr(ctx, record, "scale", JS_NewFloat64(ctx, effect.scale));
        JS_SetPropertyStr(ctx, record, "opacity", JS_NewFloat64(ctx, effect.opacity));
        JS_SetPropertyStr(ctx, record, "color", JS_NewInt64(ctx, static_cast<int64_t>(effect.color)));
        JS_SetPropertyUint32(ctx, list, index++, record);
    }
    return list;
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
    const bool fullNativeTrace =
        std::string_view(filename) == "native-abort-trace.txt" ||
        std::string_view(filename) == "native-game-abort-trace.txt" ||
        std::string_view(filename) == "native-game-abort-previous.txt" ||
        std::string_view(filename) == "mono-domain-thread-audit-latest.txt";
    char buffer[8193]{};
    stream.read(buffer, fullNativeTrace ? 8192 : 2048);
    return std::string(buffer, static_cast<size_t>(stream.gcount()));
}

JSValue jsManagedGameProbe(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if ((argc != 1 && argc != 2) || !JS_IsString(argv[0]) ||
        (argc == 2 && !JS_IsString(argv[1])))
        return JS_ThrowTypeError(ctx,
            "managedGameProbe('CONFIRM_IN_PROCESS', 'NO_MONO'|'LIBRARY_ONLY'|'PATH_ONLY'|'SYSTEM_NATIVE_ONLY'|'DOMAIN_ONLY'|'ATTACH_ONLY'|'DOMAIN_THREAD_AUDIT'|'PINNED_MONO'|'MONO_ONLY'|'ABORT_ONLY'|'FULL_DIAGNOSTICS')");
    std::string selectedMode = "FULL_DIAGNOSTICS";
    if (argc == 2) {
        const char* rawMode = JS_ToCString(ctx, argv[1]);
        if (!rawMode) return JS_EXCEPTION;
        selectedMode = rawMode;
        JS_FreeCString(ctx, rawMode);
    }
    if (selectedMode != "MONO_ONLY" && selectedMode != "ABORT_ONLY" &&
        selectedMode != "FULL_DIAGNOSTICS" && selectedMode != "NO_MONO" &&
        selectedMode != "PINNED_MONO" && selectedMode != "LIBRARY_ONLY" &&
        selectedMode != "PATH_ONLY" && selectedMode != "SYSTEM_NATIVE_ONLY" &&
        selectedMode != "DOMAIN_ONLY" && selectedMode != "ATTACH_ONLY" &&
        selectedMode != "DOMAIN_THREAD_AUDIT")
        return JS_ThrowTypeError(ctx, "Modo invalido: NO_MONO, LIBRARY_ONLY, PATH_ONLY, SYSTEM_NATIVE_ONLY, DOMAIN_ONLY, ATTACH_ONLY, DOMAIN_THREAD_AUDIT, PINNED_MONO, MONO_ONLY, ABORT_ONLY ou FULL_DIAGNOSTICS");
    static std::string lockedMode;
    if (!lockedMode.empty() && lockedMode != selectedMode)
        return JS_ThrowInternalError(ctx,
            "Modo MonoVM ja selecionado nesta sessao: %s; reinicie o processo do jogo",
            lockedMode.c_str());
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

    if (lockedMode.empty()) {
        lockedMode = selectedMode;
        const auto modeFile = checkpoint.substr(0, slash + 1) + "native-game-probe-mode.txt";
        std::ofstream record(modeFile, std::ios::trunc);
        if (record) record << "mode=" << lockedMode << "\npid="
                           << static_cast<long>(getpid()) << "\n";
    }
    static bool gameAbortTraceInstalled = false;
    static bool gameMutexTraceInstalled = false;
    if (selectedMode != "MONO_ONLY" && selectedMode != "NO_MONO" &&
        selectedMode != "PINNED_MONO" && selectedMode != "LIBRARY_ONLY" &&
        selectedMode != "PATH_ONLY" && selectedMode != "SYSTEM_NATIVE_ONLY" &&
        selectedMode != "DOMAIN_ONLY" && selectedMode != "ATTACH_ONLY" &&
        selectedMode != "DOMAIN_THREAD_AUDIT" && !gameAbortTraceInstalled)
        gameAbortTraceInstalled = bl::csharp::installNativeAbortProbe(
            checkpoint.substr(0, slash), "native-game-abort-trace.txt");
    bl::csharp::ManagedRuntimeStatus result;
    if (selectedMode == "NO_MONO") {
        result = bl::csharp::managedRuntimeStatus();
        result.phase = "no-mono-control";
    } else if (selectedMode == "PINNED_MONO" ||
               selectedMode == "LIBRARY_ONLY" ||
               selectedMode == "PATH_ONLY" ||
               selectedMode == "SYSTEM_NATIVE_ONLY" ||
               selectedMode == "DOMAIN_ONLY" ||
               selectedMode == "ATTACH_ONLY" ||
               selectedMode == "DOMAIN_THREAD_AUDIT") {
        result = bl::csharp::probeManagedRuntimePinnedThread(assembly, selectedMode);
    } else {
        result = bl::csharp::probeManagedRuntime(assembly);
    }
    if (gameAbortTraceInstalled) {
        bl::csharp::rearmNativeAbortProbe("after-game-mono-probe");
        if (result.methodExecuted && selectedMode == "FULL_DIAGNOSTICS" &&
            !gameMutexTraceInstalled)
            gameMutexTraceInstalled = bl::csharp::installDestroyedMutexTrace();
    }
    const bool moduleMapsSaved = bl::csharp::persistGameNativeModuleMaps(
        checkpoint.substr(0, slash));
    JSValue out = makeManagedStatus(ctx, result);
    JS_SetPropertyStr(ctx, out, "gameProbeMode", JS_NewString(ctx, lockedMode.c_str()));
    JS_SetPropertyStr(ctx, out, "gameAbortTraceInstalled",
                      JS_NewBool(ctx, gameAbortTraceInstalled));
    JS_SetPropertyStr(ctx, out, "gameMutexTraceInstalled",
                      JS_NewBool(ctx, gameMutexTraceInstalled));
    JS_SetPropertyStr(ctx, out, "gameModuleMapsSaved",
                      JS_NewBool(ctx, moduleMapsSaved));
    JS_SetPropertyStr(ctx, out, "pinnedOwnerTid",
                      JS_NewInt64(ctx, bl::csharp::managedRuntimePinnedThreadTid()));
    return out;
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

JSValue makeLiveSourceStatus(JSContext* ctx, const bl::csharp::LiveSourceStatus& s) {
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "modId", JS_NewString(ctx, s.modId.c_str()));
    JS_SetPropertyStr(ctx, out, "loaded", JS_NewBool(ctx, s.loaded));
    JS_SetPropertyStr(ctx, out, "ready", JS_NewBool(ctx, s.ready));
    JS_SetPropertyStr(ctx, out, "callbacks", JS_NewInt32(ctx, s.callbacks));
    JS_SetPropertyStr(ctx, out, "failures", JS_NewInt32(ctx, s.failures));
    JS_SetPropertyStr(ctx, out, "npcThreadAttaches", JS_NewInt32(ctx, s.npcThreadAttaches));
    JS_SetPropertyStr(ctx, out, "npcThreadDetaches", JS_NewInt32(ctx, s.npcThreadDetaches));
    JS_SetPropertyStr(ctx, out, "npcThreadAlreadyAttached",
                      JS_NewInt32(ctx, s.npcThreadAlreadyAttached));
    JS_SetPropertyStr(ctx, out, "lastTick", JS_NewInt32(ctx, s.lastTick));
    JS_SetPropertyStr(ctx, out, "lastState", JS_NewInt32(ctx, s.lastState));
    JS_SetPropertyStr(ctx, out, "lastResult", JS_NewInt32(ctx, s.lastResult));
    JS_SetPropertyStr(ctx, out, "phase", JS_NewString(ctx, s.phase.c_str()));
    JS_SetPropertyStr(ctx, out, "error", JS_NewString(ctx, s.error.c_str()));
    return out;
}

JSValue makeManagedHitContextStatus(
    JSContext* ctx, const bl::csharp::ManagedHitContextStats& stats) {
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "v2Calls", JS_NewInt32(ctx, stats.v2Calls));
    JS_SetPropertyStr(ctx, out, "v3Calls", JS_NewInt32(ctx, stats.v3Calls));
    JS_SetPropertyStr(ctx, out, "v3Successes", JS_NewInt32(ctx, stats.v3Successes));
    JS_SetPropertyStr(ctx, out, "v3Failures", JS_NewInt32(ctx, stats.v3Failures));
    JS_SetPropertyStr(ctx, out, "v4Calls", JS_NewInt32(ctx, stats.v4Calls));
    JS_SetPropertyStr(ctx, out, "v4Successes", JS_NewInt32(ctx, stats.v4Successes));
    JS_SetPropertyStr(ctx, out, "v4Failures", JS_NewInt32(ctx, stats.v4Failures));
    JS_SetPropertyStr(ctx, out, "lastPlayerIndex", JS_NewInt32(ctx, stats.lastPlayerIndex));
    JS_SetPropertyStr(ctx, out, "lastPlayerLife", JS_NewInt32(ctx, stats.lastPlayerLife));
    JS_SetPropertyStr(ctx, out, "lastPlayerLifeMax", JS_NewInt32(ctx, stats.lastPlayerLifeMax));
    JS_SetPropertyStr(ctx, out, "lastPlayerX", JS_NewFloat64(ctx, stats.lastPlayerX));
    JS_SetPropertyStr(ctx, out, "lastPlayerY", JS_NewFloat64(ctx, stats.lastPlayerY));
    JS_SetPropertyStr(ctx, out, "lastNpcType", JS_NewInt32(ctx, stats.lastNpcType));
    JS_SetPropertyStr(ctx, out, "lastNpcLife", JS_NewInt32(ctx, stats.lastNpcLife));
    JS_SetPropertyStr(ctx, out, "lastNpcX", JS_NewFloat64(ctx, stats.lastNpcX));
    JS_SetPropertyStr(ctx, out, "lastNpcY", JS_NewFloat64(ctx, stats.lastNpcY));
    JS_SetPropertyStr(ctx, out, "v2Successes", JS_NewInt32(ctx, stats.v2Successes));
    JS_SetPropertyStr(ctx, out, "v2Failures", JS_NewInt32(ctx, stats.v2Failures));
    JS_SetPropertyStr(ctx, out, "legacyCalls", JS_NewInt32(ctx, stats.legacyCalls));
    JS_SetPropertyStr(ctx, out, "legacyFailures", JS_NewInt32(ctx, stats.legacyFailures));
    JS_SetPropertyStr(ctx, out, "lastNpcIndex", JS_NewInt32(ctx, stats.lastNpcIndex));
    JS_SetPropertyStr(ctx, out, "minNpcIndex", JS_NewInt32(ctx, stats.minNpcIndex));
    JS_SetPropertyStr(ctx, out, "maxNpcIndex", JS_NewInt32(ctx, stats.maxNpcIndex));
    JS_SetPropertyStr(ctx, out, "uniqueNpcIndices", JS_NewInt32(ctx, stats.uniqueNpcIndices));
    JS_SetPropertyStr(ctx, out, "invalidV2Indices", JS_NewInt32(ctx, stats.invalidV2Indices));
    return out;
}

JSValue makeManagedNpcBuffEffectStatus(
    JSContext* ctx, const bl::csharp::ManagedNpcBuffEffectStatus& status) {
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "gameHookInstalled", JS_NewBool(ctx, status.gameHookInstalled));
    JS_SetPropertyStr(ctx, out, "requested", JS_NewInt32(ctx, status.requested));
    JS_SetPropertyStr(ctx, out, "applied", JS_NewInt32(ctx, status.applied));
    JS_SetPropertyStr(ctx, out, "rejected", JS_NewInt32(ctx, status.rejected));
    JS_SetPropertyStr(ctx, out, "stale", JS_NewInt32(ctx, status.stale));
    JS_SetPropertyStr(ctx, out, "queueDrops", JS_NewInt32(ctx, status.queueDrops));
    JS_SetPropertyStr(ctx, out, "queueDepth", JS_NewInt32(ctx, status.queueDepth));
    JS_SetPropertyStr(ctx, out, "lastBuffId", JS_NewInt32(ctx, status.lastBuffId));
    JS_SetPropertyStr(ctx, out, "lastDurationTicks", JS_NewInt32(ctx, status.lastDurationTicks));
    JS_SetPropertyStr(ctx, out, "lastNpcIndex", JS_NewInt32(ctx, status.lastNpcIndex));
    JS_SetPropertyStr(ctx, out, "impulsesApplied", JS_NewInt32(ctx, status.impulsesApplied));
    JS_SetPropertyStr(ctx, out, "lastImpulseDirection", JS_NewInt32(ctx, status.lastImpulseDirection));
    JS_SetPropertyStr(ctx, out, "lastImpulseTenths", JS_NewInt32(ctx, status.lastImpulseTenths));
    return out;
}

JSValue makeManagedLiveHitStatus(JSContext* ctx, const bl::csharp::ManagedLiveHitStatus& status) {
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "enabled", JS_NewBool(ctx, status.enabled));
    JS_SetPropertyStr(ctx, out, "automatic", JS_NewBool(ctx, status.automatic));
    JS_SetPropertyStr(ctx, out, "projectileType", JS_NewInt32(ctx, status.projectileType));
    JS_SetPropertyStr(ctx, out, "queued", JS_NewInt32(ctx, status.queued));
    JS_SetPropertyStr(ctx, out, "processed", JS_NewInt32(ctx, status.processed));
    JS_SetPropertyStr(ctx, out, "hitProcessed", JS_NewInt32(ctx, status.hitProcessed));
    JS_SetPropertyStr(ctx, out, "hitContext",
                      makeManagedHitContextStatus(ctx, status.hitContext));
    JS_SetPropertyStr(ctx, out, "effects",
                      makeManagedNpcBuffEffectStatus(ctx, status.effects));
    JS_SetPropertyStr(ctx, out, "killProcessed", JS_NewInt32(ctx, status.killProcessed));
    JS_SetPropertyStr(ctx, out, "dropped", JS_NewInt32(ctx, status.dropped));
    JS_SetPropertyStr(ctx, out, "lockDrops", JS_NewInt32(ctx, status.lockDrops));
    JS_SetPropertyStr(ctx, out, "queueDepth", JS_NewInt32(ctx, status.queueDepth));
    JS_SetPropertyStr(ctx, out, "maxQueueDepth", JS_NewInt32(ctx, status.maxQueueDepth));
    JS_SetPropertyStr(ctx, out, "queueCapacity", JS_NewInt32(ctx, status.queueCapacity));
    JS_SetPropertyStr(ctx, out, "eventLimit", JS_NewInt32(ctx, status.eventLimit));
    JS_SetPropertyStr(ctx, out, "primaryEnabled", JS_NewBool(ctx, status.primaryEnabled));
    JS_SetPropertyStr(ctx, out, "bindingCapacity", JS_NewInt32(ctx, status.bindingCapacity));
    JS_SetPropertyStr(ctx, out, "perBindingQueueCapacity", JS_NewInt32(ctx, status.perBindingQueueCapacity));
    JS_SetPropertyStr(ctx, out, "primaryQueueDrops", JS_NewInt32(ctx, status.primaryQueueDrops));
    const auto scheduler = bl::csharp::managedHurtSchedulerStatus();
    JSValue hurt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, hurt, "submitted", JS_NewInt64(ctx, scheduler.submitted));
    JS_SetPropertyStr(ctx, hurt, "completed", JS_NewInt64(ctx, scheduler.completed));
    JS_SetPropertyStr(ctx, hurt, "timedOut", JS_NewInt64(ctx, scheduler.timedOut));
    JS_SetPropertyStr(ctx, hurt, "cancelledBeforeRun", JS_NewInt64(ctx, scheduler.cancelledBeforeRun));
    JS_SetPropertyStr(ctx, hurt, "lateResults", JS_NewInt64(ctx, scheduler.lateResults));
    JS_SetPropertyStr(ctx, hurt, "waitTotalUs", JS_NewInt64(ctx, scheduler.waitTotalUs));
    JS_SetPropertyStr(ctx, hurt, "waitMaxUs", JS_NewInt64(ctx, scheduler.waitMaxUs));
    JS_SetPropertyStr(ctx, hurt, "pending", JS_NewInt32(ctx, scheduler.pending));
    JS_SetPropertyStr(ctx, hurt, "timeoutMs", JS_NewInt32(ctx, scheduler.timeoutMs));
    JS_SetPropertyStr(ctx, out, "hurtScheduler", hurt);
    JS_SetPropertyStr(ctx, out, "inFlight", JS_NewBool(ctx, status.inFlight));
    JS_SetPropertyStr(ctx, out, "failures", JS_NewInt32(ctx, status.failures));
    JS_SetPropertyStr(ctx, out, "lastDamage", JS_NewInt32(ctx, status.lastDamage));
    JS_SetPropertyStr(ctx, out, "lastResult", JS_NewInt32(ctx, status.lastResult));
    JS_SetPropertyStr(ctx, out, "lastEvent", JS_NewString(ctx, status.lastEvent.c_str()));
    JS_SetPropertyStr(ctx, out, "modId", JS_NewString(ctx, status.modId.c_str()));
    JS_SetPropertyStr(ctx, out, "phase", JS_NewString(ctx, status.phase.c_str()));
    JS_SetPropertyStr(ctx, out, "error", JS_NewString(ctx, status.error.c_str()));
    return out;
}

JSValue makeManagedBenchmarkStatus(JSContext* ctx,
                                   const bl::csharp::ManagedLiveBenchmarkStatus& value) {
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "started", JS_NewBool(ctx, value.started));
    JS_SetPropertyStr(ctx, out, "running", JS_NewBool(ctx, value.running));
    JS_SetPropertyStr(ctx, out, "finished", JS_NewBool(ctx, value.finished));
    JS_SetPropertyStr(ctx, out, "passed", JS_NewBool(ctx, value.passed));
    JS_SetPropertyStr(ctx, out, "target", JS_NewInt32(ctx, value.target));
    JS_SetPropertyStr(ctx, out, "accepted", JS_NewInt32(ctx, value.accepted));
    JS_SetPropertyStr(ctx, out, "processed", JS_NewInt32(ctx, value.processed));
    JS_SetPropertyStr(ctx, out, "hits", JS_NewInt32(ctx, value.hits));
    JS_SetPropertyStr(ctx, out, "kills", JS_NewInt32(ctx, value.kills));
    JS_SetPropertyStr(ctx, out, "dropped", JS_NewInt32(ctx, value.dropped));
    JS_SetPropertyStr(ctx, out, "maxQueueDepth", JS_NewInt32(ctx, value.maxQueueDepth));
    JS_SetPropertyStr(ctx, out, "elapsedMs", JS_NewInt32(ctx, value.elapsedMs));
    JS_SetPropertyStr(ctx, out, "phase", JS_NewString(ctx, value.phase.c_str()));
    JS_SetPropertyStr(ctx, out, "error", JS_NewString(ctx, value.error.c_str()));
    return out;
}

JSValue jsSourceLiveBenchmarkStatus(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourceLiveBenchmarkStatus()");
    return makeManagedBenchmarkStatus(ctx, bl::csharp::managedLiveBenchmarkStatus());
}

JSValue jsSourceLiveBenchmarkStart(JSContext* ctx, JSValueConst,
                                   int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx,
            "sourceLiveBenchmarkStart('CONFIRM_25000_CSHARP_CALLBACKS')");
    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw) return JS_EXCEPTION;
    const bool valid = std::string_view(raw) == "CONFIRM_25000_CSHARP_CALLBACKS";
    JS_FreeCString(ctx, raw);
    if (!valid)
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_25000_CSHARP_CALLBACKS");

    bool worldReady = false;
    for (const auto& mod : bl::csharp::autoSourceStatuses()) {
        if (mod.modId == "68c3115f-62f4-49f2-a56b-388256781d5f" &&
            mod.applied && mod.worldReady && mod.netMode == 0) {
            worldReady = true;
            break;
        }
    }
    if (!worldReady)
        return JS_ThrowInternalError(ctx,
            "ExampleModCSharp precisa estar carregado em um mundo singleplayer");
    std::string error;
    if (!bl::csharp::startManagedLiveBenchmark(&error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    return makeManagedBenchmarkStatus(ctx, bl::csharp::managedLiveBenchmarkStatus());
}

JSValue jsSourceLiveBenchmarkStop(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourceLiveBenchmarkStop()");
    return makeManagedBenchmarkStatus(ctx, bl::csharp::stopManagedLiveBenchmark());
}

JSValue makeManagedLivePerfStatus(JSContext* ctx,
    const bl::csharp::ManagedLivePerfStatus& status) {
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "started", JS_NewBool(ctx, status.started));
    JS_SetPropertyStr(ctx, out, "recording", JS_NewBool(ctx, status.recording));
    JS_SetPropertyStr(ctx, out, "phase", JS_NewString(ctx, status.phase.c_str()));
    JS_SetPropertyStr(ctx, out, "targetMs", JS_NewInt32(ctx, status.targetMs));
    JS_SetPropertyStr(ctx, out, "elapsedMs", JS_NewInt32(ctx, status.elapsedMs));
    JS_SetPropertyStr(ctx, out, "updateSamples", JS_NewInt32(ctx, status.updateSamples));
    JS_SetPropertyStr(ctx, out, "updateHz", JS_NewFloat64(ctx, status.updateHz));
    JS_SetPropertyStr(ctx, out, "updateP90Ms", JS_NewFloat64(ctx, status.updateP90Ms));
    JS_SetPropertyStr(ctx, out, "updateP99Ms", JS_NewFloat64(ctx, status.updateP99Ms));
    JS_SetPropertyStr(ctx, out, "updatesOver25Ms", JS_NewInt32(ctx, status.updatesOver25Ms));
    JS_SetPropertyStr(ctx, out, "updatesOver33Ms", JS_NewInt32(ctx, status.updatesOver33Ms));
    JS_SetPropertyStr(ctx, out, "callbacksSampled", JS_NewInt32(ctx, status.callbacksSampled));
    JS_SetPropertyStr(ctx, out, "queueWaitAvgUs", JS_NewFloat64(ctx, status.queueWaitAvgUs));
    JS_SetPropertyStr(ctx, out, "queueWaitMaxUs", JS_NewInt32(ctx, status.queueWaitMaxUs));
    JS_SetPropertyStr(ctx, out, "queueWaitSamples", JS_NewInt32(ctx, status.queueWaitSamples));
    JS_SetPropertyStr(ctx, out, "queueWaitP90Us", JS_NewInt32(ctx, status.queueWaitP90Us));
    JS_SetPropertyStr(ctx, out, "queueWaitP99Us", JS_NewInt32(ctx, status.queueWaitP99Us));
    JS_SetPropertyStr(ctx, out, "callbackAvgUs", JS_NewFloat64(ctx, status.callbackAvgUs));
    JS_SetPropertyStr(ctx, out, "callbackMaxUs", JS_NewInt32(ctx, status.callbackMaxUs));
    JS_SetPropertyStr(ctx, out, "callbackP90Us", JS_NewInt32(ctx, status.callbackP90Us));
    JS_SetPropertyStr(ctx, out, "callbackP99Us", JS_NewInt32(ctx, status.callbackP99Us));
    JS_SetPropertyStr(ctx, out, "callbackCpuAvgUs", JS_NewFloat64(ctx, status.callbackCpuAvgUs));
    JS_SetPropertyStr(ctx, out, "callbackCpuMaxUs", JS_NewInt32(ctx, status.callbackCpuMaxUs));
    JS_SetPropertyStr(ctx, out, "callbackCpuP90Us", JS_NewInt32(ctx, status.callbackCpuP90Us));
    JS_SetPropertyStr(ctx, out, "eventsAccepted", JS_NewInt32(ctx, status.eventsAccepted));
    JS_SetPropertyStr(ctx, out, "eventsProcessed", JS_NewInt32(ctx, status.eventsProcessed));
    JS_SetPropertyStr(ctx, out, "eventsFailed", JS_NewInt32(ctx, status.eventsFailed));
    JS_SetPropertyStr(ctx, out, "eventsQueueDropped", JS_NewInt32(ctx, status.eventsQueueDropped));
    JS_SetPropertyStr(ctx, out, "maxQueueDepth", JS_NewInt32(ctx, status.maxQueueDepth));
    JS_SetPropertyStr(ctx, out, "projectileCurveUpdates", JS_NewInt32(ctx, status.projectileCurveUpdates));
    JS_SetPropertyStr(ctx, out, "projectileVelocityWrites", JS_NewInt32(ctx, status.projectileVelocityWrites));
    JS_SetPropertyStr(ctx, out, "lockDrops", JS_NewInt32(ctx, status.lockDrops));
    JSValue projectiles = JS_NewArray(ctx);
    uint32_t index = 0;
    for (const auto& p : status.projectiles) {
        JSValue item = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, item, "modId", JS_NewString(ctx, p.modId.c_str()));
        JS_SetPropertyStr(ctx, item, "callbackClass",
                          JS_NewString(ctx, p.callbackClass.c_str()));
        JS_SetPropertyStr(ctx, item, "projectileType", JS_NewInt32(ctx, p.projectileType));
        JS_SetPropertyStr(ctx, item, "enabled", JS_NewBool(ctx, p.enabled));
        JS_SetPropertyStr(ctx, item, "callbacksSampled", JS_NewInt32(ctx, p.callbacksSampled));
        JS_SetPropertyStr(ctx, item, "hitSamples", JS_NewInt32(ctx, p.hitSamples));
        JS_SetPropertyStr(ctx, item, "killSamples", JS_NewInt32(ctx, p.killSamples));
        JS_SetPropertyStr(ctx, item, "failures", JS_NewInt32(ctx, p.failures));
        JS_SetPropertyStr(ctx, item, "queueWaitSamples", JS_NewInt32(ctx, p.queueWaitSamples));
        JS_SetPropertyStr(ctx, item, "queueWaitAvgUs", JS_NewFloat64(ctx, p.queueWaitAvgUs));
        JS_SetPropertyStr(ctx, item, "queueWaitMaxUs", JS_NewInt32(ctx, p.queueWaitMaxUs));
        JS_SetPropertyStr(ctx, item, "callbackAvgUs", JS_NewFloat64(ctx, p.callbackAvgUs));
        JS_SetPropertyStr(ctx, item, "callbackMaxUs", JS_NewInt32(ctx, p.callbackMaxUs));
        JS_SetPropertyStr(ctx, item, "callbackP90Us", JS_NewInt32(ctx, p.callbackP90Us));
        JS_SetPropertyStr(ctx, item, "callbackCpuAvgUs",
                          JS_NewFloat64(ctx, p.callbackCpuAvgUs));
        JS_SetPropertyStr(ctx, item, "callbackCpuMaxUs",
                          JS_NewInt32(ctx, p.callbackCpuMaxUs));
        JS_SetPropertyUint32(ctx, projectiles, index++, item);
    }
    JS_SetPropertyStr(ctx, out, "projectiles", projectiles);
    return out;
}

int64_t vmCompareJsThreadCpuNs() {
    timespec time{};
    return clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) == 0
        ? static_cast<int64_t>(time.tv_sec) * 1000000000LL + time.tv_nsec : 0;
}

int vmCompareProcessRssKb() {
    FILE* file = std::fopen("/proc/self/status", "rb");
    if (!file) return 0;
    char line[128]{};
    int value = 0;
    while (std::fgets(line, sizeof(line), file)) {
        if (std::sscanf(line, "VmRSS: %d", &value) == 1) break;
    }
    std::fclose(file);
    return value;
}

JSValue vmCompareStatusToJs(JSContext* ctx, const bl::csharp::VmCompareStatus& status) {
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "started", JS_NewBool(ctx, status.started));
    JS_SetPropertyStr(ctx, result, "running", JS_NewBool(ctx, status.running));
    JS_SetPropertyStr(ctx, result, "finished", JS_NewBool(ctx, status.finished));
    JS_SetPropertyStr(ctx, result, "passed", JS_NewBool(ctx, status.passed));
    JS_SetPropertyStr(ctx, result, "iterations", JS_NewInt32(ctx, status.iterations));
    JS_SetPropertyStr(ctx, result, "kind", JS_NewString(ctx,
        status.kind == 1 ? "projectile" : status.kind == 2 ? "npc" : "math"));
    JS_SetPropertyStr(ctx, result, "entities", JS_NewInt32(ctx, status.kind == 0 ? 0 : 64));
    JS_SetPropertyStr(ctx, result, "updates", JS_NewInt32(ctx,
        status.kind == 0 ? 0 : status.iterations * 64));
    JS_SetPropertyStr(ctx, result, "jsResult", JS_NewInt32(ctx, status.jsResult));
    JS_SetPropertyStr(ctx, result, "monoResult", JS_NewInt32(ctx, status.monoResult));
    JS_SetPropertyStr(ctx, result, "jsWallUs", JS_NewInt64(ctx, status.jsWallUs));
    JS_SetPropertyStr(ctx, result, "jsThreadCpuUs", JS_NewInt64(ctx, status.jsThreadCpuUs));
    JS_SetPropertyStr(ctx, result, "monoWallUs", JS_NewInt64(ctx, status.monoWallUs));
    JS_SetPropertyStr(ctx, result, "monoThreadCpuUs", JS_NewInt64(ctx, status.monoThreadCpuUs));
    JS_SetPropertyStr(ctx, result, "rssBeforeJsKb", JS_NewInt32(ctx, status.rssBeforeJsKb));
    JS_SetPropertyStr(ctx, result, "rssAfterJsKb", JS_NewInt32(ctx, status.rssAfterJsKb));
    JS_SetPropertyStr(ctx, result, "rssBeforeMonoKb", JS_NewInt32(ctx, status.rssBeforeMonoKb));
    JS_SetPropertyStr(ctx, result, "rssAfterMonoKb", JS_NewInt32(ctx, status.rssAfterMonoKb));
    JS_SetPropertyStr(ctx, result, "phase", JS_NewString(ctx, status.phase.c_str()));
    JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, status.error.c_str()));
    const double wallRatio = status.monoWallUs > 0
        ? static_cast<double>(status.jsWallUs) / status.monoWallUs : 0.0;
    const double cpuRatio = status.monoThreadCpuUs > 0
        ? static_cast<double>(status.jsThreadCpuUs) / status.monoThreadCpuUs : 0.0;
    JS_SetPropertyStr(ctx, result, "jsOverMonoWallRatio",
        JS_NewFloat64(ctx, wallRatio));
    JS_SetPropertyStr(ctx, result, "jsOverMonoCpuRatio",
        JS_NewFloat64(ctx, cpuRatio));
    return result;
}

constexpr char kVmCompareMathJs[] = R"JS(
(function(iterations, seed) {
    let x = (seed ^ 0xA5A5A5A5) >>> 0;
    for (let i = 0; i < iterations; ++i) {
        x = (x + Math.imul(i, 0x9E3779B9)) >>> 0;
        x = ((x << 7) | (x >>> 25)) >>> 0;
        x = (x ^ (x >>> 13)) >>> 0;
        x = Math.imul(x, 0x85EBCA6B) >>> 0;
        x = (x ^ (x << 5)) >>> 0;
    }
    return x | 0;
})
)JS";

constexpr char kVmProjectileJs[] = R"JS(
(function(ticks, seed) {
    const count = 64;
    const x = new Int32Array(count);
    const y = new Int32Array(count);
    const vx = new Int32Array(count);
    const vy = new Int32Array(count);
    for (let i = 0; i < count; i++) {
        x[i] = i * 13 - 320;
        y[i] = i * 7 - 180;
        vx[i] = (i % 11 - 5) * 3;
        vy[i] = (i % 9 - 4) * 2;
    }
    let hash = seed | 0;
    for (let t = 0; t < ticks; t++) {
        for (let i = 0; i < count; i++) {
            const tx = ((t * 7 + i * 31) & 1023) - 512;
            const ty = ((t * 5 + i * 19) & 511) - 256;
            vx[i] += tx > x[i] ? 2 : -2;
            vy[i] += ty > y[i] ? 1 : -1;
            vx[i] = (vx[i] * 15 / 16) | 0;
            vy[i] = (vy[i] * 15 / 16) | 0;
            x[i] += vx[i];
            y[i] += vy[i];
            if (x[i] < -512 || x[i] > 512) {
                vx[i] = (-vx[i] / 2) | 0;
                x[i] = x[i] < 0 ? -512 : 512;
            }
            if (y[i] < -256 || y[i] > 256) {
                vy[i] = (-vy[i] / 2) | 0;
                y[i] = y[i] < 0 ? -256 : 256;
            }
            hash = Math.imul(hash ^ x[i] ^ (y[i] << 1) ^
                (vx[i] << 2) ^ (vy[i] << 3), 16777619) | 0;
        }
    }
    return hash | 0;
})
)JS";

constexpr char kVmNpcJs[] = R"JS(
(function(ticks, seed) {
    const count = 64;
    const x = new Int32Array(count);
    const y = new Int32Array(count);
    const vx = new Int32Array(count);
    const vy = new Int32Array(count);
    const cooldown = new Int32Array(count);
    for (let i = 0; i < count; i++) {
        x[i] = i * 23 - 700;
        y[i] = i * 11 - 350;
        vx[i] = i % 7 - 3;
        vy[i] = i % 5 - 2;
        cooldown[i] = 10 + i % 17;
    }
    let hash = seed | 0;
    for (let t = 0; t < ticks; t++) {
        for (let i = 0; i < count; i++) {
            const tx = ((t * 3 + i * 47) & 2047) - 1024;
            const ty = ((t * 2 + i * 37) & 1023) - 512;
            const dx = tx - x[i];
            const dy = ty - y[i];
            const attacking = Math.abs(dx) < 90 && Math.abs(dy) < 70;
            const mode = attacking ? 1 : 0;
            const force = attacking ? 2 : 1;
            vx[i] += dx > 0 ? force : -force;
            vy[i] += dy > 0 ? 1 : -1;
            if (vx[i] > 12) vx[i] = 12;
            if (vx[i] < -12) vx[i] = -12;
            if (vy[i] > 10) vy[i] = 10;
            if (vy[i] < -10) vy[i] = -10;
            if ((t & 7) === 0) {
                vx[i] = (vx[i] * 7 / 8) | 0;
                vy[i] = (vy[i] * 7 / 8) | 0;
            }
            x[i] += vx[i];
            y[i] += vy[i];
            if (--cooldown[i] <= 0)
                cooldown[i] = attacking ? 30 + i % 11 : 16 + i % 13;
            hash = Math.imul(hash ^ x[i] ^ (y[i] << 1) ^
                (cooldown[i] << 2) ^ (mode << 6), 16777619) | 0;
        }
    }
    return hash | 0;
})
)JS";

JSValue runVmCompare(JSContext* ctx, int32_t iterations, int kind) {
    if (kind < 0 || kind > 2 ||
        (kind == 0 && (iterations < 1000 || iterations > 5000000)) ||
        (kind != 0 && (iterations < 64 || iterations > 5000)))
        return JS_ThrowRangeError(ctx, "math: 1000..5000000; AI ticks: 64..5000");
    bool ready = false;
    for (const auto& mod : bl::csharp::autoSourceStatuses()) {
        if (mod.modId == "68c3115f-62f4-49f2-a56b-388256781d5f" &&
            mod.applied && mod.worldReady && mod.netMode == 0) {
            ready = true;
            break;
        }
    }
    if (!ready)
        return JS_ThrowInternalError(ctx,
            "Enter a singleplayer world with ExampleModCSharp 0.1.7");
    const auto live = bl::csharp::managedLiveHitStatus();
    if (!live.enabled || !live.automatic || live.inFlight || live.queueDepth)
        return JS_ThrowInternalError(ctx, "Managed callbacks must be idle and ready");

    const char* source = kind == 1 ? kVmProjectileJs :
                         kind == 2 ? kVmNpcJs : kVmCompareMathJs;
    const size_t sourceSize = kind == 1 ? sizeof(kVmProjectileJs) - 1 :
                              kind == 2 ? sizeof(kVmNpcJs) - 1 :
                              sizeof(kVmCompareMathJs) - 1;
    JSValue routine = JS_Eval(ctx, source, sourceSize,
                             kind == 1 ? "vm-projectile-ai.js" :
                             kind == 2 ? "vm-npc-ai.js" : "vm-compare-math.js",
                             JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(routine)) return routine;
    const int32_t seed = 0x12345678;
    JSValue warmArgs[] = {JS_NewInt32(ctx, kind == 0 ? 4096 : 64), JS_NewInt32(ctx, seed)};
    JSValue warm = JS_Call(ctx, routine, JS_UNDEFINED, 2, warmArgs);
    JS_FreeValue(ctx, warmArgs[0]);
    JS_FreeValue(ctx, warmArgs[1]);
    if (JS_IsException(warm)) {
        JS_FreeValue(ctx, routine);
        return warm;
    }
    JS_FreeValue(ctx, warm);

    bl::csharp::VmCompareStatus sample;
    sample.iterations = iterations;
    sample.kind = kind;
    sample.seed = seed;
    sample.rssBeforeJsKb = vmCompareProcessRssKb();
    const int64_t cpuBefore = vmCompareJsThreadCpuNs();
    const auto before = std::chrono::steady_clock::now();
    JSValue args[] = {JS_NewInt32(ctx, iterations), JS_NewInt32(ctx, seed)};
    JSValue value = JS_Call(ctx, routine, JS_UNDEFINED, 2, args);
    const auto after = std::chrono::steady_clock::now();
    const int64_t cpuAfter = vmCompareJsThreadCpuNs();
    sample.rssAfterJsKb = vmCompareProcessRssKb();
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, routine);
    if (JS_IsException(value)) return value;
    if (JS_ToInt32(ctx, &sample.jsResult, value) < 0) {
        JS_FreeValue(ctx, value);
        return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, value);
    sample.jsWallUs = std::chrono::duration_cast<
        std::chrono::microseconds>(after - before).count();
    sample.jsThreadCpuUs = cpuAfter >= cpuBefore
        ? (cpuAfter - cpuBefore) / 1000 : 0;
    std::string error;
    if (!bl::csharp::beginVmCompare(sample, &error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    return vmCompareStatusToJs(ctx, bl::csharp::vmCompareStatus());
}

JSValue jsVmCompareStart(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int32_t iterations = 1000000;
    if (argc > 1 || (argc == 1 && JS_ToInt32(ctx, &iterations, argv[0]) < 0))
        return JS_ThrowTypeError(ctx, "vmCompareStart(iterations)");
    return runVmCompare(ctx, iterations, 0);
}

JSValue jsVmGameplayCompareStart(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "vmGameplayCompareStart('projectile'|'npc', ticks)");
    const char* kindRaw = JS_ToCString(ctx, argv[0]);
    if (!kindRaw) return JS_EXCEPTION;
    const std::string kindName(kindRaw);
    JS_FreeCString(ctx, kindRaw);
    int kind = kindName == "projectile" ? 1 : kindName == "npc" ? 2 : -1;
    if (kind == -1)
        return JS_ThrowTypeError(ctx, "mode: 'projectile' or 'npc'");
    int32_t ticks = 0;
    if (JS_ToInt32(ctx, &ticks, argv[1]) < 0) return JS_EXCEPTION;
    return runVmCompare(ctx, ticks, kind);
}

JSValue jsVmCompareStatus(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "vmCompareStatus()");
    return vmCompareStatusToJs(ctx, bl::csharp::vmCompareStatus());
}

JSValue jsSourcePerfStart(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourcePerfStart()");
    bool worldReady = false;
    for (const auto& mod : bl::csharp::autoSourceStatuses()) {
        if (mod.modId == "68c3115f-62f4-49f2-a56b-388256781d5f" &&
            mod.applied && mod.worldReady && mod.netMode == 0) {
            worldReady = true;
            break;
        }
    }
    if (!worldReady)
        return JS_ThrowInternalError(ctx,
            "Profiler disponivel somente em mundo singleplayer com ExampleModCSharp");
    std::string error;
    if (!bl::csharp::startManagedLivePerf(&error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    return makeManagedLivePerfStatus(ctx, bl::csharp::managedLivePerfStatus());
}

JSValue jsSourcePerfStatus(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourcePerfStatus()");
    return makeManagedLivePerfStatus(ctx, bl::csharp::managedLivePerfStatus());
}

JSValue jsSourcePerfStop(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourcePerfStop()");
    return makeManagedLivePerfStatus(ctx, bl::csharp::stopManagedLivePerf());
}

JSValue makeManagedMemoryStatus(JSContext* ctx,
                                const bl::csharp::ManagedMemoryStatus& mem) {
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "available", JS_NewBool(ctx, mem.available));
    JS_SetPropertyStr(ctx, out, "monoReady", JS_NewBool(ctx, mem.monoReady));
    JS_SetPropertyStr(ctx, out, "rssKb", JS_NewInt64(ctx, mem.rssKb));
    JS_SetPropertyStr(ctx, out, "peakRssKb", JS_NewInt64(ctx, mem.peakRssKb));
    JS_SetPropertyStr(ctx, out, "virtualKb", JS_NewInt64(ctx, mem.virtualKb));
    JS_SetPropertyStr(ctx, out, "dataKb", JS_NewInt64(ctx, mem.dataKb));
    JS_SetPropertyStr(ctx, out, "swapKb", JS_NewInt64(ctx, mem.swapKb));
    JS_SetPropertyStr(ctx, out, "threads", JS_NewInt32(ctx, mem.threads));
    JS_SetPropertyStr(ctx, out, "baselineRssKb", JS_NewInt64(ctx, mem.baselineRssKb));
    JS_SetPropertyStr(ctx, out, "rssDeltaKb", JS_NewInt64(ctx, mem.rssDeltaKb));
    JS_SetPropertyStr(ctx, out, "cachedCallbackEntries",
                      JS_NewInt32(ctx, mem.cachedCallbackEntries));
    JS_SetPropertyStr(ctx, out, "cachedAssemblyPaths",
                      JS_NewInt32(ctx, mem.cachedAssemblyPaths));
    JS_SetPropertyStr(ctx, out, "monoGcStatsAvailable",
                      JS_NewBool(ctx, mem.monoGcStatsAvailable));
    JS_SetPropertyStr(ctx, out, "monoGcHeapBytes",
                      JS_NewInt64(ctx, mem.monoGcHeapBytes));
    JS_SetPropertyStr(ctx, out, "monoGcUsedBytes",
                      JS_NewInt64(ctx, mem.monoGcUsedBytes));
    JS_SetPropertyStr(ctx, out, "liveMethodCacheHits",
                      JS_NewInt64(ctx, static_cast<int64_t>(mem.liveMethodCacheHits)));
    JS_SetPropertyStr(ctx, out, "liveMethodCacheMisses",
                      JS_NewInt64(ctx, static_cast<int64_t>(mem.liveMethodCacheMisses)));
    JS_SetPropertyStr(ctx, out, "liveAssemblyOpenAttempts",
                      JS_NewInt64(ctx, static_cast<int64_t>(mem.liveAssemblyOpenAttempts)));
    JS_SetPropertyStr(ctx, out, "liveAssemblyOpenSuccesses",
                      JS_NewInt64(ctx, static_cast<int64_t>(mem.liveAssemblyOpenSuccesses)));
    JS_SetPropertyStr(ctx, out, "distinctLiveOpenedAssemblyPaths",
                      JS_NewInt32(ctx, mem.distinctLiveOpenedAssemblyPaths));
    JS_SetPropertyStr(ctx, out, "boundProjectileTypes",
                      JS_NewInt32(ctx, mem.boundProjectileTypes));
    JS_SetPropertyStr(ctx, out, "activeProjectileTypes",
                      JS_NewInt32(ctx, mem.activeProjectileTypes));
    JS_SetPropertyStr(ctx, out, "workerTid", JS_NewInt64(ctx, mem.workerTid));
    const auto lifecycle = bl::csharp::autoWorldLifecycleStatus();
    JSValue world = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, world, "epoch",
                      JS_NewInt64(ctx, static_cast<int64_t>(lifecycle.epoch)));
    JS_SetPropertyStr(ctx, world, "worldLoads",
                      JS_NewInt32(ctx, lifecycle.worldLoads));
    JS_SetPropertyStr(ctx, world, "worldExits",
                      JS_NewInt32(ctx, lifecycle.worldExits));
    JS_SetPropertyStr(ctx, world, "rebindSchedules",
                      JS_NewInt32(ctx, lifecycle.rebindSchedules));
    JS_SetPropertyStr(ctx, world, "configuredBindings",
                      JS_NewInt32(ctx, lifecycle.configuredBindings));
    JS_SetPropertyStr(ctx, world, "worldReady",
                      JS_NewBool(ctx, lifecycle.worldReady));
    JS_SetPropertyStr(ctx, out, "world", world);
    return out;
}

JSValue jsSourceMemoryStatus(JSContext* ctx, JSValueConst,
                             int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourceMemoryStatus()");
    return makeManagedMemoryStatus(ctx, bl::csharp::managedMemoryStatus());
}

JSValue jsSourceMemoryBaseline(JSContext* ctx, JSValueConst,
                               int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourceMemoryBaseline()");
    return makeManagedMemoryStatus(ctx, bl::csharp::managedMemoryStatus(true));
}

JSValue jsSourceLiveHitStatus(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourceLiveHitStatus()");
    return makeManagedLiveHitStatus(ctx, bl::csharp::managedLiveHitStatus());
}

JSValue jsSourceLiveProjectileStatus(JSContext* ctx, JSValueConst,
                                     int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourceLiveProjectileStatus()");
    JSValue result = JS_NewArray(ctx);
    const auto bindings = bl::csharp::managedLiveProjectileStatuses();
    for (uint32_t i = 0; i < bindings.size(); ++i) {
        const auto& value = bindings[i];
        JSValue row = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, row, "modId", JS_NewString(ctx, value.modId.c_str()));
        JS_SetPropertyStr(ctx, row, "projectile", JS_NewString(ctx, value.projectile.c_str()));
        JS_SetPropertyStr(ctx, row, "callbackClass", JS_NewString(ctx, value.callbackClass.c_str()));
        JS_SetPropertyStr(ctx, row, "projectileType", JS_NewInt32(ctx, value.projectileType));
        JS_SetPropertyStr(ctx, row, "enabled", JS_NewBool(ctx, value.enabled));
        JS_SetPropertyStr(ctx, row, "queued", JS_NewInt32(ctx, value.queued));
        JS_SetPropertyStr(ctx, row, "processed", JS_NewInt32(ctx, value.processed));
        JS_SetPropertyStr(ctx, row, "hits", JS_NewInt32(ctx, value.hits));
        JS_SetPropertyStr(ctx, row, "hitContext",
                          makeManagedHitContextStatus(ctx, value.hitContext));
        JS_SetPropertyStr(ctx, row, "kills", JS_NewInt32(ctx, value.kills));
        JS_SetPropertyStr(ctx, row, "failures", JS_NewInt32(ctx, value.failures));
        JS_SetPropertyStr(ctx, row, "dropped", JS_NewInt32(ctx, value.dropped));
        JS_SetPropertyStr(ctx, row, "lastResult", JS_NewInt32(ctx, value.lastResult));
        JS_SetPropertyStr(ctx, row, "error", JS_NewString(ctx, value.error.c_str()));
        JS_SetPropertyUint32(ctx, result, i, row);
    }
    return result;
}

JSValue jsSourceLiveModPause(JSContext* ctx, JSValueConst,
                              int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1]))
        return JS_ThrowTypeError(ctx,
            "sourceLiveModPause(modId, 'CONFIRM_PAUSE_MOD')");
    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw) return JS_EXCEPTION;
    const std::string modId(raw);
    JS_FreeCString(ctx, raw);
    const char* token = JS_ToCString(ctx, argv[1]);
    if (!token) return JS_EXCEPTION;
    const bool confirmed = std::string_view(token) == "CONFIRM_PAUSE_MOD";
    JS_FreeCString(ctx, token);
    if (!confirmed) return JS_ThrowTypeError(ctx, "CONFIRM_PAUSE_MOD required");
    std::string error;
    const bool paused = bl::csharp::pauseManagedLiveMod(modId, &error);
    JSValue status = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, status, "paused", JS_NewBool(ctx, paused));
    JS_SetPropertyStr(ctx, status, "error", JS_NewString(ctx, error.c_str()));
    return status;
}

JSValue jsSourceLiveModResume(JSContext* ctx, JSValueConst,
                               int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1]))
        return JS_ThrowTypeError(ctx,
            "sourceLiveModResume(modId, 'CONFIRM_RESUME_MOD')");
    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw) return JS_EXCEPTION;
    const std::string modId(raw);
    JS_FreeCString(ctx, raw);
    const char* token = JS_ToCString(ctx, argv[1]);
    if (!token) return JS_EXCEPTION;
    const bool confirmed = std::string_view(token) == "CONFIRM_RESUME_MOD";
    JS_FreeCString(ctx, token);
    if (!confirmed)
        return JS_ThrowTypeError(ctx, "CONFIRM_RESUME_MOD required");
    std::string error;
    const bool resumed = bl::csharp::resumeManagedLiveMod(modId, &error);
    JSValue status = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, status, "resumed", JS_NewBool(ctx, resumed));
    JS_SetPropertyStr(ctx, status, "error", JS_NewString(ctx, error.c_str()));
    return status;
}

JSValue jsSourceLiveHitPause(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourceLiveHitPause()");
    return makeManagedLiveHitStatus(ctx, bl::csharp::pauseManagedLiveHit());
}

JSValue jsSourceLiveHitEnable(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1]))
        return JS_ThrowTypeError(ctx,
            "sourceLiveHitEnable(modId, 'CONFIRM_LIVE_ONHIT')");
    const char* rawId = JS_ToCString(ctx, argv[0]);
    if (!rawId) return JS_EXCEPTION;
    const std::string modId(rawId);
    JS_FreeCString(ctx, rawId);
    const char* rawConfirm = JS_ToCString(ctx, argv[1]);
    if (!rawConfirm) return JS_EXCEPTION;
    const bool confirmed = std::string_view(rawConfirm) == "CONFIRM_LIVE_ONHIT";
    JS_FreeCString(ctx, rawConfirm);
    if (!confirmed)
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_LIVE_ONHIT");
    if (modId != "68c3115f-62f4-49f2-a56b-388256781d5f")
        return JS_ThrowTypeError(ctx, "Teste experimental restrito ao ExampleMod CSharp");
    bool loaded = false;
    for (const auto& mod : bl::csharp::sourceModStatuses()) {
        if (mod.id == modId && mod.loaded && mod.runtimeApplied &&
            mod.registeredTypes.contains("ExampleBolt")) {
            loaded = true;
            break;
        }
    }
    const auto hit = bl::csharp::autoProjectileHitStatus(modId, "ExampleBolt");
    if (!loaded || !hit.registered || !hit.installed || !hit.enabled || hit.type < 0)
        return JS_ThrowInternalError(ctx,
            "ExampleBolt OnHitNPC nao esta registrado; aguarde o auto loader em um mundo solo");
    const std::string& checkpoint = bl::config().managedCheckpointPath;
    const size_t slash = checkpoint.find_last_of('/');
    if (slash == std::string::npos)
        return JS_ThrowInternalError(ctx, "Diretorio do MonoVM nao configurado");
    const std::string dll = checkpoint.substr(0, slash + 1) +
        "compiled/" + modId + ".dll";
    std::string error;
    if (!bl::csharp::enableManagedLiveHit(modId, dll, hit.type, &error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    return makeManagedLiveHitStatus(ctx, bl::csharp::managedLiveHitStatus());
}

JSValue jsSourceLiveStatus(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "sourceLiveStatus(modId)");
    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    const std::string id(name);
    JS_FreeCString(ctx, name);
    return makeLiveSourceStatus(ctx, bl::csharp::liveSourceStatus(id));
}

JSValue jsSourceLiveTick(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 3 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "sourceLiveTick(modId, tick, state)");
    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_EXCEPTION;
    const std::string id(name);
    JS_FreeCString(ctx, name);
    int32_t tick = 0, state = 0;
    if (JS_ToInt32(ctx, &tick, argv[1]) < 0 ||
        JS_ToInt32(ctx, &state, argv[2]) < 0) return JS_EXCEPTION;
    if (id.empty() || id.size() > 128 ||
        id.find_first_not_of("0123456789abcdefABCDEF-") != std::string::npos)
        return JS_ThrowTypeError(ctx, "Invalid C# mod ID");
    bool loaded = false;
    for (const auto& mod : bl::csharp::sourceModStatuses()) {
        if (mod.id == id && mod.loaded && mod.runtimeApplied) {
            loaded = true;
            break;
        }
    }
    if (!loaded)
        return JS_ThrowInternalError(ctx, "C# source mod is not loaded or applied");
    const auto& checkpoint = bl::config().managedCheckpointPath;
    const size_t slash = checkpoint.find_last_of('/');
    if (slash == std::string::npos)
        return JS_ThrowInternalError(ctx, "MonoVM data path missing");
    const std::string path = checkpoint.substr(0, slash + 1) +
        "compiled/" + id + ".dll";
    return makeLiveSourceStatus(ctx,
        bl::csharp::invokeLiveSourceTick(id, path, tick, state));
}

JSValue jsSourceLiveNpcStatus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    const auto status = bl::csharp::liveNpcAiStatus();
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "installed", JS_NewBool(ctx, status.installed));
    JS_SetPropertyStr(ctx, out, "enabled", JS_NewBool(ctx, status.enabled));
    JS_SetPropertyStr(ctx, out, "limitReached", JS_NewBool(ctx, status.limitReached));
    JS_SetPropertyStr(ctx, out, "npcType", JS_NewInt32(ctx, status.npcType));
    JS_SetPropertyStr(ctx, out, "events", JS_NewInt32(ctx, status.events));
    JS_SetPropertyStr(ctx, out, "writes", JS_NewInt32(ctx, status.writes));
    JS_SetPropertyStr(ctx, out, "errors", JS_NewInt32(ctx, status.errors));
    JS_SetPropertyStr(ctx, out, "queued", JS_NewInt32(ctx, status.queued));
    JS_SetPropertyStr(ctx, out, "discarded", JS_NewInt32(ctx, status.discarded));
    JS_SetPropertyStr(ctx, out, "workerStarted", JS_NewBool(ctx, status.workerStarted));
    JS_SetPropertyStr(ctx, out, "workerPending", JS_NewBool(ctx, status.workerPending));
    JS_SetPropertyStr(ctx, out, "lastSlot", JS_NewInt32(ctx, status.lastSlot));
    JS_SetPropertyStr(ctx, out, "lastDeltaX", JS_NewInt32(ctx, status.lastDeltaX));
    JS_SetPropertyStr(ctx, out, "lastVelocityTenths",
                      JS_NewInt32(ctx, status.lastVelocityTenths));
    JS_SetPropertyStr(ctx, out, "maxCallbackMs", JS_NewFloat64(ctx, status.maxCallbackMs));
    JS_SetPropertyStr(ctx, out, "modId", JS_NewString(ctx, status.modId.c_str()));
    JS_SetPropertyStr(ctx, out, "phase", JS_NewString(ctx, status.phase.c_str()));
    return out;
}

JSValue jsSourceLiveNpcPause(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    bl::csharp::pauseLiveNpcAi();
    return jsSourceLiveNpcStatus(ctx, JS_UNDEFINED, 0, nullptr);
}

JSValue jsSourceLiveNpcEnable(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1]))
        return JS_ThrowTypeError(ctx,
            "sourceLiveNpcEnable(modId, 'CONFIRM_LIVE_NPC_AI')");
    const char* uid = JS_ToCString(ctx, argv[0]);
    const char* confirmation = JS_ToCString(ctx, argv[1]);
    if (!uid || !confirmation) {
        if (uid) JS_FreeCString(ctx, uid);
        if (confirmation) JS_FreeCString(ctx, confirmation);
        return JS_EXCEPTION;
    }
    const std::string modId(uid);
    const std::string confirm(confirmation);
    JS_FreeCString(ctx, uid);
    JS_FreeCString(ctx, confirmation);
    if (confirm != "CONFIRM_LIVE_NPC_AI")
        return JS_ThrowTypeError(ctx, "Live NPC AI requires explicit confirmation");
    int npcType = -1;
    for (const auto& mod : bl::csharp::sourceModStatuses()) {
        if (mod.id != modId || !mod.loaded || !mod.runtimeApplied) continue;
        const auto it = mod.registeredTypes.find("ExampleSlimeNPC");
        if (it != mod.registeredTypes.end()) npcType = it->second;
        break;
    }
    if (npcType < 0)
        return JS_ThrowInternalError(ctx, "ExampleSlimeNPC source class not registered");
    const std::string& checkpoint = bl::config().managedCheckpointPath;
    const size_t slash = checkpoint.find_last_of('/');
    if (slash == std::string::npos)
        return JS_ThrowInternalError(ctx, "MonoVM framework directory missing");
    const std::string dll = checkpoint.substr(0, slash + 1) +
        "compiled/" + modId + ".dll";
    std::string error;
    if (!bl::csharp::enableLiveNpcAi(modId, npcType, dll, &error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    return jsSourceLiveNpcStatus(ctx, JS_UNDEFINED, 0, nullptr);
}

JSValue makeSourceBoltHookStatus(JSContext* ctx) {
    const auto status = bl::csharp::managedSourceBoltHookStatus();
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "installed", JS_NewBool(ctx, status.installed));
    JS_SetPropertyStr(ctx, result, "enabled", JS_NewBool(ctx, status.enabled));
    JS_SetPropertyStr(ctx, result, "buffered", JS_NewBool(ctx, status.buffered));
    JS_SetPropertyStr(ctx, result, "bufferSamples", JS_NewInt32(ctx, status.bufferSamples));
    JS_SetPropertyStr(ctx, result, "homing", JS_NewBool(ctx, status.homing));
    JS_SetPropertyStr(ctx, result, "homingScans", JS_NewInt32(ctx, status.homingScans));
    JS_SetPropertyStr(ctx, result, "homingLocks", JS_NewInt32(ctx, status.homingLocks));
    JS_SetPropertyStr(ctx, result, "homingFrames", JS_NewInt32(ctx, status.homingFrames));
    JS_SetPropertyStr(ctx, result, "lastTarget", JS_NewInt32(ctx, status.lastTarget));
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

JSValue jsSourceEnableBufferedAI(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "sourceEnableBufferedAI('CONFIRM_BUFFERED_CSHARP_AI')");
    const char* token = JS_ToCString(ctx, argv[0]);
    if (!token) return JS_EXCEPTION;
    const bool approved = std::string_view(token) == "CONFIRM_BUFFERED_CSHARP_AI";
    JS_FreeCString(ctx, token);
    if (!approved)
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_BUFFERED_CSHARP_AI");

    const auto state = readManagedWorkerFile("source-compile-status.txt");
    const auto diagnostic = readManagedWorkerFile("source-compile-diagnostics.txt");
    if (state.find("state=finished\n") == std::string::npos ||
        state.find("phase=compile-verified\n") == std::string::npos ||
        !sourceAssemblyTypesVerified(state) ||
        state.find("assemblyReady=1\n") == std::string::npos ||
        diagnostic.find("AI_BUFFER_OK ticks=120") == std::string::npos)
        return JS_ThrowInternalError(ctx, "Source C# deve compilar, executar e exportar 120 ticks de IA");

    bool applied = false;
    for (const auto& mod : bl::csharp::sourceModStatuses()) {
        if (mod.id == "57a98f20-7a35-4d19-8ce8-079e25bb36a1" &&
            mod.loaded && mod.runtimeApplied) {
            applied = true;
            break;
        }
    }
    if (!applied)
        return JS_ThrowInternalError(ctx, "Aplicar sourceApplyCompiled antes de ativar IA");

    const auto file = readManagedWorkerFile("source-ai-turns.txt");
    std::istringstream stream(file);
    std::string header;
    std::string projectile;
    std::string data;
    if (!std::getline(stream, header) || header != "BUNNY_SOURCE_AI_TURNS_V1" ||
        !std::getline(stream, projectile) || projectile != "SourceBolt" ||
        !std::getline(stream, data) || data.empty() || data.size() > 1500)
        return JS_ThrowInternalError(ctx, "Arquivo de IA gerenciada ausente ou invalido");
    std::string extra;
    if (std::getline(stream, extra) && !extra.empty())
        return JS_ThrowInternalError(ctx, "Arquivo de IA contem dados inesperados");
    std::array<int, 120> turns{};
    std::istringstream entries(data);
    std::string part;
    for (size_t i = 0; i < turns.size(); ++i) {
        if (!std::getline(entries, part, '\t') || part.empty() || part.size() > 5)
            return JS_ThrowInternalError(ctx, "Buffer C# incompleto no tick %d", static_cast<int>(i));
        errno = 0;
        char* end = nullptr;
        const long turn = std::strtol(part.c_str(), &end, 10);
        if (errno || end == part.c_str() || *end || turn < -80 || turn > 80)
            return JS_ThrowInternalError(ctx, "Curva C# fora de limite no tick %d", static_cast<int>(i));
        turns[i] = static_cast<int>(turn);
    }
    if (std::getline(entries, part, '\t'))
        return JS_ThrowInternalError(ctx, "Buffer C# possui mais de 120 amostras");

    std::string error;
    if (!bl::csharp::enableBufferedSourceBoltHook(turns, &error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    JSValue result = makeSourceBoltHookStatus(ctx);
    JS_SetPropertyStr(ctx, result, "engine", JS_NewString(ctx, "Roslyn-MonoVM-AI-Buffered-v1"));
    return result;
}

JSValue jsSourceAIStatus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    const auto status = bl::csharp::managedSourceBoltHookStatus();
    JSValue result = makeSourceBoltHookStatus(ctx);
    JS_SetPropertyStr(ctx, result, "engine", JS_NewString(ctx, status.homing ? "Roslyn-MonoVM-Homing-Policy-v1" : "Roslyn-MonoVM-AI-Buffered-v1"));
    return result;
}

JSValue jsSourceEnableHomingAI(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "sourceEnableHomingAI('CONFIRM_HOMING_CSHARP_AI')");
    const char* token = JS_ToCString(ctx, argv[0]);
    if (!token) return JS_EXCEPTION;
    const bool approved = std::string_view(token) == "CONFIRM_HOMING_CSHARP_AI";
    JS_FreeCString(ctx, token);
    if (!approved)
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_HOMING_CSHARP_AI");

    const std::string state = readManagedWorkerFile("source-compile-status.txt");
    const std::string diagnostics = readManagedWorkerFile("source-compile-diagnostics.txt");
    if (state.find("state=finished\n") == std::string::npos ||
        state.find("phase=compile-verified\n") == std::string::npos ||
        !sourceAssemblyTypesVerified(state) ||
        state.find("assemblyReady=1\n") == std::string::npos ||
        diagnostics.find("HOMING_POLICY_OK samples=129") == std::string::npos ||
        diagnostics.find("AI_BUFFER_OK ticks=120") == std::string::npos)
        return JS_ThrowInternalError(ctx, "Compilar a versao C# com politica de homing primeiro");

    bool applied = false;
    for (const auto& mod : bl::csharp::sourceModStatuses()) {
        if (mod.id == "57a98f20-7a35-4d19-8ce8-079e25bb36a1" &&
            mod.loaded && mod.runtimeApplied && mod.sourceFiles > 0 &&
            mod.registeredTypes.contains("SourceBolt")) {
            applied = true;
            break;
        }
    }
    if (!applied)
        return JS_ThrowInternalError(ctx, "sourceApplyCompiled precisa estar aplicado e SourceBolt registrado");

    auto parseTurns = [](const std::string& line, int* values, size_t count) -> bool {
        if (line.empty() || line.size() > count * 6) return false;
        std::istringstream stream(line);
        std::string text;
        for (size_t i = 0; i < count; ++i) {
            if (!std::getline(stream, text, '\t') || text.empty() || text.size() > 5) return false;
            errno = 0;
            char* end = nullptr;
            long number = std::strtol(text.c_str(), &end, 10);
            if (errno || end == text.c_str() || *end != 0 || number < -80 || number > 80)
                return false;
            values[i] = static_cast<int>(number);
        }
        return !std::getline(stream, text, '\t');
    };
    auto parsePositive = [](const std::string& value, int minValue, int maxValue, int* result) -> bool {
        if (value.empty() || value.size() > 6) return false;
        errno = 0;
        char* end = nullptr;
        long x = std::strtol(value.c_str(), &end, 10);
        if (errno || end == value.c_str() || *end || x < minValue || x > maxValue) return false;
        *result = static_cast<int>(x);
        return true;
    };

    std::array<int, 120> fallback{};
    {
        std::istringstream file(readManagedWorkerFile("source-ai-turns.txt"));
        std::string header, projectile, data, extra;
        if (!std::getline(file, header) || header != "BUNNY_SOURCE_AI_TURNS_V1" ||
            !std::getline(file, projectile) || projectile != "SourceBolt" ||
            !std::getline(file, data) || !parseTurns(data, fallback.data(), fallback.size()) ||
            (std::getline(file, extra) && !extra.empty()))
            return JS_ThrowInternalError(ctx, "Buffer da curva C# invalido");
    }
    bl::csharp::SourceBoltHomingPolicy policy;
    {
        std::istringstream file(readManagedWorkerFile("source-homing-policy.txt"));
        std::string header, projectile, config, data, extra;
        if (!std::getline(file, header) || header != "BUNNY_SOURCE_HOMING_V1" ||
            !std::getline(file, projectile) || projectile != "SourceBolt" ||
            !std::getline(file, config) || !std::getline(file, data) ||
            (std::getline(file, extra) && !extra.empty()))
            return JS_ThrowInternalError(ctx, "Arquivo de politica C# invalido");
        const size_t divider = config.find('\t');
        if (divider == std::string::npos || config.find('\t', divider + 1) != std::string::npos ||
            !parsePositive(config.substr(0, divider), 64, 720, &policy.radius) ||
            !parsePositive(config.substr(divider + 1), 4, 24, &policy.retargetTicks) ||
            !parseTurns(data, policy.turnsMilliRadians.data(), policy.turnsMilliRadians.size()) ||
            policy.turnsMilliRadians[64] != 0 ||
            policy.turnsMilliRadians[0] >= 0 ||
            policy.turnsMilliRadians[128] <= 0)
            return JS_ThrowInternalError(ctx, "Dados da curva homing C# invalidos");
    }

    std::string error;
    if (!bl::csharp::enableHomingSourceBoltHook(policy, fallback, &error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    JSValue result = makeSourceBoltHookStatus(ctx);
    JS_SetPropertyStr(ctx, result, "engine", JS_NewString(ctx, "Roslyn-MonoVM-Homing-Policy-v1"));
    JS_SetPropertyStr(ctx, result, "radius", JS_NewInt32(ctx, policy.radius));
    JS_SetPropertyStr(ctx, result, "retargetTicks", JS_NewInt32(ctx, policy.retargetTicks));
    return result;
}

JSValue jsSourcePauseBufferedAI(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0)
        return JS_ThrowTypeError(ctx, "sourcePauseBufferedAI()");
    const bool wasEnabled = bl::csharp::pauseManagedSourceBoltHook();
    JSValue result = makeSourceBoltHookStatus(ctx);
    JS_SetPropertyStr(ctx, result, "wasEnabled", JS_NewBool(ctx, wasEnabled));
    return result;
}

bool sourceAssemblyTypesVerified(
    const std::string& state, const std::string& expectedModId) {
    if (expectedModId.empty() ||
        state.find("modId=" + expectedModId + "\n") == std::string::npos)
        return false;
    const std::string marker = "executedTypes=";
    const auto at = state.find(marker);
    if (at == std::string::npos) return false;
    const char* start = state.c_str() + at + marker.size();
    char* end = nullptr;
    errno = 0;
    const long types = std::strtol(start, &end, 10);
    if (errno || end == start || (*end != '\n' && *end != '\r') ||
        types < 2 || types > 32) return false;
    for (const auto& mod : bl::csharp::sourceModStatuses()) {
        if (mod.id == expectedModId &&
            mod.loaded && mod.projectiles > 0 && mod.items > 0)
            return types == mod.projectiles + mod.items;
    }
    return false;
}

JSValue jsSourceEnableOnHit(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "sourceEnableOnHit('CONFIRM_CSHARP_ONHIT')");
    const char* token = JS_ToCString(ctx, argv[0]);
    if (!token) return JS_EXCEPTION;
    const bool confirmed = std::string_view(token) == "CONFIRM_CSHARP_ONHIT";
    JS_FreeCString(ctx, token);
    if (!confirmed)
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_CSHARP_ONHIT");

    const std::string state = readManagedWorkerFile("source-compile-status.txt");
    const std::string diagnostics = readManagedWorkerFile("source-compile-diagnostics.txt");
    if (state.find("state=finished\n") == std::string::npos ||
        state.find("phase=compile-verified\n") == std::string::npos ||
        !sourceAssemblyTypesVerified(state) ||
        state.find("assemblyReady=1\n") == std::string::npos ||
        diagnostics.find("ONHIT_POLICY_OK buff=24 hits=256") == std::string::npos)
        return JS_ThrowInternalError(ctx, "Compile o Source C# Test com OnHitNPC antes da ativacao");
    bool applied = false;
    for (const auto& mod : bl::csharp::sourceModStatuses()) {
        if (mod.id == "57a98f20-7a35-4d19-8ce8-079e25bb36a1" &&
            mod.loaded && mod.runtimeApplied && mod.projectiles > 0) {
            applied = true;
            break;
        }
    }
    if (!applied)
        return JS_ThrowInternalError(ctx, "Aplique os valores gerenciados antes de ativar OnHitNPC");

    std::istringstream input(readManagedWorkerFile("source-onhit-policy.txt"));
    std::string header, name, buff, values, trailing;
    if (!std::getline(input, header) || header != "BUNNY_SOURCE_ONHIT_V2" ||
        !std::getline(input, name) || name != "SourceBolt" ||
        !std::getline(input, buff) || buff != "24" ||
        !std::getline(input, values) ||
        (std::getline(input, trailing) && !trailing.empty()))
        return JS_ThrowInternalError(ctx, "Arquivo OnHitNPC C# invalido");
    std::istringstream durations(values);
    bl::csharp::SourceBoltHitPolicy policy{};
    std::string piece;
    for (size_t i = 0; i < policy.durations.size(); ++i) {
        if (!std::getline(durations, piece, '\t') ||
            piece.empty() || piece.size() > 4)
            return JS_ThrowInternalError(ctx, "Tabela OnHitNPC C# incompleta");
        errno = 0;
        char* end = nullptr;
        const long n = std::strtol(piece.c_str(), &end, 10);
        if (errno || end == piece.c_str() || *end || n < 30 || n > 180)
            return JS_ThrowInternalError(ctx, "Duracao OnHitNPC C# invalida");
        policy.durations[i] = static_cast<int>(n);
    }
    if (std::getline(durations, piece, '\t'))
        return JS_ThrowInternalError(ctx, "Tabela OnHitNPC C# contem valores excedentes");

    std::string error;
    if (!bl::csharp::enableSourceBoltOnHit(policy, &error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    const auto status = bl::csharp::sourceBoltHitStatus();
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "enabled", JS_NewBool(ctx, status.enabled));
    JS_SetPropertyStr(ctx, result, "engine", JS_NewString(ctx, "Roslyn-MonoVM-OnHitNPC-v2"));
    JS_SetPropertyStr(ctx, result, "buffId", JS_NewInt32(ctx, status.buffId));
    JS_SetPropertyStr(ctx, result, "soloOnly", JS_NewBool(ctx, true));
    return result;
}

JSValue jsSourceHitStatus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    const auto status = bl::csharp::sourceBoltHitStatus();
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "installed", JS_NewBool(ctx, status.installed));
    JS_SetPropertyStr(ctx, result, "enabled", JS_NewBool(ctx, status.enabled));
    JS_SetPropertyStr(ctx, result, "hitEvents", JS_NewInt32(ctx, status.events));
    JS_SetPropertyStr(ctx, result, "buffsApplied", JS_NewInt32(ctx, status.applied));
    JS_SetPropertyStr(ctx, result, "errors", JS_NewInt32(ctx, status.errors));
    JS_SetPropertyStr(ctx, result, "lastNpc", JS_NewInt32(ctx, status.lastNpc));
    JS_SetPropertyStr(ctx, result, "lastDamage", JS_NewInt32(ctx, status.lastDamage));
    JS_SetPropertyStr(ctx, result, "lastDuration", JS_NewInt32(ctx, status.lastDuration));
    JS_SetPropertyStr(ctx, result, "buffId", JS_NewInt32(ctx, status.buffId));
    JS_SetPropertyStr(ctx, result, "engine", JS_NewString(ctx, "Roslyn-MonoVM-OnHitNPC-v2"));
    return result;
}

JSValue jsSourcePauseOnHit(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourcePauseOnHit()");
    const bool wasEnabled = bl::csharp::pauseSourceBoltOnHit();
    JSValue result = jsSourceHitStatus(ctx, JS_UNDEFINED, 0, nullptr);
    JS_SetPropertyStr(ctx, result, "wasEnabled", JS_NewBool(ctx, wasEnabled));
    return result;
}

JSValue jsSourceKillStatus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    const auto status = bl::csharp::sourceBoltKillStatus();
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "installed", JS_NewBool(ctx, status.installed));
    JS_SetPropertyStr(ctx, result, "enabled", JS_NewBool(ctx, status.enabled));
    JS_SetPropertyStr(ctx, result, "killEvents", JS_NewInt32(ctx, status.killEvents));
    JS_SetPropertyStr(ctx, result, "buffsApplied", JS_NewInt32(ctx, status.buffsApplied));
    JS_SetPropertyStr(ctx, result, "errors", JS_NewInt32(ctx, status.errors));
    JS_SetPropertyStr(ctx, result, "lastDamage", JS_NewInt32(ctx, status.lastDamage));
    JS_SetPropertyStr(ctx, result, "lastRadius", JS_NewInt32(ctx, status.lastRadius));
    JS_SetPropertyStr(ctx, result, "lastDuration", JS_NewInt32(ctx, status.lastDuration));
    JS_SetPropertyStr(ctx, result, "lastAffected", JS_NewInt32(ctx, status.lastAffected));
    JS_SetPropertyStr(ctx, result, "buffId", JS_NewInt32(ctx, status.buffId));
    JS_SetPropertyStr(ctx, result, "engine", JS_NewString(ctx, "Roslyn-MonoVM-OnKill-v1"));
    return result;
}

JSValue jsSourcePauseOnKill(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0)
        return JS_ThrowTypeError(ctx, "sourcePauseOnKill()");
    const bool wasEnabled = bl::csharp::pauseSourceBoltOnKill();
    JSValue result = jsSourceKillStatus(ctx, JS_UNDEFINED, 0, nullptr);
    JS_SetPropertyStr(ctx, result, "wasEnabled", JS_NewBool(ctx, wasEnabled));
    return result;
}

JSValue jsSourceEnableOnKill(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "sourceEnableOnKill('CONFIRM_CSHARP_ONKILL')");
    const char* token = JS_ToCString(ctx, argv[0]);
    if (!token) return JS_EXCEPTION;
    const bool confirmed = std::string_view(token) == "CONFIRM_CSHARP_ONKILL";
    JS_FreeCString(ctx, token);
    if (!confirmed)
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_CSHARP_ONKILL");

    const std::string state = readManagedWorkerFile("source-compile-status.txt");
    const std::string diagnostics = readManagedWorkerFile("source-compile-diagnostics.txt");
    if (state.find("state=finished\n") == std::string::npos ||
        state.find("phase=compile-verified\n") == std::string::npos ||
        !sourceAssemblyTypesVerified(state) ||
        state.find("assemblyReady=1\n") == std::string::npos ||
        diagnostics.find("ONKILL_POLICY_OK buff=24 radius=256 duration=256") == std::string::npos)
        return JS_ThrowInternalError(ctx, "Compile o Source C# Test com OnKill antes da ativacao");

    bool applied = false;
    for (const auto& mod : bl::csharp::sourceModStatuses()) {
        if (mod.id == "57a98f20-7a35-4d19-8ce8-079e25bb36a1" &&
            mod.loaded && mod.runtimeApplied && mod.projectiles > 0) {
            applied = true;
            break;
        }
    }
    if (!applied)
        return JS_ThrowInternalError(ctx, "Aplique SetDefaults gerenciado antes de OnKill");

    std::istringstream input(readManagedWorkerFile("source-onkill-policy.txt"));
    std::string header, name, buff, radiusValues, durationValues, trailing;
    if (!std::getline(input, header) || header != "BUNNY_SOURCE_ONKILL_V1" ||
        !std::getline(input, name) || name != "SourceBolt" ||
        !std::getline(input, buff) || buff != "24" ||
        !std::getline(input, radiusValues) ||
        !std::getline(input, durationValues) ||
        (std::getline(input, trailing) && !trailing.empty()))
        return JS_ThrowInternalError(ctx, "Arquivo C# OnKill invalido");

    auto parseTable = [](const std::string& line, int* output, size_t count, int min, int max) {
        std::istringstream values(line);
        std::string piece;
        for (size_t i = 0; i < count; ++i) {
            if (!std::getline(values, piece, '\t') || piece.empty() || piece.size() > 4)
                return false;
            errno = 0;
            char* end = nullptr;
            const long value = std::strtol(piece.c_str(), &end, 10);
            if (errno || end == piece.c_str() || *end || value < min || value > max)
                return false;
            output[i] = static_cast<int>(value);
        }
        return !std::getline(values, piece, '\t');
    };

    bl::csharp::SourceBoltKillPolicy policy{};
    if (!parseTable(radiusValues, policy.radii.data(), policy.radii.size(), 24, 120) ||
        !parseTable(durationValues, policy.durations.data(), policy.durations.size(), 30, 180))
        return JS_ThrowInternalError(ctx, "Tabelas C# OnKill invalidas ou incompletas");

    std::string error;
    if (!bl::csharp::enableSourceBoltOnKill(policy, &error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    JSValue result = jsSourceKillStatus(ctx, JS_UNDEFINED, 0, nullptr);
    JS_SetPropertyStr(ctx, result, "soloOnly", JS_NewBool(ctx, true));
    return result;
}

JSValue jsSourceSparkStatus(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    const auto status = bl::csharp::sourceSparkAiStatus();
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "installed", JS_NewBool(ctx, status.installed));
    JS_SetPropertyStr(ctx, result, "enabled", JS_NewBool(ctx, status.enabled));
    JS_SetPropertyStr(ctx, result, "type", JS_NewInt32(ctx, status.type));
    JS_SetPropertyStr(ctx, result, "samples", JS_NewInt32(ctx, status.samples));
    JS_SetPropertyStr(ctx, result, "spawns", JS_NewInt32(ctx, status.spawns));
    JS_SetPropertyStr(ctx, result, "callbacks", JS_NewInt32(ctx, status.callbacks));
    JS_SetPropertyStr(ctx, result, "velocityWrites", JS_NewInt32(ctx, status.velocityWrites));
    JS_SetPropertyStr(ctx, result, "errors", JS_NewInt32(ctx, status.errors));
    JS_SetPropertyStr(ctx, result, "lastSlot", JS_NewInt32(ctx, status.lastSlot));
    JS_SetPropertyStr(ctx, result, "lastTurn", JS_NewInt32(ctx, status.lastTurn));
    JS_SetPropertyStr(ctx, result, "engine", JS_NewString(ctx, "Roslyn-MonoVM-SourceSpark-AI-v1"));
    return result;
}

JSValue jsSourcePauseSparkAI(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0)
        return JS_ThrowTypeError(ctx, "sourcePauseSparkAI()");
    bool previous = bl::csharp::pauseSourceSparkAi();
    JSValue result = jsSourceSparkStatus(ctx, JS_UNDEFINED, 0, nullptr);
    JS_SetPropertyStr(ctx, result, "wasEnabled", JS_NewBool(ctx, previous));
    return result;
}

JSValue jsSourceEnableSparkAI(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "sourceEnableSparkAI('CONFIRM_SPARK_CSHARP_AI')");
    const char* supplied = JS_ToCString(ctx, argv[0]);
    if (!supplied) return JS_EXCEPTION;
    const bool approved = std::string_view(supplied) == "CONFIRM_SPARK_CSHARP_AI";
    JS_FreeCString(ctx, supplied);
    if (!approved)
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_SPARK_CSHARP_AI");
    const std::string state = readManagedWorkerFile("source-compile-status.txt");
    const std::string diagnostics = readManagedWorkerFile("source-compile-diagnostics.txt");
    if (state.find("state=finished\n") == std::string::npos ||
        state.find("phase=compile-verified\n") == std::string::npos ||
        !sourceAssemblyTypesVerified(state) ||
        state.find("assemblyReady=1\n") == std::string::npos ||
        diagnostics.find("SPARK_AI_BUFFER_OK ticks=120 source=SourceSparkMotion") == std::string::npos)
        return JS_ThrowInternalError(ctx, "Compile a politica SourceSparkMotion C# antes de ativar");
    bool applied = false;
    for (const auto& mod : bl::csharp::sourceModStatuses()) {
        if (mod.id == "57a98f20-7a35-4d19-8ce8-079e25bb36a1" &&
            mod.loaded && mod.runtimeApplied && mod.registeredTypes.contains("SourceSpark")) {
            applied = true;
            break;
        }
    }
    if (!applied)
        return JS_ThrowInternalError(ctx, "Aplique os SetDefaults e registre SourceSpark antes");
    std::istringstream input(readManagedWorkerFile("source-spark-turns.txt"));
    std::string header, projectile, values, trailing;
    if (!std::getline(input, header) || header != "BUNNY_SOURCE_SPARK_TURNS_V1" ||
        !std::getline(input, projectile) || projectile != "SourceSpark" ||
        !std::getline(input, values) || (std::getline(input, trailing) && !trailing.empty()))
        return JS_ThrowInternalError(ctx, "Arquivo de IA C# SourceSpark invalido");
    std::istringstream samples(values);
    std::array<int, 120> turns{};
    int nonzero = 0;
    for (size_t i = 0; i < turns.size(); ++i) {
        std::string part;
        if (!std::getline(samples, part, '\t') || part.empty() || part.size() > 5)
            return JS_ThrowInternalError(ctx, "Curva SourceSpark C# incompleta");
        char* end = nullptr;
        errno = 0;
        long value = std::strtol(part.c_str(), &end, 10);
        if (errno || end == part.c_str() || *end != 0 || value < -60 || value > 60)
            return JS_ThrowInternalError(ctx, "Amostra SourceSpark C# invalida");
        turns[i] = static_cast<int>(value);
        if (value) ++nonzero;
    }
    std::string overflow;
    if (std::getline(samples, overflow, '\t') || nonzero < 80)
        return JS_ThrowInternalError(ctx, "Curva SourceSpark C# maior que 120 amostras ou sem movimento");
    std::string error;
    if (!bl::csharp::enableSourceSparkAi(turns, &error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    JSValue result = jsSourceSparkStatus(ctx, JS_UNDEFINED, 0, nullptr);
    JS_SetPropertyStr(ctx, result, "soloOnly", JS_NewBool(ctx, true));
    return result;
}

JSValue makeProjectileCurveStatus(JSContext* ctx, const std::string& modId,
                                  const std::string& name) {
    const auto status = bl::csharp::managedProjectileCurveStatus(modId, name);
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "modId", JS_NewString(ctx, modId.c_str()));
    JS_SetPropertyStr(ctx, result, "projectile", JS_NewString(ctx, name.c_str()));
    JS_SetPropertyStr(ctx, result, "registered", JS_NewBool(ctx, status.registered));
    JS_SetPropertyStr(ctx, result, "installed", JS_NewBool(ctx, status.installed));
    JS_SetPropertyStr(ctx, result, "enabled", JS_NewBool(ctx, status.enabled));
    JS_SetPropertyStr(ctx, result, "type", JS_NewInt32(ctx, status.type));
    JS_SetPropertyStr(ctx, result, "samples", JS_NewInt32(ctx, status.samples));
    JS_SetPropertyStr(ctx, result, "spawns", JS_NewInt32(ctx, status.spawns));
    JS_SetPropertyStr(ctx, result, "callbacks", JS_NewInt32(ctx, status.callbacks));
    JS_SetPropertyStr(ctx, result, "velocityWrites", JS_NewInt32(ctx, status.velocityWrites));
    JS_SetPropertyStr(ctx, result, "errors", JS_NewInt32(ctx, status.errors));
    JS_SetPropertyStr(ctx, result, "lastSlot", JS_NewInt32(ctx, status.lastSlot));
    JS_SetPropertyStr(ctx, result, "lastTurn", JS_NewInt32(ctx, status.lastTurn));
    const auto hit = bl::csharp::autoProjectileHitStatus(modId, name);
    const auto kill = bl::csharp::autoProjectileKillStatus(modId, name);
    JSValue onHit = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, onHit, "registered", JS_NewBool(ctx, hit.registered));
    JS_SetPropertyStr(ctx, onHit, "enabled", JS_NewBool(ctx, hit.enabled));
    JS_SetPropertyStr(ctx, onHit, "events", JS_NewInt32(ctx, hit.events));
    JS_SetPropertyStr(ctx, onHit, "buffsApplied", JS_NewInt32(ctx, hit.buffsApplied));
    JS_SetPropertyStr(ctx, onHit, "errors", JS_NewInt32(ctx, hit.errors));
    JS_SetPropertyStr(ctx, onHit, "buffId", JS_NewInt32(ctx, hit.buffId));
    JS_SetPropertyStr(ctx, result, "autoOnHit", onHit);
    JSValue onKill = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, onKill, "registered", JS_NewBool(ctx, kill.registered));
    JS_SetPropertyStr(ctx, onKill, "enabled", JS_NewBool(ctx, kill.enabled));
    JS_SetPropertyStr(ctx, onKill, "events", JS_NewInt32(ctx, kill.events));
    JS_SetPropertyStr(ctx, onKill, "buffsApplied", JS_NewInt32(ctx, kill.buffsApplied));
    JS_SetPropertyStr(ctx, onKill, "errors", JS_NewInt32(ctx, kill.errors));
    JS_SetPropertyStr(ctx, onKill, "buffId", JS_NewInt32(ctx, kill.buffId));
    JS_SetPropertyStr(ctx, result, "autoOnKill", onKill);
    JS_SetPropertyStr(ctx, result, "engine", JS_NewString(ctx, "Roslyn-MonoVM-Projectile-Curve-v1"));
    return result;
}

bool parseProjectileCurveArgs(JSContext* ctx, int argc, JSValueConst* argv,
                              int expected, std::string* modId, std::string* name) {
    if (argc != expected || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        JS_ThrowTypeError(ctx, "Informe o ID do mod e o nome do projetil");
        return false;
    }
    const char* mod = JS_ToCString(ctx, argv[0]);
    if (!mod) return false;
    const char* projectile = JS_ToCString(ctx, argv[1]);
    if (!projectile) {
        JS_FreeCString(ctx, mod);
        return false;
    }
    *modId = mod;
    *name = projectile;
    JS_FreeCString(ctx, projectile);
    JS_FreeCString(ctx, mod);
    if (modId->empty() || modId->size() > 128 ||
        name->empty() || name->size() > 72) {
        JS_ThrowTypeError(ctx, "ID do mod ou nome do projetil invalido");
        return false;
    }
    return true;
}

JSValue jsSourceProjectileAIStatus(JSContext* ctx, JSValueConst,
                                   int argc, JSValueConst* argv) {
    std::string id, name;
    if (!parseProjectileCurveArgs(ctx, argc, argv, 2, &id, &name)) return JS_EXCEPTION;
    return makeProjectileCurveStatus(ctx, id, name);
}

JSValue jsSourcePauseProjectileAI(JSContext* ctx, JSValueConst,
                                  int argc, JSValueConst* argv) {
    std::string id, name;
    if (!parseProjectileCurveArgs(ctx, argc, argv, 2, &id, &name)) return JS_EXCEPTION;
    const bool previous = bl::csharp::pauseManagedProjectileCurve(id, name);
    JSValue result = makeProjectileCurveStatus(ctx, id, name);
    JS_SetPropertyStr(ctx, result, "wasEnabled", JS_NewBool(ctx, previous));
    return result;
}

JSValue jsSourceEnableProjectileAI(JSContext* ctx, JSValueConst,
                                   int argc, JSValueConst* argv) {
    std::string id, name;
    if (!parseProjectileCurveArgs(ctx, argc, argv, 3, &id, &name)) return JS_EXCEPTION;
    if (!JS_IsString(argv[2]))
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_CSHARP_PROJECTILE_AI");
    const char* supplied = JS_ToCString(ctx, argv[2]);
    if (!supplied) return JS_EXCEPTION;
    const bool confirmed = std::string_view(supplied) == "CONFIRM_CSHARP_PROJECTILE_AI";
    JS_FreeCString(ctx, supplied);
    if (!confirmed)
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_CSHARP_PROJECTILE_AI");

    const std::string state = readManagedWorkerFile("source-compile-status.txt");
    const std::string diagnostics = readManagedWorkerFile("source-compile-diagnostics.txt");
    if (state.find("state=finished\n") == std::string::npos ||
        state.find("phase=compile-verified\n") == std::string::npos ||
        !sourceAssemblyTypesVerified(state, id) ||
        state.find("assemblyReady=1\n") == std::string::npos ||
        diagnostics.find("PROJECTILE_TURNS_V1_OK curves=") == std::string::npos)
        return JS_ThrowInternalError(ctx, "Compile novamente as politicas C# antes de ativar");
    bool applied = false;
    for (const auto& mod : bl::csharp::sourceModStatuses()) {
        if (mod.id == id && mod.loaded && mod.runtimeApplied &&
            mod.registeredTypes.contains(name)) {
            applied = true;
            break;
        }
    }
    if (!applied)
        return JS_ThrowInternalError(ctx, "Projetil nao registrado ou SetDefaults nao aplicado");

    std::istringstream input(readManagedWorkerFile("source-projectile-turns.txt"));
    std::string header, line;
    if (!std::getline(input, header) || header != "BUNNY_PROJECTILE_TURNS_V1")
        return JS_ThrowInternalError(ctx, "Cabecalho da tabela C# de projeteis invalido");
    std::array<int, 120> turns{};
    bool found = false;
    int rows = 0;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        if (++rows > 16 || line.size() > 1300)
            return JS_ThrowInternalError(ctx, "Tabela C# de projeteis excedeu os limites");
        std::istringstream samples(line);
        std::string candidate;
        if (!std::getline(samples, candidate, '\t') || candidate.empty())
            return JS_ThrowInternalError(ctx, "Nome de projetil na tabela invalido");
        if (candidate != name) continue;
        if (found)
            return JS_ThrowInternalError(ctx, "Politica C# de projetil duplicada");
        for (size_t i = 0; i < turns.size(); ++i) {
            std::string piece;
            if (!std::getline(samples, piece, '\t') || piece.empty() || piece.size() > 5)
                return JS_ThrowInternalError(ctx, "Curva C# de projetil incompleta");
            errno = 0;
            char* end = nullptr;
            long value = std::strtol(piece.c_str(), &end, 10);
            if (errno || end == piece.c_str() || *end || value < -80 || value > 80)
                return JS_ThrowInternalError(ctx, "Curva C# de projetil fora dos limites");
            turns[i] = static_cast<int>(value);
        }
        std::string extra;
        if (std::getline(samples, extra, '\t'))
            return JS_ThrowInternalError(ctx, "Curva C# de projetil com amostras extras");
        found = true;
    }
    if (!found)
        return JS_ThrowInternalError(ctx, "Projetil nao possui metodo C# TurnAtTick(int)");
    std::string error;
    if (!bl::csharp::enableManagedProjectileCurve(id, name, turns, &error))
        return JS_ThrowInternalError(ctx, "%s", error.c_str());
    JSValue result = makeProjectileCurveStatus(ctx, id, name);
    JS_SetPropertyStr(ctx, result, "soloOnly", JS_NewBool(ctx, true));
    return result;
}

JSValue jsSourceCompileMod(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "sourceCompileMod('MOD_UUID')");
    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw) return JS_EXCEPTION;
    const std::string id(raw);
    JS_FreeCString(ctx, raw);
    if (id.empty() || id.size() > 128)
        return JS_ThrowTypeError(ctx, "ID do mod C# invalido");
    bool loaded = false;
    for (const auto& status : bl::csharp::sourceModStatuses()) {
        if (status.id == id && status.loaded && status.sourceFiles > 0 &&
            status.projectiles > 0 && status.items > 0) {
            loaded = true;
            break;
        }
    }
    if (!loaded)
        return JS_ThrowInternalError(ctx, "Mod C# nao encontrado ou conteudo nao registrado");
    const std::string root = bl::mods::rootOf(id);
    if (root.empty())
        return JS_ThrowInternalError(ctx, "Diretorio do mod C# indisponivel");
    std::string error;
    const bool queued = bl::csharp::requestIsolatedMonoCompile(root, id, error);
    JSValue out = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, out, "modId", JS_NewString(ctx, id.c_str()));
    JS_SetPropertyStr(ctx, out, "queued", JS_NewBool(ctx, queued));
    JS_SetPropertyStr(ctx, out, "isolated", JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, out, "engine", JS_NewString(ctx, "Roslyn-net8"));
    JS_SetPropertyStr(ctx, out, "error", JS_NewString(ctx, error.c_str()));
    return out;
}

JSValue jsSourceApplyMod(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1]))
        return JS_ThrowTypeError(ctx, "sourceApplyMod('MOD_UUID', 'CONFIRM_MANAGED_DEFAULTS')");
    const char* mod = JS_ToCString(ctx, argv[0]);
    if (!mod) return JS_EXCEPTION;
    const std::string id(mod);
    JS_FreeCString(ctx, mod);
    const char* confirm = JS_ToCString(ctx, argv[1]);
    if (!confirm) return JS_EXCEPTION;
    const bool approved = std::string_view(confirm) == "CONFIRM_MANAGED_DEFAULTS";
    JS_FreeCString(ctx, confirm);
    if (!approved)
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_MANAGED_DEFAULTS");
    if (id.empty() || id.size() > 128)
        return JS_ThrowTypeError(ctx, "ID do mod C# invalido");
    const std::string state = readManagedWorkerFile("source-compile-status.txt");
    if (state.find("state=finished\n") == std::string::npos ||
        state.find("phase=compile-verified\n") == std::string::npos ||
        !sourceAssemblyTypesVerified(state, id) ||
        state.find("assemblyReady=1\n") == std::string::npos)
        return JS_ThrowInternalError(ctx, "Compile este mesmo mod C# e aguarde a verificacao");
    const std::string values = readManagedWorkerFile("source-runtime-values.txt");
    std::string error;
    const bool applied = bl::csharp::applyManagedSourceDefaults(id, values, &error);
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "modId", JS_NewString(ctx, id.c_str()));
    JS_SetPropertyStr(ctx, result, "applied", JS_NewBool(ctx, applied));
    JS_SetPropertyStr(ctx, result, "engine", JS_NewString(ctx, "Roslyn-MonoVM-SetDefaults-v2"));
    int items = 0;
    int projectiles = 0;
    if (applied) {
        for (const auto& status : bl::csharp::sourceModStatuses()) {
            if (status.id == id && status.loaded && status.runtimeApplied) {
                items = status.items;
                projectiles = status.projectiles;
                break;
            }
        }
    }
    JS_SetPropertyStr(ctx, result, "items", JS_NewInt32(ctx, items));
    JS_SetPropertyStr(ctx, result, "projectiles", JS_NewInt32(ctx, projectiles));
    JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, error.c_str()));
    return result;
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

JSValue jsSourceApplyCompiled(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc != 1 || !JS_IsString(argv[0]))
        return JS_ThrowTypeError(ctx, "sourceApplyCompiled('CONFIRM_MANAGED_DEFAULTS')");
    const char* raw = JS_ToCString(ctx, argv[0]);
    if (!raw) return JS_EXCEPTION;
    const bool confirmed = std::string_view(raw) == "CONFIRM_MANAGED_DEFAULTS";
    JS_FreeCString(ctx, raw);
    if (!confirmed)
        return JS_ThrowTypeError(ctx, "Confirmacao exigida: CONFIRM_MANAGED_DEFAULTS");

    constexpr const char* uid = "57a98f20-7a35-4d19-8ce8-079e25bb36a1";
    const std::string state = readManagedWorkerFile("source-compile-status.txt");
    if (state.find("state=finished\n") == std::string::npos ||
        state.find("phase=compile-verified\n") == std::string::npos ||
        !sourceAssemblyTypesVerified(state) ||
        state.find("assemblyReady=1\n") == std::string::npos)
        return JS_ThrowInternalError(ctx, "Compilar e executar o mod C# no worker antes de aplicar os valores");
    const std::string values = readManagedWorkerFile("source-runtime-values.txt");
    std::string error;
    const bool applied = bl::csharp::applyManagedSourceDefaults(uid, values, &error);
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "applied", JS_NewBool(ctx, applied));
    JS_SetPropertyStr(ctx, result, "engine",
                      JS_NewString(ctx, applied ? "Roslyn-MonoVM-SetDefaults-v1" : "SetDefaults-literals-v1"));
    int itemCount = 0;
    int projectileCount = 0;
    if (applied) {
        for (const auto& mod : bl::csharp::sourceModStatuses())
            if (mod.id == uid && mod.loaded && mod.runtimeApplied) {
                itemCount = mod.items;
                projectileCount = mod.projectiles;
                break;
            }
    }
    JS_SetPropertyStr(ctx, result, "items", JS_NewInt32(ctx, itemCount));
    JS_SetPropertyStr(ctx, result, "projectiles", JS_NewInt32(ctx, projectileCount));
    JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, error.c_str()));
    return result;
}

JSValue jsSourceAutoStatus(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "sourceAutoStatus()");
    JSValue output = JS_NewArray(ctx);
    uint32_t index = 0;
    for (const auto& status : bl::csharp::autoSourceStatuses()) {
        JSValue item = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, item, "modId", JS_NewString(ctx, status.modId.c_str()));
        JS_SetPropertyStr(ctx, item, "phase", JS_NewString(ctx, status.phase.c_str()));
        JS_SetPropertyStr(ctx, item, "cached", JS_NewBool(ctx, status.cached));
        JS_SetPropertyStr(ctx, item, "applied", JS_NewBool(ctx, status.applied));
        JS_SetPropertyStr(ctx, item, "curves", JS_NewInt32(ctx, status.curves));
        JS_SetPropertyStr(ctx, item, "enabledCurves", JS_NewInt32(ctx, status.enabledCurves));
        JS_SetPropertyStr(ctx, item, "hitPolicies", JS_NewInt32(ctx, status.hitPolicies));
        JS_SetPropertyStr(ctx, item, "killPolicies", JS_NewInt32(ctx, status.killPolicies));
        JS_SetPropertyStr(ctx, item, "enabledHits", JS_NewInt32(ctx, status.enabledHits));
        JS_SetPropertyStr(ctx, item, "enabledKills", JS_NewInt32(ctx, status.enabledKills));
        JS_SetPropertyStr(ctx, item, "netMode", JS_NewInt32(ctx, status.netMode));
        JS_SetPropertyStr(ctx, item, "gameMenu", JS_NewInt32(ctx, status.gameMenu));
        JS_SetPropertyStr(ctx, item, "worldReady", JS_NewBool(ctx, status.worldReady));
        JS_SetPropertyStr(ctx, item, "error", JS_NewString(ctx, status.error.c_str()));
        JS_SetPropertyUint32(ctx, output, index++, item);
    }
    return output;
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
    std::string exitTrace;
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
        exitTrace = bl::csharp::historicalMonoWorkerTrace(
            static_cast<int>(workerPid), minimumTimestampMs);
    }
    JS_SetPropertyStr(ctx, out, "exitInfo", JS_NewString(ctx, exitInfo.c_str()));
    JS_SetPropertyStr(ctx, out, "exitTrace", JS_NewString(ctx, exitTrace.c_str()));
    const auto audit = readManagedWorkerFile("mono-domain-thread-audit-latest.txt");
    JS_SetPropertyStr(ctx, out, "monoThreadAudit", JS_NewString(ctx, audit.c_str()));
    const auto gameProbeMode = readManagedWorkerFile("native-game-probe-mode.txt");
    const auto gameModuleMaps = readManagedWorkerFile("native-game-module-maps.txt");
    JS_SetPropertyStr(ctx, out, "gameProbeMode",
                      JS_NewString(ctx, gameProbeMode.c_str()));
    JS_SetPropertyStr(ctx, out, "gameModuleMaps",
                      JS_NewString(ctx, gameModuleMaps.c_str()));
    const auto gameNativeAbort = readManagedWorkerFile("native-game-abort-trace.txt");
    const auto gameNativePrevious = readManagedWorkerFile("native-game-abort-previous.txt");
    JS_SetPropertyStr(ctx, out, "gameNativeAbort",
                      JS_NewString(ctx, gameNativeAbort.c_str()));
    JS_SetPropertyStr(ctx, out, "gameNativePrevious",
                      JS_NewString(ctx, gameNativePrevious.c_str()));
    const auto nativeAbort = readManagedWorkerFile("native-abort-trace.txt");
    const auto nativeMaps = readManagedWorkerFile("native-abort-maps.txt");
    const auto nativeThreads = readManagedWorkerFile("native-abort-threads.txt");
    JS_SetPropertyStr(ctx, out, "nativeAbort", JS_NewString(ctx, nativeAbort.c_str()));
    JS_SetPropertyStr(ctx, out, "nativeMaps", JS_NewString(ctx, nativeMaps.c_str()));
    JS_SetPropertyStr(ctx, out, "nativeThreads", JS_NewString(ctx, nativeThreads.c_str()));
    const auto abortWatch = readManagedWorkerFile("native-abort-watch.txt");
    JS_SetPropertyStr(ctx, out, "abortWatch", JS_NewString(ctx, abortWatch.c_str()));
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
        JS_SetPropertyStr(ctx, row, "npcs", JS_NewInt32(ctx, mods[i].npcs));
        JSValue registered = JS_NewObject(ctx);
        for (const auto& [name, type] : mods[i].registeredTypes)
            JS_SetPropertyStr(ctx, registered, name.c_str(), JS_NewInt32(ctx, type));
        JS_SetPropertyStr(ctx, row, "registeredTypes", registered);
        JS_SetPropertyStr(ctx, row, "engine", JS_NewString(ctx,
        mods[i].runtimeApplied ? "Roslyn-MonoVM-SetDefaults-v1" : "SetDefaults-literals-v1"));
    JS_SetPropertyStr(ctx, row, "runtimeApplied", JS_NewBool(ctx, mods[i].runtimeApplied));
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
    const auto mono = bl::csharp::managedRuntimeStatus();
    const bool monoReady = mono.domainInitialized && mono.methodExecuted;
    int managedMods = 0;
    for (const auto& mod : bl::csharp::sourceModStatuses())
        if (mod.loaded && mod.runtimeApplied) ++managedMods;
    const bool legacyAvailable = bl::csharp::available();
    JS_SetPropertyStr(ctx, out, "available",
                      JS_NewBool(ctx, monoReady || managedMods > 0 ||
                                       legacyAvailable || bl::csharpmods::loadedCount() > 0));
    JS_SetPropertyStr(ctx, out, "runtimeMode", JS_NewString(ctx, "Roslyn+MonoVM"));
    JS_SetPropertyStr(ctx, out, "monoVmReady", JS_NewBool(ctx, monoReady));
    JS_SetPropertyStr(ctx, out, "sourceModCount", JS_NewInt32(ctx, managedMods));
    JS_SetPropertyStr(ctx, out, "nativeAotPrototypeAvailable", JS_NewBool(ctx, legacyAvailable));
    JS_SetPropertyStr(ctx, out, "nativeAotExternalCount",
                      JS_NewInt32(ctx, static_cast<int32_t>(bl::csharpmods::loadedCount())));
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

JSValue jsManagedAvailable(JSContext* ctx, JSValueConst, int argc, JSValueConst*) {
    if (argc != 0) return JS_ThrowTypeError(ctx, "managedAvailable()");
    const auto runtime = bl::csharp::managedRuntimeStatus();
    return JS_NewBool(ctx, runtime.domainInitialized && runtime.methodExecuted);
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
    JS_SetPropertyStr(ctx, api, "reserveRarityId", JS_NewCFunction(ctx, jsReserveRarityId, "reserveRarityId", 0));
    JS_SetPropertyStr(ctx, api, "rarityCount", JS_NewCFunction(ctx, jsRarityCount, "rarityCount", 0));
    JS_SetPropertyStr(ctx, api, "rarityColor", JS_NewCFunction(ctx, jsManagedRarityColor, "rarityColor", 1));
    JS_SetPropertyStr(ctx, api, "managedAvailable",
                      JS_NewCFunction(ctx, jsManagedAvailable, "managedAvailable", 0));
    JS_SetPropertyStr(ctx, api, "hostInfo", JS_NewCFunction(ctx, jsHostInfo, "hostInfo", 0));
    JS_SetPropertyStr(ctx, api, "sourceMods", JS_NewCFunction(ctx, jsSourceMods, "sourceMods", 0));
    JS_SetPropertyStr(ctx, api, "managedRuntime",
                      JS_NewCFunction(ctx, jsManagedRuntime, "managedRuntime", 0));
    JS_SetPropertyStr(ctx, api, "visualActiveCount",
                      JS_NewCFunction(ctx, jsManagedVisualActiveCount, "visualActiveCount", 0));
    JS_SetPropertyStr(ctx, api, "visualSnapshot",
                      JS_NewCFunction(ctx, jsManagedVisualSnapshot, "visualSnapshot", 1));
    JS_SetPropertyStr(ctx, api, "visualTexturePath",
                      JS_NewCFunction(ctx, jsManagedVisualTexturePath, "visualTexturePath", 1));
    JS_SetPropertyStr(ctx, api, "setNativeItemDefense", JS_NewCFunction(ctx, jsSetNativeItemDefense, "setNativeItemDefense", 2));
    JS_SetPropertyStr(ctx, api, "resolveNativeItemKey", JS_NewCFunction(ctx, jsResolveNativeItemKey, "resolveNativeItemKey", 1));
    JS_SetPropertyStr(ctx, api, "setNativeItemEquipSlot", JS_NewCFunction(ctx, jsSetNativeItemEquipSlot, "setNativeItemEquipSlot", 3));
    JS_SetPropertyStr(ctx, api, "nativeArmorPolicies", JS_NewCFunction(ctx, jsNativeArmorPolicies, "nativeArmorPolicies", 0));
    JS_SetPropertyStr(ctx, api, "nativeItemPolicies", JS_NewCFunction(ctx, jsNativeItemPolicies, "nativeItemPolicies", 0));
    JS_SetPropertyStr(ctx, api, "pendingItemBehaviors", JS_NewCFunction(ctx, jsPendingItemBehaviors, "pendingItemBehaviors", 0));
    JS_SetPropertyStr(ctx, api, "acknowledgeItemBehavior", JS_NewCFunction(ctx, jsAcknowledgeItemBehavior, "acknowledgeItemBehavior", 2));
    JS_SetPropertyStr(ctx, api, "reportItemRecipeError", JS_NewCFunction(ctx, jsReportItemRecipeError, "reportItemRecipeError", 2));
    JS_SetPropertyStr(ctx, api, "pendingItemRecipes", JS_NewCFunction(ctx, jsPendingItemRecipes, "pendingItemRecipes", 0));
    JS_SetPropertyStr(ctx, api, "nativeRecipes", JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, api, "acknowledgeItemRecipe", JS_NewCFunction(ctx, jsAcknowledgeItemRecipe, "acknowledgeItemRecipe", 2));
    JS_SetPropertyStr(ctx, api, "nativeEquipTextures",
                      JS_NewCFunction(ctx, jsManagedNativeEquipTextures, "nativeEquipTextures", 0));
    JS_SetPropertyStr(ctx, api, "setNativeEquipSlot",
                      JS_NewCFunction(ctx, jsManagedSetNativeEquipSlot, "setNativeEquipSlot", 3));
    JS_SetPropertyStr(ctx, api, "managedInspect",
                      JS_NewCFunction(ctx, jsManagedInspect, "managedInspect", 0));
    JS_SetPropertyStr(ctx, api, "managedProbe",
                      JS_NewCFunction(ctx, jsManagedProbe, "managedProbe", 0));
    JS_SetPropertyStr(ctx, api, "managedWorkerStatus",
                      JS_NewCFunction(ctx, jsManagedWorkerStatus, "managedWorkerStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourceLiveHitEnable",
                      JS_NewCFunction(ctx, jsSourceLiveHitEnable, "sourceLiveHitEnable", 2));
    JS_SetPropertyStr(ctx, api, "sourceMemoryStatus",
                      JS_NewCFunction(ctx, jsSourceMemoryStatus, "sourceMemoryStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourceMemoryBaseline",
                      JS_NewCFunction(ctx, jsSourceMemoryBaseline, "sourceMemoryBaseline", 0));
    JS_SetPropertyStr(ctx, api, "sourceLiveHitStatus",
                      JS_NewCFunction(ctx, jsSourceLiveHitStatus, "sourceLiveHitStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourceLiveProjectileStatus",
                      JS_NewCFunction(ctx, jsSourceLiveProjectileStatus,
                                      "sourceLiveProjectileStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourceLiveModPause",
                      JS_NewCFunction(ctx, jsSourceLiveModPause,
                                      "sourceLiveModPause", 2));
    JS_SetPropertyStr(ctx, api, "sourceLiveModResume",
                      JS_NewCFunction(ctx, jsSourceLiveModResume,
                                      "sourceLiveModResume", 2));
    JS_SetPropertyStr(ctx, api, "sourceLiveHitPause",
                      JS_NewCFunction(ctx, jsSourceLiveHitPause, "sourceLiveHitPause", 0));
    JS_SetPropertyStr(ctx, api, "vmCompareStart",
                      JS_NewCFunction(ctx, jsVmCompareStart, "vmCompareStart", 1));
    JS_SetPropertyStr(ctx, api, "vmGameplayCompareStart",
                      JS_NewCFunction(ctx, jsVmGameplayCompareStart, "vmGameplayCompareStart", 2));
    JS_SetPropertyStr(ctx, api, "vmCompareStatus",
                      JS_NewCFunction(ctx, jsVmCompareStatus, "vmCompareStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourcePerfStart",
                      JS_NewCFunction(ctx, jsSourcePerfStart, "sourcePerfStart", 0));
    JS_SetPropertyStr(ctx, api, "sourcePerfStatus",
                      JS_NewCFunction(ctx, jsSourcePerfStatus, "sourcePerfStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourcePerfStop",
                      JS_NewCFunction(ctx, jsSourcePerfStop, "sourcePerfStop", 0));
    JS_SetPropertyStr(ctx, api, "sourceLiveBenchmarkStart",
                      JS_NewCFunction(ctx, jsSourceLiveBenchmarkStart, "sourceLiveBenchmarkStart", 1));
    JS_SetPropertyStr(ctx, api, "sourceLiveBenchmarkStatus",
                      JS_NewCFunction(ctx, jsSourceLiveBenchmarkStatus, "sourceLiveBenchmarkStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourceLiveBenchmarkStop",
                      JS_NewCFunction(ctx, jsSourceLiveBenchmarkStop, "sourceLiveBenchmarkStop", 0));
    JS_SetPropertyStr(ctx, api, "sourceLiveStatus",
                      JS_NewCFunction(ctx, jsSourceLiveStatus, "sourceLiveStatus", 1));
    JS_SetPropertyStr(ctx, api, "sourceLiveTick",
                      JS_NewCFunction(ctx, jsSourceLiveTick, "sourceLiveTick", 3));
    JS_SetPropertyStr(ctx, api, "sourceLiveNpcStatus",
                      JS_NewCFunction(ctx, jsSourceLiveNpcStatus, "sourceLiveNpcStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourceLiveNpcEnable",
                      JS_NewCFunction(ctx, jsSourceLiveNpcEnable, "sourceLiveNpcEnable", 2));
    JS_SetPropertyStr(ctx, api, "sourceLiveNpcPause",
                      JS_NewCFunction(ctx, jsSourceLiveNpcPause, "sourceLiveNpcPause", 0));
    JS_SetPropertyStr(ctx, api, "sourceCompile",
                      JS_NewCFunction(ctx, jsManagedSourceCompile, "sourceCompile", 0));
    JS_SetPropertyStr(ctx, api, "sourceCompileMod",
                      JS_NewCFunction(ctx, jsSourceCompileMod, "sourceCompileMod", 1));
    JS_SetPropertyStr(ctx, api, "sourceApplyMod",
                      JS_NewCFunction(ctx, jsSourceApplyMod, "sourceApplyMod", 2));
    JS_SetPropertyStr(ctx, api, "sourceCompileStatus",
                      JS_NewCFunction(ctx, jsManagedSourceCompileStatus, "sourceCompileStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourceAutoStatus",
                      JS_NewCFunction(ctx, jsSourceAutoStatus, "sourceAutoStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourceApplyCompiled",
                      JS_NewCFunction(ctx, jsSourceApplyCompiled, "sourceApplyCompiled", 1));
    JS_SetPropertyStr(ctx, api, "sourceEnableBufferedAI",
                      JS_NewCFunction(ctx, jsSourceEnableBufferedAI, "sourceEnableBufferedAI", 1));
    JS_SetPropertyStr(ctx, api, "sourceAIStatus",
                      JS_NewCFunction(ctx, jsSourceAIStatus, "sourceAIStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourcePauseBufferedAI",
                      JS_NewCFunction(ctx, jsSourcePauseBufferedAI, "sourcePauseBufferedAI", 0));
    JS_SetPropertyStr(ctx, api, "sourceEnableHomingAI",
                      JS_NewCFunction(ctx, jsSourceEnableHomingAI, "sourceEnableHomingAI", 1));
    JS_SetPropertyStr(ctx, api, "sourceEnableSparkAI",
                      JS_NewCFunction(ctx, jsSourceEnableSparkAI, "sourceEnableSparkAI", 1));
    JS_SetPropertyStr(ctx, api, "sourceSparkStatus",
                      JS_NewCFunction(ctx, jsSourceSparkStatus, "sourceSparkStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourceEnableProjectileAI",
                      JS_NewCFunction(ctx, jsSourceEnableProjectileAI, "sourceEnableProjectileAI", 3));
    JS_SetPropertyStr(ctx, api, "sourceProjectileAIStatus",
                      JS_NewCFunction(ctx, jsSourceProjectileAIStatus, "sourceProjectileAIStatus", 2));
    JS_SetPropertyStr(ctx, api, "sourcePauseProjectileAI",
                      JS_NewCFunction(ctx, jsSourcePauseProjectileAI, "sourcePauseProjectileAI", 2));
    JS_SetPropertyStr(ctx, api, "sourcePauseSparkAI",
                      JS_NewCFunction(ctx, jsSourcePauseSparkAI, "sourcePauseSparkAI", 0));
    JS_SetPropertyStr(ctx, api, "sourceEnableOnHit",
                      JS_NewCFunction(ctx, jsSourceEnableOnHit, "sourceEnableOnHit", 1));
    JS_SetPropertyStr(ctx, api, "sourceHitStatus",
                      JS_NewCFunction(ctx, jsSourceHitStatus, "sourceHitStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourcePauseOnHit",
                      JS_NewCFunction(ctx, jsSourcePauseOnHit, "sourcePauseOnHit", 0));
    JS_SetPropertyStr(ctx, api, "sourceEnableOnKill",
                      JS_NewCFunction(ctx, jsSourceEnableOnKill, "sourceEnableOnKill", 1));
    JS_SetPropertyStr(ctx, api, "sourceKillStatus",
                      JS_NewCFunction(ctx, jsSourceKillStatus, "sourceKillStatus", 0));
    JS_SetPropertyStr(ctx, api, "sourcePauseOnKill",
                      JS_NewCFunction(ctx, jsSourcePauseOnKill, "sourcePauseOnKill", 0));
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

