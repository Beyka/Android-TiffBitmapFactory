//
// Created by beyka on 3.2.17.
//

#ifndef TIFFSAMPLE_NATIVEDECODER_H
#define TIFFSAMPLE_NATIVEDECODER_H

#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <stdlib.h>
#include <stdio.h>
#include <tiffio.h>
#include "NativeExceptions.h"

#define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "NativeDecoder", "%s", x)
#define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeDecoder", "%s %d", x, y)
#define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeDecoder", "%s %s", x, y)

#define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "NativeDecoder", "%s", x)
#define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "NativeDecoder", "%s %s", x, y)

class NativeDecoder
{
    public:
        explicit NativeDecoder(JNIEnv *, jclass, jstring, jobject);
        ~NativeDecoder();
        jobject getBitmap();

    private:
        //constants
        static int const colorMask = 0xFF;
        static int const ARGB_8888 = 2;
        static int const RGB_565 = 4;
        static int const ALPHA_8 = 8;
        //fields
        JNIEnv *env;
        jclass clazz;
        jobject optionsObject;
        jstring jPath;
        TIFF *image;
        int origwidth;
        int origheight;
        short origorientation;
        int origcompressionscheme;
        jobject preferedConfig;
        jboolean invertRedAndBlue;
        long availableMemory;
        //methods
        int getDyrectoryCount();
        void writeDataToOptions(int);
        jobject createBitmap(int, int);
        jint *getSampledRasterFromImage(int , int *, int *);
        jbyte * createBitmapAlpha8(jint *, int, int);
        unsigned short *createBitmapRGB565(jint *, int, int);
        /*jbyte * createBitmapAlpha8(JNIEnv *, jint *, int, int);
        unsigned short *createBitmapRGB565(JNIEnv *, jint *, int, int);
        jobject createBlankBitmap(JNIEnv *, int width, int height);
        void releaseImage(JNIEnv *);
        jint *getSampledRasterFromImage(JNIEnv *, int , int *, int *);
        jint *getSampledRasterFromStrip(JNIEnv *, int , int *, int *);
        */

};

#endif //TIFFSAMPLE_NATIVEDECODER_H
