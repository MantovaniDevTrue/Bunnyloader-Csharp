#include "experiments/csharp/ManagedRuntime.h"
#include "experiments/csharp/NativeAbortProbe.h"

#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_dev_bunnyloader_csharp_ManagedProbeService_signalHandlerSnapshot(
    JNIEnv* env, jobject) {
    const auto value = bl::csharp::currentNativeAbortHandler();
    return env->NewStringUTF(value.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_bunnyloader_csharp_ManagedProbeService_runNative(
    JNIEnv* env, jobject, jstring assemblyPath, jstring checkpointPath) {
    if (!assemblyPath || !checkpointPath) return env->NewStringUTF("missing-arguments");
    const char* assembly = env->GetStringUTFChars(assemblyPath, nullptr);
    const char* checkpoint = env->GetStringUTFChars(checkpointPath, nullptr);
    const std::string assemblyFile = assembly ? assembly : "";
    const std::string checkpointFile = checkpoint ? checkpoint : "";
    if (assembly) env->ReleaseStringUTFChars(assemblyPath, assembly);
    if (checkpoint) env->ReleaseStringUTFChars(checkpointPath, checkpoint);

    bl::csharp::setManagedCheckpointPath(checkpointFile);
    const auto result = bl::csharp::probeManagedRuntime(assemblyFile);
    const std::string status = "phase=" + result.phase +
        "\ndomainInitialized=" + (result.domainInitialized ? "1" : "0") +
        "\nmethodExecuted=" + (result.methodExecuted ? "1" : "0") +
        "\nresult=" + std::to_string(result.result) +
        "\nerror=" + result.error;
    return env->NewStringUTF(status.c_str());
}

static jstring compileWorkerNative(
    JNIEnv* env, jstring probe, jstring compiler, jstring source,
    jstring output, jstring diagnostic, jstring checkpoint,
    bool precompileOnly, bool verifyOnly) {
    if (!probe || !compiler || !source || !output || !diagnostic || !checkpoint)
        return env->NewStringUTF("result=-1\nphase=missing-arguments\n");
    auto get = [env](jstring value) {
        const char* raw = env->GetStringUTFChars(value, nullptr);
        const std::string result = raw ? raw : "";
        if (raw) env->ReleaseStringUTFChars(value, raw);
        return result;
    };
    const std::string probePath = get(probe);
    const std::string compilerPath = get(compiler);
    const std::string sourceRoot = get(source);
    const std::string outputPath = get(output);
    const std::string diagnosticPath = get(diagnostic);
    bl::csharp::setManagedCheckpointPath(get(checkpoint));
    const auto slash = diagnosticPath.find_last_of('/');
    if (slash != std::string::npos)
        bl::csharp::installNativeAbortProbe(diagnosticPath.substr(0, slash));
    const auto result = bl::csharp::compileManagedSource(
        probePath, compilerPath, sourceRoot, outputPath, diagnosticPath,
        precompileOnly, verifyOnly);
    const std::string status = "result=" + std::to_string(result.result) +
        "\nphase=" + result.phase +
        "\nexecutedTypes=" + std::to_string(result.executedTypes) +
        "\nhookModules=" + std::to_string(result.hookModules) +
        "\nmonoProbeMs=" + std::to_string(result.monoProbeMs) +
        "\nroslynInvokeMs=" + std::to_string(result.roslynInvokeMs) +
        "\nverifyInvokeMs=" + std::to_string(result.verifyInvokeMs) +
        "\nerror=" + result.error;
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_bunnyloader_csharp_ManagedProbeService_compileNative(
    JNIEnv* env, jobject, jstring probe, jstring compiler, jstring source,
    jstring output, jstring diagnostic, jstring checkpoint) {
    return compileWorkerNative(env, probe, compiler, source,
                               output, diagnostic, checkpoint, false, false);
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_bunnyloader_csharp_ManagedProbeService_precompileNative(
    JNIEnv* env, jobject, jstring probe, jstring compiler, jstring source,
    jstring output, jstring diagnostic, jstring checkpoint) {
    return compileWorkerNative(env, probe, compiler, source,
                               output, diagnostic, checkpoint, true, false);
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_bunnyloader_csharp_ManagedProbeService_verifyPrefetchedNative(
    JNIEnv* env, jobject, jstring probe, jstring compiler, jstring source,
    jstring output, jstring diagnostic, jstring checkpoint) {
    return compileWorkerNative(env, probe, compiler, source,
                               output, diagnostic, checkpoint, false, true);
}
