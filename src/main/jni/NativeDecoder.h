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
#define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeDecoder", "%s %f", x, y)
#define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "NativeDecoder", "%s %s", x, y)

#define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "NativeDecoder", "%s", x)
#define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "NativeDecoder", "%s %s", x, y)

class NativeDecoder
{
    public:
        explicit NativeDecoder(JNIEnv *, jclass, jstring, jobject, jobject);
        ~NativeDecoder();
        jobject getBitmap();

    private:
        //constants
        static int const colorMask = 0xFF;
        static int const ARGB_8888 = 2;
        static int const RGB_565 = 4;
        static int const ALPHA_8 = 8;

        static int const DECODE_METHOD_IMAGE = 1;
        static int const DECODE_METHOD_TILE = 2;
        static int const DECODE_METHOD_STRIP = 3;
        //fields
        JNIEnv *env;
        jclass clazz;
        jobject optionsObject;
        jobject listenerObject;
        jclass jIProgressListenerClass;
        jclass jBitmapOptionsClass;
        jstring jPath;
        jboolean throwException;
        jboolean useOrientationTag;
        TIFF *image;
        jlong progressTotal;
        int origwidth;
        int origheight;
        short origorientation;
        int origcompressionscheme;
        jobject preferedConfig;
        jboolean invertRedAndBlue;
        jint boundX;
        jint boundY;
        jint boundWidth;
        jint boundHeight;
        char hasBounds;
        unsigned long availableMemory;
        //methods
        int getDyrectoryCount();
        void writeDataToOptions(int);
        jobject createBitmap(int, int);
        jint *getSampledRasterFromImage(int, int *, int *);
        jint *getSampledRasterFromImageWithBounds(int , int *, int *);
        jint *getSampledRasterFromStrip(int, int *, int *);
        jint *getSampledRasterFromStripWithBounds(int, int *, int *);
        void rotateTileLinesVertical(uint32, uint32, uint32 *, uint32 *);
        void rotateTileLinesHorizontal(uint32, uint32, uint32 *, uint32 *);
        void flipPixelsVertical(uint32, uint32, jint *);
        void flipPixelsHorizontal(uint32, uint32, jint *);
        jint *getSampledRasterFromTile(int, int *, int *);
        jint *getSampledRasterFromTileWithBounds(int, int *, int *);
        int getDecodeMethod();
        void fixOrientation(jint *, uint32, int, int);
        void rotateRaster(jint *, int, int *, int *);
        jbyte * createBitmapAlpha8(jint *, int, int);
        unsigned short *createBitmapRGB565(jint *, int, int);
        jstring charsToJString(char *);
        jboolean checkStop();
        void sendProgress(jlong, jlong);
};

#endif //TIFFSAMPLE_NATIVEDECODER_H
