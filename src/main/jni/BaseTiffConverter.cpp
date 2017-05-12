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
    appendTiff = JNI_FALSE;
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
    jlong am = env->GetLongField(optionsObj, availablememfield);
    if (am > 0) availableMemory = am;

    jfieldID throwexceptionsfield = env->GetFieldID(optionsClass, "throwExceptions", "Z");
    throwException = env->GetBooleanField(optionsObj, throwexceptionsfield);

    jfieldID appentifffield = env->GetFieldID(optionsClass, "appendTiff", "Z");
    appendTiff = env->GetBooleanField(optionsObj, appentifffield);

    jfieldID gOptions_CompressionModeFieldID = env->GetFieldID(optionsClass,
            "compressionScheme",
            "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
    jobject compressionMode = env->GetObjectField(optionsObj, gOptions_CompressionModeFieldID);
    jclass compressionModeClass = env->FindClass("org/beyka/tiffbitmapfactory/CompressionScheme");
    jfieldID ordinalFieldID = env->GetFieldID(compressionModeClass, "ordinal", "I");
    jint ci = env->GetIntField(compressionMode, ordinalFieldID);
    LOGII("ci", ci);
    //check is compression scheme is right. if not - set default LZW
    if (ci == 1 || ci == 3 || ci == 4 || ci == 5 || ci == 7 || ci == 8 || ci == 32773 || ci == 32946)
        compressionInt = ci;
    else
        compressionInt = 5;

    LOGII("compressionInt ", compressionInt );
    env->DeleteLocalRef(compressionModeClass);

    env->DeleteLocalRef(optionsClass);
}

unsigned char * BaseTiffConverter::convertArgbToBilevel(uint32 *source, jint width, jint height) {
        long long threshold = 0;
        uint32 crPix;
        uint32 grayPix;
        int bilevelWidth = (width / 8 + 0.5);

        unsigned char *dest = (unsigned char *) malloc(sizeof(unsigned char) * bilevelWidth * height);

        for (int i = 0; i < width; i++) {
            for (int j = 0; j < height; j++) {
                crPix = source[j * width + i];
                grayPix = (0.2125 * (colorMask & crPix >> 16) + 0.7154 * (colorMask & crPix >> 8) + 0.0721 * (colorMask & crPix));
                threshold += grayPix;
            }
        }

        uint32 shift = 0;
        unsigned char charsum = 0;
        int k = 7;
        long long barier = threshold / (width * height);
        for (int j = 0; j < height; j++) {
            shift = 0;
            charsum = 0;
            k = 7;
            for (int i = 0; i < width; i++) {
                crPix = source[j * width + i];
                grayPix = (0.2125 * (colorMask & crPix >> 16) + 0.7154 * (colorMask & crPix >> 8) + 0.0721 * (colorMask & crPix));

                if (grayPix < barier) charsum &= ~(1 << k);
                else charsum |= 1 << k;

                if (k == 0) {
                    dest[j * bilevelWidth + shift] = charsum;
                    shift++;
                    k = 7;
                    charsum = 0;
                } else {
                    k--;
                }
            }
        }
        return dest;
}
