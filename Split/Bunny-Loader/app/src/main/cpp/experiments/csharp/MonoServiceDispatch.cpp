#include "experiments/csharp/MonoServiceDispatch.h"

#include "core/Config.h"

#include <cstdio>
#include <cstdint>
#include <mutex>
#include <string>

namespace bl::csharp {
namespace {

std::mutex g_mutex;
JavaVM* g_vm = nullptr;
jobject g_context = nullptr;
jclass g_serviceClass = nullptr;

bool checkException(JNIEnv* env, std::string& error, const char* operation) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    error = operation;
    return true;
}

}

bool bindMonoWorkerContext(JNIEnv* env, jobject context, std::string& error) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!context) {
        error = "application context ausente";
        return false;
    }
    if (g_context && g_serviceClass && g_vm) return true;
    if (env->GetJavaVM(&g_vm) != JNI_OK) {
        error = "GetJavaVM falhou";
        return false;
    }

    jclass klass = env->FindClass("dev/bunnyloader/csharp/ManagedProbeService");
    if (checkException(env, error, "ManagedProbeService nao encontrado") || !klass) return false;
    g_serviceClass = reinterpret_cast<jclass>(env->NewGlobalRef(klass));
    env->DeleteLocalRef(klass);
    g_context = env->NewGlobalRef(context);
    if (!g_context || !g_serviceClass) {
        error = "NewGlobalRef falhou";
        return false;
    }
    return true;
}

bool dispatchMonoWorker(std::string& error, const std::string& sourceRoot,
                        const std::string& modId) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_vm || !g_context || !g_serviceClass) {
        error = "worker nao inicializado";
        return false;
    }

    JNIEnv* env = nullptr;
    bool attached = false;
    const jint state = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (state == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            error = "AttachCurrentThread falhou";
            return false;
        }
        attached = true;
    } else if (state != JNI_OK) {
        error = "GetEnv falhou";
        return false;
    }

    bool success = false;
    jclass intentClass = env->FindClass("android/content/Intent");
    if (!checkException(env, error, "Intent class indisponivel") && intentClass) {
        jmethodID ctor = env->GetMethodID(
            intentClass, "<init>", "(Landroid/content/Context;Ljava/lang/Class;)V");
        if (!checkException(env, error, "Intent constructor indisponivel") && ctor) {
            jobject intent = env->NewObject(intentClass, ctor, g_context, g_serviceClass);
            if (!checkException(env, error, "Intent creation falhou") && intent) {
                bool extrasReady = true;
                if (!sourceRoot.empty()) {
                    jmethodID putExtra = env->GetMethodID(intentClass, "putExtra",
                        "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;");
                    if (checkException(env, error, "Intent.putExtra indisponivel") || !putExtra) {
                        extrasReady = false;
                    } else {
                        jstring key = env->NewStringUTF("sourceRoot");
                        jstring value = env->NewStringUTF(sourceRoot.c_str());
                        if (!key || !value) {
                            extrasReady = false;
                            error = "Intent extras sem memoria";
                        } else {
                            jobject updated = env->CallObjectMethod(intent, putExtra, key, value);
                            if (checkException(env, error, "Intent.putExtra falhou") || !updated)
                                extrasReady = false;
                            if (updated) env->DeleteLocalRef(updated);
                        }
                        if (key) env->DeleteLocalRef(key);
                        if (value) env->DeleteLocalRef(value);
                        if (extrasReady && !modId.empty()) {
                            jstring ownerKey = env->NewStringUTF("sourceModId");
                            jstring ownerValue = env->NewStringUTF(modId.c_str());
                            if (!ownerKey || !ownerValue) {
                                extrasReady = false;
                                error = "Identidade do mod C# sem memoria";
                            } else {
                                jobject updatedOwner = env->CallObjectMethod(
                                    intent, putExtra, ownerKey, ownerValue);
                                if (checkException(env, error, "Identidade do mod no worker falhou") ||
                                    !updatedOwner)
                                    extrasReady = false;
                                if (updatedOwner) env->DeleteLocalRef(updatedOwner);
                            }
                            if (ownerKey) env->DeleteLocalRef(ownerKey);
                            if (ownerValue) env->DeleteLocalRef(ownerValue);
                        }
                    }
                }
                jclass contextClass = extrasReady
                    ? env->FindClass("android/content/Context") : nullptr;
                if (extrasReady &&
                    !checkException(env, error, "Context class indisponivel") && contextClass) {
                    jmethodID start = env->GetMethodID(
                        contextClass, "startService",
                        "(Landroid/content/Intent;)Landroid/content/ComponentName;");
                    if (!checkException(env, error, "Context.startService indisponivel") && start) {
                        jobject component = nullptr;
                        if (!sourceRoot.empty()) {
                            jmethodID leasedStart = env->GetStaticMethodID(g_serviceClass,
                                "startCompilerRequest",
                                "(Landroid/content/Context;Landroid/content/Intent;)Landroid/content/ComponentName;");
                            if (!checkException(env, error, "Compiler process lease API indisponivel") && leasedStart)
                                component = env->CallStaticObjectMethod(g_serviceClass, leasedStart, g_context, intent);
                        } else {
                            component = env->CallObjectMethod(g_context, start, intent);
                        }
                        if (!checkException(env, error, "startService lancou excecao") && component) {
                            success = true;
                            env->DeleteLocalRef(component);
                        } else if (error.empty()) {
                            error = "startService retornou nulo";
                        }
                    }
                    env->DeleteLocalRef(contextClass);
                }
                env->DeleteLocalRef(intent);
            }
        }
        env->DeleteLocalRef(intentClass);
    }

    if (attached) g_vm->DetachCurrentThread();
    return success;
}


std::string invokeMonoHistoryMethod(const char* methodName, int pid, long long minimumTimestampMs) {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (pid <= 0 || !g_vm || !g_context || !g_serviceClass)
        return "context-unavailable";

    JNIEnv* env = nullptr;
    bool attached = false;
    const jint state = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (state == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
            return "thread-attach-failed";
        attached = true;
    } else if (state != JNI_OK) {
        return "jni-env-unavailable";
    }

    std::string result;
    jmethodID method = env->GetStaticMethodID(g_serviceClass, methodName,
        "(Landroid/content/Context;IJ)Ljava/lang/String;");
    if (env->ExceptionCheck() || !method) {
        env->ExceptionClear();
        result = "exit-method-unavailable";
    } else {
        jobject value = env->CallStaticObjectMethod(g_serviceClass, method,
            g_context, static_cast<jint>(pid), static_cast<jlong>(minimumTimestampMs));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            result = "exit-query-failed";
        } else if (value) {
            const char* raw = env->GetStringUTFChars(static_cast<jstring>(value), nullptr);
            if (raw) {
                result = raw;
                env->ReleaseStringUTFChars(static_cast<jstring>(value), raw);
            }
            env->DeleteLocalRef(value);
        }
    }
    if (attached) g_vm->DetachCurrentThread();
    return result;
}

std::string historicalMonoWorkerExit(int pid, long long minimumTimestampMs) {
    return invokeMonoHistoryMethod("historicalWorkerExit", pid, minimumTimestampMs);
}

std::string historicalMonoWorkerTrace(int pid, long long minimumTimestampMs) {
    return invokeMonoHistoryMethod("historicalWorkerTrace", pid, minimumTimestampMs);
}

bool requestIsolatedMonoProbe(std::string& error) {
    return dispatchMonoWorker(error, "", "");
}

bool requestIsolatedMonoCompile(const std::string& sourceRoot, std::string& error) {
    return requestIsolatedMonoCompile(
        sourceRoot, "57a98f20-7a35-4d19-8ce8-079e25bb36a1", error);
}

bool requestIsolatedMonoCompile(const std::string& sourceRoot,
                               const std::string& modId, std::string& error) {
    if (sourceRoot.empty() || sourceRoot.size() > 2048 ||
        sourceRoot.find("..") != std::string::npos ||
        modId.empty() || modId.size() > 128) {
        error = "Diretorio ou identidade do mod C# invalida";
        return false;
    }
    return dispatchMonoWorker(error, sourceRoot, modId);
}

}
