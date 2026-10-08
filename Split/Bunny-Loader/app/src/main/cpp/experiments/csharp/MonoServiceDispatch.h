#pragma once

#include <jni.h>
#include <string>

namespace bl::csharp {

bool bindMonoWorkerContext(JNIEnv* env, jobject appContext, std::string& error);
bool requestIsolatedMonoProbe(std::string& error);

}
