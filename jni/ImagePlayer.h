/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *  @author   Tellen Yu
 *  @version  1.0
 *  @date     2015/04/07
 *  @par function description:
 *  - 1 transparent the video player
 */

#define LOG_NDEBUG 0
#define LOG_TAG "SurfaceOverlay-jni"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <nativehelper/JNIHelp.h>
#include <jni.h>
#include "Bitmap.h"
#include <SkAndroidCodec.h>
#include <SkBitmap.h>
#include <SkStream.h>
#include <SkCanvas.h>
#include <SkData.h>
#include <SkSurface.h>
#include <utils/Log.h>
#include <utils/KeyedVector.h>
#include <android_runtime/AndroidRuntime.h>
#include <android_runtime/android_view_Surface.h>
#include <nativehelper/scoped_local_ref.h>
#include <nativehelper/scoped_utf_chars.h>
#include <android/native_window.h>
#include <gui/Surface.h>
#include <gui/IGraphicBufferProducer.h>
#include <ui/GraphicBuffer.h>
#include <hardware/gralloc1.h>
#include <cutils/ashmem.h>


#ifndef CORE_JNI_HELPERS
#define CORE_JNI_HELPERS

// Host targets (layoutlib) do not differentiate between regular and critical native methods,
// and they need all the JNI methods to have JNIEnv* and jclass/jobject as their first two arguments.
// The following macro allows to have those arguments when compiling for host while omitting them when
// compiling for Android.
#ifdef __ANDROID__
#define CRITICAL_JNI_PARAMS
#define CRITICAL_JNI_PARAMS_COMMA
#else
#define CRITICAL_JNI_PARAMS JNIEnv*, jclass
#define CRITICAL_JNI_PARAMS_COMMA JNIEnv*, jclass,
#endif

#define CHECK assert
#define CHECK_EQ(a,b) CHECK((a)==(b))
namespace android {

// Defines some helpful functions.

static inline jclass FindClassOrDie(JNIEnv* env, const char* class_name) {
    jclass clazz = env->FindClass(class_name);
    LOG_ALWAYS_FATAL_IF(clazz == NULL, "Unable to find class %s", class_name);
    return clazz;
}

static inline jfieldID GetFieldIDOrDie(JNIEnv* env, jclass clazz, const char* field_name,
                                       const char* field_signature) {
    jfieldID res = env->GetFieldID(clazz, field_name, field_signature);
    LOG_ALWAYS_FATAL_IF(res == NULL, "Unable to find static field %s with signature %s", field_name,
                        field_signature);
    return res;
}

static inline jmethodID GetMethodIDOrDie(JNIEnv* env, jclass clazz, const char* method_name,
                                         const char* method_signature) {
    jmethodID res = env->GetMethodID(clazz, method_name, method_signature);
    LOG_ALWAYS_FATAL_IF(res == NULL, "Unable to find method %s with signature %s", method_name,
                        method_signature);
    return res;
}

static inline jfieldID GetStaticFieldIDOrDie(JNIEnv* env, jclass clazz, const char* field_name,
                                             const char* field_signature) {
    jfieldID res = env->GetStaticFieldID(clazz, field_name, field_signature);
    LOG_ALWAYS_FATAL_IF(res == NULL, "Unable to find static field %s with signature %s", field_name,
                        field_signature);
    return res;
}

static inline jmethodID GetStaticMethodIDOrDie(JNIEnv* env, jclass clazz, const char* method_name,
                                               const char* method_signature) {
    jmethodID res = env->GetStaticMethodID(clazz, method_name, method_signature);
    LOG_ALWAYS_FATAL_IF(res == NULL, "Unable to find static method %s with signature %s",
                        method_name, method_signature);
    return res;
}

template <typename T>
static inline T MakeGlobalRefOrDie(JNIEnv* env, T in) {
    jobject res = env->NewGlobalRef(in);
    LOG_ALWAYS_FATAL_IF(res == NULL, "Unable to create global reference.");
    return static_cast<T>(res);
}

static inline int RegisterMethodsOrDie(JNIEnv* env, const char* className,
                                       const JNINativeMethod* gMethods, int numMethods) {
    int res = AndroidRuntime::registerNativeMethods(env, className, gMethods, numMethods);
    LOG_ALWAYS_FATAL_IF(res < 0, "Unable to register native methods.");
    return res;
}

/**
 * Read the specified field from jobject, and convert to std::string.
 * If the field cannot be obtained, return defaultValue.
 */
static inline std::string getStringField(JNIEnv* env, jobject obj, jfieldID fieldId,
        const char* defaultValue) {
    ScopedLocalRef<jstring> strObj(env, jstring(env->GetObjectField(obj, fieldId)));
    if (strObj != nullptr) {
        ScopedUtfChars chars(env, strObj.get());
        return std::string(chars.c_str());
    }
    return std::string(defaultValue);
}

}  // namespace android

#endif  // CORE_JNI_HELPERS


