//
// Created by beyka on 3.2.17.
//

#include "NativeDecoder.h"
#include <string>

jmp_buf NativeDecoder::tile_buf;
jmp_buf NativeDecoder::strip_buf;
jmp_buf NativeDecoder::image_buf;
jmp_buf NativeDecoder::general_buf;

//Constructor for decoding from file descriptor
NativeDecoder::NativeDecoder(JNIEnv *e, jclass c, jint fd, jobject opts, jobject listener)
{

    decodingMode = DECODE_MODE_FILE_DESCRIPTOR;

    availableMemory = 8000*8000*4; // use 244Mb restriction for decoding full image
    env = e;
    clazz = c;
    optionsObject = opts;
    listenerObject = listener;
    jFd = fd;

    origwidth = 0;
    origheight = 0;
    origorientation = 0;
    origcompressionscheme = 0;
    progressTotal = 0;
    invertRedAndBlue = false;

    boundX = boundY = boundWidth = boundHeight = -1;
    hasBounds = 0;

    preferedConfig = NULL;
    image = NULL;

    jBitmapOptionsClass = env->FindClass(
                        "org/beyka/tiffbitmapfactory/TiffBitmapFactory$Options");
    jIProgressListenerClass = env->FindClass("org/beyka/tiffbitmapfactory/IProgressListener");
    jThreadClass = env->FindClass("java/lang/Thread");
    threadInterruptedMethodId = env->GetStaticMethodID(jThreadClass, "interrupted", "()Z");
    stoppedFieldId = env->GetFieldID(jBitmapOptionsClass, "isStoped", "Z");
    progressReportMethodId = listenerObject == NULL ? NULL :
            env->GetMethodID(jIProgressListenerClass, "reportProgress", "(JJ)V");
    lastProgressCurrent = -1;
    lastProgressTotal = -1;
}

//Constructor for decoding from file path
NativeDecoder::NativeDecoder(JNIEnv *e, jclass c, jstring path, jobject opts, jobject listener)
{

    decodingMode = DECODE_MODE_FILE_PATH;

    availableMemory = 8000*8000*4; // use 244Mb restriction for decoding full image
    env = e;
    clazz = c;
    optionsObject = opts;
    listenerObject = listener;
    jPath = path;
    jFd = 0;

    origwidth = 0;
    origheight = 0;
    origorientation = 0;
    origcompressionscheme = 0;
    progressTotal = 0;
    invertRedAndBlue = false;

    boundX = boundY = boundWidth = boundHeight = -1;
    hasBounds = 0;

    preferedConfig = NULL;
    image = NULL;

    jBitmapOptionsClass = env->FindClass(
                        "org/beyka/tiffbitmapfactory/TiffBitmapFactory$Options");
    jIProgressListenerClass = env->FindClass("org/beyka/tiffbitmapfactory/IProgressListener");
    jThreadClass = env->FindClass("java/lang/Thread");
    threadInterruptedMethodId = env->GetStaticMethodID(jThreadClass, "interrupted", "()Z");
    stoppedFieldId = env->GetFieldID(jBitmapOptionsClass, "isStoped", "Z");
    progressReportMethodId = listenerObject == NULL ? NULL :
            env->GetMethodID(jIProgressListenerClass, "reportProgress", "(JJ)V");
    lastProgressCurrent = -1;
    lastProgressTotal = -1;
}

NativeDecoder::~NativeDecoder()
{
    LOGI("Destructor");
    if (image) {
        TIFFClose(image);
        image = NULL;
    } else if (decodingMode == DECODE_MODE_FILE_DESCRIPTOR && jFd >= 0) {
        close(jFd);
        jFd = -1;
    }

    //Release global reference for Bitmap.Config
    if (preferedConfig) {
        env->DeleteGlobalRef(preferedConfig);
        preferedConfig = NULL;
    }

    if (jBitmapOptionsClass) {
        env->DeleteLocalRef(jBitmapOptionsClass);
        jBitmapOptionsClass = NULL;
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

jobject NativeDecoder::getBitmap()
{
        jfieldID rawField = env->GetFieldID(jBitmapOptionsClass, "inUseRawCoordinates", "Z");
        if (env->GetBooleanField(optionsObject, rawField)) {
            return getRawBitmap();
        }
        //init signal handler for catch SIGSEGV error that could be raised in libtiff
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        sigemptyset(&act.sa_mask);
        act.sa_sigaction = generalErrorHandler;
        act.sa_flags = SA_SIGINFO | SA_ONSTACK;
        if(sigaction(SIGSEGV, &act, 0) < 0) {
            LOGE("Can\'t setup signal handler. Working without errors catching mechanism");
        }

        //check for error
        if (setjmp(NativeDecoder::general_buf)) {
             const char * err = "Caught SIGSEGV signal(Segmentation fault or invalid memory reference)";
             LOGE(err);
             if (throwException) {
                 throwDecodeFileException(err);
             }
            return NULL;
        }

        //Get options from TiffBitmapFactory$Options
        jfieldID gOptions_ThrowExceptionFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                                          "inThrowException",
                                                                          "Z");
        throwException = env->GetBooleanField(optionsObject, gOptions_ThrowExceptionFieldID);

        jfieldID gOptions_UseOrientationTagFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                                                  "inUseOrientationTag",
                                                                                  "Z");
        useOrientationTag = env->GetBooleanField(optionsObject, gOptions_UseOrientationTagFieldID);

        jfieldID gOptions_sampleSizeFieldID = env->GetFieldID(jBitmapOptionsClass, "inSampleSize", "I");
        jint inSampleSize = env->GetIntField(optionsObject, gOptions_sampleSizeFieldID);
        LOGII("inSampleSize ", inSampleSize);
        if (inSampleSize <= 0) {
            const char *message = "inSampleSize should be a positive integer\0";
            LOGE(message);
            if (throwException) {
                throwDecodeFileException(message);
            }
            return NULL;
        }

        jfieldID gOptions_UseSinglePixelSampleFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                                        "inUseSinglePixelSample",
                                                                        "Z");
        useSinglePixelSample = env->GetBooleanField(optionsObject,
                                                    gOptions_UseSinglePixelSampleFieldID);

        jfieldID gOptions_justDecodeBoundsFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                                    "inJustDecodeBounds", "Z");
        jboolean inJustDecodeBounds = env->GetBooleanField(optionsObject, gOptions_justDecodeBoundsFieldID);

        jfieldID gOptions_invertRedAndBlueFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                                    "inSwapRedBlueColors", "Z");
        invertRedAndBlue = env->GetBooleanField(optionsObject, gOptions_invertRedAndBlueFieldID);

        jfieldID gOptions_DirectoryCountFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                                  "inDirectoryNumber",
                                                                  "I");
        jint inDirectoryNumber = env->GetIntField(optionsObject, gOptions_DirectoryCountFieldID);
        LOGII("param directoryCount", inDirectoryNumber);

        jfieldID gOptions_AvailableMemoryFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                                  "inAvailableMemory",
                                                                  "J");
        unsigned long inAvailableMemory = env->GetLongField(optionsObject, gOptions_AvailableMemoryFieldID);

        jfieldID gOptions_PreferedConfigFieldID = env->GetFieldID(jBitmapOptionsClass,
                                                                  "inPreferredConfig",
                                                                  "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageConfig;");
        jobject config = env->GetObjectField(optionsObject, gOptions_PreferedConfigFieldID);

        if (inAvailableMemory > 0) {
            availableMemory = inAvailableMemory;
        }

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

        jfieldID gOptions_DecodeAreaFieldId = env->GetFieldID(jBitmapOptionsClass, "inDecodeArea",
                                                                "Lorg/beyka/tiffbitmapfactory/DecodeArea;");
        jobject decodeArea = env->GetObjectField(optionsObject, gOptions_DecodeAreaFieldId);

        //if directory number < 0 set it to 0
        if (inDirectoryNumber < 0) inDirectoryNumber = 0;

        //Open tiff file
        const char *strPath = NULL;
        if (decodingMode == DECODE_MODE_FILE_DESCRIPTOR) {
            image = TIFFFdOpen(jFd, "", "r");
        } else if (decodingMode == DECODE_MODE_FILE_PATH) {
            strPath = env->GetStringUTFChars(jPath, 0);
            image = TIFFOpen(strPath, "r");
        }

        if (image == NULL) {
            if (throwException) {
                throwCantOpenFileException();
            }

            if (decodingMode == DECODE_MODE_FILE_PATH) {
                LOGES("Can\'t open bitmap", strPath);
                env->ReleaseStringUTFChars(jPath, strPath);
            } else {
                LOGEI("Can\'t open file descriptor", jFd);
            }
            return NULL;
        } else {
            if (decodingMode == DECODE_MODE_FILE_PATH) {
                env->ReleaseStringUTFChars(jPath, strPath);
            }
        }
        LOGI("Tiff is open");

        TIFFSetDirectory(image, inDirectoryNumber);
        TIFFGetField(image, TIFFTAG_IMAGEWIDTH, &origwidth);
        TIFFGetField(image, TIFFTAG_IMAGELENGTH, &origheight);

        //Read decode bounds if exists
        if (decodeArea) {
            LOGI("Decode bounds present");
            jclass decodeAreaClass = env->FindClass("org/beyka/tiffbitmapfactory/DecodeArea");
            jfieldID xFieldID = env->GetFieldID(decodeAreaClass, "x", "I");
            jfieldID yFieldID = env->GetFieldID(decodeAreaClass, "y", "I");
            jfieldID widthFieldID = env->GetFieldID(decodeAreaClass, "width", "I");
            jfieldID heightFieldID = env->GetFieldID(decodeAreaClass, "height", "I");

            boundX = env->GetIntField(decodeArea, xFieldID);
            boundY = env->GetIntField(decodeArea, yFieldID);
            boundWidth = env->GetIntField(decodeArea, widthFieldID);
            boundHeight = env->GetIntField(decodeArea, heightFieldID);
            if (boundX >= origwidth-1) {
                const char *message = "X of left top corner of decode area should be less than image width";
                LOGE(*message);
                if (throwException) {
                    throwDecodeFileException(message);
                }
                env->DeleteLocalRef(decodeAreaClass);
                return NULL;
            }
            if (boundY >= origheight-1) {
                const char *message = "Y of left top corner of decode area should be less than image height";
                LOGE(*message);
                if (throwException) {
                    throwDecodeFileException(message);
                }
                env->DeleteLocalRef(decodeAreaClass);
                return NULL;
            }

            if (boundX < 0) boundX = 0;
            if (boundY < 0) boundY = 0;
            if (boundX + boundWidth >= origwidth) boundWidth = origwidth - boundX -1;
            if (boundY + boundHeight >= origheight) boundHeight = origheight - boundY -1;

            if (boundWidth < 1) {
                const char *message = "Width of decode area can\'t be less than 1";
                LOGE(*message);
                if (throwException) {
                    throwDecodeFileException(message);
                }
                env->DeleteLocalRef(decodeAreaClass);
                return NULL;
            }
            if (boundHeight < 1) {
                const char *message = "Height of decode area can\'t be less than 1";
                LOGE(*message);
                if (throwException) {
                    throwDecodeFileException(message);
                }
                env->DeleteLocalRef(decodeAreaClass);
                return NULL;
            }

            LOGII("Decode X", boundX);
            LOGII("Decode Y", boundY);
            LOGII("Decode width", boundWidth);
            LOGII("Decode height", boundHeight);

            hasBounds = 1;
            env->DeleteLocalRef(decodeAreaClass);
            env->DeleteLocalRef(decodeArea);
        }

        jobject java_bitmap = NULL;

        writeDataToOptions(inDirectoryNumber);

        if (!inJustDecodeBounds) {
            progressTotal = origwidth * origheight;
            sendProgress(0, progressTotal);
            java_bitmap = createBitmap(inSampleSize, inDirectoryNumber);
        }

        return java_bitmap;
}

jobject NativeDecoder::createBitmap(int inSampleSize, int directoryNumber)
{
//Read Config from options. Use ordinal field from ImageConfig class
    jint configInt = ARGB_8888;
    if(preferedConfig) {
        jclass configClass = env->FindClass(
            "org/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageConfig");
        jfieldID ordinalFieldID = env->GetFieldID(configClass, "ordinal", "I");
        configInt = env->GetIntField(preferedConfig, ordinalFieldID);
        env->DeleteLocalRef(configClass);
    }

    int bitdepth = 1;
    TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bitdepth);
    if (bitdepth != 1 && bitdepth != 4 && bitdepth != 8 && bitdepth != 16) {
        const char * err = "Only 1, 4, 8 and 16 bits per sample are supported";
        LOGE(err);
        if (throwException) {
            throwDecodeFileException(err);
        }
        return NULL;
    }

    const int decodeMethod = getDecodeMethod();
    int newBitmapWidth = 0;
    int newBitmapHeight = 0;
    jint *raster = NULL;

    if (canDecodeNativeBilevel() || canDecodeNativeGray8() || canDecodeNativeRgb8()) {
        return createStreamingBitmap(inSampleSize, configInt);
    }

    if (!hasBounds && inSampleSize == 1 && configInt == ARGB_8888 &&
        !invertRedAndBlue && origorientation == ORIENTATION_TOPLEFT &&
        decodeMethod == DECODE_METHOD_IMAGE) {
        return createDirectArgbBitmap(origwidth, origheight);
    }

    if (canStreamToBitmap(inSampleSize)) {
        return createStreamingBitmap(inSampleSize, configInt);
    }

    if (raster == NULL && !hasBounds) {
        switch(decodeMethod) {
            case DECODE_METHOD_IMAGE:
                raster = getSampledRasterFromImage(inSampleSize, &newBitmapWidth, &newBitmapHeight);
                break;
            case DECODE_METHOD_TILE:
                raster = getSampledRasterFromTile(inSampleSize, &newBitmapWidth, &newBitmapHeight);
                break;
            case DECODE_METHOD_STRIP:
                raster = getSampledRasterFromStrip(inSampleSize,  &newBitmapWidth, &newBitmapHeight);
                break;
        }
    } else if (raster == NULL) {
        switch(decodeMethod) {
            case DECODE_METHOD_IMAGE:
                raster = getSampledRasterFromImageWithBounds(inSampleSize, &newBitmapWidth, &newBitmapHeight);
                break;
            case DECODE_METHOD_TILE:
                raster = getSampledRasterFromTileWithBounds(inSampleSize, &newBitmapWidth, &newBitmapHeight);
                break;
            case DECODE_METHOD_STRIP:
                raster = getSampledRasterFromStripWithBounds(inSampleSize,  &newBitmapWidth, &newBitmapHeight);
                break;
        }

    }

    if (raster == NULL) {
        return NULL;
    }

    // Convert ABGR to ARGB
    if (invertRedAndBlue) {
        int i = 0;
        int j = 0;
        int tmp = 0;
        for (i = 0; i < newBitmapHeight; i++) {
            for (j = 0; j < newBitmapWidth; j++) {
                tmp = raster[j + newBitmapWidth * i];
                raster[j + newBitmapWidth * i] =
                        (tmp & 0xff000000) | ((tmp & 0x00ff0000) >> 16) | (tmp & 0x0000ff00) |
                        ((tmp & 0xff) << 16);
            }
        }
    }

    sendProgress(progressTotal, progressTotal);

    if(checkStop()) {
        if (raster) {
            free(raster);
        }
        LOGI("Thread stopped");
        return NULL;
    }

    int destWidth = newBitmapWidth;
    int destHeight = newBitmapHeight;
    if (useOrientationTag && origorientation > 4) {
        destWidth = newBitmapHeight;
        destHeight = newBitmapWidth;
    }

    jobject java_bitmap = createConfiguredBitmap(destWidth, destHeight, configInt);
    if (java_bitmap == NULL) {
        if (raster) {
            free(raster);
        }
        LOGE("Error while decoding image");
        return NULL;
    }

    if(checkStop()) {
        if (raster) {
            free(raster);
        }
        env->DeleteLocalRef(java_bitmap);
        LOGI("Thread stopped");
        return NULL;
    }

    if (!packRasterToBitmap(raster, destWidth, destHeight, java_bitmap, configInt)) {
        LOGE("Lock pixels failed");
        if (raster) {
            free(raster);
        }
        env->DeleteLocalRef(java_bitmap);
        return NULL;
    }

    free(raster);
    return java_bitmap;
}

bool NativeDecoder::canDecodeBilevelCcittStreaming() {
    if (TIFFIsTiled(image)) {
        return false;
    }

    if (origcompressionscheme != COMPRESSION_CCITTRLE &&
        origcompressionscheme != COMPRESSION_CCITTRLEW &&
        origcompressionscheme != COMPRESSION_CCITTFAX3 &&
        origcompressionscheme != COMPRESSION_CCITTFAX4) {
        return false;
    }

    uint16_t bitsPerSample = 0;
    uint16_t samplesPerPixel = 0;
    uint16_t photometric = 0;
    TIFFGetFieldDefaulted(image, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetFieldDefaulted(image, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photometric);

    return bitsPerSample == 1 && samplesPerPixel == 1 &&
           (photometric == PHOTOMETRIC_MINISWHITE ||
            photometric == PHOTOMETRIC_MINISBLACK);
}

jint *NativeDecoder::getSampledBilevelRaster(int inSampleSize, int *bitmapwidth,
                                              int *bitmapheight) {
    const int sourceX = hasBounds ? boundX : 0;
    const int sourceY = hasBounds ? boundY : 0;
    const int sourceWidth = hasBounds ? boundWidth : origwidth;
    const int sourceHeight = hasBounds ? boundHeight : origheight;

    *bitmapwidth = sourceWidth / inSampleSize;
    *bitmapheight = sourceHeight / inSampleSize;
    if (*bitmapwidth <= 0 || *bitmapheight <= 0) {
        return NULL;
    }

    const tmsize_t scanlineSize = TIFFScanlineSize(image);
    if (scanlineSize <= 0) {
        if (throwException) {
            throwDecodeFileException("Invalid bilevel TIFF scanline size");
        }
        return NULL;
    }

    const uint64_t pixelCount = static_cast<uint64_t>(*bitmapwidth) *
                                static_cast<uint64_t>(*bitmapheight);
    uint64_t orientationMemory = 0;
    if (useOrientationTag && origorientation > ORIENTATION_BOTLEFT) {
        orientationMemory = pixelCount * sizeof(bool);
    } else if (!useOrientationTag &&
               (origorientation == ORIENTATION_BOTRIGHT ||
                origorientation == ORIENTATION_RIGHTBOT ||
                origorientation == ORIENTATION_BOTLEFT ||
                origorientation == ORIENTATION_LEFTBOT)) {
        orientationMemory = pixelCount * sizeof(jint);
    }
    const uint64_t estimateMem = pixelCount * sizeof(jint) +
                                 static_cast<uint64_t>(scanlineSize) +
                                 orientationMemory;
    if (estimateMem > availableMemory || pixelCount > SIZE_MAX / sizeof(jint) ||
        pixelCount > UINT32_MAX) {
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return NULL;
    }

    jint *pixels = static_cast<jint *>(malloc(static_cast<size_t>(pixelCount) *
                                               sizeof(jint)));
    uint8_t *scanline = static_cast<uint8_t *>(_TIFFmalloc(scanlineSize));
    if (pixels == NULL || scanline == NULL) {
        free(pixels);
        if (scanline != NULL) {
            _TIFFfree(scanline);
        }
        if (throwException) {
            throwDecodeFileException("Cannot allocate bilevel decode buffers");
        }
        return NULL;
    }

    uint16_t photometric = PHOTOMETRIC_MINISWHITE;
    TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photometric);
    const bool msbFirst = TIFFIsMSB2LSB(image) != 0;
    const int lastSourceY = sourceY + (*bitmapheight - 1) * inSampleSize;
    int outputY = 0;

    for (int row = 0; row <= lastSourceY; ++row) {
        if (checkStop()) {
            free(pixels);
            _TIFFfree(scanline);
            return NULL;
        }

        if (TIFFReadScanline(image, scanline, static_cast<uint32_t>(row), 0) < 0) {
            free(pixels);
            _TIFFfree(scanline);
            if (throwException) {
                throwDecodeFileException("Error reading CCITT scanline");
            }
            return NULL;
        }

        sendProgress(static_cast<jlong>(row) * origwidth, progressTotal);
        if (row < sourceY || (row - sourceY) % inSampleSize != 0) {
            continue;
        }

        for (int outputX = 0; outputX < *bitmapwidth; ++outputX) {
            const int sourcePixelX = sourceX + outputX * inSampleSize;
            const uint8_t packed = scanline[sourcePixelX >> 3];
            const uint8_t mask = msbFirst
                    ? static_cast<uint8_t>(0x80U >> (sourcePixelX & 7))
                    : static_cast<uint8_t>(1U << (sourcePixelX & 7));
            const bool sampleIsOne = (packed & mask) != 0;
            const bool black = photometric == PHOTOMETRIC_MINISWHITE
                    ? sampleIsOne
                    : !sampleIsOne;
            pixels[outputY * *bitmapwidth + outputX] =
                    black ? static_cast<jint>(0xFF000000U)
                          : static_cast<jint>(0xFFFFFFFFU);
        }
        ++outputY;
    }

    _TIFFfree(scanline);

    if (useOrientationTag) {
        fixOrientation(pixels, static_cast<uint32_t>(pixelCount), *bitmapwidth,
                       *bitmapheight);
    } else {
        switch (origorientation) {
            case ORIENTATION_TOPLEFT:
            case ORIENTATION_LEFTTOP:
                break;
            case ORIENTATION_TOPRIGHT:
            case ORIENTATION_RIGHTTOP:
                flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                break;
            case ORIENTATION_BOTRIGHT:
            case ORIENTATION_RIGHTBOT:
                rotateRaster(pixels, 180, bitmapwidth, bitmapheight);
                break;
            case ORIENTATION_BOTLEFT:
            case ORIENTATION_LEFTBOT:
                rotateRaster(pixels, 180, bitmapwidth, bitmapheight);
                flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                break;
        }
    }

    return pixels;
}

jobject NativeDecoder::createDirectArgbBitmap(int width, int height) {
    jclass bitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
    jfieldID argbField = env->GetStaticFieldID(bitmapConfigClass, "ARGB_8888",
            "Landroid/graphics/Bitmap$Config;");
    jobject config = env->GetStaticObjectField(bitmapConfigClass, argbField);
    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createMethod = env->GetStaticMethodID(bitmapClass, "createBitmap",
            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject bitmap = env->CallStaticObjectMethod(bitmapClass, createMethod, width, height, config);

    env->DeleteLocalRef(config);
    env->DeleteLocalRef(bitmapConfigClass);
    env->DeleteLocalRef(bitmapClass);

    if (bitmap == NULL || env->ExceptionCheck()) {
        return NULL;
    }

    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0 ||
        info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 ||
        info.stride != static_cast<uint32_t>(width * sizeof(uint32_t))) {
        env->DeleteLocalRef(bitmap);
        return NULL;
    }

    void *bitmapPixels = NULL;
    if (AndroidBitmap_lockPixels(env, bitmap, &bitmapPixels) < 0) {
        env->DeleteLocalRef(bitmap);
        return NULL;
    }

    sendProgress(0, progressTotal);
    const int readResult = TIFFReadRGBAImageOriented(image, width, height,
            static_cast<uint32_t *>(bitmapPixels), ORIENTATION_TOPLEFT, 0);
    AndroidBitmap_unlockPixels(env, bitmap);

    if (readResult == 0) {
        const char *message = "Error reading image";
        env->DeleteLocalRef(bitmap);
        if (throwException) {
            throwDecodeFileException(message);
        }
        return NULL;
    }
    if (checkStop()) {
        env->DeleteLocalRef(bitmap);
        return NULL;
    }
    sendProgress(progressTotal, progressTotal);
    return bitmap;
}

jint * NativeDecoder::getSampledRasterFromStrip(int inSampleSize, int *bitmapwidth, int *bitmapheight) {

    //init signal handler for catch SIGSEGV error that could be raised in libtiff
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = stripErrorHandler;
    act.sa_flags = SA_SIGINFO | SA_ONSTACK;
    if(sigaction(SIGSEGV, &act, 0) < 0) {
        LOGE("Can\'t setup signal handler. Working without errors catching mechanism");
    }

    LOGII("width", origwidth);
    LOGII("height", origheight);

    jint *pixels = NULL;
    *bitmapwidth = origwidth / inSampleSize;
    *bitmapheight = origheight / inSampleSize;
    uint32_t pixelsBufferSize = *bitmapwidth * *bitmapheight;
    int origImageBufferSize = origwidth * origheight;

    LOGII("new width", *bitmapwidth);
    LOGII("new height", *bitmapheight);

    uint32_t stripSize = TIFFStripSize (image);
    uint32_t stripMax = TIFFNumberOfStrips (image);
    LOGII("strip size ", stripSize);
    LOGII("stripMax  ", stripMax);
    int rowPerStrip = -1;
    TIFFGetField(image, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);
    LOGII("rowsperstrip", rowPerStrip);

    unsigned long estimateMem = 0;
    estimateMem += (sizeof(jint) * pixelsBufferSize); //buffer for decoded pixels
    estimateMem += (origwidth * sizeof(uint32_t)); //work line for rotate strip
    estimateMem += (origwidth * rowPerStrip * sizeof (uint32_t) * 2); //current and next strips
    estimateMem += (sizeof(jint) * origwidth * 2); //bottom and top lines for reading pixel(matrixBottomLine, matrixTopLine)
    LOGII("estimateMem", estimateMem);
    if (estimateMem > availableMemory) {
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return NULL;
    }

    pixels = (jint *) malloc(sizeof(jint) * pixelsBufferSize);
    if (pixels == NULL) {
        LOGE("Can\'t allocate memory for temp buffer");
        return NULL;
    }

    uint32_t* work_line_buf = (uint32_t *)_TIFFmalloc(origwidth * sizeof(uint32_t));

    uint32_t* raster;
    uint32_t* rasterForBottomLine; // in this raster copy next strip for getting bottom line in matrix color selection
    if (rowPerStrip == -1 && stripMax == 1) {
            raster = (uint32_t *)_TIFFmalloc(origImageBufferSize * sizeof (uint32_t));
            rasterForBottomLine = (uint32_t *)_TIFFmalloc(origImageBufferSize * sizeof (uint32_t));
    } else {
            raster = (uint32_t *)_TIFFmalloc(origwidth * rowPerStrip * sizeof (uint32_t));
            rasterForBottomLine = (uint32_t *)_TIFFmalloc(origwidth * rowPerStrip * sizeof (uint32_t));
    }
    if (rowPerStrip == -1) {
            rowPerStrip = origheight;
    }

    int writedLines = 0;
    int nextStripOffset = 0;
    int globalLineCounter = 0;

    unsigned int *matrixTopLine = (uint32_t *) malloc(sizeof(jint) * origwidth);
    unsigned int *matrixBottomLine = (uint32_t *) malloc(sizeof(jint) * origwidth);

    int isSecondRasterExist = 0;
    int ok = 1;
    uint32_t rows_to_write = 0;

    //check for error
    if (setjmp(NativeDecoder::strip_buf)) {
        if (raster) {
            _TIFFfree(raster);
            raster = NULL;
        }
        if (rasterForBottomLine) {
            _TIFFfree(rasterForBottomLine);
            rasterForBottomLine = NULL;
        }
        if (matrixTopLine) {
            _TIFFfree(matrixTopLine);
            matrixTopLine = NULL;
        }
        if (matrixBottomLine) {
            _TIFFfree(matrixBottomLine);
            matrixBottomLine = NULL;
        }

        const char * err = "Caught SIGSEGV signal(Segmentation fault or invalid memory reference)";
        LOGE(err);
        if (throwException) {
            throwDecodeFileException(err);
        }

        return NULL;
    }

    for (int i = 0; i < stripMax*rowPerStrip; i += rowPerStrip) {

            sendProgress(i * origwidth, progressTotal);

            //if second raster is exist - copy it to work raster end decode next strip
            if (isSecondRasterExist) {
                uint32_t *previousRaster = raster;
                raster = rasterForBottomLine;
                rasterForBottomLine = previousRaster;

                //If next strip is exist - decode it, invert lines
                if (i + rowPerStrip < stripMax*rowPerStrip) {
                    TIFFReadRGBAStrip(image, i+rowPerStrip, rasterForBottomLine);
                    isSecondRasterExist = 1;

                    rows_to_write = 0;
                    if ( i + rowPerStrip * 2 > origheight )
                        rows_to_write = origheight - i - rowPerStrip;
                    else
                        rows_to_write = rowPerStrip;

                    if (origorientation <= 4) {
                        for (int line = 0; line < rows_to_write / 2; line++) {
                            unsigned int  *top_line, *bottom_line;

                            top_line = rasterForBottomLine + origwidth * line;
                            bottom_line = rasterForBottomLine + origwidth * (rows_to_write - line - 1);

                            _TIFFmemcpy(work_line_buf, top_line, sizeof(unsigned int) * origwidth);
                            _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * origwidth);
                            _TIFFmemcpy(bottom_line, work_line_buf, sizeof(unsigned int) * origwidth);
                        }
                    }
                } else {
                    isSecondRasterExist = 0;
                }
            } else {
                //if second raster is not exist - first processing - read first and second raster
                 TIFFReadRGBAStrip(image, i, raster);
                 //invert lines, because libtiff origin is bottom left instead of top left
                 rows_to_write = 0;
                 if( i + rowPerStrip > origheight )
                    rows_to_write = origheight - i;
                 else
                    rows_to_write = rowPerStrip;

                 if (origorientation <= 4) {
                     for (int line = 0; line < rows_to_write / 2; line++) {
                         unsigned int  *top_line, *bottom_line;

                         top_line = raster + origwidth * line;
                         bottom_line = raster + origwidth * (rows_to_write - line - 1);

                         _TIFFmemcpy(work_line_buf, top_line, sizeof(unsigned int) * origwidth);
                         _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * origwidth);
                         _TIFFmemcpy(bottom_line, work_line_buf, sizeof(unsigned int) * origwidth);
                     }
                 }

                 //if next strip is exist - read it and invert lines
                 if (i + rowPerStrip < origheight) {
                    TIFFReadRGBAStrip(image, i+rowPerStrip, rasterForBottomLine);
                    isSecondRasterExist = 1;

                    //invert lines, because libtiff origin is bottom left instead of top left
                    rows_to_write = 0;
                    if ( i + rowPerStrip * 2 > origheight )
                        rows_to_write = origheight - i - rowPerStrip;
                    else
                        rows_to_write = rowPerStrip;
                    if (origorientation <= 4) {
                        for (int line = 0; line < rows_to_write / 2; line++) {
                            unsigned int  *top_line, *bottom_line;

                            top_line = rasterForBottomLine + origwidth * line;
                            bottom_line = rasterForBottomLine + origwidth * (rows_to_write - line - 1);

                            _TIFFmemcpy(work_line_buf, top_line, sizeof(unsigned int) * origwidth);
                            _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * origwidth);
                            _TIFFmemcpy(bottom_line, work_line_buf, sizeof(unsigned int) * origwidth);
                        }
                    }
                 }

            }

            if (inSampleSize == 1) {
                int byteToCopy = 0;
                if (i + rowPerStrip < origheight) {
                    byteToCopy = sizeof(unsigned int) * rowPerStrip * origwidth;
                } else {
                    byteToCopy = sizeof(unsigned int) * rows_to_write * origwidth;
                }
                int position = i * origwidth;
                memcpy(&pixels[position], raster, byteToCopy);
                //sendProgress(position, progressTotal);
            } else {
                if (isSecondRasterExist) {
                    _TIFFmemcpy(matrixBottomLine, rasterForBottomLine /*+ lineAddrToCopyBottomLine * origwidth*/, sizeof(unsigned int) * origwidth);
                }
                 int workWritedLines = writedLines;
                 for (int resBmpY = workWritedLines, workY = 0; resBmpY < *bitmapheight && workY < rowPerStrip; /*wj++,*/ workY ++/*= inSampleSize*/) {

                 if (checkStop()) {
                     if (raster) {
                         _TIFFfree(raster);
                         raster = NULL;
                     }
                     if (rasterForBottomLine) {
                         _TIFFfree(rasterForBottomLine);
                         rasterForBottomLine = NULL;
                     }
                     if (matrixTopLine) {
                         _TIFFfree(matrixTopLine);
                         matrixTopLine = NULL;
                     }
                     if (matrixBottomLine) {
                         _TIFFfree(matrixBottomLine);
                         matrixBottomLine = NULL;
                     }
                     LOGI("Thread stopped");
                     return NULL;
                 }

                    // if total line of source image is equal to inSampleSize*N then process this line
                    if (globalLineCounter % inSampleSize == 0) {
                        for (int resBmpX = 0, workX = 0; resBmpX < *bitmapwidth; resBmpX++, workX += inSampleSize) {

                            //Apply filter to pixel
                            jint crPix = raster[workY * origwidth + workX];
                            int sum = 1;


                            int alpha = colorMask & crPix >> 24;
                            int red = colorMask & crPix >> 16;
                            int green = colorMask & crPix >> 8;
                            int blue = colorMask & crPix;

                            if (!useSinglePixelSample) {

                            //topleft
                            if (workX - 1 >= 0 && workY - 1 >= 0) {
                                crPix = raster[(workY - 1) * origwidth + workX - 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workX - 1 >= 0 && workY - 1 == -1 && globalLineCounter > 0) {
                                crPix = matrixTopLine[workX - 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }
                            //top
                            if (workY - 1 >= 0) {
                                crPix = raster[(workY - 1) * origwidth + workX];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workY - 1 == -1 && globalLineCounter > 0) {
                                crPix = matrixTopLine[workX];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }

                            // topright
                            if (workX + 1 < origwidth && workY - 1 >= 0) {
                                crPix = raster[(workY - 1) * origwidth + workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workX + 1 < origwidth && workY - 1 == -1 && globalLineCounter > 0) {
                                crPix = matrixTopLine[workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }

                            //right
                            if (workX + 1 < origwidth) {
                                crPix = raster[workY * origwidth + workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }

                            //bottomright
                            if (workX + 1 < origwidth && workY + 1 < rowPerStrip) {
                                crPix = raster[(workY + 1) * origwidth + workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workX + 1 < origwidth && workY + 1 == rowPerStrip && isSecondRasterExist) {
                                crPix = matrixBottomLine[workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }

                            //bottom
                            if (workY + 1 < rowPerStrip) {
                                crPix = raster[(workY + 1) * origwidth + workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workY + 1 == rowPerStrip && isSecondRasterExist) {
                                crPix = matrixBottomLine[workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }

                            //bottomleft
                            if (workX - 1 >= 0 && workY + 1 < rowPerStrip) {
                                crPix = raster[(workY + 1) * origwidth + workX - 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workX - 1 >= 0 && workY + 1 == rowPerStrip  && isSecondRasterExist) {
                                crPix = matrixBottomLine[workX - 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }


                            //left
                            if (workX - 1 >= 0) {
                                crPix = raster[workY * origwidth + workX - 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }
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

                            pixels[resBmpY * *bitmapwidth + resBmpX] = crPix;
                        }
                        //if line was processed - increment counter of lines that was writed to result image
                        writedLines++;
                        //and incremetncounter of current Y for writing
                        resBmpY++;
                    }
                    if (workY == rowPerStrip - 1 && i + rowPerStrip < origheight) {
                        _TIFFmemcpy(matrixTopLine, raster + workY * origwidth, sizeof(unsigned int) * origwidth);
                    }
                    //incremetn global source image line counter
                    globalLineCounter++;

                }
            }
        }
        LOGI("Decoding finished. Free memmory");

        //Close Buffers
        if (raster) {
            _TIFFfree(raster);
            raster = NULL;
        }

        if (rasterForBottomLine) {
            _TIFFfree(rasterForBottomLine);
            rasterForBottomLine = NULL;
        }

        if (matrixTopLine) {
            _TIFFfree(matrixTopLine);
            matrixTopLine = NULL;
        }

        if (matrixBottomLine) {
            _TIFFfree(matrixBottomLine);
            matrixBottomLine = NULL;
        }

        if (useOrientationTag) {
            uint32_t buf;
            //fixOrientation(pixels, pixelsBufferSize, *bitmapwidth, *bitmapheight);
            switch(origorientation) {
                 case ORIENTATION_TOPLEFT:
                 case ORIENTATION_TOPRIGHT:
                    break;
                 case ORIENTATION_BOTRIGHT:
                 case ORIENTATION_BOTLEFT:
                    flipPixelsVertical(*bitmapwidth, *bitmapheight, pixels);
                    break;
                 case ORIENTATION_LEFTTOP:
                    rotateRaster(pixels, 90, bitmapwidth, bitmapheight);
                    flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                    buf = *bitmapwidth;
                    *bitmapwidth = *bitmapheight;
                    *bitmapheight = buf;
                    break;
                 case ORIENTATION_RIGHTTOP:
                    rotateRaster(pixels, 270, bitmapwidth, bitmapheight);
                    flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                    buf = *bitmapwidth;
                    *bitmapwidth = *bitmapheight;
                    *bitmapheight = buf;
                    break;
                 case ORIENTATION_RIGHTBOT:
                    rotateRaster(pixels, 90, bitmapwidth, bitmapheight);
                    buf= *bitmapwidth;
                    *bitmapwidth = *bitmapheight;
                    *bitmapheight = buf;
                    break;
                 case ORIENTATION_LEFTBOT:
                    rotateRaster(pixels, 270, bitmapwidth, bitmapheight);
                    buf = *bitmapwidth;
                    *bitmapwidth = *bitmapheight;
                    *bitmapheight = buf;
                    break;
            }

        } else if (origorientation == 2 || origorientation == 3 || origorientation == 6 || origorientation == 7) {
            flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
        }

        return pixels;
}

jint * NativeDecoder::getSampledRasterFromStripWithBounds(int inSampleSize, int *bitmapwidth, int *bitmapheight) {

    //init signal handler for catch SIGSEGV error that could be raised in libtiff
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = stripErrorHandler;
    act.sa_flags = SA_SIGINFO | SA_ONSTACK;
    if(sigaction(SIGSEGV, &act, 0) < 0) {
        LOGE("Can\'t setup signal handler. Working without errors catching mechanism");
    }

    LOGII("width", origwidth);
    LOGII("height", origheight);

    jint *pixels = NULL;
    *bitmapwidth = origwidth / inSampleSize;
    *bitmapheight = boundHeight / inSampleSize;//origheight / inSampleSize;
    uint32_t pixelsBufferSize = *bitmapwidth * *bitmapheight;
    int origImageBufferSize = origwidth * origheight;

    LOGII("new width", *bitmapwidth);
    LOGII("new height", *bitmapheight);

    uint32_t stripSize = TIFFStripSize (image);
    uint32_t stripMax = TIFFNumberOfStrips (image);
    LOGII("strip size ", stripSize);
    LOGII("stripMax  ", stripMax);
    int rowPerStrip = -1;
    TIFFGetField(image, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);
    LOGII("rowsperstrip", rowPerStrip);

    unsigned long estimateMem = 0;
    estimateMem += (sizeof(jint) * pixelsBufferSize); //temp buffer for decoded pixels
    estimateMem += (sizeof(jint) * (boundWidth / inSampleSize) * (boundHeight/inSampleSize)); //final buffer that will store original image
    estimateMem += (origwidth * sizeof(uint32_t)); //work line for rotate strip
    estimateMem += (origwidth * rowPerStrip * sizeof (uint32_t) * 2); //current and next strips
    estimateMem += (sizeof(jint) * origwidth * 2); //bottom and top lines for reading pixel(matrixBottomLine, matrixTopLine)
    LOGII("estimateMem", estimateMem);
    if (estimateMem > availableMemory) {
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return NULL;
    }

    progressTotal = pixelsBufferSize + (boundWidth/inSampleSize) * (boundHeight / inSampleSize);
    sendProgress(0, progressTotal);
    jlong processedProgress = 0;

    pixels = (jint *) malloc(sizeof(jint) * pixelsBufferSize);
    if (pixels == NULL) {
        LOGE("Can\'t allocate memory for temp buffer");
        return NULL;
    }

    uint32_t* work_line_buf = (uint32_t *)_TIFFmalloc(origwidth * sizeof(uint32_t));

    uint32_t* raster;
    uint32_t* rasterForBottomLine; // in this raster copy next strip for getting bottom line in matrix color selection
    if (rowPerStrip == -1 && stripMax == 1) {
            raster = (uint32_t *)_TIFFmalloc(origImageBufferSize * sizeof (uint32_t));
            rasterForBottomLine = (uint32_t *)_TIFFmalloc(origImageBufferSize * sizeof (uint32_t));
    } else {
            raster = (uint32_t *)_TIFFmalloc(origwidth * rowPerStrip * sizeof (uint32_t));
            rasterForBottomLine = (uint32_t *)_TIFFmalloc(origwidth * rowPerStrip * sizeof (uint32_t));
    }
    if (rowPerStrip == -1) {
            rowPerStrip = origheight;
    }

    int writedLines = 0;
    int nextStripOffset = 0;
    int globalLineCounter = 0;

    unsigned int *matrixTopLine = (uint32_t *) malloc(sizeof(jint) * origwidth);
    unsigned int *matrixBottomLine = (uint32_t *) malloc(sizeof(jint) * origwidth);

    int isSecondRasterExist = 0;
    int ok = 1;
    uint32_t rows_to_write = 0;

    //check for error
    if (setjmp(NativeDecoder::strip_buf)) {
        if (raster) {
            _TIFFfree(raster);
            raster = NULL;
        }
        if (rasterForBottomLine) {
            _TIFFfree(rasterForBottomLine);
            rasterForBottomLine = NULL;
        }
        if (matrixTopLine) {
            _TIFFfree(matrixTopLine);
            matrixTopLine = NULL;
        }
        if (matrixBottomLine) {
            _TIFFfree(matrixBottomLine);
            matrixBottomLine = NULL;
        }

        const char * err = "Caught SIGSEGV signal(Segmentation fault or invalid memory reference)";
        LOGE(err);
        if (throwException) {
            throwDecodeFileException(err);
        }

        return NULL;
    }

    for (int i = 0; i < stripMax * rowPerStrip && i <= boundY + boundHeight;
         i += rowPerStrip) {

            if (i + rowPerStrip <= boundY) {
                continue;
            }
            if (i > boundY + boundHeight) {
                break;
            }

            sendProgress(processedProgress * *bitmapwidth, progressTotal);
            processedProgress += rowPerStrip/inSampleSize;

            //if second raster is exist - copy it to work raster end decode next strip
            if (isSecondRasterExist) {
                uint32_t *previousRaster = raster;
                raster = rasterForBottomLine;
                rasterForBottomLine = previousRaster;

                //If next strip is exist - decode it, invert lines
                if (i + rowPerStrip < stripMax * rowPerStrip &&
                    i + rowPerStrip <= boundY + boundHeight) {
                    TIFFReadRGBAStrip(image, i+rowPerStrip, rasterForBottomLine);
                    isSecondRasterExist = 1;

                    rows_to_write = 0;
                    if ( i + rowPerStrip * 2 > origheight )
                        rows_to_write = origheight - i - rowPerStrip;
                    else
                        rows_to_write = rowPerStrip;

                    if (origorientation <= 4) {
                        for (int line = 0; line < rows_to_write / 2; line++) {
                            unsigned int  *top_line, *bottom_line;

                            top_line = rasterForBottomLine + origwidth * line;
                            bottom_line = rasterForBottomLine + origwidth * (rows_to_write - line - 1);

                            _TIFFmemcpy(work_line_buf, top_line, sizeof(unsigned int) * origwidth);
                            _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * origwidth);
                            _TIFFmemcpy(bottom_line, work_line_buf, sizeof(unsigned int) * origwidth);
                        }
                    }
                } else {
                    isSecondRasterExist = 0;
                }
            } else {
                //if second raster is not exist - first processing - read first and second raster
                 TIFFReadRGBAStrip(image, i, raster);
                 //invert lines, because libtiff origin is bottom left instead of top left
                 rows_to_write = 0;
                 if( i + rowPerStrip > origheight )
                    rows_to_write = origheight - i;
                 else
                    rows_to_write = rowPerStrip;

                 if (origorientation <= 4) {
                     for (int line = 0; line < rows_to_write / 2; line++) {
                         unsigned int  *top_line, *bottom_line;

                         top_line = raster + origwidth * line;
                         bottom_line = raster + origwidth * (rows_to_write - line - 1);

                         _TIFFmemcpy(work_line_buf, top_line, sizeof(unsigned int) * origwidth);
                         _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * origwidth);
                         _TIFFmemcpy(bottom_line, work_line_buf, sizeof(unsigned int) * origwidth);
                     }
                 }

                 //if next strip is exist - read it and invert lines
                 if (i + rowPerStrip < origheight &&
                     i + rowPerStrip <= boundY + boundHeight) {
                    TIFFReadRGBAStrip(image, i+rowPerStrip, rasterForBottomLine);
                    isSecondRasterExist = 1;

                    //invert lines, because libtiff origin is bottom left instead of top left
                    rows_to_write = 0;
                    if ( i + rowPerStrip * 2 > origheight )
                        rows_to_write = origheight - i - rowPerStrip;
                    else
                        rows_to_write = rowPerStrip;
                    if (origorientation <= 4) {
                        for (int line = 0; line < rows_to_write / 2; line++) {
                            unsigned int  *top_line, *bottom_line;

                            top_line = rasterForBottomLine + origwidth * line;
                            bottom_line = rasterForBottomLine + origwidth * (rows_to_write - line - 1);

                            _TIFFmemcpy(work_line_buf, top_line, sizeof(unsigned int) * origwidth);
                            _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * origwidth);
                            _TIFFmemcpy(bottom_line, work_line_buf, sizeof(unsigned int) * origwidth);
                        }
                    }
                 }

            }

            /*if (inSampleSize == 1) {
                int byteToCopy = 0;
                if (i + rowPerStrip < origheight) {
                    byteToCopy = sizeof(unsigned int) * rowPerStrip * origwidth;
                } else {
                    byteToCopy = sizeof(unsigned int) * rows_to_write * origwidth;
                }
                int position = i * origwidth;
                memcpy(&pixels[position], raster, byteToCopy);
                //sendProgress(position, progressTotal);
            } else {*/
                if (isSecondRasterExist) {
                    _TIFFmemcpy(matrixBottomLine, rasterForBottomLine /*+ lineAddrToCopyBottomLine * origwidth*/, sizeof(unsigned int) * origwidth);
                }
                 int workWritedLines = writedLines;
                 for (int resBmpY = workWritedLines, workY = 0; resBmpY < *bitmapheight && workY < rowPerStrip; /*wj++,*/ workY ++/*= inSampleSize*/) {

                 if (checkStop()) {
                     if (raster) {
                         _TIFFfree(raster);
                         raster = NULL;
                     }
                     if (rasterForBottomLine) {
                         _TIFFfree(rasterForBottomLine);
                         rasterForBottomLine = NULL;
                     }
                     if (matrixTopLine) {
                         _TIFFfree(matrixTopLine);
                         matrixTopLine = NULL;
                     }
                     if (matrixBottomLine) {
                         _TIFFfree(matrixBottomLine);
                         matrixBottomLine = NULL;
                     }
                     LOGI("Thread stopped");
                     return NULL;
                 }

                    // if total line of source image is equal to inSampleSize*N then process this line
                    if (globalLineCounter % inSampleSize == 0) {
                        for (int resBmpX = 0, workX = 0; resBmpX < *bitmapwidth; workX += inSampleSize) {

                            /*if (workX <= boundX) {
                                continue;
                            }
                            if (workX > boundX + boundWidth) {
                                break;
                            }
                            LOGII("J", workX);*/

                            //Apply filter to pixel
                            jint crPix = raster[workY * origwidth + workX];
                            int sum = 1;


                            int alpha = colorMask & crPix >> 24;
                            int red = colorMask & crPix >> 16;
                            int green = colorMask & crPix >> 8;
                            int blue = colorMask & crPix;

                            if (!useSinglePixelSample) {

                            //topleft
                            if (workX - 1 >= 0 && workY - 1 >= 0) {
                                crPix = raster[(workY - 1) * origwidth + workX - 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workX - 1 >= 0 && workY - 1 == -1 && globalLineCounter > 0) {
                                crPix = matrixTopLine[workX - 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }
                            //top
                            if (workY - 1 >= 0) {
                                crPix = raster[(workY - 1) * origwidth + workX];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workY - 1 == -1 && globalLineCounter > 0) {
                                crPix = matrixTopLine[workX];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }

                            // topright
                            if (workX + 1 < origwidth && workY - 1 >= 0) {
                                crPix = raster[(workY - 1) * origwidth + workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workX + 1 < origwidth && workY - 1 == -1 && globalLineCounter > 0) {
                                crPix = matrixTopLine[workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }

                            //right
                            if (workX + 1 < origwidth) {
                                crPix = raster[workY * origwidth + workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }

                            //bottomright
                            if (workX + 1 < origwidth && workY + 1 < rowPerStrip) {
                                crPix = raster[(workY + 1) * origwidth + workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workX + 1 < origwidth && workY + 1 == rowPerStrip && isSecondRasterExist) {
                                crPix = matrixBottomLine[workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }

                            //bottom
                            if (workY + 1 < rowPerStrip) {
                                crPix = raster[(workY + 1) * origwidth + workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workY + 1 == rowPerStrip && isSecondRasterExist) {
                                crPix = matrixBottomLine[workX + 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }

                            //bottomleft
                            if (workX - 1 >= 0 && workY + 1 < rowPerStrip) {
                                crPix = raster[(workY + 1) * origwidth + workX - 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            } else if (workX - 1 >= 0 && workY + 1 == rowPerStrip  && isSecondRasterExist) {
                                crPix = matrixBottomLine[workX - 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }


                            //left
                            if (workX - 1 >= 0) {
                                crPix = raster[workY * origwidth + workX - 1];
                                red += colorMask & crPix >> 16;
                                green += colorMask & crPix >> 8;
                                blue += colorMask & crPix;
                                alpha += colorMask & crPix >> 24;
                                sum++;
                            }
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

                            pixels[resBmpY * *bitmapwidth + resBmpX] = crPix;

                            resBmpX++;
                        }
                        //if line was processed - increment counter of lines that was writed to result image
                        writedLines++;
                        //and incremetncounter of current Y for writing
                        resBmpY++;
                    }
                    if (workY == rowPerStrip - 1 && i + rowPerStrip < origheight) {
                        _TIFFmemcpy(matrixTopLine, raster + workY * origwidth, sizeof(unsigned int) * origwidth);
                    }
                    //incremetn global source image line counter
                    globalLineCounter++;

                }
            /*}*/
        }
        LOGI("Decoding finished. Free memmory");

        //Close Buffers
        if (raster) {
            _TIFFfree(raster);
            raster = NULL;
        }

        if (rasterForBottomLine) {
            _TIFFfree(rasterForBottomLine);
            rasterForBottomLine = NULL;
        }

        if (matrixTopLine) {
            _TIFFfree(matrixTopLine);
            matrixTopLine = NULL;
        }

        if (matrixBottomLine) {
            _TIFFfree(matrixBottomLine);
            matrixBottomLine = NULL;
        }

        processedProgress *= *bitmapwidth;

        if (useOrientationTag) {
            uint32_t buf;
            switch(origorientation) {
                 case ORIENTATION_TOPLEFT:
                 case ORIENTATION_TOPRIGHT:
                    break;
                 case ORIENTATION_BOTRIGHT:
                 case ORIENTATION_BOTLEFT:
                    flipPixelsVertical(*bitmapwidth, *bitmapheight, pixels);
                    break;
                 case ORIENTATION_LEFTTOP:
                    rotateRaster(pixels, 90, bitmapwidth, bitmapheight);
                    flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                    buf = *bitmapwidth;
                    *bitmapwidth = *bitmapheight;
                    *bitmapheight = buf;
                    break;
                 case ORIENTATION_RIGHTTOP:
                    rotateRaster(pixels, 270, bitmapwidth, bitmapheight);
                    flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                    buf = *bitmapwidth;
                    *bitmapwidth = *bitmapheight;
                    *bitmapheight = buf;
                    break;
                 case ORIENTATION_RIGHTBOT:
                    rotateRaster(pixels, 90, bitmapwidth, bitmapheight);
                    buf= *bitmapwidth;
                    *bitmapwidth = *bitmapheight;
                    *bitmapheight = buf;
                    break;
                 case ORIENTATION_LEFTBOT:
                    rotateRaster(pixels, 270, bitmapwidth, bitmapheight);
                    buf = *bitmapwidth;
                    *bitmapwidth = *bitmapheight;
                    *bitmapheight = buf;
                    break;
            }

        } else if (origorientation == 2 || origorientation == 3 || origorientation == 6 || origorientation == 7) {
            flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
        }

        uint32_t tmpPixelBufferSize = (boundWidth / inSampleSize) * (boundHeight / inSampleSize);

        estimateMem = (sizeof(jint) * pixelsBufferSize); //temp buffer for decoded pixels
        estimateMem += (sizeof(jint) * tmpPixelBufferSize); //final buffer that will store original image
        LOGII("estimateMem", estimateMem);
        if (estimateMem > availableMemory) {
            if (throwException) {
                throw_not_enought_memory_exception(env, availableMemory, estimateMem);
            }
            return NULL;
        }

        jint* tmpPixels = (jint *) malloc(sizeof(jint) * tmpPixelBufferSize);
        uint32_t startPosX = 0;

        if (useOrientationTag && (origorientation == ORIENTATION_TOPRIGHT || origorientation == ORIENTATION_BOTRIGHT
                                    || origorientation == ORIENTATION_LEFTBOT || origorientation == ORIENTATION_RIGHTBOT)) {
            startPosX = *bitmapwidth - boundX/inSampleSize;
            for (int ox = startPosX, nx = 0; nx < boundWidth/inSampleSize; ox--, nx++) {
                sendProgress(processedProgress + nx * boundWidth/inSampleSize, progressTotal);
                for (int oy = 0, ny = 0; ny < boundHeight/inSampleSize; oy++, ny++) {
                    if (useOrientationTag && (origorientation > 4)) {
                        tmpPixels[nx * (boundHeight/inSampleSize) + ny] = pixels[ox * *bitmapheight + oy];
                    } else {
                        tmpPixels[ny * (boundWidth/inSampleSize) + nx] = pixels[oy * *bitmapwidth + ox];
                    }
                }
            }
        } else {
            startPosX = boundX/inSampleSize;
            for (int ox = startPosX, nx = 0; nx < boundWidth/inSampleSize; ox++, nx++) {
                sendProgress(processedProgress + nx * boundWidth/inSampleSize, progressTotal);
                for (int oy = 0, ny = 0; ny < boundHeight/inSampleSize; oy++, ny++) {
                    if (useOrientationTag && (origorientation > 4)) {
                        tmpPixels[nx * (boundHeight/inSampleSize) + ny] = pixels[ox * *bitmapheight + oy];
                    } else {
                        tmpPixels[ny * (boundWidth/inSampleSize) + nx] = pixels[oy * *bitmapwidth + ox];
                    }
                }
            }
        }

        free(pixels);
        pixels = tmpPixels;
        *bitmapwidth = boundWidth/inSampleSize;
        *bitmapheight = boundHeight/inSampleSize;

        return pixels;
}

void NativeDecoder::rotateTileLinesVertical(uint32_t tileHeight, uint32_t tileWidth, uint32_t* whatRotate, uint32_t *bufferLine) {
    for (int line = 0; line < tileHeight / 2; line++) {
        unsigned int  *top_line, *bottom_line;
        top_line = whatRotate + tileWidth * line;
        bottom_line = whatRotate + tileWidth * (tileHeight - line -1);
        _TIFFmemcpy(bufferLine, top_line, sizeof(unsigned int) * tileWidth);
        _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * tileWidth);
        _TIFFmemcpy(bottom_line, bufferLine, sizeof(unsigned int) * tileWidth);
    }
}

void NativeDecoder::rotateTileLinesHorizontal(uint32_t tileHeight, uint32_t tileWidth, uint32_t* whatRotate, uint32_t *bufferLine) {
    uint32_t buf;
    for (int y = 0; y < tileHeight; y++) {
        for (int x = 0; x < tileWidth / 2; x++) {
            buf = whatRotate[y * tileWidth + x];
            whatRotate[y * tileWidth + x] = whatRotate[y * tileWidth + tileWidth - x - 1];
            whatRotate[y * tileWidth + tileWidth - x - 1] = buf;
        }
    }
}

void NativeDecoder::orientDecodedTile(uint32_t tileHeight, uint32_t tileWidth,
                                      uint32_t *tile, uint32_t *bufferLine) {
    switch (origorientation) {
        case 1:
        case 5:
            rotateTileLinesVertical(tileHeight, tileWidth, tile, bufferLine);
            break;
        case 2:
        case 6:
            rotateTileLinesVertical(tileHeight, tileWidth, tile, bufferLine);
            rotateTileLinesHorizontal(tileHeight, tileWidth, tile, bufferLine);
            break;
        case 3:
        case 7:
            rotateTileLinesHorizontal(tileHeight, tileWidth, tile, bufferLine);
            break;
    }
}

jint * NativeDecoder::getSampledRasterFromTile(int inSampleSize, int *bitmapwidth, int *bitmapheight) {

        //init signal handler for catch SIGSEGV error that could be raised in libtiff
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        sigemptyset(&act.sa_mask);
        act.sa_sigaction = tileErrorHandler;
        act.sa_flags = SA_SIGINFO | SA_ONSTACK;
        if(sigaction(SIGSEGV, &act, 0) < 0) {
            LOGE("Can\'t setup signal handler. Working without errors catching mechanism");
        }

        jint *pixels = NULL;
        *bitmapwidth = origwidth / inSampleSize;
        *bitmapheight = origheight / inSampleSize;
        uint32_t pixelsBufferSize = *bitmapwidth * *bitmapheight;

        uint32_t tileWidth = 0, tileHeight = 0;
        TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileWidth);
        TIFFGetField(image, TIFFTAG_TILELENGTH, &tileHeight);

        unsigned long estimateMem = 0;
        estimateMem += (sizeof(jint) * pixelsBufferSize); //buffer for decoded pixels
        estimateMem += (tileWidth * tileHeight * sizeof(uint32_t)) * 3; //current, left and right tiles buffers
        estimateMem += (tileWidth * sizeof(uint32_t)); //work line for rotate tile
        LOGII("estimateMem", estimateMem);
        if (estimateMem > availableMemory) {
            if (throwException) {
                throw_not_enought_memory_exception(env, availableMemory, estimateMem);
            }
            return NULL;
        }

        pixels = (jint *) calloc(pixelsBufferSize, sizeof(jint));
        if (pixels == NULL) {
            LOGE("Can\'t allocate memory for temp buffer");
            return NULL;
        }

        uint32_t row, column;

        //main worker tile
        uint32_t *rasterTile = (uint32_t *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32_t));
        //left tile
        uint32_t *rasterTileLeft = (uint32_t *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32_t));
        //right tile
        uint32_t *rasterTileRight = (uint32_t *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32_t));

        uint32_t *work_line_buf = (uint32_t*)_TIFFmalloc(tileWidth * sizeof (uint32_t));

        //this variable calculate processed pixels for x and y direction to make right offsets at the begining of next tile
        //offset calculated from condition globalProcessed % inSampleSize should be 0
        uint32_t globalProcessedX = 0;
        uint32_t globalProcessedY = 0;

        //check for error
        if (setjmp(NativeDecoder::tile_buf)) {
            if (rasterTile) {
                _TIFFfree(rasterTile);
                rasterTile = NULL;
            }
            if (rasterTileLeft) {
                _TIFFfree(rasterTileLeft);
                rasterTileLeft = NULL;
            }
            if (rasterTileRight) {
                _TIFFfree(rasterTileRight);
                rasterTileRight = NULL;
            }
            if (work_line_buf) {
                _TIFFfree(work_line_buf);
                work_line_buf = NULL;
            }

            const char * err = "Caught SIGSEGV signal(Segmentation fault or invalid memory reference)";
            LOGE(err);
            if (throwException) {
                throwDecodeFileException(err);
            }

            return NULL;
        }

        for (row = 0; row < origheight; row += tileHeight) {
            short leftTileExists = 0;
            short rightTileExists = 0;
            for (column = 0; column < origwidth; column += tileWidth) {
                sendProgress(row * origwidth + column, progressTotal);

                if (useSinglePixelSample && inSampleSize > 1) {
                    const uint64_t firstSampleX = ((static_cast<uint64_t>(column) + inSampleSize - 1) / inSampleSize) * inSampleSize;
                    const uint64_t firstSampleY = ((static_cast<uint64_t>(row) + inSampleSize - 1) / inSampleSize) * inSampleSize;
                    const uint64_t validRight = std::min<uint64_t>(static_cast<uint64_t>(column) + tileWidth, origwidth);
                    const uint64_t validBottom = std::min<uint64_t>(static_cast<uint64_t>(row) + tileHeight, origheight);
                    const uint64_t sampledRight = static_cast<uint64_t>(*bitmapwidth) * inSampleSize;
                    const uint64_t sampledBottom = static_cast<uint64_t>(*bitmapheight) * inSampleSize;
                    if (firstSampleX >= validRight || firstSampleX >= sampledRight ||
                        firstSampleY >= validBottom || firstSampleY >= sampledBottom) {
                        // No output pixel addresses this tile.  Neighbour tiles are only
                        // required by the optional 3x3 filter, which is disabled here.
                        rightTileExists = 0;
                        leftTileExists = 0;
                        continue;
                    }
                }

                bool currentTileAlreadyOriented = false;
                if (!useSinglePixelSample && rightTileExists) {
                    uint32_t *reusableTile = rasterTileLeft;
                    rasterTileLeft = rasterTile;
                    rasterTile = rasterTileRight;
                    rasterTileRight = reusableTile;
                    leftTileExists = 1;
                    currentTileAlreadyOriented = true;
                } else {
                    leftTileExists = 0;
                    TIFFReadRGBATile(image, column, row, rasterTile);
                }

                rightTileExists = 0;
                if (!useSinglePixelSample && column + tileWidth < origwidth) {
                    TIFFReadRGBATile(image, column + tileWidth, row, rasterTileRight);
                    rightTileExists = 1;
                }

                if (!currentTileAlreadyOriented) {
                    orientDecodedTile(tileHeight, tileWidth, rasterTile, work_line_buf);
                }
                if (!useSinglePixelSample && rightTileExists) {
                    orientDecodedTile(tileHeight, tileWidth, rasterTileRight, work_line_buf);
                }

                if (inSampleSize > 1 && useSinglePixelSample) {
                    const uint32_t right = std::min(column + tileWidth, static_cast<uint32_t>(origwidth));
                    const uint32_t bottom = std::min(row + tileHeight, static_cast<uint32_t>(origheight));
                    const uint32_t sampledRight = static_cast<uint32_t>(*bitmapwidth) * inSampleSize;
                    const uint32_t sampledBottom = static_cast<uint32_t>(*bitmapheight) * inSampleSize;
                    for (uint32_t sourceY = ((row + inSampleSize - 1) / inSampleSize) * inSampleSize;
                         sourceY < bottom && sourceY < sampledBottom; sourceY += inSampleSize) {
                        if (checkStop()) return NULL;
                        const uint32_t pixY = sourceY / inSampleSize;
                        for (uint32_t sourceX = ((column + inSampleSize - 1) / inSampleSize) * inSampleSize;
                             sourceX < right && sourceX < sampledRight; sourceX += inSampleSize) {
                            const uint32_t pixX = sourceX / inSampleSize;
                            const jint pixel = rasterTile[(sourceY - row) * tileWidth + sourceX - column];
                            const uint32_t position = origorientation <= 4
                                    ? pixY * *bitmapwidth + pixX
                                    : pixX * *bitmapheight + pixY;
                            pixels[position] = pixel;
                        }
                    }
                    continue;
                }

                if (inSampleSize > 1 )
                {
                    //Tile could begin from not filled pixel(pixel[x,y] == 0). This variables allow to calculate begining of filled pixels
                    int tileStartDataX = -1;
                    int tileStartDataY = -1;

                    for (int origTileY = 0, pixY = row/inSampleSize; origTileY < tileHeight && pixY < *bitmapheight; origTileY++) {

                        if (checkStop()) {
                            if (rasterTile) {
                                _TIFFfree(rasterTile);
                                rasterTile = NULL;
                            }
                            if (rasterTileLeft) {
                                _TIFFfree(rasterTileLeft);
                                rasterTileLeft = NULL;
                            }
                            if (rasterTileRight) {
                                _TIFFfree(rasterTileRight);
                                rasterTileRight = NULL;
                            }
                            if (work_line_buf) {
                                _TIFFfree(work_line_buf);
                                work_line_buf = NULL;
                            }
                            LOGI("Thread stopped");
                            return NULL;
                        }

                        if (tileStartDataY != -1 && globalProcessedY % inSampleSize != 0) {
                            const uint32_t skip = inSampleSize - globalProcessedY % inSampleSize;
                            origTileY += skip - 1;
                            globalProcessedY += skip;
                        }
                        else
                        {
                            for (int origTileX = 0, pixX = column/inSampleSize; origTileX < tileWidth && pixX < *bitmapwidth; origTileX++) {
                                if (tileStartDataX != -1 && globalProcessedX % inSampleSize != 0)
                                {
                                    const uint32_t skip = inSampleSize - globalProcessedX % inSampleSize;
                                    origTileX += skip - 1;
                                    globalProcessedX += skip;
                                }
                                else
                                {
                                    uint32_t srcPosition = origTileY * tileWidth + origTileX;
                                    if (rasterTile[srcPosition] != 0) {
                                        if (tileStartDataX == -1) {
                                            tileStartDataX = origTileX;
                                        }
                                        if (tileStartDataY == -1) {
                                            tileStartDataY = origTileY;
                                        }

                                        //Apply filter to pixel
                                        jint crPix = rasterTile[srcPosition];//origBuffer[j1 * origwidth + i1];
                                        int sum = 1;

                                        int alpha = colorMask & crPix >> 24;
                                        int red = colorMask & crPix >> 16;
                                        int green = colorMask & crPix >> 8;
                                        int blue = colorMask & crPix;

                                        if (!useSinglePixelSample) {
                                        //using kernel 3x3

                                        //topleft
                                        if (origTileX - 1 >= 0 && origTileY - 1 >= 0) {
                                            crPix = rasterTile[(origTileY - 1) * tileWidth + origTileX - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (origTileY - 1 >= 0 && leftTileExists) {
                                            crPix = rasterTileLeft[(origTileY - 1) * tileWidth + tileWidth - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //top
                                        if (origTileY - 1 >= 0) {
                                            crPix = rasterTile[(origTileY - 1) * tileWidth + origTileX];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        // topright
                                        if (origTileX + 1 < tileWidth && origTileY - 1 >= 0) {
                                            crPix = rasterTile[(origTileY - 1) * tileWidth + origTileX + 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (origTileY - 1 >= 0 && rightTileExists) {
                                            crPix = rasterTileRight[(origTileY - 1) * tileWidth];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //right
                                        if (origTileX + 1 < tileWidth) {
                                            crPix = rasterTile[origTileY * tileWidth + origTileX + 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (rightTileExists) {
                                            crPix = rasterTileRight[origTileY * tileWidth];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //bottomright
                                        if (origTileX + 1 < tileWidth && origTileY + 1 < tileHeight) {
                                            crPix = rasterTile[(origTileY + 1) * tileWidth + origTileX + 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (origTileY + 1 < tileHeight && rightTileExists) {
                                            crPix = rasterTileRight[(origTileY + 1) * tileWidth];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //bottom
                                        if (origTileY + 1 < tileHeight) {
                                            crPix = rasterTile[(origTileY + 1) * tileWidth + origTileX];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //bottomleft
                                        if (origTileX - 1 >= 0 && origTileY + 1 < tileHeight) {
                                            crPix = rasterTile[(origTileY + 1) * tileWidth + origTileX - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (origTileY + 1 < tileHeight && leftTileExists) {
                                            crPix = rasterTileLeft[(origTileY + 1) * tileWidth + tileWidth - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //left
                                        if (origTileX - 1 >= 0) {
                                            crPix = rasterTile[origTileY * tileWidth + origTileX - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (leftTileExists) {
                                            crPix = rasterTileLeft[origTileY * tileWidth + tileWidth - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }
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

                                        int position;
                                        if (origorientation <= 4) {
                                            position = pixY * *bitmapwidth + pixX;
                                        } else {
                                            position = pixX * *bitmapheight + pixY;
                                        }
                                        pixels[position] = crPix;
                                    } else {
                                        if (tileStartDataX != -1) tileStartDataX = -1;
                                        if (tileStartDataY != -1) tileStartDataY = -1;
                                    }

                                    if (tileStartDataX != -1) {
                                        pixX++;
                                        globalProcessedX++;
                                    }

                                }
                            }
                            if (tileStartDataY != -1) {
                                pixY++;
                                globalProcessedY++;
                            }
                        }
                    }
                } else {
                    int rowHasPixels = 0;
                        for (int th = 0, bh = 0; th < tileHeight; th++) {
                            for (int tw = 0, bw = 0; tw < tileWidth; tw++) {
                                uint32_t srcPosition = th * tileWidth + tw;
                                if (rasterTile[srcPosition] != 0) {
                                    int position = 0;
                                    if (origorientation <= 4) {
                                        position = (row + bh) * *bitmapwidth + column + bw;
                                    } else {
                                        position = (column + bw) * *bitmapheight + row + bh;
                                    }
                                    pixels[position] = rasterTile[srcPosition];
                                    rowHasPixels = 1;
                                    bw++;
                                }
                            }
                            if (rowHasPixels) {
                                bh++;
                                rowHasPixels = 0;
                            }
                        }
                }
            }
        }

        if (rasterTile) {
            _TIFFfree(rasterTile);
            rasterTile = NULL;
        }
        if (rasterTileLeft) {
            _TIFFfree(rasterTileLeft);
            rasterTileLeft = NULL;
        }
        if (rasterTileRight) {
            _TIFFfree(rasterTileRight);
            rasterTileRight = NULL;
        }
        if (work_line_buf) {
            _TIFFfree(work_line_buf);
            work_line_buf = NULL;
        }

        if (useOrientationTag) {
            switch (origorientation) {
                case ORIENTATION_TOPLEFT:
                case ORIENTATION_LEFTTOP:
                    break;
                case ORIENTATION_TOPRIGHT:
                    flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                    break;
                case ORIENTATION_RIGHTTOP:
                    flipPixelsHorizontal(*bitmapheight, *bitmapwidth, pixels);
                    break;
                case ORIENTATION_BOTRIGHT:
                case ORIENTATION_RIGHTBOT:
                    rotateRaster(pixels, 180, bitmapwidth, bitmapheight);
                    break;
                case ORIENTATION_BOTLEFT:
                    flipPixelsVertical(*bitmapwidth, *bitmapheight, pixels);
                    break;
                case ORIENTATION_LEFTBOT:
                    flipPixelsVertical(*bitmapheight, *bitmapwidth, pixels);
                    break;
            }
        } else {
            if (origorientation > 4) {
                uint32_t buf = *bitmapwidth;
                *bitmapwidth = *bitmapheight;
                *bitmapheight = buf;
                rotateRaster(pixels, 90, bitmapwidth, bitmapheight);
                flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
            }
        }

        return pixels;
}

jint * NativeDecoder::getSampledRasterFromTileWithBounds(int inSampleSize, int *bitmapwidth, int *bitmapheight) {

        //init signal handler for catch SIGSEGV error that could be raised in libtiff
        struct sigaction act;
        memset(&act, 0, sizeof(act));
        sigemptyset(&act.sa_mask);
        act.sa_sigaction = tileErrorHandler;
        act.sa_flags = SA_SIGINFO | SA_ONSTACK;
        if(sigaction(SIGSEGV, &act, 0) < 0) {
            LOGE("Can\'t setup signal handler. Working without errors catching mechanism");
        }


        //First read all tiles that are on necessary area

        uint32_t tileWidth = 0, tileHeight = 0;
        TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileWidth);
        TIFFGetField(image, TIFFTAG_TILELENGTH, &tileHeight);

        //find first and last tile to process
        uint32_t firstTileX = (uint32_t)(boundX / tileWidth);
        uint32_t firstTileY = (uint32_t)(boundY / tileHeight);

        uint32_t lastTileX = (uint32_t)((boundX + boundWidth) / tileWidth) + 1;
        uint32_t lastTileY = (uint32_t)((boundY + boundHeight) / tileHeight) + 1;
        const uint32_t tilesAcross = (origwidth + tileWidth - 1) / tileWidth;
        const uint32_t tilesDown = (origheight + tileHeight - 1) / tileHeight;
        if (lastTileX > tilesAcross) lastTileX = tilesAcross;
        if (lastTileY > tilesDown) lastTileY = tilesDown;

        jint *pixels = NULL;
        *bitmapwidth = /*boundWidth*/ (lastTileX - firstTileX) * tileWidth / inSampleSize;//origwidth / inSampleSize;
        *bitmapheight = /*boundHeight*/ (lastTileY - firstTileY) * tileHeight / inSampleSize;//origheight / inSampleSize;
        uint32_t pixelsBufferSize = *bitmapwidth * *bitmapheight;

         unsigned long estimateMem = 0;
         estimateMem += (sizeof(jint) * pixelsBufferSize); //buffer for decoded pixels
         estimateMem += (tileWidth * tileHeight * sizeof(uint32_t)) * 3; //current, left and right tiles buffers
         estimateMem += (tileWidth * sizeof(uint32_t)); //work line for rotate tile
         LOGII("estimateMem", estimateMem);
         if (estimateMem > availableMemory) {
            if (throwException) {
                throw_not_enought_memory_exception(env, availableMemory, estimateMem);
            }
            return NULL;
         }

        pixels = (jint *) calloc(pixelsBufferSize, sizeof(jint));
        if (pixels == NULL) {
            LOGE("Can\'t allocate memory for temp buffer");
            return NULL;
        }

        progressTotal = pixelsBufferSize + (boundWidth/inSampleSize) * (boundHeight/inSampleSize);
        sendProgress(0, progressTotal);
        jlong processedProgress = 0;

        uint32_t row, column, rowDest, columnDest;

        //main worker tile
        uint32_t *rasterTile = (uint32_t *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32_t));
        //left tile
        uint32_t *rasterTileLeft = (uint32_t *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32_t));
        //right tile
        uint32_t *rasterTileRight = (uint32_t *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32_t));

        uint32_t *work_line_buf = (uint32_t*)_TIFFmalloc(tileWidth * sizeof (uint32_t));

        //check for error
        if (setjmp(NativeDecoder::tile_buf)) {
            if (rasterTile) {
                _TIFFfree(rasterTile);
                rasterTile = NULL;
            }
            if (rasterTileLeft) {
                _TIFFfree(rasterTileLeft);
                rasterTileLeft = NULL;
            }
            if (rasterTileRight) {
                _TIFFfree(rasterTileRight);
                rasterTileRight = NULL;
            }
            if (work_line_buf) {
                _TIFFfree(work_line_buf);
                work_line_buf = NULL;
            }

        const char * err = "Caught SIGSEGV signal(Segmentation fault or invalid memory reference)";
        LOGE(err);
        if (throwException) {
            throwDecodeFileException(err);
        }

            return NULL;
        }

        //this variable calculate processed pixels for x and y direction to make right offsets at the begining of next tile
        //offset calculated from condition globalProcessed % inSampleSize should be 0
        uint32_t globalProcessedX = 0;
        uint32_t globalProcessedY = 0;

        uint32_t progressRow = 0;
        uint32_t progressColumn = 0;

        rowDest = columnDest = 0;
        for (row = firstTileY * tileHeight; row < lastTileY * tileHeight; row += tileHeight, progressRow += tileHeight) {
            columnDest = 0;
            progressColumn = 0;
            short leftTileExists = 0;
            short rightTileExists = 0;
            for (column = firstTileX * tileWidth; column < lastTileX * tileWidth; column += tileWidth, progressColumn += tileWidth) {
                processedProgress = progressRow * *bitmapwidth + progressColumn;
                sendProgress(processedProgress, progressTotal);

                bool currentTileAlreadyOriented = false;
                if (!useSinglePixelSample && rightTileExists) {
                    uint32_t *reusableTile = rasterTileLeft;
                    rasterTileLeft = rasterTile;
                    rasterTile = rasterTileRight;
                    rasterTileRight = reusableTile;
                    leftTileExists = 1;
                    currentTileAlreadyOriented = true;
                } else {
                    leftTileExists = 0;
                    TIFFReadRGBATile(image, column, row, rasterTile);
                }

                rightTileExists = 0;
                if (!useSinglePixelSample && column + tileWidth < origwidth &&
                    column + tileWidth < lastTileX * tileWidth) {
                    TIFFReadRGBATile(image, column + tileWidth, row, rasterTileRight);
                    rightTileExists = 1;
                }

                if (!currentTileAlreadyOriented) {
                    orientDecodedTile(tileHeight, tileWidth, rasterTile, work_line_buf);
                }
                if (!useSinglePixelSample && rightTileExists) {
                    orientDecodedTile(tileHeight, tileWidth, rasterTileRight, work_line_buf);
                }

                if (inSampleSize > 1 && useSinglePixelSample) {
                    const uint32_t destRight = std::min(columnDest + tileWidth,
                            static_cast<uint32_t>(*bitmapwidth) * inSampleSize);
                    const uint32_t destBottom = std::min(rowDest + tileHeight,
                            static_cast<uint32_t>(*bitmapheight) * inSampleSize);
                    for (uint32_t destY = ((rowDest + inSampleSize - 1) / inSampleSize) * inSampleSize;
                         destY < destBottom; destY += inSampleSize) {
                        if (checkStop()) return NULL;
                        const uint32_t pixY = destY / inSampleSize;
                        for (uint32_t destX = ((columnDest + inSampleSize - 1) / inSampleSize) * inSampleSize;
                             destX < destRight; destX += inSampleSize) {
                            const uint32_t pixX = destX / inSampleSize;
                            const jint pixel = rasterTile[(destY - rowDest) * tileWidth + destX - columnDest];
                            const uint32_t position = origorientation <= 4
                                    ? pixY * *bitmapwidth + pixX
                                    : pixX * *bitmapheight + pixY;
                            pixels[position] = pixel;
                        }
                    }
                    columnDest += tileWidth;
                    continue;
                }

                    //Tile could begin from not filled pixel(pixel[x,y] == 0). This variables allow to calculate begining of filled pixels
                    int tileStartDataX = -1;
                    int tileStartDataY = -1;

                    for (int origTileY = 0, pixY = rowDest/inSampleSize; origTileY < tileHeight && pixY < *bitmapheight; origTileY++) {
                        if (checkStop()) {
                            if (rasterTile) {
                                _TIFFfree(rasterTile);
                                rasterTile = NULL;
                            }
                            if (rasterTileLeft) {
                                _TIFFfree(rasterTileLeft);
                                rasterTileLeft = NULL;
                            }
                            if (rasterTileRight) {
                                _TIFFfree(rasterTileRight);
                                rasterTileRight = NULL;
                            }
                            if (work_line_buf) {
                                _TIFFfree(work_line_buf);
                                work_line_buf = NULL;
                            }
                            LOGI("Thread stopped");
                            return NULL;
                        }

                        if (tileStartDataY != -1 && globalProcessedY % inSampleSize != 0) {
                            const uint32_t skip = inSampleSize - globalProcessedY % inSampleSize;
                            origTileY += skip - 1;
                            globalProcessedY += skip;
                        }
                        else
                        {

                            for (int origTileX = 0, pixX = columnDest/inSampleSize; origTileX < tileWidth && pixX < *bitmapwidth; origTileX++) {


                                if (tileStartDataX != -1 && globalProcessedX % inSampleSize != 0)
                                {
                                    const uint32_t skip = inSampleSize - globalProcessedX % inSampleSize;
                                    origTileX += skip - 1;
                                    globalProcessedX += skip;
                                }
                                else
                                {
                                    uint32_t srcPosition = origTileY * tileWidth + origTileX;
                                    if (rasterTile[srcPosition] != 0) {

                                        if (tileStartDataX == -1) {
                                            tileStartDataX = origTileX;
                                        }
                                        if (tileStartDataY == -1) {
                                            tileStartDataY = origTileY;
                                        }

                                        //Apply filter to pixel
                                        jint crPix = rasterTile[srcPosition];//origBuffer[j1 * origwidth + i1];
                                        int sum = 1;

                                        int alpha = colorMask & crPix >> 24;
                                        int red = colorMask & crPix >> 16;
                                        int green = colorMask & crPix >> 8;
                                        int blue = colorMask & crPix;

                                        if (!useSinglePixelSample) {
                                        //using kernel 3x3

                                        //topleft
                                        if (origTileX - 1 >= 0 && origTileY - 1 >= 0) {
                                            crPix = rasterTile[(origTileY - 1) * tileWidth + origTileX - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (origTileY - 1 >= 0 && leftTileExists) {
                                            crPix = rasterTileLeft[(origTileY - 1) * tileWidth + tileWidth - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //top
                                        if (origTileY - 1 >= 0) {
                                            crPix = rasterTile[(origTileY - 1) * tileWidth + origTileX];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        // topright
                                        if (origTileX + 1 < tileWidth && origTileY - 1 >= 0) {
                                            crPix = rasterTile[(origTileY - 1) * tileWidth + origTileX + 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (origTileY - 1 >= 0 && rightTileExists) {
                                            crPix = rasterTileRight[(origTileY - 1) * tileWidth];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //right
                                        if (origTileX + 1 < tileWidth) {
                                            crPix = rasterTile[origTileY * tileWidth + origTileX + 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (rightTileExists) {
                                            crPix = rasterTileRight[origTileY * tileWidth];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //bottomright
                                        if (origTileX + 1 < tileWidth && origTileY + 1 < tileHeight) {
                                            crPix = rasterTile[(origTileY + 1) * tileWidth + origTileX + 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (origTileY + 1 < tileHeight && rightTileExists) {
                                            crPix = rasterTileRight[(origTileY + 1) * tileWidth];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //bottom
                                        if (origTileY + 1 < tileHeight) {
                                            crPix = rasterTile[(origTileY + 1) * tileWidth + origTileX];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //bottomleft
                                        if (origTileX - 1 >= 0 && origTileY + 1 < tileHeight) {
                                            crPix = rasterTile[(origTileY + 1) * tileWidth + origTileX - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (origTileY + 1 < tileHeight && leftTileExists) {
                                            crPix = rasterTileLeft[(origTileY + 1) * tileWidth + tileWidth - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }

                                        //left
                                        if (origTileX - 1 >= 0) {
                                            crPix = rasterTile[origTileY * tileWidth + origTileX - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        } else if (leftTileExists) {
                                            crPix = rasterTileLeft[origTileY * tileWidth + tileWidth - 1];
                                            if (crPix != 0) {
                                                red += colorMask & crPix >> 16;
                                                green += colorMask & crPix >> 8;
                                                blue += colorMask & crPix;
                                                alpha += colorMask & crPix >> 24;
                                                sum++;
                                            }
                                        }
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

                                        int position;
                                        if (origorientation <= 4) {
                                            position = pixY * *bitmapwidth + pixX;
                                        } else {
                                            position = pixX * *bitmapheight + pixY;
                                        }
                                        pixels[position] = crPix;
                                    } else {
                                        if (tileStartDataX != -1) tileStartDataX = -1;
                                        if (tileStartDataY != -1) tileStartDataY = -1;
                                    }

                                    if (tileStartDataX != -1) {
                                        pixX++;
                                        globalProcessedX++;
                                    }

                                }
                            }
                            if (tileStartDataY != -1) {
                                pixY++;
                                globalProcessedY++;
                            }
                        }
                    }
                columnDest += tileWidth;
            }
            rowDest += tileHeight;
        }

        if (rasterTile) {
            _TIFFfree(rasterTile);
            rasterTile = NULL;
        }
        if (rasterTileLeft) {
            _TIFFfree(rasterTileLeft);
            rasterTileLeft = NULL;
        }
        if (rasterTileRight) {
            _TIFFfree(rasterTileRight);
            rasterTileRight = NULL;
        }
        if (work_line_buf) {
            _TIFFfree(work_line_buf);
            work_line_buf = NULL;
        }

        //Copy necessary pixels to new array if orientation <=4
        uint32_t tmpPixelBufferSize = (boundWidth / inSampleSize) * (boundHeight / inSampleSize);

        estimateMem = (sizeof(jint) * pixelsBufferSize); //buffer for decoded pixels
        estimateMem += (sizeof(jint) * tmpPixelBufferSize); //finall buffer
        LOGII("estimateMem", estimateMem);
        if (estimateMem > availableMemory) {
            if (throwException) {
                throw_not_enought_memory_exception(env, availableMemory, estimateMem);
            }
            return NULL;
        }

        if (origorientation <= 4) {

            jint* tmpPixels = (jint *) malloc(sizeof(jint) * tmpPixelBufferSize);
            uint32_t startPosX = boundX%tileWidth /inSampleSize;//(firstTileX * tileWidth - tileWidth + boundX) / inSampleSize;
            uint32_t startPosY = boundY%tileHeight /inSampleSize;//(firstTileY * tileHeight - tileHeight + boundY) /inSampleSize;
            for (int ox = startPosX, nx = 0; nx < boundWidth/inSampleSize; ox++, nx++) {
                sendProgress(processedProgress + nx * (boundHeight/inSampleSize), progressTotal);
                for (int oy = startPosY, ny = 0; ny < boundHeight/inSampleSize; oy++, ny++) {
                    tmpPixels[ny * (boundWidth/inSampleSize) + nx] = pixels[oy * *bitmapwidth + ox];
                }
            }

            free(pixels);
            pixels = tmpPixels;
            *bitmapwidth = boundWidth/inSampleSize;
            *bitmapheight = boundHeight/inSampleSize;
        }

        if (useOrientationTag) {
            switch (origorientation) {
                case ORIENTATION_TOPLEFT:
                case ORIENTATION_LEFTTOP:
                    break;
                case ORIENTATION_TOPRIGHT:
                    flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                    break;
                case ORIENTATION_RIGHTTOP:
                    flipPixelsHorizontal(*bitmapheight, *bitmapwidth, pixels);
                    break;
                case ORIENTATION_BOTRIGHT:
                case ORIENTATION_RIGHTBOT:
                    rotateRaster(pixels, 180, bitmapwidth, bitmapheight);
                    break;
                case ORIENTATION_BOTLEFT:
                    flipPixelsVertical(*bitmapwidth, *bitmapheight, pixels);
                    break;
                case ORIENTATION_LEFTBOT:
                    flipPixelsVertical(*bitmapheight, *bitmapwidth, pixels);
                    break;
            }
        } else {
            if (origorientation > 4) {
                uint32_t buf = *bitmapwidth;
                *bitmapwidth = *bitmapheight;
                *bitmapheight = buf;
                rotateRaster(pixels, 90, bitmapwidth, bitmapheight);
                flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
            }
        }

        //Copy necessary pixels to new array if orientation >4
        if (origorientation > 4) {
            jint* tmpPixels = (jint *) malloc(sizeof(jint) * tmpPixelBufferSize);
            uint32_t startPosX = boundX%tileWidth /inSampleSize;
            uint32_t startPosY = boundY%tileHeight /inSampleSize;
            for (int ox = startPosX, nx = 0; nx < boundWidth/inSampleSize; ox++, nx++) {
                sendProgress(processedProgress + nx * (boundHeight/inSampleSize), progressTotal);
                for (int oy = startPosY, ny = 0; ny < boundHeight/inSampleSize; oy++, ny++) {
                    if (useOrientationTag) {
                        tmpPixels[nx * (boundHeight/inSampleSize) + ny] = pixels[ox * *bitmapheight + oy];
                    } else {
                        tmpPixels[ny * (boundWidth/inSampleSize) + nx] = pixels[oy * *bitmapwidth + ox];
                    }
                }
            }

            free(pixels);
            pixels = tmpPixels;
            *bitmapwidth = boundWidth/inSampleSize;
            *bitmapheight = boundHeight/inSampleSize;
        }

        return pixels;
}



jint * NativeDecoder::getSampledRasterFromImage(int inSampleSize, int *bitmapwidth, int *bitmapheight)
{
    //init signal handler for catch SIGSEGV error that could be raised in libtiff
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = imageErrorHandler;
    act.sa_flags = SA_SIGINFO | SA_ONSTACK;
    if(sigaction(SIGSEGV, &act, 0) < 0) {
        LOGE("Can\'t setup signal handler. Working without errors catching mechanism");
    }

    //buffer size for decoding tiff image in RGBA format
    int origBufferSize = origwidth * origheight * sizeof(unsigned int);

    *bitmapwidth = origwidth / inSampleSize;
    *bitmapheight = origheight / inSampleSize;
    //buffer size for creating scaled image;
    uint32_t pixelsBufferSize = *bitmapwidth * *bitmapheight * sizeof(jint);

    /**Estimate usage of memory for decoding*/
    unsigned long estimateMem = origBufferSize;//origBufferSize - size of decoded RGBA image
    if (inSampleSize > 1) {
        estimateMem += pixelsBufferSize; //if inSmapleSize greater than 1 we need aditional vevory for scaled image
    }
    LOGII("estimateMem", estimateMem);

    if (estimateMem > availableMemory) {
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return NULL;
    }

    unsigned int *origBuffer = NULL;

    origBuffer = (unsigned int *) _TIFFmalloc(origBufferSize);
    if (origBuffer == NULL) {
        LOGE("Can\'t allocate memory for origBuffer");
        return NULL;
    }

    jint *pixels = NULL;

    //check for error
    if (setjmp(NativeDecoder::image_buf)) {
        if (origBuffer) {
            _TIFFfree(origBuffer);
            origBuffer = NULL;
        }
        if (pixels) {
            free(pixels);
            pixels = NULL;
        }

        const char * err = "Caught SIGSEGV signal(Segmentation fault or invalid memory reference)";
        LOGE(err);
        if (throwException) {
            throwDecodeFileException(err);
        }

        return NULL;
    }


	if (0 ==
        TIFFReadRGBAImageOriented(image, origwidth, origheight, origBuffer, ORIENTATION_TOPLEFT, 0)) {
	    free(origBuffer);
	    const char *message = "Error reading image";
        LOGE(*message);
        if (throwException) {
            throwDecodeFileException(message);
        }
        return NULL;
    }

    if (inSampleSize == 1) {
        // Use buffer as is.
        pixels = (jint*) origBuffer;
    }
    else {
        // Sample the buffer.
        pixels = (jint *) malloc(pixelsBufferSize);
        if (pixels == NULL) {
            LOGE("Can\'t allocate memory for temp buffer");
            return NULL;
        }
        else {
            for (int j = 0, j1 = 0; j < *bitmapheight; j++, j1 += inSampleSize) {

                sendProgress(j1 * origwidth, progressTotal);

                if (checkStop()) {
                    //TODO clear memory
                    if (origBuffer) {
                        _TIFFfree(origBuffer);
                        origBuffer = NULL;
                    }
                    if (pixels) {
                        free(pixels);
                        pixels = NULL;
                    }
                    LOGI("Thread stopped");
                    return NULL;
                }

                for (int i = 0, i1 = 0; i < *bitmapwidth; i++, i1 += inSampleSize) {
                    //Apply filter to pixel
                    jint crPix = origBuffer[j1 * origwidth + i1];
                    int sum = 1;

                    int alpha = colorMask & crPix >> 24;
                    int red = colorMask & crPix >> 16;
                    int green = colorMask & crPix >> 8;
                    int blue = colorMask & crPix;

                    if (!useSinglePixelSample) {
                    //using kernel 3x3

                    //topleft
                    if (i1 - 1 >= 0 && j1 - 1 >= 0) {
                        crPix = origBuffer[(j1 - 1) * origwidth + i1 - 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //top
                    if (j1 - 1 >= 0) {
                        crPix = origBuffer[(j1 - 1) * origwidth + i1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    // topright
                    if (i1 + 1 < origwidth && j1 - 1 >= 0) {
                        crPix = origBuffer[(j1 - 1) * origwidth + i1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //right
                    if (i1 + 1 < origwidth) {
                        crPix = origBuffer[j1 * origwidth + i1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //bottomright
                    if (i1 + 1 < origwidth && j1 + 1 < origheight) {
                        crPix = origBuffer[(j1 + 1) * origwidth + i1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //bottom
                    if (j1 + 1 < origheight) {
                        crPix = origBuffer[(j1 + 1) * origwidth + i1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //bottomleft
                    if (i1 - 1 >= 0 && j1 + 1 < origheight) {
                        crPix = origBuffer[(j1 + 1) * origwidth + i1 - 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //left
                    if (i1 - 1 >= 0) {
                        crPix = origBuffer[j1 * origwidth + i1 - 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
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
        if (origBuffer) {
            _TIFFfree(origBuffer);
            origBuffer = NULL;
        }
    }

    if (useOrientationTag) {
        fixOrientation(pixels, pixelsBufferSize, *bitmapwidth, *bitmapheight);
    } else {
        uint32_t buf;
        switch(origorientation) {
                         case ORIENTATION_TOPLEFT:
                         case ORIENTATION_LEFTTOP:
                            break;
                         case ORIENTATION_TOPRIGHT:
                         case ORIENTATION_RIGHTTOP:
                            flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                            break;
                         case ORIENTATION_BOTRIGHT:
                         case ORIENTATION_RIGHTBOT:
                            rotateRaster(pixels, 180, bitmapwidth, bitmapheight);
                            break;
                         case ORIENTATION_BOTLEFT:
                         case ORIENTATION_LEFTBOT:
                            rotateRaster(pixels, 180, bitmapwidth, bitmapheight);
                            flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                            break;
                         }
    }

    return pixels;
}

jint * NativeDecoder::getSampledRasterFromImageWithBounds(int inSampleSize, int *bitmapwidth, int *bitmapheight)
{
    //init signal handler for catch SIGSEGV error that could be raised in libtiff
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = imageErrorHandler;
    act.sa_flags = SA_SIGINFO | SA_ONSTACK;
    if(sigaction(SIGSEGV, &act, 0) < 0) {
        LOGE("Can\'t setup signal handler. Working without errors catching mechanism");
    }

    //buffer size for decoding tiff image in RGBA format
    int origBufferSize = origwidth * origheight * sizeof(unsigned int);

    *bitmapwidth = boundWidth / inSampleSize;//origwidth / inSampleSize;
    *bitmapheight = boundHeight / inSampleSize;//origheight / inSampleSize;
    //buffer size for creating scaled image;
    uint32_t pixelsBufferSize = *bitmapwidth * *bitmapheight * sizeof(jint);

    /**Estimate usage of memory for decoding*/
    unsigned long estimateMem = origBufferSize;//origBufferSize - size of decoded RGBA image
    //if (inSampleSize > 1) {
        estimateMem += pixelsBufferSize; //if inSmapleSize greater than 1 we need aditional vevory for scaled image
    //}
    LOGII("estimateMem", estimateMem);

    if (estimateMem > availableMemory) {
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return NULL;
    }

    unsigned int *origBuffer = NULL;
    jint *pixels = NULL;

    //check for error
    if (setjmp(NativeDecoder::image_buf)) {
        if (origBuffer) {
            _TIFFfree(origBuffer);
            origBuffer = NULL;
        }
        if (pixels) {
            free(pixels);
            pixels = NULL;
        }

        const char * err = "Caught SIGSEGV signal(Segmentation fault or invalid memory reference)";
        LOGE(err);
        if (throwException) {
            throwDecodeFileException(err);
        }

        return NULL;
    }

    origBuffer = (unsigned int *) _TIFFmalloc(origBufferSize);
    if (origBuffer == NULL) {
        LOGE("Can\'t allocate memory for origBuffer");
        return NULL;
    }

	if (0 ==
        TIFFReadRGBAImageOriented(image, origwidth, origheight, origBuffer, ORIENTATION_TOPLEFT, 0)) {
	    free(origBuffer);
	    const char *message = "Error reading image";
        LOGE(*message);
        if (throwException) {
            throwDecodeFileException(message);
        }
        return NULL;
    }

    progressTotal = boundWidth/inSampleSize * boundHeight/inSampleSize;

    // Sample the buffer.
    pixels = (jint *) malloc(pixelsBufferSize);
    if (pixels == NULL) {
        LOGE("Can\'t allocate memory for temp buffer");
        return NULL;
    } else {
        for (int y = 0, y1 = boundY; y < *bitmapheight; y++, y1 += inSampleSize) {

            sendProgress(y1 * boundWidth, progressTotal);

                if (checkStop()) {
                    //TODO clear memory
                    if (origBuffer) {
                        _TIFFfree(origBuffer);
                        origBuffer = NULL;
                    }
                    if (pixels) {
                        free(pixels);
                        pixels = NULL;
                    }
                    LOGI("Thread stopped");
                    return NULL;
                }

                for (int x = 0, x1 = boundX; x < *bitmapwidth; x++, x1 += inSampleSize) {
                    //Apply filter to pixel
                    jint crPix = origBuffer[y1 * origwidth + x1];
                    int sum = 1;

                    int alpha = colorMask & crPix >> 24;
                    int red = colorMask & crPix >> 16;
                    int green = colorMask & crPix >> 8;
                    int blue = colorMask & crPix;

                    if (!useSinglePixelSample) {
                    //using kernel 3x3

                    //topleft
                    if (x1 - 1 >= 0 && y1 - 1 >= 0) {
                        crPix = origBuffer[(y1 - 1) * origwidth + x1 - 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //top
                    if (y1 - 1 >= 0) {
                        crPix = origBuffer[(y1 - 1) * origwidth + x1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    // topright
                    if (x1 + 1 < origwidth && y1 - 1 >= 0) {
                        crPix = origBuffer[(y1 - 1) * origwidth + x1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //right
                    if (x1 + 1 < origwidth) {
                        crPix = origBuffer[y1 * origwidth + x1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //bottomright
                    if (x1 + 1 < origwidth && y1 + 1 < origheight) {
                        crPix = origBuffer[(y1 + 1) * origwidth + x1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //bottom
                    if (y1 + 1 < origheight) {
                        crPix = origBuffer[(y1 + 1) * origwidth + x1 + 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //bottomleft
                    if (x1 - 1 >= 0 && y1 + 1 < origheight) {
                        crPix = origBuffer[(y1 + 1) * origwidth + x1 - 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
                    //left
                    if (x1 - 1 >= 0) {
                        crPix = origBuffer[y1 * origwidth + x1 - 1];
                        red += colorMask & crPix >> 16;
                        green += colorMask & crPix >> 8;
                        blue += colorMask & crPix;
                        alpha += colorMask & crPix >> 24;
                        sum++;
                    }
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

                    pixels[y * *bitmapwidth + x] = crPix;
                }
            }
    }

        //Close Buffer
        if (origBuffer) {
            _TIFFfree(origBuffer);
            origBuffer = NULL;
        }


    if (useOrientationTag) {
        fixOrientation(pixels, pixelsBufferSize, *bitmapwidth, *bitmapheight);
    } else {
        uint32_t buf;
        switch(origorientation) {
                         case ORIENTATION_TOPLEFT:
                         case ORIENTATION_LEFTTOP:
                            break;
                         case ORIENTATION_TOPRIGHT:
                         case ORIENTATION_RIGHTTOP:
                            flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                            break;
                         case ORIENTATION_BOTRIGHT:
                         case ORIENTATION_RIGHTBOT:
                            rotateRaster(pixels, 180, bitmapwidth, bitmapheight);
                            break;
                         case ORIENTATION_BOTLEFT:
                         case ORIENTATION_LEFTBOT:
                            rotateRaster(pixels, 180, bitmapwidth, bitmapheight);
                            flipPixelsHorizontal(*bitmapwidth, *bitmapheight, pixels);
                            break;
                         }
    }

    return pixels;
}

int NativeDecoder::getDecodeMethod()
{
	int method = -1;
	uint32_t tileWidth, tileHeight;
	int readTW = 0, readTH = 0;
    readTW = TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileWidth);
    readTH = TIFFGetField(image, TIFFTAG_TILELENGTH, &tileHeight);
    if (tileWidth > 0 && tileHeight > 0 && readTH > 0 && readTW > 0) {
        method = DECODE_METHOD_TILE;
    } else {
        int rowPerStrip = -1;
    	TIFFGetField(image, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);
    	uint32_t stripSize = TIFFStripSize (image);
    	uint32_t stripMax = TIFFNumberOfStrips (image);
    	int estimate = origwidth * 3;
    	LOGII("RPS", rowPerStrip);
    	LOGII("stripSize", stripSize);
    	LOGII("stripMax", stripMax);
    	if (rowPerStrip != -1 && stripSize > 0 && stripMax > 1 && rowPerStrip < origheight) {
    	    method = DECODE_METHOD_STRIP;
    	} else {
    	method = DECODE_METHOD_IMAGE;
    	}
    }

	LOGII("Decode method", method);
	return method;
}

void NativeDecoder::flipPixelsVertical(uint32_t width, uint32_t height, jint* raster) {
    jint *bufferLine = (jint *) malloc(sizeof(jint) * width);
    for (int line = 0; line < height / 2; line++) {
        jint  *top_line, *bottom_line;
        top_line = raster + width * line;
        bottom_line = raster + width * (height - line -1);
        _TIFFmemcpy(bufferLine, top_line, sizeof(jint) * width);
        _TIFFmemcpy(top_line, bottom_line, sizeof(jint) * width);
        _TIFFmemcpy(bottom_line, bufferLine, sizeof(jint) * width);
    }
    free(bufferLine);
}

void NativeDecoder::flipPixelsHorizontal(uint32_t width, uint32_t height, jint* raster) {
    jint buf;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width / 2; x++) {
            buf = raster[y * width + x];
            raster[y * width + x] = raster[y * width + width - x - 1];
            raster[y * width + width - x - 1] = buf;
        }
    }
}

void NativeDecoder::rotateRaster(jint *raster, int angle, int *width, int *height)
        {
            int rotatedWidth = *width;
            int rotatedHeight = *height;
            int numberOf90s = angle / 90;
            if (numberOf90s % 2 != 0)
            {
                int tmp = rotatedWidth;
                rotatedWidth = rotatedHeight;
                rotatedHeight = tmp;
            }

            jint *rotated = (jint *) malloc(sizeof(jint) * rotatedWidth * rotatedHeight);//new int[rotatedWidth * rotatedHeight];

            for (int h = 0; h < *height; ++h)
            {
                for (int w = 0; w < *width; ++w)
                {
                    uint32_t item = raster[h * *width + w];
                    int x = 0;
                    int y = 0;
                    switch (numberOf90s % 4)
                    {
                        case 0:
                            x = w;
                            y = h;
                            break;
                        case 1:
                            x = (*height - h - 1);
                            y = (rotatedHeight - 1) - (*width - w - 1);
                            break;
                        case 2:
                            x = (*width - w - 1);
                            y = (*height - h - 1);
                            break;
                        case 3:
                            x = (rotatedWidth - 1) - (*height - h - 1);
                            y = (*width - w - 1);
                            break;
                    }

                    rotated[y * rotatedWidth + x] = item;
                }
            }

            *width = rotatedWidth;
            *height = rotatedHeight;

            memcpy(raster, rotated, sizeof(jint) * *width * *height);

            free(rotated);

        }

void NativeDecoder::fixOrientation(jint *pixels, uint32_t pixelsBufferSize, int bitmapwidth, int bitmapheight)
{
	if (origorientation > 4) {
        unsigned int size = bitmapheight * bitmapwidth - 1;
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
                        next = (k * bitmapheight) % size;
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
                        next = (k * bitmapheight) % size;
                        jint buf = pixels[next];
                        pixels[next] = t;
                        t = buf;
                        barray[k] = true;
                        k = next;
                    } while (k != cycleBegin);
                    for (k = 1; k < size && barray[k]; k++);
                }
                //flip horizontally
                for (int j = 0, j1 = bitmapwidth - 1; j < bitmapwidth / 2; j++, j1--) {
                    for (int i = 0; i < bitmapheight; i++) {
                        jint tmp = pixels[j * bitmapheight + i];
                        pixels[j * bitmapheight + i] = pixels[j1 * bitmapheight + i];
                        pixels[j1 * bitmapheight + i] = tmp;
                    }
                }
                //flip vertically
                for (int i = 0, i1 = bitmapheight - 1; i < bitmapheight / 2; i++, i1--) {
                    for (int j = 0; j < bitmapwidth; j++) {
                        jint tmp = pixels[j * bitmapheight + i];
                        pixels[j * bitmapheight + i] = pixels[j * bitmapheight + i1];
                        pixels[j * bitmapheight + i1] = tmp;
                    }
                }
                break;
        }
        free(barray);
    }
}

jbyte * NativeDecoder::createBitmapAlpha8(jint *raster, int bitmapwidth, int bitmapheight)
{
    jbyte *pixels = NULL;
	int pixelsBufferSize = bitmapwidth * bitmapheight;
	pixels = (jbyte *) malloc(sizeof(jbyte) * pixelsBufferSize);
    if (pixels == NULL) {
        LOGE("Can\'t allocate memory for temp buffer");
        return NULL;
    }

	for (int i = 0; i < bitmapwidth; i++) {

	    if (checkStop()) {
            if (pixels) {
                free(pixels);
                pixels = NULL;
            }
            LOGI("Thread stopped");
            return NULL;
        }
    		for (int j = 0; j < bitmapheight; j++) {
    			uint32_t crPix = raster[j * bitmapwidth + i];
    			int alpha = colorMask & crPix >> 24;
    			pixels[j * bitmapwidth + i] = alpha;
    		}
    	}

    	//Close Buffer
        if (raster) {
            _TIFFfree(raster);
            raster = NULL;
        }

	return pixels;
}

unsigned short * NativeDecoder::createBitmapRGB565(jint *buffer, int bitmapwidth, int bitmapheight)
{
    unsigned short *pixels = NULL;
	int pixelsBufferSize = bitmapwidth * bitmapheight;
	pixels = (unsigned short *) malloc(sizeof(unsigned short) * pixelsBufferSize);
    if (pixels == NULL) {
        LOGE("Can\'t allocate memory for temp buffer");
        return NULL;
    }

    for (int i = 0; i < bitmapwidth; i++) {

        if (checkStop()) {
            if (pixels) {
                free(pixels);
                pixels = NULL;
            }
            LOGI("Thread stopped");
            return NULL;
        }

		for (int j = 0; j < bitmapheight; j++) {


			jint crPix = buffer[j * bitmapwidth + i];
			int blue = colorMask & crPix >> 16;
            int green = colorMask & crPix >> 8;
            int red = colorMask & crPix;

            unsigned char B = (blue >> 3);
            unsigned char G = (green >> 2);
            unsigned char R = (red >> 3);

            jint curPix = (R << 11) | (G << 5) | B;

			pixels[j * bitmapwidth + i] = curPix;
		}
	}

	//Close Buffer
    if (buffer) {
        _TIFFfree(buffer);
        buffer = NULL;
    }
    return pixels;
}

int NativeDecoder::getDyrectoryCount()
{
    TIFFSetDirectory(image, 0);
    int dircount = 0;
    do {
        dircount++;
        if (checkStop()) break;
    } while (TIFFReadDirectory(image));
    return dircount;
}

void NativeDecoder::writeDataToOptions(int directoryNumber)
{
    TIFFSetDirectory(image, directoryNumber);
        jfieldID gOptions_outDirectoryCountFieldId = env->GetFieldID(jBitmapOptionsClass,
            "outDirectoryCount", "I");
        int dircount = getDyrectoryCount();
        env->SetIntField(optionsObject, gOptions_outDirectoryCountFieldId, dircount);

        TIFFSetDirectory(image, directoryNumber);
        TIFFGetField(image, TIFFTAG_IMAGEWIDTH, & origwidth);
        TIFFGetField(image, TIFFTAG_IMAGELENGTH, & origheight);

        //Getting image orientation and createing ImageOrientation enum
        TIFFGetField(image, TIFFTAG_ORIENTATION, & origorientation);
        //If orientation field is empty - use ORIENTATION_TOPLEFT
        if (origorientation == 0) {
            origorientation = ORIENTATION_TOPLEFT;
        }
        jclass gOptions_ImageOrientationClass = env->FindClass(
            "org/beyka/tiffbitmapfactory/Orientation");
        jfieldID gOptions_ImageOrientationFieldId = NULL;
        bool flipHW = false;
        LOGII("Orientation", origorientation);
        switch (origorientation) {
            case ORIENTATION_TOPLEFT:
                gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                    "TOP_LEFT",
                    "Lorg/beyka/tiffbitmapfactory/Orientation;");
                break;
            case ORIENTATION_TOPRIGHT:
                gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                    "TOP_RIGHT",
                    "Lorg/beyka/tiffbitmapfactory/Orientation;");
                break;
            case ORIENTATION_BOTRIGHT:
                gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                    "BOT_RIGHT",
                    "Lorg/beyka/tiffbitmapfactory/Orientation;");
                break;
            case ORIENTATION_BOTLEFT:
                gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                    "BOT_LEFT",
                    "Lorg/beyka/tiffbitmapfactory/Orientation;");
                break;
            case ORIENTATION_LEFTTOP:
                flipHW = true;
                gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                    "LEFT_TOP",
                    "Lorg/beyka/tiffbitmapfactory/Orientation;");
                break;
            case ORIENTATION_RIGHTTOP:
                flipHW = true;
                gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                    "RIGHT_TOP",
                    "Lorg/beyka/tiffbitmapfactory/Orientation;");
                break;
            case ORIENTATION_RIGHTBOT:
                flipHW = true;
                gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                    "RIGHT_BOT",
                    "Lorg/beyka/tiffbitmapfactory/Orientation;");
                break;
            case ORIENTATION_LEFTBOT:
                flipHW = true;
                gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                    "LEFT_BOT",
                    "Lorg/beyka/tiffbitmapfactory/Orientation;");
                break;
        }
        if (gOptions_ImageOrientationFieldId != NULL) {
            jobject gOptions_ImageOrientationObj = env->GetStaticObjectField(
                gOptions_ImageOrientationClass,
                gOptions_ImageOrientationFieldId);

            //Set outImageOrientation field to options object
            jfieldID gOptions_outImageOrientationField = env->GetFieldID(jBitmapOptionsClass,
                "outImageOrientation",
                "Lorg/beyka/tiffbitmapfactory/Orientation;");
            env->SetObjectField(optionsObject, gOptions_outImageOrientationField,
                gOptions_ImageOrientationObj);
        }

        //Get resolution variables
        /*
        jfieldID gOptions_outDirectoryCountFieldId = env->GetFieldID(jOptionsClass,
                    "outDirectoryCount", "I");
                int dircount = getDyrectoryCount();
                env->SetIntField(optionsObject, gOptions_outDirectoryCountFieldId, dircount);
        */
        float xresolution, yresolution;
        uint16_t resunit;
        TIFFGetField(image, TIFFTAG_XRESOLUTION, &xresolution);
        LOGIF("xres", xresolution);
        jfieldID gOptions_outXResolutionFieldID = env->GetFieldID(jBitmapOptionsClass, "outXResolution", "F");
        env->SetFloatField(optionsObject, gOptions_outXResolutionFieldID, xresolution);
        TIFFGetField(image, TIFFTAG_YRESOLUTION, &yresolution);
        LOGIF("yres", yresolution);
        jfieldID gOptions_outYResolutionFieldID = env->GetFieldID(jBitmapOptionsClass, "outYResolution", "F");
        env->SetFloatField(optionsObject, gOptions_outYResolutionFieldID, yresolution);
        TIFFGetField(image, TIFFTAG_RESOLUTIONUNIT, &resunit);
        LOGII("resunit", resunit);
        jclass gOptions_ResolutionUnitClass = env->FindClass("org/beyka/tiffbitmapfactory/ResolutionUnit");
        jfieldID gOptions_ResolutionUnitFieldId = NULL;
        switch(resunit) {
            case RESUNIT_INCH:
                gOptions_ResolutionUnitFieldId = env->GetStaticFieldID(gOptions_ResolutionUnitClass,
                            "INCH",
                            "Lorg/beyka/tiffbitmapfactory/ResolutionUnit;");
                break;
            case RESUNIT_CENTIMETER:
                gOptions_ResolutionUnitFieldId = env->GetStaticFieldID(gOptions_ResolutionUnitClass,
                            "CENTIMETER",
                            "Lorg/beyka/tiffbitmapfactory/ResolutionUnit;");
                break;
            case RESUNIT_NONE:
            default:
                gOptions_ResolutionUnitFieldId = env->GetStaticFieldID(gOptions_ResolutionUnitClass,
                            "NONE",
                            "Lorg/beyka/tiffbitmapfactory/ResolutionUnit;");
                break;
        }
        if (gOptions_ResolutionUnitFieldId != NULL) {
            jobject gOptions_ResolutionUnitObj = env->GetStaticObjectField(
                        gOptions_ResolutionUnitClass,
                        gOptions_ResolutionUnitFieldId);

            //Set resolution unit field to options object
            jfieldID gOptions_outResUnitField = env->GetFieldID(jBitmapOptionsClass,
                        "outResolutionUnit",
                        "Lorg/beyka/tiffbitmapfactory/ResolutionUnit;");
            env->SetObjectField(optionsObject, gOptions_outResUnitField,
                        gOptions_ResolutionUnitObj);
        }

        //Get image planar config
        int planarConfig = 0;
        TIFFGetField(image, TIFFTAG_PLANARCONFIG, &planarConfig);
        LOGII("planar config", planarConfig);
        jclass gOptions_PlanarConfigClass = env->FindClass("org/beyka/tiffbitmapfactory/PlanarConfig");
        jfieldID gOptions_PlanarConfigFieldId = NULL;
        switch(planarConfig) {
            case PLANARCONFIG_CONTIG:
                gOptions_PlanarConfigFieldId = env->GetStaticFieldID(gOptions_PlanarConfigClass,
                "CONTIG",
                "Lorg/beyka/tiffbitmapfactory/PlanarConfig;");
                break;
            case PLANARCONFIG_SEPARATE:
                gOptions_PlanarConfigFieldId = env->GetStaticFieldID(gOptions_PlanarConfigClass,
                "SEPARATE",
                "Lorg/beyka/tiffbitmapfactory/PlanarConfig;");
                break;
        }
        if (gOptions_PlanarConfigFieldId != NULL) {
            jobject gOptions_PlanarConfigObj = env->GetStaticObjectField(
                    gOptions_PlanarConfigClass,
                    gOptions_PlanarConfigFieldId);

            jfieldID gOptions_outPlanarConfigField = env->GetFieldID(jBitmapOptionsClass,
                    "outPlanarConfig",
                    "Lorg/beyka/tiffbitmapfactory/PlanarConfig;");
            env->SetObjectField(optionsObject, gOptions_outPlanarConfigField,
                    gOptions_PlanarConfigObj);
        }

        //Getting image compression scheme and createing CompressionScheme enum
        TIFFGetField(image, TIFFTAG_COMPRESSION, & origcompressionscheme);
        LOGII("compression", origcompressionscheme);

        jclass gOptions_ImageCompressionClass = env->FindClass(
            "org/beyka/tiffbitmapfactory/CompressionScheme");
        jfieldID gOptions_ImageCompressionFieldId = NULL;
        switch (origcompressionscheme) {
        case COMPRESSION_NONE:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "NONE",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_CCITTRLE:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "CCITTRLE",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_CCITTFAX3:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "CCITTFAX3",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
                break;
        case COMPRESSION_CCITTFAX4:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
            "CCITTFAX4",
            "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_LZW:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "LZW",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_JPEG:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "JPEG",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_PACKBITS:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "PACKBITS",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_DEFLATE:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "DEFLATE",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_ADOBE_DEFLATE:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "ADOBE_DEFLATE",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        default:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "OTHER",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");

        }
        if (gOptions_ImageCompressionFieldId != NULL) {
            jobject gOptions_ImageCompressionObj = env->GetStaticObjectField(
                gOptions_ImageCompressionClass,
                gOptions_ImageCompressionFieldId);

            //Set outImageOrientation field to options object
            jfieldID gOptions_outCompressionSchemeField = env->GetFieldID(jBitmapOptionsClass,
                "outCompressionScheme",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            env->SetObjectField(optionsObject, gOptions_outCompressionSchemeField,
                gOptions_ImageCompressionObj);
        }

        jfieldID gOptions_OutCurDirNumberFieldID = env->GetFieldID(jBitmapOptionsClass,
            "outCurDirectoryNumber",
            "I");
        env->SetIntField(optionsObject, gOptions_OutCurDirNumberFieldID, directoryNumber);
        if (!flipHW) {
            jfieldID gOptions_outWidthFieldId = env->GetFieldID(jBitmapOptionsClass, "outWidth", "I");
            env->SetIntField(optionsObject, gOptions_outWidthFieldId, origwidth);

            jfieldID gOptions_outHeightFieldId = env->GetFieldID(jBitmapOptionsClass, "outHeight", "I");
            env->SetIntField(optionsObject, gOptions_outHeightFieldId, origheight);
        } else {
            jfieldID gOptions_outWidthFieldId = env->GetFieldID(jBitmapOptionsClass, "outWidth", "I");
            env->SetIntField(optionsObject, gOptions_outWidthFieldId, origheight);

            jfieldID gOptions_outHeightFieldId = env->GetFieldID(jBitmapOptionsClass, "outHeight", "I");
            env->SetIntField(optionsObject, gOptions_outHeightFieldId, origwidth);
        }

        int tagRead = 0;

        int bitPerSample = 0;
        tagRead = TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bitPerSample);
        if (tagRead == 1) {
            LOGII("bit per sample", bitPerSample);
            jfieldID gOptions_outBitPerSampleFieldID = env->GetFieldID(jBitmapOptionsClass, "outBitsPerSample", "I");
            env->SetIntField(optionsObject, gOptions_outBitPerSampleFieldID, bitPerSample);
        }

        int samplePerPixel = 0;
        tagRead = TIFFGetField(image, TIFFTAG_SAMPLESPERPIXEL, &samplePerPixel);
        if (tagRead == 1) {
            LOGII("sample per pixel", samplePerPixel);
            jfieldID gOptions_outSamplePerPixelFieldID = env->GetFieldID(jBitmapOptionsClass, "outSamplePerPixel", "I");
            env->SetIntField(optionsObject, gOptions_outSamplePerPixelFieldID, samplePerPixel);
        }

        //Tile size
        int tileWidth = 0;
        tagRead = TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileWidth);
            if (tagRead == 1) {
            LOGII("tile width", tileWidth);
            jfieldID gOptions_outTileWidthFieldID = env->GetFieldID(jBitmapOptionsClass, "outTileWidth", "I");
            env->SetIntField(optionsObject, gOptions_outTileWidthFieldID, tileWidth);
        }
        int tileHeight = 0;
        tagRead = TIFFGetField(image, TIFFTAG_TILELENGTH, &tileHeight);
        if (tagRead == 1) {
            LOGII("tile height", tileHeight);
            jfieldID gOptions_outTileHeightFieldID = env->GetFieldID(jBitmapOptionsClass, "outTileHeight", "I");
            env->SetIntField(optionsObject, gOptions_outTileHeightFieldID, tileHeight);
        }

        //row per strip
        int rowPerStrip = 0;
        tagRead = TIFFGetField(image, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);
        if (tagRead == 1) {
            LOGII("row per strip", rowPerStrip);
            jfieldID gOptions_outRowPerStripFieldID = env->GetFieldID(jBitmapOptionsClass, "outRowPerStrip", "I");
            env->SetIntField(optionsObject, gOptions_outRowPerStripFieldID, rowPerStrip);
        }

        //strip size
        uint32_t stripSize = TIFFStripSize (image);
        LOGII("strip size", stripSize);
        jfieldID gOptions_outStripSizeFieldID = env->GetFieldID(jBitmapOptionsClass, "outStripSize", "I");
        env->SetIntField(optionsObject, gOptions_outStripSizeFieldID, stripSize);

        //strip max
        uint32_t stripMax = TIFFNumberOfStrips (image);
        LOGII("number of strips", stripMax);
        jfieldID gOptions_outStripMaxFieldID = env->GetFieldID(jBitmapOptionsClass, "outNumberOfStrips", "I");
        env->SetIntField(optionsObject, gOptions_outStripMaxFieldID, stripMax);

        //photometric
        uint16_t photometric = PHOTOMETRIC_MINISWHITE;
        TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photometric);
        LOGII("photometric", photometric);
        jclass gOptions_PhotometricClass = env->FindClass("org/beyka/tiffbitmapfactory/Photometric");
        jfieldID gOptions_PhotometricFieldId = NULL;
                switch(photometric) {
                    case PHOTOMETRIC_MINISWHITE:
                        gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                        "MINISWHITE",
                        "Lorg/beyka/tiffbitmapfactory/Photometric;");
                        break;
                    case PHOTOMETRIC_MINISBLACK:
                        gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                        "MINISBLACK",
                        "Lorg/beyka/tiffbitmapfactory/Photometric;");
                        break;
                    case PHOTOMETRIC_RGB:
                         gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                         "RGB",
                         "Lorg/beyka/tiffbitmapfactory/Photometric;");
                         break;
                    case PHOTOMETRIC_PALETTE:
                         gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                         "PALETTE",
                         "Lorg/beyka/tiffbitmapfactory/Photometric;");
                         break;
                    case PHOTOMETRIC_MASK:
                         gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                         "MASK",
                         "Lorg/beyka/tiffbitmapfactory/Photometric;");
                         break;
                    case PHOTOMETRIC_SEPARATED:
                         gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                         "SEPARATED",
                         "Lorg/beyka/tiffbitmapfactory/Photometric;");
                         break;
                    case PHOTOMETRIC_YCBCR:
                         gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                         "YCBCR",
                         "Lorg/beyka/tiffbitmapfactory/Photometric;");
                         break;
                    case PHOTOMETRIC_CIELAB:
                         gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                         "CIELAB",
                         "Lorg/beyka/tiffbitmapfactory/Photometric;");
                         break;
                    case PHOTOMETRIC_ICCLAB:
                         gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                         "ICCLAB",
                         "Lorg/beyka/tiffbitmapfactory/Photometric;");
                         break;
                    case PHOTOMETRIC_ITULAB:
                         gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                         "ITULAB",
                         "Lorg/beyka/tiffbitmapfactory/Photometric;");
                         break;
                    case PHOTOMETRIC_LOGL:
                         gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                         "LOGL",
                         "Lorg/beyka/tiffbitmapfactory/Photometric;");
                         break;
                    case PHOTOMETRIC_LOGLUV:
                         gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                         "LOGLUV",
                         "Lorg/beyka/tiffbitmapfactory/Photometric;");
                         break;
                    default:
                        gOptions_PhotometricFieldId = env->GetStaticFieldID(gOptions_PhotometricClass,
                        "OTHER",
                        "Lorg/beyka/tiffbitmapfactory/Photometric;");
                        break;
                }
        if (gOptions_PhotometricFieldId != NULL) {
                    jobject gOptions_PhotometricObj = env->GetStaticObjectField(
                            gOptions_PhotometricClass,
                            gOptions_PhotometricFieldId);

                    jfieldID gOptions_outPhotometricField = env->GetFieldID(jBitmapOptionsClass,
                            "outPhotometric",
                            "Lorg/beyka/tiffbitmapfactory/Photometric;");
                    env->SetObjectField(optionsObject, gOptions_outPhotometricField,
                            gOptions_PhotometricObj);
        }

        //FillOrder
        int fillOrder = 0;
        TIFFGetField(image, TIFFTAG_FILLORDER, &fillOrder);
        LOGII("fill Order", fillOrder);
        jclass gOptions_FillOrderClass = env->FindClass("org/beyka/tiffbitmapfactory/FillOrder");
        jfieldID gOptions_FillOrderFieldId = NULL;
        switch(fillOrder) {
        case FILLORDER_MSB2LSB:
            gOptions_FillOrderFieldId = env->GetStaticFieldID(gOptions_FillOrderClass,
            "MSB2LSB",
            "Lorg/beyka/tiffbitmapfactory/FillOrder;");
            break;
        case PLANARCONFIG_SEPARATE:
            gOptions_FillOrderFieldId = env->GetStaticFieldID(gOptions_FillOrderClass,
            "LSB2MSB",
            "Lorg/beyka/tiffbitmapfactory/FillOrder;");
            break;
        }
        if (gOptions_FillOrderFieldId != NULL) {
            jobject gOptions_FillOrderObj = env->GetStaticObjectField(
            gOptions_FillOrderClass,
            gOptions_FillOrderFieldId);

            jfieldID gOptions_outFillOrderField = env->GetFieldID(jBitmapOptionsClass,
            "outFillOrder",
            "Lorg/beyka/tiffbitmapfactory/FillOrder;");
            env->SetObjectField(optionsObject, gOptions_outFillOrderField,
            gOptions_FillOrderObj);
        }

        //Author
        const char * artist;
        tagRead = TIFFGetField(image, TIFFTAG_ARTIST, & artist);
        if (tagRead == 1) {
            LOGI(artist);
            jstring jauthor = charsToJString(artist);//env->NewStringUTF(artist);
            jfieldID gOptions_outAuthorFieldId = env->GetFieldID(jBitmapOptionsClass, "outAuthor", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outAuthorFieldId, jauthor);
            env->DeleteLocalRef(jauthor);
            //free(artist);
        }

        //Copyright
        const char * copyright;
        tagRead = TIFFGetField(image, TIFFTAG_COPYRIGHT, & copyright);
        if (tagRead == 1) {
            LOGI(copyright);
            jstring jcopyright = charsToJString(copyright);//env->NewStringUTF(copyright);
            jfieldID gOptions_outCopyrightFieldId = env->GetFieldID(jBitmapOptionsClass, "outCopyright", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outCopyrightFieldId, jcopyright);
            env->DeleteLocalRef(jcopyright);
            //free(copyright);
        }

        //ImageDescription
        const char * imgDescr;
        tagRead = TIFFGetField(image, TIFFTAG_IMAGEDESCRIPTION, & imgDescr);
        if (tagRead == 1) {
            LOGI(imgDescr);
            jstring jimgDescr = charsToJString(imgDescr);//env->NewStringUTF(imgDescr);
            jfieldID gOptions_outimgDescrFieldId = env->GetFieldID(jBitmapOptionsClass, "outImageDescription", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outimgDescrFieldId, jimgDescr);
            env->DeleteLocalRef(jimgDescr);
            //free(imgDescr);
        }

        //Software
        const char * software;
        tagRead = TIFFGetField(image, TIFFTAG_SOFTWARE, & software);
        if (tagRead == 1) {
            LOGI(software);
            jstring jsoftware = charsToJString(software);//env->NewStringUTF(software);
            jfieldID gOptions_outsoftwareFieldId = env->GetFieldID(jBitmapOptionsClass, "outSoftware", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outsoftwareFieldId, jsoftware);
            env->DeleteLocalRef(jsoftware);
            //free(software);
        }

        //DateTime
        const char * datetime;
        tagRead = TIFFGetField(image, TIFFTAG_DATETIME, & datetime);
        if (tagRead == 1) {
            LOGI(datetime);
            jstring jdatetime = charsToJString(datetime);//env->NewStringUTF(datetime);
            jfieldID gOptions_outdatetimeFieldId = env->GetFieldID(jBitmapOptionsClass, "outDatetime", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outdatetimeFieldId, jdatetime);
            env->DeleteLocalRef(jdatetime);
            //free(datetime);
        }

        //Host Computer
        const char * host;
        tagRead = TIFFGetField(image, TIFFTAG_HOSTCOMPUTER, & host);
        if (tagRead == 1) {
            LOGI(host);
            jstring jhost = charsToJString(host);//env->NewStringUTF(host);
            jfieldID gOptions_outhostFieldId = env->GetFieldID(jBitmapOptionsClass, "outHostComputer", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outhostFieldId, jhost);
            env->DeleteLocalRef(jhost);
            //free(host);
        }
}

jstring NativeDecoder::charsToJString(const char *chars) {
    std::string str(chars);
    jbyteArray array = env->NewByteArray(str.size());
    env->SetByteArrayRegion(array, 0, str.size(), (const jbyte*)str.c_str());
    jstring strEncode = env->NewStringUTF("UTF-8");
    jclass cls = env->FindClass("java/lang/String");
    jmethodID ctor = env->GetMethodID(cls, "<init>", "([BLjava/lang/String;)V");
    jstring object = (jstring) env->NewObject(cls, ctor, array, strEncode);
    return object;
 //return NULL;
}

jboolean NativeDecoder::checkStop() {
    jboolean interupted = env->CallStaticBooleanMethod(jThreadClass, threadInterruptedMethodId);

    jboolean stop;

    if (optionsObject) {
        stop = env->GetBooleanField(optionsObject, stoppedFieldId);

    } else {
        stop = JNI_FALSE;
    }

    return interupted || stop;
}

void NativeDecoder::sendProgress(jlong current, jlong total) {
    if (listenerObject != NULL && progressReportMethodId != NULL) {
        if (total > 0) {
            if (current < 0) current = 0;
            if (current > total) current = total;
        }
        if (total == lastProgressTotal && current == lastProgressCurrent) {
            return;
        }
        const jlong minDelta = total > 0 ? (total / 100 > 0 ? total / 100 : 1) : 1;
        const bool newStage = total != lastProgressTotal;
        const bool endpoint = current <= 0 || current >= total;
        if (!newStage && !endpoint && current - lastProgressCurrent < minDelta) {
            return;
        }
        env->CallVoidMethod(listenerObject, progressReportMethodId, current, total);
        lastProgressCurrent = current;
        lastProgressTotal = total;
    }
}

void NativeDecoder::tileErrorHandler(int code, siginfo_t *siginfo, void *sc) {
    LOGE("tileErrorHandler");
    longjmp(tile_buf, 1);
}

void NativeDecoder::stripErrorHandler(int code, siginfo_t *siginfo, void *sc) {
    LOGE("stripErrorHandler");
    longjmp(strip_buf, 1);
}

void NativeDecoder::imageErrorHandler(int code, siginfo_t *siginfo, void *sc) {
    LOGE("imageErrorHandler");
    longjmp(image_buf, 1);
}

void NativeDecoder::generalErrorHandler(int code, siginfo_t *siginfo, void *sc) {
    LOGE("generalErrorHandler");
    longjmp(general_buf, 1);
}

void NativeDecoder::throwDecodeFileException(const char *message) {
    jstring adinf = env->NewStringUTF(message);
    if (decodingMode == DECODE_MODE_FILE_PATH) {
        throw_decode_file_exception(env, jPath, adinf);
    } else if (decodingMode == DECODE_MODE_FILE_DESCRIPTOR) {
        throw_decode_file_exception_fd(env, jFd, adinf);
    }
    env->DeleteLocalRef(adinf);
}

void NativeDecoder::throwCantOpenFileException() {
    if (decodingMode == DECODE_MODE_FILE_PATH) {
        throw_cant_open_file_exception(env, jPath);
    } else if (decodingMode == DECODE_MODE_FILE_DESCRIPTOR) {
        throw_cant_open_file_exception_fd(env, jFd);
    }
}
