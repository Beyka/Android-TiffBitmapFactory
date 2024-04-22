#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <stdlib.h>
#include <stdio.h>
#include <tiffio.h>
#include "fcntl.h"
#include "unistd.h"
#include <ctime>
#include "string.h"
#include "NativeExceptions.h"

#ifdef NDEBUG
    #define LOGI(x)
    #define LOGII(x, y)
    #define LOGIF(x, y)
    #define LOGIS(x, y)
    #define LOGE(x)
    #define LOGES(x, y)
#else
    #define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffSaver", "%s", x)
    #define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffSaver", "%s %d", x, y)
    #define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffSaver", "%s %f", x, y)
    #define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffSaver", "%s %s", x, y)
    #define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "NativeTiffSaver", "%s", x)
    #define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "NativeTiffSaver", "%s %s", x, y)
#endif

#ifndef _Included_org_beyka_tiffbitmapfactory_TiffSaver
#define _Included_org_beyka_tiffbitmapfactory_TiffSaver
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     org_beyka_tiffbitmapfactory_TiffSaver
 * Method:    save
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffSaver_save
  (JNIEnv *, jclass, jstring, jint, jobject, jobject, jboolean);

JNIEXPORT jobject JNICALL Java_org_beyka_tiffbitmapfactory_TiffSaver_nativeCloseFd
        (JNIEnv *, jclass, jint);

unsigned char *convertArgbToBilevel(uint32 *, jint, jint);

char *getCreationDate();

char *concat(const char *, const char *);

#ifdef __cplusplus
}
#endif
#endif
