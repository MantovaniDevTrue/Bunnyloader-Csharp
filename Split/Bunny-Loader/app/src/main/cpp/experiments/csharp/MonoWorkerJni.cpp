#include "experiments/csharp/ManagedRuntime.h"

#include <jni.h>
#include <string>

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
