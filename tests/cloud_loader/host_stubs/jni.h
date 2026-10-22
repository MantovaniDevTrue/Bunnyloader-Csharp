#pragma once

// Linux CI-only JNI declarations for syntax-checking CSharp game update.
// The Android Gradle/NDK build uses its actual jni.h, not this test shim.
struct _JNIEnv;
using JNIEnv = _JNIEnv;
struct _jobject;
using jobject = _jobject*;
