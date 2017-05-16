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

#define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s", x)
#define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s %d", x, y)
#define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s %f", x, y)
#define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s %s", x, y)

#define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "BaseTiffConverter", "%s", x)
#define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "BaseTiffConverter", "%s %s", x, y)

class BaseTiffConverter {
    public:
        explicit BaseTiffConverter(JNIEnv *, jclass, jstring, jstring, jobject, jobject);
        ~BaseTiffConverter();
        virtual jboolean convert() = 0;

    protected:
        const int colorMask = 0xFF;

        jboolean conversion_result = JNI_FALSE;

        JNIEnv *env;
        jstring inPath;
        jstring outPath;
        jobject optionsObj = NULL;
        jobject listener = NULL;
        jclass jConvertOptionsClass = NULL;
        jclass jIProgressListenerClass = NULL;

        uint32 width;
        uint32 height;

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

        void readOptions();
        char *getCreationDate();
        void sendProgress(jlong, jlong);
        jboolean checkStop();
};

#endif //TIFFSAMPLE_BASETOTIFFCONVERTER_H
