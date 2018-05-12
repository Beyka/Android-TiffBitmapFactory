//
// Created by beyka on 5/11/17.
//

#include "BaseTiffConverter.h"

BaseTiffConverter::BaseTiffConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts, jobject listener)
{
    boundX = boundY = boundWidth = boundHeight = -1;
    hasBounds = 0;
    availableMemory = 8000 * 8000 * 4;
    env = e;
    inPath = in;
    outPath = out;
    optionsObj = opts;
    this->listener = listener;
    throwException = JNI_FALSE;
    appendTiff = JNI_FALSE;
    tiffDirectory = 0;
    jIProgressListenerClass = env->FindClass("org/beyka/tiffbitmapfactory/IProgressListener");
    jConvertOptionsClass = env->FindClass("org/beyka/tiffbitmapfactory/TiffConverter$ConverterOptions");
    jThreadClass = env->FindClass("java/lang/Thread");

    compressionInt = 5;
}

BaseTiffConverter::~BaseTiffConverter()
{
    LOGI("base destructor");

    if (cdescription) {
        env->ReleaseStringUTFChars(description, cdescription);
    }

    if (csoftware) {
        env->ReleaseStringUTFChars(software, csoftware);
    }

    if (jIProgressListenerClass) {
        env->DeleteLocalRef(jIProgressListenerClass);
        jIProgressListenerClass = NULL;
    }

    if (jThreadClass) {
        env->DeleteLocalRef(jThreadClass);
        jThreadClass = NULL;
    }
}

void BaseTiffConverter::readOptions()
{
    if (optionsObj == NULL) return;

    jfieldID tiffdirfield = env->GetFieldID(jConvertOptionsClass, "readTiffDirectory", "I");
    tiffDirectory = env->GetIntField(optionsObj, tiffdirfield);

    jfieldID availablememfield = env->GetFieldID(jConvertOptionsClass, "availableMemory", "J");
    jlong am = env->GetLongField(optionsObj, availablememfield);
    if (am > 0 || am == -1) availableMemory = am;

    jfieldID throwexceptionsfield = env->GetFieldID(jConvertOptionsClass, "throwExceptions", "Z");
    throwException = env->GetBooleanField(optionsObj, throwexceptionsfield);

    jfieldID appentifffield = env->GetFieldID(jConvertOptionsClass, "appendTiff", "Z");
    appendTiff = env->GetBooleanField(optionsObj, appentifffield);

    //read compression
    jfieldID gOptions_CompressionModeFieldID = env->GetFieldID(jConvertOptionsClass,
            "compressionScheme",
            "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
    jobject compressionMode = env->GetObjectField(optionsObj, gOptions_CompressionModeFieldID);
    jclass compressionModeClass = env->FindClass("org/beyka/tiffbitmapfactory/CompressionScheme");
    jfieldID ordinalFieldID = env->GetFieldID(compressionModeClass, "ordinal", "I");
    jint ci = env->GetIntField(compressionMode, ordinalFieldID);
    LOGII("ci", ci);
    //check is compression scheme is right. if not - set default LZW
    if (ci == 1 || ci == 2 || ci == 3 || ci == 4 || ci == 5 || ci == 7 || ci == 8 || ci == 32773 || ci == 32946)
        compressionInt = ci;
    else
        compressionInt = 5;

    LOGII("compressionInt ", compressionInt );
    env->DeleteLocalRef(compressionModeClass);

    //Get image orientation from options object
    /*jfieldID orientationFieldID = env->GetFieldID(jConvertOptionsClass,
            "orientation",
            "Lorg/beyka/tiffbitmapfactory/Orientation;");
    jobject orientation = env->GetObjectField(optionsObj, orientationFieldID);

    jclass orientationClass = env->FindClass("org/beyka/tiffbitmapfactory/Orientation");
    jfieldID orientationOrdinalFieldID = env->GetFieldID(orientationClass, "ordinal", "I");
    orientationInt = env->GetIntField(orientation, orientationOrdinalFieldID);
    env->DeleteLocalRef(orientationClass);*/
    orientationInt = ORIENTATION_TOPLEFT;

    //Get image description field if exist
    jfieldID imgDescrFieldID = env->GetFieldID(jConvertOptionsClass, "imageDescription", "Ljava/lang/String;");
    description = (jstring)env->GetObjectField(optionsObj, imgDescrFieldID);
    if (description) {
        cdescription = env->GetStringUTFChars(description, 0);
        LOGIS("Image Description: ", cdescription);
    }

    //Get software field if exist
    jfieldID softwareFieldID = env->GetFieldID(jConvertOptionsClass, "software", "Ljava/lang/String;");
    software = (jstring)env->GetObjectField(optionsObj, softwareFieldID);
    if (software) {
        csoftware = env->GetStringUTFChars(software, 0);
        LOGIS("Software tag: ", csoftware);
    }

    // variables for resolution
    jfieldID gOptions_xResolutionFieldID = env->GetFieldID(jConvertOptionsClass, "xResolution", "F");
    xRes = env->GetFloatField(optionsObj, gOptions_xResolutionFieldID);
    jfieldID gOptions_yResolutionFieldID = env->GetFieldID(jConvertOptionsClass, "yResolution", "F");
    yRes = env->GetFloatField(optionsObj, gOptions_yResolutionFieldID);
    jfieldID gOptions_resUnitFieldID = env->GetFieldID(jConvertOptionsClass,
                                                       "resUnit",
                                                       "Lorg/beyka/tiffbitmapfactory/ResolutionUnit;");
    jobject resUnitObject = env->GetObjectField(optionsObj, gOptions_resUnitFieldID);
    //Get res int from resUnitObject
    jclass resolutionUnitClass = env->FindClass("org/beyka/tiffbitmapfactory/ResolutionUnit");
    jfieldID resUnitOrdinalFieldID = env->GetFieldID(resolutionUnitClass, "ordinal", "I");
    resUnit = env->GetIntField(resUnitObject, resUnitOrdinalFieldID);
    env->DeleteLocalRef(resolutionUnitClass);

    //Get decode are
    jfieldID gOptions_decodeAreaFieldID = env->GetFieldID(jConvertOptionsClass,
                                                           "inTiffDecodeArea",
                                                           "Lorg/beyka/tiffbitmapfactory/DecodeArea;");
    jobject decodeAreaObj = env->GetObjectField(optionsObj, gOptions_decodeAreaFieldID);
    if (decodeAreaObj) {
        LOGI("Decode bounds present");
        jclass decodeAreaClass = env->FindClass("org/beyka/tiffbitmapfactory/DecodeArea");
        jfieldID xFieldID = env->GetFieldID(decodeAreaClass, "x", "I");
        jfieldID yFieldID = env->GetFieldID(decodeAreaClass, "y", "I");
        jfieldID widthFieldID = env->GetFieldID(decodeAreaClass, "width", "I");
        jfieldID heightFieldID = env->GetFieldID(decodeAreaClass, "height", "I");

        boundX = env->GetIntField(decodeAreaObj, xFieldID);
        boundY = env->GetIntField(decodeAreaObj, yFieldID);
        boundWidth = env->GetIntField(decodeAreaObj, widthFieldID);
        boundHeight = env->GetIntField(decodeAreaObj, heightFieldID);


        LOGII("Decode X", boundX);
        LOGII("Decode Y", boundY);
        LOGII("Decode width", boundWidth);
        LOGII("Decode height", boundHeight);

        hasBounds = 1;
        env->DeleteLocalRef(decodeAreaClass);
    }
}

char BaseTiffConverter::normalizeDecodeArea() {
    if (hasBounds) {
        if (boundX >= width-1) {
                const char *message = "X of left top corner of decode area should be less than image width";
                LOGE(*message);
                if (throwException) {
                    jstring adinf = env->NewStringUTF(message);
                    throw_decode_file_exception(env, inPath, adinf);
                    env->DeleteLocalRef(adinf);
                }
                return 0;
            }
            if (boundY >= height-1) {
                const char *message = "Y of left top corner of decode area should be less than image height";
                LOGE(*message);
                if (throwException) {
                    jstring adinf = env->NewStringUTF(message);
                    throw_decode_file_exception(env, inPath, adinf);
                    env->DeleteLocalRef(adinf);
                }
                return 0;
            }

            if (boundX < 0) boundX = 0;
            if (boundY < 0) boundY = 0;
            if (boundX + boundWidth >= width) boundWidth = width - boundX -1;
            if (boundY + boundHeight >= height) boundHeight = height - boundY -1;

            if (boundWidth < 1) {
                const char *message = "Width of decode area can\'t be less than 1";
                LOGE(*message);
                if (throwException) {
                    jstring adinf = env->NewStringUTF(message);
                    throw_decode_file_exception(env, inPath, adinf);
                    env->DeleteLocalRef(adinf);
                }
                return 0;
            }
            if (boundHeight < 1) {
                const char *message = "Height of decode area can\'t be less than 1";
                LOGE(*message);
                if (throwException) {
                    jstring adinf = env->NewStringUTF(message);
                    throw_decode_file_exception(env, inPath, adinf);
                    env->DeleteLocalRef(adinf);
                }
                return 0;
            }

            outWidth = boundWidth;
            outHeight = boundHeight;
            outStartX = boundX;
            outStartY = boundY;

            return 1;
    } else {
            outWidth = width;
            outHeight = height;
            outStartX = 0;
            outStartY = 0;

            return 1;
    }
}

char *BaseTiffConverter::getCreationDate() {
    char * datestr = (char *) malloc(sizeof(char) * 20);
    time_t rawtime;
    struct tm * timeinfo;
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    strftime (datestr,20,/*"Now it's %I:%M%p."*/"%Y:%m:%d %H:%M:%S",timeinfo);

    return datestr;
}

void BaseTiffConverter::sendProgress(jlong current, jlong total) {
    if (listener != NULL) {
        jmethodID methodid = env->GetMethodID(jIProgressListenerClass, "reportProgress", "(JJ)V");
        env->CallVoidMethod(listener, methodid, current, total);
    }
}

jboolean BaseTiffConverter::checkStop() {
    jmethodID methodID = env->GetStaticMethodID(jThreadClass, "interrupted", "()Z");
    jboolean interupted = env->CallStaticBooleanMethod(jThreadClass, methodID);

    jboolean stop;

    if (optionsObj) {
        jfieldID stopFieldId = env->GetFieldID(jConvertOptionsClass,
                                               "isStoped",
                                               "Z");
        stop = env->GetBooleanField(optionsObj, stopFieldId);

    } else {
        stop = JNI_FALSE;
    }

    return interupted || stop;
}

void BaseTiffConverter::rotateTileLinesVertical(uint32 tileHeight, uint32 tileWidth, uint32* whatRotate, uint32 *bufferLine) {
    for (int line = 0; line < tileHeight / 2; line++) {
        unsigned int  *top_line, *bottom_line;
        top_line = whatRotate + tileWidth * line;
        bottom_line = whatRotate + tileWidth * (tileHeight - line -1);
        _TIFFmemcpy(bufferLine, top_line, sizeof(unsigned int) * tileWidth);
        _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * tileWidth);
        _TIFFmemcpy(bottom_line, bufferLine, sizeof(unsigned int) * tileWidth);
    }
}

void BaseTiffConverter::rotateTileLinesHorizontal(uint32 tileHeight, uint32 tileWidth, uint32* whatRotate, uint32 *bufferLine) {
    uint32 buf;
    for (int y = 0; y < tileHeight; y++) {
        for (int x = 0; x < tileWidth / 2; x++) {
            buf = whatRotate[y * tileWidth + x];
            whatRotate[y * tileWidth + x] = whatRotate[y * tileWidth + tileWidth - x - 1];
            whatRotate[y * tileWidth + tileWidth - x - 1] = buf;
        }
    }
}

void BaseTiffConverter::normalizeTile(uint32 tileHeight, uint32 tileWidth, uint32* rasterTile) {
    //normalize tile
    //find start and end pixels
    int sx = -1, ex= -1, sy= -1, ey= -1;
    for (int y = 0; y < tileHeight; y++) {
        for (int x = 0; x < tileWidth; x++) {
            if (rasterTile[y * tileWidth + x] != 0) {
                sx = x;
                sy = y;
                break;
            }
        }
        if (sx != -1) break;
    }
    for (int y = tileHeight -1; y >= 0; y--) {
        for (int x = tileWidth -1; x >= 0; x--) {
            if (rasterTile[y * tileWidth + x] != 0) {
                ex = x;
                ey = y;
                break;
            }
        }
        if (ex != -1) break;
    }
    if (sy != 0) {
        for (int y = 0; y < tileHeight - sy -1; y++) {
            memcpy(rasterTile + (y * tileWidth), rasterTile + ((y+sy)*tileWidth), tileWidth * sizeof(uint32));
        }
    }
    if (sx != 0) {
        for (int y = 0; y < tileHeight; y++) {
           for (int x = 0; x < tileWidth - sx -1; x++) {
               rasterTile[y * tileWidth + x] = rasterTile[y * tileWidth + x + sx];
           }
        }
    }
}
