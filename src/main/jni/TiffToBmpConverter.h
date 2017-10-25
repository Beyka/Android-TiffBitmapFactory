//
// Created by beyka on 9/22/17.
//

#ifndef TIFFSAMPLE_TIFFTOBMPCONVERTER_H
#define TIFFSAMPLE_TIFFTOBMPCONVERTER_H

#include <stdlib.h>
#include <stdio.h>
#include <tiffio.h>
#include "fcntl.h"
#include "unistd.h"
#include "jpeglib.h"
#include <setjmp.h>
#include "BaseTiffConverter.h"
#include "BMP.h"

#ifdef NDEBUG
    #define LOGI(x)
    #define LOGII(x, y)
    #define LOGIF(x, y)
    #define LOGIS(x, y)
    #define LOGE(x)
    #define LOGES(x, y)
    #define LOGEI(x, y)
#else
    #define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "TiffToBmpConverter", "%s", x)
    #define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "TiffToBmpConverter", "%s %d", x, y)
    #define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "TiffToBmpConverter", "%s %f", x, y)
    #define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "TiffToBmpConverter", "%s %s", x, y)
    #define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "TiffToBmpConverter", "%s", x)
    #define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "TiffToBmpConverter", "%s %s", x, y)
    #define LOGEI(x, y) __android_log_print(ANDROID_LOG_ERROR, "TiffToBmpConverter", "%s %d", x, y)
#endif

class TiffToBmpConverter : public BaseTiffConverter
{
    public:
        explicit TiffToBmpConverter(JNIEnv *, jclass, jstring, jstring, jobject, jobject);
        ~TiffToBmpConverter();
        virtual jboolean convert();

    private:
        static int const DECODE_METHOD_IMAGE = 1;
        static int const DECODE_METHOD_TILE = 2;
        static int const DECODE_METHOD_STRIP = 3;

        int getDecodeMethod();
        jboolean convertFromImage();
        jboolean convertFromTile();
        jboolean convertFromStrip();
        //jboolean convertFromImageWithBounds();
        //jboolean convertFromTileWithBounds();
        //jboolean convertFromStripWithBounds();

        TIFF *tiffImage;
        short origorientation;
        FILE *outFIle = NULL;

        BITMAPFILEHEADER *bmpHeader;
        BITMAPINFOHEADER *bmpInfo;



};

#endif //TIFFSAMPLE_TIFFTOBMPCONVERTER_H
