//
// Created by beyka on 5/11/17.
//

#include "BaseTiffConverter.h"

BaseTiffConverter::BaseTiffConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts)
{
    availableMemory = 8000 * 8000 * 4;
    env = e;
    inPath = in;
    outPath = out;
    optionsObj = opts;
    throwException = JNI_FALSE;
    tiffDirectory = 0;
}

BaseTiffConverter::~BaseTiffConverter()
{
    LOGI("base destructor");
}

void BaseTiffConverter::readOptions()
{
    if (optionsObj == NULL) return;
    jclass optionsClass = env->FindClass("org/beyka/tiffbitmapfactory/TiffConverter$ConverterOptions");

    jfieldID tiffdirfield = env->GetFieldID(optionsClass, "tiffDirectoryRead", "I");
    tiffDirectory = env->GetIntField(optionsObj, tiffdirfield);

    jfieldID availablememfield = env->GetFieldID(optionsClass, "availableMemory", "J");
    availableMemory = env->GetLongField(optionsObj, availablememfield);

    jfieldID throwexceptionsfield = env->GetFieldID(optionsClass, "throwExceptions", "Z");
    throwException = env->GetBooleanField(optionsObj, throwexceptionsfield);

    env->DeleteLocalRef(optionsClass);
}
