//
// Created by beyka on 5/9/17.
//
#include <jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <stdio.h>

#include "jpeglib.h"
#include <setjmp.h>

#include "TiffToPngConverter.h"
#include "TiffToJpgConverter.h"


#define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffConverter", "%s", x)
#define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffConverter", "%s %d", x, y)
#define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffConverter", "%s %f", x, y)
#define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffConverter", "%s %s", x, y)

#define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "NativeTiffConverter", "%s", x)
#define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "NativeTiffConverter", "%s %s", x, y)

#ifndef TIFFSAMPLE_NATIVETIFFCONVERTER_H
#define TIFFSAMPLE_NATIVETIFFCONVERTER_H

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffPng
  (JNIEnv *, jclass, jstring, jstring);

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffJpg
  (JNIEnv *, jclass, jstring, jstring);

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertPngTiff
  (JNIEnv *, jclass, jstring, jstring);

#ifdef __cplusplus
}
#endif
#endif //TIFFSAMPLE_NATIVETIFFCONVERTER_H
