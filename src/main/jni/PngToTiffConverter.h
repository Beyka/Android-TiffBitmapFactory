//
// Created by beyka on 5/12/17.
//

#ifndef TIFFSAMPLE_PNGTOTIFFCONVERTER_H
#define TIFFSAMPLE_PNGTOTIFFCONVERTER_H

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
    #define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "PngToTiffConverter", "%s", x)
    #define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "PngToTiffConverter", "%s %d", x, y)
    #define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "PngToTiffConverter", "%s %f", x, y)
    #define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "PngToTiffConverter", "%s %s", x, y)
    #define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "PngToTiffConverter", "%s", x)
    #define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "PngToTiffConverter", "%s %s", x, y)
    #define LOGEI(x, y) __android_log_print(ANDROID_LOG_ERROR, "PngToTiffConverter", "%s %d", x, y)
#endif

class PngToTiffConverter : public BaseTiffConverter
{
    public:
        explicit PngToTiffConverter(JNIEnv *, jclass, jstring, jstring, jobject, jobject);
        ~PngToTiffConverter();
        virtual jboolean convert();

     private:
         TIFF *tiffImage;
         FILE *inFile;
         char png_ptr_init;
         png_structp png_ptr;
         char png_info_init;
         png_infop info_ptr;

         int bit_depth;
         int color_type;

         //unsigned char * convertArgbToBilevel(png_byte *, int, uint32, uint32);
         unsigned char * convertArgbToBilevel(png_bytep *, int, uint32, uint32);
};

#endif //TIFFSAMPLE_PNGTOTIFFCONVERTER_H
