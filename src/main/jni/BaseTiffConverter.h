//
// Created by beyka on 5/11/17.
//

#ifndef TIFFSAMPLE_BASETIFFCONVERTER_H
#define TIFFSAMPLE_BASETIFFCONVERTER_H

#include <stdlib.h>
#include <stdio.h>
#include <tiffio.h>
#include "NativeExceptions.h"
#include <jni.h>
#include <android/log.h>
#include "unistd.h"
#include <ctime>

#ifdef NDEBUG
    #define LOGI(x)
    #define LOGII(x, y)
    #define LOGIF(x, y)
    #define LOGIS(x, y)
    #define LOGE(x)
    #define LOGES(x, y)
#else
    #define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s", x)
    #define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s %d", x, y)
    #define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s %f", x, y)
    #define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s %s", x, y)
    #define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "BaseTiffConverter", "%s", x)
    #define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "BaseTiffConverter", "%s %s", x, y)
#endif


class BaseTiffConverter {
    public:
        explicit BaseTiffConverter(JNIEnv *, jclass, jstring, jstring, jobject, jobject);
        ~BaseTiffConverter();
        virtual jboolean convert() = 0;

    protected:
        static const int colorMask = 0xFF;

        jboolean conversion_result = JNI_FALSE;

        JNIEnv *env;
        jstring inPath;
        jstring outPath;
        jobject optionsObj = NULL;
        jobject listener = NULL;
        jclass jConvertOptionsClass = NULL;
        jclass jIProgressListenerClass = NULL;
        jclass jThreadClass = NULL;

        uint32 width;
        uint32 height;
        uint32 outWidth;
        uint32 outHeight;
        uint32 outStartX;
        uint32 outStartY;

        jlong availableMemory;
        jboolean throwException;

        jint tiffDirectory;

        jboolean appendTiff;
        jint compressionInt;
        jint orientationInt;
        uint16 resUnit;
        float xRes;
        float yRes;
        jstring description;
        const char *cdescription = NULL;
        jstring software;
        const char *csoftware = NULL;
        int boundX;
        int boundY;
        int boundWidth;
        int boundHeight;
        char hasBounds;

        void readOptions();
        char normalizeDecodeArea();
        char *getCreationDate();
        void sendProgress(jlong, jlong);
        jboolean checkStop();
        void rotateTileLinesVertical(uint32, uint32, uint32*, uint32*);
        void rotateTileLinesHorizontal(uint32, uint32, uint32*, uint32*);
        void normalizeTile(uint32, uint32, uint32*);

};

#endif //TIFFSAMPLE_BASETOTIFFCONVERTER_H
