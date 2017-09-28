//
// Created by beyka on 5/15/17.
//

#ifndef TIFFSAMPLE_JPGTOTIFFCONVERTER_H
#define TIFFSAMPLE_JPGTOTIFFCONVERTER_H

#include <stdlib.h>
#include <stdio.h>
#include <tiffio.h>
#include "fcntl.h"
#include "unistd.h"
#include <jpeglib.h>
#include <setjmp.h>
#include "BaseTiffConverter.h"

#ifdef NDEBUG
    #define LOGI(x)
    #define LOGII(x, y)
    #define LOGIF(x, y)
    #define LOGIS(x, y)
    #define LOGE(x)
    #define LOGES(x, y)
    #define LOGEI(x, y);
#else
    #define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "JpgToTiffConverter", "%s", x)
    #define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "JpgToTiffConverter", "%s %d", x, y)
    #define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "JpgToTiffConverter", "%s %f", x, y)
    #define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "JpgToTiffConverter", "%s %s", x, y)
    #define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "JpgToTiffConverter", "%s", x)
    #define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "JpgToTiffConverter", "%s %s", x, y)
    #define LOGEI(x, y) __android_log_print(ANDROID_LOG_ERROR, "JpgToTiffConverter", "%s %d", x, y)
#endif

class JpgToTiffConverter : public BaseTiffConverter
{
    public:
        explicit JpgToTiffConverter(JNIEnv *, jclass, jstring, jstring, jobject, jobject);
        ~JpgToTiffConverter();
        virtual jboolean convert();

    private:
        TIFF *tiffImage;
        FILE *inFile;

        int componentsPerPixel;

        char jpeg_struct_init;
        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;

        //METHODDEF(void) JpgToTiffConverter::my_error_exit (j_common_ptr)
        unsigned char * convertArgbToBilevel(unsigned char *, int, uint32, uint32);

};


#endif //TIFFSAMPLE_JPGTOTIFFCONVERTER_H
