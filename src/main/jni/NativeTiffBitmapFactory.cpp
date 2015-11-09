using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

#include "NativeTiffBitmapFactory.h"
#include "readTiffIncremental.h"
#include "NativeExceptions.h"

int const colorMask = 0xFF;
int const ARGB_8888 = 2;
int const RGB_565 = 4;
int const ALPHA_8 = 8;

TIFF *image;
int origwidth = 0;
int origheight = 0;
short origorientation = 0;
jobject preferedConfig;
jboolean invertRedAndBlue = false;
int availableMemory = -1;

JNIEXPORT jobject

JNICALL Java_org_beyka_tiffbitmapfactory_TiffBitmapFactory_nativeDecodePath
        (JNIEnv *env, jclass clazz, jstring path, jobject options) {

    //Drop some global variables
    origorientation = 0;
    origwidth = 0;
    origheight = 0;
    preferedConfig = NULL;
    invertRedAndBlue = false;
    availableMemory = -1;
    image = NULL;

    //Get options
    jclass jBitmapOptionsClass = env->FindClass(
            "org/beyka/tiffbitmapfactory/TiffBitmapFactory$Options");

    jfieldID gOptions_sampleSizeFieldID = env->GetFieldID(jBitmapOptionsClass, "inSampleSize", "I");
    jint inSampleSize = env->GetIntField(options, gOptions_sampleSizeFieldID);

    jfieldID gOptions_justDecodeBoundsFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                                "inJustDecodeBounds", "Z");
    jboolean inJustDecodeBounds = env->GetBooleanField(options, gOptions_justDecodeBoundsFieldID);

    jfieldID gOptions_invertRedAndBlueFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                                "inSwapRedBlueColors", "Z");
    invertRedAndBlue = env->GetBooleanField(options, gOptions_invertRedAndBlueFieldID);

    jfieldID gOptions_DirectoryCountFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                              "inDirectoryNumber",
                                                              "I");
    jint inDirectoryNumber = env->GetIntField(options, gOptions_DirectoryCountFieldID);
    LOGII("param directoryCount", inDirectoryNumber);
    
    jfieldID gOptions_AvailableMemoryFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                              "inAvailableMemory",
                                                              "I");
    availableMemory = env->GetIntField(options, gOptions_AvailableMemoryFieldID);

    jfieldID gOptions_PreferedConfigFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                              "inPreferredConfig",
                                                              "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageConfig;");
    jobject config = env->GetObjectField(options, gOptions_PreferedConfigFieldID);
    if (config == NULL) {
        LOGI("config is NULL, creating default options");
        jclass bitmapConfig = env->FindClass(
                "org/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageConfig");
        jfieldID argb8888FieldID = env->GetStaticFieldID(bitmapConfig, "ARGB_8888",
                                                         "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageConfig;");
        config = env->GetStaticObjectField(bitmapConfig, argb8888FieldID);
        env->DeleteLocalRef(bitmapConfig);
    }
    preferedConfig = env->NewGlobalRef(config);
    env->DeleteLocalRef(config);

    //if directory number < 0 set it to 0
    if (inDirectoryNumber < 0) inDirectoryNumber = 0;

    //Open image and read data;
    const char *strPath = NULL;
    strPath = env->GetStringUTFChars(path, 0);
    LOGIS("nativeTiffOpen", strPath);

    image = TIFFOpen(strPath, "r");
    env->ReleaseStringUTFChars(path, strPath);
    if (image == NULL) {
        throw_no_such_file_exception(env, path);
        LOGES("Can\'t open bitmap", strPath);
        return NULL;
    }

    jobject java_bitmap = NULL;

    writeDataToOptions(env, options, inDirectoryNumber);

    if (!inJustDecodeBounds) {
        TIFFSetDirectory(image, inDirectoryNumber);
        TIFFGetField(image, TIFFTAG_IMAGEWIDTH, &origwidth);
        TIFFGetField(image, TIFFTAG_IMAGELENGTH, &origheight);
        java_bitmap = createBitmap(env, inSampleSize, inDirectoryNumber, options, path);
    }

    releaseImage(env);

    return java_bitmap;
}

void writeDataToOptions(JNIEnv *env, jobject options, int directoryNumber) {
    TIFFSetDirectory(image, 0);

    jclass jOptionsClass = env->FindClass(
            "org/beyka/tiffbitmapfactory/TiffBitmapFactory$Options");

    jfieldID gOptions_outDirectoryCountFieldId = env->GetFieldID(jOptionsClass,
                                                                 "outDirectoryCount", "I");
    int dircount = getDyrectoryCount();
    env->SetIntField(options, gOptions_outDirectoryCountFieldId, dircount);

    TIFFSetDirectory(image, directoryNumber);
    TIFFGetField(image, TIFFTAG_IMAGEWIDTH, &origwidth);
    TIFFGetField(image, TIFFTAG_IMAGELENGTH, &origheight);

    //Getting image orientation and createing ImageOrientation enum
    TIFFGetField(image, TIFFTAG_ORIENTATION, &origorientation);
    //If orientation field is empty - use ORIENTATION_TOPLEFT
    if (origorientation == 0) {
        origorientation = ORIENTATION_TOPLEFT;
    }
    jclass gOptions_ImageOrientationClass = env->FindClass(
            "org/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageOrientation");
    jfieldID gOptions_ImageOrientationFieldId = NULL;
    bool flipHW = false;
    switch (origorientation) {
        case ORIENTATION_TOPLEFT :
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                                                                     "ORIENTATION_TOPLEFT",
                                                                     "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageOrientation;");
            break;
        case ORIENTATION_TOPRIGHT :
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                                                                     "ORIENTATION_TOPRIGHT",
                                                                     "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageOrientation;");
            break;
        case ORIENTATION_BOTRIGHT :
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                                                                     "ORIENTATION_BOTRIGHT",
                                                                     "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageOrientation;");
            break;
        case ORIENTATION_BOTLEFT :
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                                                                     "ORIENTATION_BOTLEFT",
                                                                     "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageOrientation;");
            break;
        case ORIENTATION_LEFTTOP :
            flipHW = true;
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                                                                     "ORIENTATION_LEFTTOP",
                                                                     "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageOrientation;");
            break;
        case ORIENTATION_RIGHTTOP :
            flipHW = true;
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                                                                     "ORIENTATION_RIGHTTOP",
                                                                     "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageOrientation;");
            break;
        case ORIENTATION_RIGHTBOT :
            flipHW = true;
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                                                                     "ORIENTATION_RIGHTBOT",
                                                                     "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageOrientation;");
            break;
        case ORIENTATION_LEFTBOT :
            flipHW = true;
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                                                                     "ORIENTATION_LEFTBOT",
                                                                     "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageOrientation;");
            break;
    }
    if (gOptions_ImageOrientationFieldId != NULL) {
        jobject gOptions_ImageOrientationObj = env->GetStaticObjectField(
                gOptions_ImageOrientationClass,
                gOptions_ImageOrientationFieldId);

        //Set outImageOrientation field to options object
        jfieldID gOptions_outImageOrientationField = env->GetFieldID(jOptionsClass,
                                                                     "outImageOrientation",
                                                                     "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageOrientation;");
        env->SetObjectField(options, gOptions_outImageOrientationField,
                            gOptions_ImageOrientationObj);
    }

    jfieldID gOptions_OutCurDirNumberFieldID = env->GetFieldID(jOptionsClass,
                                                               "outCurDirectoryNumber",
                                                               "I");
    env->SetIntField(options, gOptions_OutCurDirNumberFieldID, directoryNumber);
    if (!flipHW) {
        jfieldID gOptions_outWidthFieldId = env->GetFieldID(jOptionsClass, "outWidth", "I");
        env->SetIntField(options, gOptions_outWidthFieldId, origwidth);

        jfieldID gOptions_outHeightFieldId = env->GetFieldID(jOptionsClass, "outHeight", "I");
        env->SetIntField(options, gOptions_outHeightFieldId, origheight);
    } else {
        jfieldID gOptions_outWidthFieldId = env->GetFieldID(jOptionsClass, "outWidth", "I");
        env->SetIntField(options, gOptions_outWidthFieldId, origheight);

        jfieldID gOptions_outHeightFieldId = env->GetFieldID(jOptionsClass, "outHeight", "I");
        env->SetIntField(options, gOptions_outHeightFieldId, origwidth);
    }

    env->DeleteLocalRef(jOptionsClass);
}

jobject createBitmap(JNIEnv *env, int inSampleSize, int directoryNumber, jobject options, jstring path) {
    //Read Config from options. Use ordinal field from ImageConfig class
    jclass configClass = env->FindClass(
            "org/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageConfig");
    jfieldID ordinalFieldID = env->GetFieldID(configClass, "ordinal", "I");
    jint configInt = env->GetIntField(preferedConfig, ordinalFieldID);

    int bitdepth = 0;
    TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bitdepth);
    if (bitdepth != 1 && bitdepth != 4 && bitdepth != 8 && bitdepth != 16) {
        //TODO Drop exception
        LOGE("Only 1, 4, 8, and 16 bits per sample supported");
        throw_read_file_exception(env, path);
        return NULL;
    }

    // Estimate the memory required when the entire file is read in: 
    // original file size plus two copies of sampled file plus a little extra to be conservative.
    int origBufferSize = origwidth * origheight * sizeof(unsigned int);
    int estimatedMemory = origBufferSize + 2 * (origBufferSize / (inSampleSize * inSampleSize));
    estimatedMemory = 11 * estimatedMemory / 10; // 10% extra.
    LOGII("estimatedMemory", estimatedMemory);
    
    unsigned int *origBuffer = NULL;
    
    // Test memory requirement.
    if ((availableMemory > 0) && (estimatedMemory > availableMemory)) {
        LOGI("Large memory is required. Read file incrementally and sample it");
        // Large memory is required. Read file incrementally and sample it.
        int returnCode = readTiffIncremental(env, image, (unsigned char**) &origBuffer, inSampleSize, availableMemory, path);
        LOGII("return code", returnCode);
        if (returnCode != 0) {
            releaseImage(env);
            LOGE("ReadTiffIncremental failed.");
            return NULL;
        }
        // File is now sampled. Adjust image and sample size variables.
        origwidth = origwidth / inSampleSize;
	    origheight = origheight / inSampleSize;
        inSampleSize = 1;
    }
    else {
        origBuffer = (unsigned int *) _TIFFmalloc(origBufferSize);
        if (origBuffer == NULL) {
            LOGE("Can\'t allocate memory for origBuffer");
            return NULL;
        }

        if (0 ==
            TIFFReadRGBAImageOriented(image, origwidth, origheight, origBuffer, ORIENTATION_TOPLEFT, 0)) {
	        free(origBuffer);
            LOGE("Error reading image.");
            return NULL;
        }
    }

    // Convert ABGR to ARGB
    if (invertRedAndBlue) {
        int i = 0;
        int j = 0;
        int tmp = 0;
        for (i = 0; i < origheight; i++) {
            for (j = 0; j < origwidth; j++) {
                tmp = origBuffer[j + origwidth * i];
                origBuffer[j + origwidth * i] =
                        (tmp & 0xff000000) | ((tmp & 0x00ff0000) >> 16) | (tmp & 0x0000ff00) |
                        ((tmp & 0xff) << 16);
            }
        }
    }

    int bitmapwidth = origwidth;
    int bitmapheight = origheight;

    //Class and field for Bitmap.Config
    jclass bitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
    jfieldID bitmapConfigField = NULL;
    void *processedBuffer = NULL;
    if (configInt == ARGB_8888) {
        processedBuffer = createBitmapARGB8888(env, inSampleSize, origBuffer, &bitmapwidth,
                                               &bitmapheight);
        bitmapConfigField = env->GetStaticFieldID(bitmapConfigClass, "ARGB_8888",
                                                  "Landroid/graphics/Bitmap$Config;");
    } else if (configInt == ALPHA_8) {
        processedBuffer = createBitmapAlpha8(env, inSampleSize, origBuffer, &bitmapwidth,
                                             &bitmapheight);
        bitmapConfigField = env->GetStaticFieldID(bitmapConfigClass, "ALPHA_8",
                                                  "Landroid/graphics/Bitmap$Config;");
    } else if (configInt == RGB_565) {
        processedBuffer = createBitmapRGB565(env, inSampleSize, origBuffer, &bitmapwidth,
                                             &bitmapheight);
        bitmapConfigField = env->GetStaticFieldID(bitmapConfigClass, "RGB_565",
                                                  "Landroid/graphics/Bitmap$Config;");
    }

    if (processedBuffer == NULL) {
        LOGE("Error while decoding image");
        return NULL;
    }

    //Create mutable bitmap
    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID methodid = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");

    //BitmapConfig
    jobject config = env->GetStaticObjectField(bitmapConfigClass, bitmapConfigField);

    jobject java_bitmap = NULL;
    if (origorientation > 4) {
        java_bitmap = env->CallStaticObjectMethod(bitmapClass, methodid, bitmapheight,
                                                  bitmapwidth, config);

    } else {
        java_bitmap = env->CallStaticObjectMethod(bitmapClass, methodid, bitmapwidth,
                                                  bitmapheight, config);
    }

    env->DeleteLocalRef(config);

    //Copy data to bitmap
    int ret;
    void *bitmapPixels;
    if ((ret = AndroidBitmap_lockPixels(env, java_bitmap, &bitmapPixels)) < 0) {
        //error
        LOGE("Lock pixels failed");
        return NULL;
    }
    int pixelsCount = bitmapwidth * bitmapheight;

    if (configInt == ARGB_8888) {
        memcpy(bitmapPixels, (jint *) processedBuffer, sizeof(jint) * pixelsCount);
    } else if (configInt == ALPHA_8) {
        memcpy(bitmapPixels, (jbyte *) processedBuffer, sizeof(jbyte) * pixelsCount);
    } else if (configInt == RGB_565) {
        memcpy(bitmapPixels, (unsigned short *) processedBuffer,
               sizeof(unsigned short) * pixelsCount);
    }

    AndroidBitmap_unlockPixels(env, java_bitmap);

    //remove array
    free(processedBuffer);

    //remove Bitmap class object
    env->DeleteLocalRef(bitmapClass);

    return java_bitmap;
}

jint *createBitmapARGB8888(JNIEnv *env, int inSampleSize, unsigned int *buffer, int *bitmapwidth,
                           int *bitmapheight) {
    jint *pixels = NULL;
    *bitmapwidth = origwidth / inSampleSize;
    *bitmapheight = origheight / inSampleSize;
    int pixelsBufferSize = *bitmapwidth * *bitmapheight;
    
    if (inSampleSize == 1) {
        // Use buffer as is.
        pixels = (jint*) buffer;
    }
    else {
        // Sample the buffer.
        pixels = (jint *) malloc(sizeof(jint) * pixelsBufferSize);
        if (pixels == NULL) {
            LOGE("Can\'t allocate memory for temp buffer");
            return NULL;
        }
        else {
            for (int i = 0, i1 = 0; i < *bitmapwidth; i++, i1 += inSampleSize) {
                for (int j = 0, j1 = 0; j < *bitmapheight; j++, j1 += inSampleSize) {

                    //Apply filter to pixel
                    jint crPix = buffer[j1 * origwidth + i1];
                    int sum = 1;

                    int alpha = colorMask & crPix >> 24;
                    int red = colorMask & crPix >> 16;
                    int green = colorMask & crPix >> 8;
                    int blue = colorMask & crPix;

                    //using kernel 3x3

                    //topleft
                    if (i1 - 1 >= 0 && j1 - 1 >= 0) {
                        crPix = buffer[(j1 - 1) * origwidth + i1 - 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //top
                    if (j1 - 1 >= 0) {
                        crPix = buffer[(j1 - 1) * origwidth + i1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    // topright
                    if (i1 + 1 < origwidth && j1 - 1 >= 0) {
                        crPix = buffer[(j1 - 1) * origwidth + i1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //right
                    if (i1 + 1 < origwidth) {
                        crPix = buffer[j1 * origwidth + i1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //bottomright
                    if (i1 + 1 < origwidth && j1 + 1 < origheight) {
                        crPix = buffer[(j1 + 1) * origwidth + i1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //bottom
                    if (j1 + 1 < origheight) {
                        crPix = buffer[(j1 + 1) * origwidth + i1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //bottomleft
                    if (i1 - 1 >= 0 && j1 + 1 < origheight) {
                        crPix = buffer[(j1 + 1) * origwidth + i1 - 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //left
                    if (i1 - 1 >= 0) {
                        crPix = buffer[j1 * origwidth + i1 - 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }

                    red /= sum;
                    if (red > 255) red = 255;
                    if (red < 0) red = 0;

                    green /= sum;
                    if (green > 255) green = 255;
                    if (green < 0) green = 0;

                    blue /= sum;
                    if (blue > 255) blue = 255;
                    if (blue < 0) blue = 0;

                    alpha /= sum;///= sum;
                    if (alpha > 255) alpha = 255;
                    if (alpha < 0) alpha = 0;

                    crPix = (alpha << 24) | (red << 16) | (green << 8) | (blue);

                    pixels[j * *bitmapwidth + i] = crPix;
                }
            }
        }

        //Close Buffer
        if (buffer) {
            _TIFFfree(buffer);
            buffer = NULL;
        }
    }

    if (origorientation > 4) {
        unsigned int size = *bitmapheight * *bitmapwidth - 1;
        jint t;
        unsigned long long next;
        unsigned long long cycleBegin;
        bool *barray = (bool *) malloc(sizeof(bool) * pixelsBufferSize);
	for (int x = 0; x < size; x++) { barray[x] = false; }
        barray[0] = barray[size] = true;
        unsigned long long k = 1;

        switch (origorientation) {
            case ORIENTATION_LEFTTOP:
            case ORIENTATION_RIGHTBOT:
                while (k < size) {
                    cycleBegin = k;
                    t = pixels[k];
                    do {
                        next = (k * *bitmapheight) % size;
                        jint buf = pixels[next];
                        pixels[next] = t;
                        t = buf;
                        barray[k] = true;
                        k = next;
                    } while (k != cycleBegin);
                    for (k = 1; k < size && barray[k]; k++);
                }
                break;
            case ORIENTATION_LEFTBOT:
            case ORIENTATION_RIGHTTOP:
                while (k < size) {
                    cycleBegin = k;
                    t = pixels[k];
                    do {
                        next = (k * *bitmapheight) % size;
                        jint buf = pixels[next];
                        pixels[next] = t;
                        t = buf;
                        barray[k] = true;
                        k = next;
                    } while (k != cycleBegin);
                    for (k = 1; k < size && barray[k]; k++);
                }
                //flip horizontally
                for (int j = 0, j1 = *bitmapwidth - 1; j < *bitmapwidth / 2; j++, j1--) {
                    for (int i = 0; i < *bitmapheight; i++) {
                        jint tmp = pixels[j * *bitmapheight + i];
                        pixels[j * *bitmapheight + i] = pixels[j1 * *bitmapheight + i];
                        pixels[j1 * *bitmapheight + i] = tmp;
                    }
                }
                //flip vertically
                for (int i = 0, i1 = *bitmapheight - 1; i < *bitmapheight / 2; i++, i1--) {
                    for (int j = 0; j < *bitmapwidth; j++) {
                        jint tmp = pixels[j * *bitmapheight + i];
                        pixels[j * *bitmapheight + i] = pixels[j * *bitmapheight + i1];
                        pixels[j * *bitmapheight + i1] = tmp;
                    }
                }
                break;
        }
        free(barray);
    }

    return pixels;
}

jbyte *createBitmapAlpha8(JNIEnv *env, int inSampleSize, unsigned int *buffer, int *bitmapwidth,
                          int *bitmapheight) {
    jbyte *pixels = NULL;
    *bitmapwidth = origwidth / inSampleSize;
    *bitmapheight = origheight / inSampleSize;
    int pixelsBufferSize = *bitmapwidth * *bitmapheight;
    pixels = (jbyte *) malloc(sizeof(jbyte) * pixelsBufferSize);
    if (pixels == NULL) {
        LOGE("Can\'t allocate memory for temp buffer");
        return NULL;
    }
    else {
        for (int i = 0, i1 = 0; i < *bitmapwidth; i++, i1 += inSampleSize) {
            for (int j = 0, j1 = 0; j < *bitmapheight; j++, j1 += inSampleSize) {

                //Apply filter to pixel
                unsigned int crPix = buffer[j1 * origwidth + i1];
                int sum = 1;

                int alpha = colorMask & crPix >> 24;

                //using kernel 3x3

                //topleft
                if (i1 - 1 >= 0 && j1 - 1 >= 0) {
                    crPix = buffer[(j1 - 1) * origwidth + i1 - 1];
                    alpha += colorMask & crPix >> 24;
                    sum++;
                }
                //top
                if (j1 - 1 >= 0) {
                    crPix = buffer[(j1 - 1) * origwidth + i1];
                    alpha += colorMask & crPix >> 24;
                    sum++;
                }
                // topright
                if (i1 + 1 < origwidth && j1 - 1 >= 0) {
                    crPix = buffer[(j1 - 1) * origwidth + i1 + 1];
                    alpha += colorMask & crPix >> 24;
                    sum++;
                }
                //right
                if (i1 + 1 < origwidth) {
                    crPix = buffer[j1 * origwidth + i1 + 1];
                    alpha += colorMask & crPix >> 24;
                    sum++;
                }
                //bottomright
                if (i1 + 1 < origwidth && j1 + 1 < origheight) {
                    crPix = buffer[(j1 + 1) * origwidth + i1 + 1];
                    alpha += colorMask & crPix >> 24;
                    sum++;
                }
                //bottom
                if (j1 + 1 < origheight) {
                    crPix = buffer[(j1 + 1) * origwidth + i1 + 1];
                    alpha += colorMask & crPix >> 24;
                    sum++;
                }
                //bottomleft
                if (i1 - 1 >= 0 && j1 + 1 < origheight) {
                    crPix = buffer[(j1 + 1) * origwidth + i1 - 1];
                    alpha += colorMask & crPix >> 24;
                    sum++;
                }
                //left
                if (i1 - 1 >= 0) {
                    crPix = buffer[j1 * origwidth + i1 - 1];
                    alpha += colorMask & crPix >> 24;
                    sum++;
                }

                alpha /= sum;
                if (alpha > 255) alpha = 255;
                if (alpha < 0) alpha = 0;

                jbyte curPix = alpha;

                pixels[j * *bitmapwidth + i] = curPix;
            }
        }
    }

    //Close Buffer
    if (buffer) {
        _TIFFfree(buffer);
        buffer = NULL;
    }

    if (origorientation > 4) {
        unsigned int size = *bitmapheight * *bitmapwidth - 1;
        jbyte t;
        unsigned long long next;
        unsigned long long cycleBegin;
        bool *barray = (bool *) malloc(sizeof(bool) * pixelsBufferSize);
	for (int x = 0; x < size; x++) { barray[x] = false; }
        barray[0] = barray[size] = true;
        unsigned long long k = 1;

        switch (origorientation) {
            case ORIENTATION_LEFTTOP:
            case ORIENTATION_RIGHTBOT:
                while (k < size) {
                    cycleBegin = k;
                    t = pixels[k];
                    do {
                        next = (k * *bitmapheight) % size;
                        jbyte buf = pixels[next];
                        pixels[next] = t;
                        t = buf;
                        barray[k] = true;
                        k = next;
                    } while (k != cycleBegin);
                    for (k = 1; k < size && barray[k]; k++);
                }
                break;
            case ORIENTATION_LEFTBOT:
            case ORIENTATION_RIGHTTOP:
                while (k < size) {
                    cycleBegin = k;
                    t = pixels[k];
                    do {
                        next = (k * *bitmapheight) % size;
                        jbyte buf = pixels[next];
                        pixels[next] = t;
                        t = buf;
                        barray[k] = true;
                        k = next;
                    } while (k != cycleBegin);
                    for (k = 1; k < size && barray[k]; k++);
                }
                //flip horizontally
                for (int j = 0, j1 = *bitmapwidth - 1; j < *bitmapwidth / 2; j++, j1--) {
                    for (int i = 0; i < *bitmapheight; i++) {
                        jbyte tmp = pixels[j * *bitmapheight + i];
                        pixels[j * *bitmapheight + i] = pixels[j1 * *bitmapheight + i];
                        pixels[j1 * *bitmapheight + i] = tmp;
                    }
                }
                //flip vertically
                for (int i = 0, i1 = *bitmapheight - 1; i < *bitmapheight / 2; i++, i1--) {
                    for (int j = 0; j < *bitmapwidth; j++) {
                        jbyte tmp = pixels[j * *bitmapheight + i];
                        pixels[j * *bitmapheight + i] = pixels[j * *bitmapheight + i1];
                        pixels[j * *bitmapheight + i1] = tmp;
                    }
                }
                break;
        }
        free(barray);
    }
    return pixels;
}

unsigned short *createBitmapRGB565(JNIEnv *env, int inSampleSize, unsigned int *buffer,
                                   int *bitmapwidth,
                                   int *bitmapheight) {
    unsigned short *pixels = NULL;

    *bitmapwidth = origwidth / inSampleSize;
    *bitmapheight = origheight / inSampleSize;
    int pixelsBufferSize = *bitmapwidth * *bitmapheight;
    pixels = (unsigned short *) malloc(sizeof(unsigned short) * pixelsBufferSize);
    if (pixels == NULL) {
        LOGE("Can\'t allocate memory for temp buffer");
        return NULL;
    }

    for (int i = 0, i1 = 0; i < *bitmapwidth; i++, i1 += inSampleSize) {
        for (int j = 0, j1 = 0; j < *bitmapheight; j++, j1 += inSampleSize) {
            unsigned int crPix = buffer[j1 * origwidth + i1];
            int sum = 1;
            int blue = colorMask & crPix >> 16;
            int green = colorMask & crPix >> 8;
            int red = colorMask & crPix;

            //topleft
            if (i1 - 1 >= 0 && j1 - 1 >= 0) {
                crPix = buffer[(j1 - 1) * origwidth + i1 - 1];
                blue += colorMask & crPix >> 16;
                green += colorMask & crPix >> 8;
                red += colorMask & crPix;
                sum++;
            }
            //top
            if (j1 - 1 >= 0) {
                crPix = buffer[(j1 - 1) * origwidth + i1];
                blue += colorMask & crPix >> 16;
                green += colorMask & crPix >> 8;
                red += colorMask & crPix;
                sum++;
            }
            // topright
            if (i1 + 1 < origwidth && j1 - 1 >= 0) {
                crPix = buffer[(j1 - 1) * origwidth + i1 + 1];
                blue += colorMask & crPix >> 16;
                green += colorMask & crPix >> 8;
                red += colorMask & crPix;
                sum++;
            }
            //right
            if (i1 + 1 < origwidth) {
                crPix = buffer[j1 * origwidth + i1 + 1];
                blue += colorMask & crPix >> 16;
                green += colorMask & crPix >> 8;
                red += colorMask & crPix;
                sum++;
            }
            //bottomright
            if (i1 + 1 < origwidth && j1 + 1 < origheight) {
                crPix = buffer[(j1 + 1) * origwidth + i1 + 1];
                blue += colorMask & crPix >> 16;
                green += colorMask & crPix >> 8;
                red += colorMask & crPix;
                sum++;
            }
            //bottom
            if (j1 + 1 < origheight) {
                crPix = buffer[(j1 + 1) * origwidth + i1 + 1];
                blue += colorMask & crPix >> 16;
                green += colorMask & crPix >> 8;
                red += colorMask & crPix;
                sum++;
            }
            //bottomleft
            if (i1 - 1 >= 0 && j1 + 1 < origheight) {
                crPix = buffer[(j1 + 1) * origwidth + i1 - 1];
                blue += colorMask & crPix >> 16;
                green += colorMask & crPix >> 8;
                red += colorMask & crPix;
                sum++;
            }
            //left
            if (i1 - 1 >= 0) {
                crPix = buffer[j1 * origwidth + i1 - 1];
                blue += colorMask & crPix >> 16;
                green += colorMask & crPix >> 8;
                red += colorMask & crPix;
                sum++;
            }

            red /= sum;
            if (red > 255) red = 255;
            if (red < 0) red = 0;

            green /= sum;
            if (green > 255) green = 255;
            if (green < 0) green = 0;

            blue /= sum;
            if (blue > 255) blue = 255;
            if (blue < 0) blue = 0;

            unsigned char B = (blue >> 3);
            unsigned char G = (green >> 2);
            unsigned char R = (red >> 3);

            unsigned short curPix = (R << 11) | (G << 5) | B;

            pixels[j * *bitmapwidth + i] = curPix;
        }
    }

    //Close Buffer
    if (buffer) {
        _TIFFfree(buffer);
        buffer = NULL;
    }

    if (origorientation > 4) {
        unsigned int size = *bitmapheight * *bitmapwidth - 1;
        unsigned short t;
        unsigned long long next;
        unsigned long long cycleBegin;
        bool *barray = (bool *) malloc(sizeof(bool) * pixelsBufferSize);
	for (int x = 0; x < size; x++) { barray[x] = false; }
        barray[0] = barray[size] = true;
        unsigned long long k = 1;

        switch (origorientation) {
            case ORIENTATION_LEFTTOP:
            case ORIENTATION_RIGHTBOT:
                while (k < size) {
                    cycleBegin = k;
                    t = pixels[k];
                    do {
                        next = (k * *bitmapheight) % size;
                        unsigned short buf = pixels[next];
                        pixels[next] = t;
                        t = buf;
                        barray[k] = true;
                        k = next;
                    } while (k != cycleBegin);
                    for (k = 1; k < size && barray[k]; k++);
                }
                break;
            case ORIENTATION_LEFTBOT:
            case ORIENTATION_RIGHTTOP:
                while (k < size) {
                    cycleBegin = k;
                    t = pixels[k];
                    do {
                        next = (k * *bitmapheight) % size;
                        unsigned short buf = pixels[next];
                        pixels[next] = t;
                        t = buf;
                        barray[k] = true;
                        k = next;
                    } while (k != cycleBegin);
                    for (k = 1; k < size && barray[k]; k++);
                }
                //flip horizontally
                for (int j = 0, j1 = *bitmapwidth - 1; j < *bitmapwidth / 2; j++, j1--) {
                    for (int i = 0; i < *bitmapheight; i++) {
                        unsigned short tmp = pixels[j * *bitmapheight + i];
                        pixels[j * *bitmapheight + i] = pixels[j1 * *bitmapheight + i];
                        pixels[j1 * *bitmapheight + i] = tmp;
                    }
                }
                //flip vertically
                for (int i = 0, i1 = *bitmapheight - 1; i < *bitmapheight / 2; i++, i1--) {
                    for (int j = 0; j < *bitmapwidth; j++) {
                        unsigned short tmp = pixels[j * *bitmapheight + i];
                        pixels[j * *bitmapheight + i] = pixels[j * *bitmapheight + i1];
                        pixels[j * *bitmapheight + i1] = tmp;
                    }
                }
                break;
        }
        free(barray);
    }

    return pixels;
}

jobject createBlankBitmap(JNIEnv *env, int width, int height) {
    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID methodid = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");

    jobject java_bitmap = env->CallStaticObjectMethod(bitmapClass, methodid, width, height,
                                                      preferedConfig);

    env->DeleteLocalRef(bitmapClass);

    return java_bitmap;
}

void releaseImage(JNIEnv * env) {
    if (image) {
        TIFFClose(image);
        image = NULL;
    }

    //Release global reference for Bitmap.Config
    if (preferedConfig) {
        env->DeleteGlobalRef(preferedConfig);
        preferedConfig = NULL;
    }
}

int getDyrectoryCount() {
    int dircount = 0;
    do {
        dircount++;
    } while (TIFFReadDirectory(image));
    return dircount;
}

#ifdef __cplusplus
}
#endif
