//
// Created by beyka on 9/21/17.
//

#ifndef TIFFSAMPLE_BITMAPREADER_H
#define TIFFSAMPLE_BITMAPREADER_H

#include <stdlib.h>
#include <stdio.h>
#include <tiffio.h>
#include "NativeExceptions.h"
#include <jni.h>
#include <android/log.h>
#include "unistd.h"
#include <ctime>

#ifdef NDEBUG
    #define LOGI(x)
    #define LOGII(x, y)
    #define LOGIF(x, y)
    #define LOGIS(x, y)
    #define LOGE(x)
    #define LOGES(x, y)
#else
    #define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "BMPReader", "%s", x)
    #define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BMPReader", "%s %d", x, y)
    #define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BMPReader", "%s %f", x, y)
    #define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BMPReader", "%s %s", x, y)
    #define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "BMPReader", "%s", x)
    #define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "BMPReader", "%s %s", x, y)
#endif

#ifdef __cplusplus
extern "C" {
#endif

jobject readBmp
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring pngPath, jobject options, jobject listener);

#ifdef __cplusplus
}
#endif
#endif //TIFFSAMPLE_BITMAPREADER_H
