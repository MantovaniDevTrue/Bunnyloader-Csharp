#include <jni.h>
#include <string>
#include "core/Config.h"
#include "core/Log.h"
#include "boot/LibWatcher.h"
#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/MonoServiceDispatch.h"

namespace {

std::string g_lastError;

std::string readString(JNIEnv* env, jobject obj, jclass cls, const char* name) {
    jfieldID id = env->GetFieldID(cls, name, "Ljava/lang/String;");
    auto js = reinterpret_cast<jstring>(env->GetObjectField(obj, id));
    if (!js) return {};
    const char* chars = env->GetStringUTFChars(js, nullptr);
    std::string out(chars ? chars : "");
    if (chars) env->ReleaseStringUTFChars(js, chars);
    return out;
}

} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_bunnyloader_nativebridge_NativeBridge_init(JNIEnv* env, jobject, jobject cfg) {
    jclass cls = env->GetObjectClass(cfg);
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        g_lastError = "NativeConfig JNI: classe indisponivel";
        return JNI_FALSE;
    }
    struct Field { const char* name; const char* signature; };
    constexpr Field fields[] = {
        {"gameLibDir", "Ljava/lang/String;"},
        {"modsDir", "Ljava/lang/String;"},
        {"enabledMods", "[Ljava/lang/String;"},
        {"logPath", "Ljava/lang/String;"},
        {"cmdPath", "Ljava/lang/String;"},
        {"showErrors", "Z"},
        {"verboseLog", "Z"},
        {"gameVersion", "J"},
        {"fastBoot", "Z"},
        {"quickPlayer", "Ljava/lang/String;"},
        {"quickWorld", "Ljava/lang/String;"},
        {"managedProbePath", "Ljava/lang/String;"},
        {"managedCheckpointPath", "Ljava/lang/String;"},
        {"appContext", "Landroid/content/Context;"},
        {"joinAddress", "Ljava/lang/String;"},
        {"joinPort", "I"},
        {"joinPassword", "Ljava/lang/String;"},
        {"joinPlayer", "Ljava/lang/String;"},
    };
    for (const Field& field : fields) {
        jfieldID id = env->GetFieldID(cls, field.name, field.signature);
        if (!id || env->ExceptionCheck()) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            g_lastError = std::string("NativeConfig JNI: campo ausente ou incompatível: ") + field.name;
            BL_ERROR("%s", g_lastError.c_str());
            env->DeleteLocalRef(cls);
            return JNI_FALSE;
        }
    }
    g_lastError.clear();
    auto& c = bl::config();

    c.gameLibDir = readString(env, cfg, cls, "gameLibDir");
    c.modsDir    = readString(env, cfg, cls, "modsDir");
    c.logPath    = readString(env, cfg, cls, "logPath");
    c.cmdPath    = readString(env, cfg, cls, "cmdPath");
    c.managedProbePath = readString(env, cfg, cls, "managedProbePath");
    c.managedCheckpointPath = readString(env, cfg, cls, "managedCheckpointPath");
    bl::csharp::setManagedCheckpointPath(c.managedCheckpointPath);
    jfieldID contextId = env->GetFieldID(cls, "appContext", "Landroid/content/Context;");
    if (contextId) {
        jobject context = env->GetObjectField(cfg, contextId);
        std::string workerError;
        if (!bl::csharp::bindMonoWorkerContext(env, context, workerError))
            BL_WARN("csharp: worker isolado indisponivel: %s", workerError.c_str());
        if (context) env->DeleteLocalRef(context);
    }
    c.showErrors = env->GetBooleanField(cfg, env->GetFieldID(cls, "showErrors", "Z"));
    c.verboseLog = env->GetBooleanField(cfg, env->GetFieldID(cls, "verboseLog", "Z"));
    c.gameVersion = env->GetLongField(cfg, env->GetFieldID(cls, "gameVersion", "J"));
    c.fastBoot   = env->GetBooleanField(cfg, env->GetFieldID(cls, "fastBoot", "Z"));
    c.quickPlayer = readString(env, cfg, cls, "quickPlayer");
    c.quickWorld  = readString(env, cfg, cls, "quickWorld");
    c.joinAddress = readString(env, cfg, cls, "joinAddress");
    c.joinPort    = env->GetIntField(cfg, env->GetFieldID(cls, "joinPort", "I"));
    c.joinPassword = readString(env, cfg, cls, "joinPassword");
    c.joinPlayer  = readString(env, cfg, cls, "joinPlayer");

    auto arr = reinterpret_cast<jobjectArray>(
        env->GetObjectField(cfg, env->GetFieldID(cls, "enabledMods", "[Ljava/lang/String;")));
    if (arr) {
        jsize count = env->GetArrayLength(arr);
        c.enabledMods.clear();
        for (jsize i = 0; i < count; ++i) {
            auto js = reinterpret_cast<jstring>(env->GetObjectArrayElement(arr, i));
            const char* chars = env->GetStringUTFChars(js, nullptr);
            if (chars) {
                c.enabledMods.push_back(bl::parseModSpec(chars));
                env->ReleaseStringUTFChars(js, chars);
            }
            env->DeleteLocalRef(js);
        }
    }

    bl::log::setVerbose(c.verboseLog);
    bl::log::open(c.logPath.c_str());
    BL_INFO("Bunny Loader iniciando, versao do jogo %lld (log %s)", (long long)c.gameVersion,
            c.verboseLog ? "detalhado" : "resumido; detalhes em Configuracoes > Log detalhado");

    if (!bl::loader::installWatcher()) {
        g_lastError = "falha ao instalar o watcher de il2cpp_init";
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_bunnyloader_nativebridge_NativeBridge_lastError(JNIEnv* env, jobject) {
    return env->NewStringUTF(g_lastError.c_str());
}

// Called from a background teardown thread after Unity has stopped, never from a frame hook.
extern "C" JNIEXPORT jboolean JNICALL
Java_dev_bunnyloader_nativebridge_NativeBridge_shutdownCSharpContentNative(JNIEnv*, jobject) {
    return bl::csharp::shutdownManagedContent(10000) ? JNI_TRUE : JNI_FALSE;
}
