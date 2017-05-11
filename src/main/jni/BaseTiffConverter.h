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

#define LOGI(x) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s", x)
#define LOGII(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s %d", x, y)
#define LOGIF(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s %f", x, y)
#define LOGIS(x, y) __android_log_print(ANDROID_LOG_DEBUG, "BaseTiffConverter", "%s %s", x, y)

#define LOGE(x) __android_log_print(ANDROID_LOG_ERROR, "BaseTiffConverter", "%s", x)
#define LOGES(x, y) __android_log_print(ANDROID_LOG_ERROR, "BaseTiffConverter", "%s %s", x, y)

class BaseTiffConverter {
    public:
        explicit BaseTiffConverter(JNIEnv *, jclass, jstring, jstring, jobject);
        ~BaseTiffConverter();
        virtual jboolean convert() = 0;

    protected:
        JNIEnv *env;
        jstring inPath;
        jstring outPath;
        jobject optionsObj;

        uint32 width;
        uint32 height;

        jint tiffDirectory;
        jlong availableMemory;
        jboolean throwException;
        jboolean appendTiff;

        void readOptions();
};

#endif //TIFFSAMPLE_BASETOTIFFCONVERTER_H
