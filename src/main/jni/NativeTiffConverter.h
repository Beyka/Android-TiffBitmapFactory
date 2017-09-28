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
#include "TiffToBmpConverter.h"
#include "PngToTiffConverter.h"
#include "JpgToTiffConverter.h"
#include "BmpToTiffConverter.h"
#include "BitmapReader.h"

#ifdef NDEBUG
    #define LOGI(x)
    #define LOGII(x, y)
    #define LOGIF(x, y)
    #define LOGIS(x, y)
    #define LOGE(x)
    #define LOGES(x, y)
#else
    #define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffConverter", "%s", x)
    #define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffConverter", "%s %d", x, y)
    #define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffConverter", "%s %f", x, y)
    #define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeTiffConverter", "%s %s", x, y)
    #define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "NativeTiffConverter", "%s", x)
    #define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "NativeTiffConverter", "%s %s", x, y)
#endif

#ifndef TIFFSAMPLE_NATIVETIFFCONVERTER_H
#define TIFFSAMPLE_NATIVETIFFCONVERTER_H

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffPng
  (JNIEnv *, jclass, jstring, jstring, jobject, jobject);

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffJpg
  (JNIEnv *, jclass, jstring, jstring, jobject, jobject);

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffbmp
  (JNIEnv *, jclass, jstring, jstring, jobject, jobject);

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertPngTiff
  (JNIEnv *, jclass, jstring, jstring, jobject, jobject);

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertJpgTiff
  (JNIEnv *, jclass, jstring, jstring, jobject, jobject);

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertBmpTiff
  (JNIEnv *, jclass, jstring, jstring, jobject, jobject);

JNIEXPORT jobject JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_readBmp
  (JNIEnv *, jclass, jstring, jstring, jobject, jobject);

   // .jpg:  FF D8 FF
   // .png:  89 50 4E 47 0D 0A 1A 0A
   // .gif:  GIF87a
   //        GIF89a
   // .tiff: 49 49 2A 00
   //        4D 4D 00 2A
   // .bmp:  BM
   // .webp: RIFF ???? WEBP
   // .ico   00 00 01 00
   //        00 00 02 00 ( cursor files )
JNIEXPORT jobject JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_getImageType
  (JNIEnv *, jclass, jstring);

//constants for check files
const jint IMAGE_FILE_INVALID = 0;
const jint IMAGE_FILE_JPG = 1;
const jint IMAGE_FILE_PNG = 2;
const jint IMAGE_FILE_GIF = 3;
const jint IMAGE_FILE_TIFF = 4;
const jint IMAGE_FILE_BMP = 5;
const jint IMAGE_FILE_WEBP = 6;
const jint IMAGE_FILE_ICO = 7;

#ifdef __cplusplus
}
#endif
#endif //TIFFSAMPLE_NATIVETIFFCONVERTER_H
