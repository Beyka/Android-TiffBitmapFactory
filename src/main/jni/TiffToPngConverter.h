//
// Created by beyka on 5/10/17.
//

#ifndef TIFFSAMPLE_TIFFTOPNGCONVERTER_H
#define TIFFSAMPLE_TIFFTOPNGCONVERTER_H

#include <stdlib.h>
#include <stdio.h>
#include <tiffio.h>
#include "fcntl.h"
#include "unistd.h"
#include <png.h>
#include <setjmp.h>
#include "NativeExceptions.h"

#define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "TiffToPngConverter", "%s", x)
#define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "TiffToPngConverter", "%s %d", x, y)
#define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "TiffToPngConverter", "%s %f", x, y)
#define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "TiffToPngConverter", "%s %s", x, y)

#define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "TiffToPngConverter", "%s", x)
#define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "TiffToPngConverter", "%s %s", x, y)

class TiffToPngConverter
{
    public:
        explicit TiffToPngConverter(JNIEnv *, jclass, jstring, jstring, jobject);
        ~TiffToPngConverter();
        jboolean convert();

    private:
        static int const DECODE_METHOD_IMAGE = 1;
        static int const DECODE_METHOD_TILE = 2;
        static int const DECODE_METHOD_STRIP = 3;

        int getDecodeMethod();
        void readOptions();
        void rotateTileLinesVertical(uint32, uint32, uint32 *, uint32 *);
        void rotateTileLinesHorizontal(uint32, uint32, uint32 *, uint32 *);
        jboolean convertFromImage();
        jboolean convertFromTile();
        jboolean convertFromStrip();

        JNIEnv *env;
        jstring inPath;
        jstring outPath;

        TIFF *tiffImage;
        short origorientation;
        FILE *pngFile;
        png_structp png_ptr;
        png_infop info_ptr;

        jobject optionsObj;
        jint tiffDirectory;
        jlong availableMemory;
        jboolean throwException;
        uint32 width;
        uint32 height;


};

#endif //TIFFSAMPLE_TIFFTOPNGCONVERTER_H