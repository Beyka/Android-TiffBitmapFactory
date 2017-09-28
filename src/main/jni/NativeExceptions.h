//
// Created by alexeyba on 09.11.15.
//

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TIFFEXAMPLE_NATIVEEXCEPTIONS_H
#define TIFFEXAMPLE_NATIVEEXCEPTIONS_H



#include <jni.h>
#include <android/log.h>

#ifdef NDEBUG
    #define LOGI(x)
    #define LOGII(x, y)
    #define LOGIS(x, y)
#else
    #define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "NativeDecoder", "%s", x)
    #define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeDecoder", "%s %d", x, y)
    #define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeDecoder", "%s %s", x, y)
#endif

void throw_not_enought_memory_exception(JNIEnv *, int, int);
void throw_decode_file_exception(JNIEnv *, jstring, jstring);
void throw_cant_open_file_exception(JNIEnv *, jstring);

#endif //TIFFEXAMPLE_NATIVEEXCEPTIONS_H

#ifdef __cplusplus
}
#endif



