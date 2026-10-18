#pragma once

#include <jni.h>
#include <string>

namespace bl::csharp {

bool bindMonoWorkerContext(JNIEnv* env, jobject appContext, std::string& error);
bool requestIsolatedMonoProbe(std::string& error);
bool requestIsolatedMonoCompile(const std::string& sourceRoot, std::string& error);
bool requestIsolatedMonoCompile(const std::string& sourceRoot,
                               const std::string& modId, std::string& error);
std::string historicalMonoWorkerExit(int pid, long long minimumTimestampMs);
std::string historicalMonoWorkerTrace(int pid, long long minimumTimestampMs);

}
