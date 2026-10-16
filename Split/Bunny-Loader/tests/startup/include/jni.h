#pragma once
// Declaration-only JNI fixture; this test never invokes JNI or a Java VM.
struct JNIEnv;
using jobject=void*;
