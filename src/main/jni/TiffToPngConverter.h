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
#include "BaseTiffConverter.h"

#ifdef NDEBUG
    #define LOGI(x)
    #define LOGII(x, y)
    #define LOGIF(x, y)
    #define LOGIS(x, y)
    #define LOGE(x)
    #define LOGES(x, y)
    #define LOGEI(x, y)
#else
    #define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "TiffToPngConverter", "%s", x)
    #define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "TiffToPngConverter", "%s %d", x, y)
    #define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "TiffToPngConverter", "%s %f", x, y)
    #define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "TiffToPngConverter", "%s %s", x, y)
    #define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "TiffToPngConverter", "%s", x)
    #define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "TiffToPngConverter", "%s %s", x, y)
    #define LOGEI(x, y) __android_log_print(ANDROID_LOG_ERROR, "TiffToPngConverter", "%s %d", x, y)
#endif

class TiffToPngConverter : public BaseTiffConverter
{
    public:
        explicit TiffToPngConverter(JNIEnv *, jclass, jstring, jstring, jobject, jobject);
        ~TiffToPngConverter();
        virtual jboolean convert();

    private:
        static int const DECODE_METHOD_IMAGE = 1;
        static int const DECODE_METHOD_TILE = 2;
        static int const DECODE_METHOD_STRIP = 3;

        int getDecodeMethod();
        jboolean convertFromImage();
        jboolean convertFromTile();
        jboolean convertFromStrip();


        TIFF *tiffImage;
        short origorientation;
        FILE *pngFile = NULL;;
        char png_ptr_init;
        png_structp png_ptr;
        char png_info_init;
        png_infop info_ptr;

};

#endif //TIFFSAMPLE_TIFFTOPNGCONVERTER_H