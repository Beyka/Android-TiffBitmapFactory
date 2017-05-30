//
// Created by beyka on 5/11/17.
//

#include "BaseTiffConverter.h"

BaseTiffConverter::BaseTiffConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts, jobject listener)
{
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
    if (ci == 1 || ci == 3 || ci == 4 || ci == 5 || ci == 7 || ci == 8 || ci == 32773 || ci == 32946)
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
    if (optionsObj) {
        jfieldID stopFieldId = env->GetFieldID(jConvertOptionsClass,
                                               "isStoped",
                                               "Z");
        jboolean stop = env->GetBooleanField(optionsObj, stopFieldId);
        return stop;
    } else {
        return JNI_FALSE;
    }
}

