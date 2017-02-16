//
// Created by beyka on 3.2.17.
//

#include "NativeDecoder.h"
//#include "NativeExceptions.h"

    /*const int NativeDecoder::colorMask = 0xFF;
    const int NativeDecoder::ARGB_8888 = 2;
    const int NativeDecoder::RGB_565 = 4;
    const int NativeDecoder::ALPHA_8 = 8;*/

NativeDecoder::NativeDecoder(JNIEnv *e, jclass c, jstring path, jobject opts)
{
    availableMemory = 8000*8000*4; // use 244Mb restriction for decoding full image
    env = e;
    clazz = c;
    optionsObject = opts;
    jPath = path;

    origwidth = 0;
    origheight = 0;
    origorientation = 0;
    origcompressionscheme = 0;
    invertRedAndBlue = false;
    //availableMemory = -1;


}

NativeDecoder::~NativeDecoder()
{
    if (image) {
        TIFFClose(image);
        image = NULL;
        LOGI("Tiff is closed");
    }

    //Release global reference for Bitmap.Config
    if (preferedConfig) {
        env->DeleteGlobalRef(preferedConfig);
        preferedConfig = NULL;
    }
}

jobject NativeDecoder::getBitmap()
{
        const char *strPath = NULL;
        strPath = env->GetStringUTFChars(jPath, 0);
        LOGIS("nativeTiffOpen", strPath);

        image = TIFFOpen(strPath, "r");
        env->ReleaseStringUTFChars(jPath, strPath);
        if (image == NULL) {
            throw_no_such_file_exception(env, jPath);
            LOGES("Can\'t open bitmap", strPath);
            return NULL;
        }
        LOGI("Tiff is open");

    //Get options
        jclass jBitmapOptionsClass = env->FindClass(
                "org/beyka/tiffbitmapfactory/TiffBitmapFactory$Options");

        jfieldID gOptions_sampleSizeFieldID = env->GetFieldID(jBitmapOptionsClass, "inSampleSize", "I");
        jint inSampleSize = env->GetIntField(optionsObject, gOptions_sampleSizeFieldID);

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

        env->DeleteLocalRef(jBitmapOptionsClass);

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

        //if directory number < 0 set it to 0
        if (inDirectoryNumber < 0) inDirectoryNumber = 0;

        jobject java_bitmap = NULL;

        writeDataToOptions(inDirectoryNumber);

        if (!inJustDecodeBounds) {
            TIFFSetDirectory(image, inDirectoryNumber);
            TIFFGetField(image, TIFFTAG_IMAGEWIDTH, &origwidth);
            TIFFGetField(image, TIFFTAG_IMAGELENGTH, &origheight);
            java_bitmap = createBitmap(inSampleSize, inDirectoryNumber);
        }

        //releaseImage(env);

        return java_bitmap;
}

jobject NativeDecoder::createBitmap(int inSampleSize, int directoryNumber)
{
//Read Config from options. Use ordinal field from ImageConfig class
    jclass configClass = env->FindClass(
            "org/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageConfig");
    jfieldID ordinalFieldID = env->GetFieldID(configClass, "ordinal", "I");
    jint configInt = env->GetIntField(preferedConfig, ordinalFieldID);

    env->DeleteLocalRef(configClass);

    int bitdepth = 0;
    TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bitdepth);
    if (bitdepth != 1 && bitdepth != 4 && bitdepth != 8 && bitdepth != 16) {
        LOGE("Only 1, 4, 8, and 16 bits per sample supported");
        throw_decode_file_exception(env, jPath);
        return NULL;
    }

    int newBitmapWidth = 0;
    int newBitmapHeight = 0;

    jint *raster = NULL;


    switch(getDecodeMethod()) {
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


    //Class and field for Bitmap.Config
    jclass bitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
    jfieldID bitmapConfigField = NULL;
    void *processedBuffer = NULL;
    if (configInt == ARGB_8888) {
        processedBuffer = raster;
        bitmapConfigField = env->GetStaticFieldID(bitmapConfigClass, "ARGB_8888",
                                                  "Landroid/graphics/Bitmap$Config;");
    } else if (configInt == ALPHA_8) {
        processedBuffer = createBitmapAlpha8(raster, newBitmapWidth,
                                             newBitmapHeight);
        bitmapConfigField = env->GetStaticFieldID(bitmapConfigClass, "ALPHA_8",
                                                  "Landroid/graphics/Bitmap$Config;");
    } else if (configInt == RGB_565) {
        processedBuffer = createBitmapRGB565(raster, newBitmapWidth,
                                             newBitmapHeight);
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

    env->DeleteLocalRef(bitmapConfigClass);

    jobject java_bitmap = NULL;
    if (origorientation > 4) {
        java_bitmap = env->CallStaticObjectMethod(bitmapClass, methodid, newBitmapHeight,
                                                  newBitmapWidth, config);

    } else {
        java_bitmap = env->CallStaticObjectMethod(bitmapClass, methodid, newBitmapWidth,
                                                  newBitmapHeight, config);
    }
    //remove not used references
    env->DeleteLocalRef(config);
    env->DeleteLocalRef(bitmapClass);

    //Copy data to bitmap
    int ret;
    void *bitmapPixels;
    if ((ret = AndroidBitmap_lockPixels(env, java_bitmap, &bitmapPixels)) < 0) {
        //error
        LOGE("Lock pixels failed");
        return NULL;
    }
    int pixelsCount = newBitmapWidth * newBitmapHeight;

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



    return java_bitmap;
}

jint * NativeDecoder::getSampledRasterFromStrip(int inSampleSize, int *bitmapwidth, int *bitmapheight) {

    unsigned long allocatedTotal = 0;

    LOGII("width", origwidth);
    LOGII("height", origheight);

    jint *pixels = NULL;
    *bitmapwidth = origwidth / inSampleSize;
    *bitmapheight = origheight / inSampleSize;
    uint32 pixelsBufferSize = *bitmapwidth * *bitmapheight;
    int origImageBufferSize = origwidth * origheight;

    LOGII("new width", *bitmapwidth);
    LOGII("new height", *bitmapheight);

    uint32 stripSize = TIFFStripSize (image);
    uint32 stripMax = TIFFNumberOfStrips (image);
    LOGII("strip size ", stripSize);
    LOGII("stripMax  ", stripMax);
    int rowPerStrip = -1;
    TIFFGetField(image, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);
    LOGII("rowsperstrip", rowPerStrip);

    unsigned long estimateMem = 0;
    estimateMem += (sizeof(jint) * pixelsBufferSize); //buffer for decoded pixels
    estimateMem += (origwidth * sizeof(uint32)); //work line for rotate strip
    estimateMem += (origwidth * rowPerStrip * sizeof (uint32) * 2); //current and next strips
    estimateMem += (sizeof(jint) * origwidth * 2); //bottom and top lines for reading pixel(matrixBottomLine, matrixTopLine)
    LOGII("estimateMem", estimateMem);
    if (estimateMem > availableMemory) {
        throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        return NULL;
    }

    pixels = (jint *) malloc(sizeof(jint) * pixelsBufferSize);
    if (pixels == NULL) {
        LOGE("Can\'t allocate memory for temp buffer");
        return NULL;
    }
    allocatedTotal += sizeof(jint) * pixelsBufferSize;

    uint32* work_line_buf = (uint32 *)_TIFFmalloc(origwidth * sizeof(uint32));
    allocatedTotal += origwidth * sizeof(uint32);
    uint32* raster;
    uint32* rasterForBottomLine; // in this raster copy next strip for getting bottom line in matrix color selection
    if (rowPerStrip == -1 && stripMax == 1) {
            raster = (uint32 *)_TIFFmalloc(origImageBufferSize * sizeof (uint32));
            rasterForBottomLine = (uint32 *)_TIFFmalloc(origImageBufferSize * sizeof (uint32));
            LOGII("Allocated ", (origImageBufferSize * sizeof (uint32) * 2));
    } else {
            raster = (uint32 *)_TIFFmalloc(origwidth * rowPerStrip * sizeof (uint32));
            rasterForBottomLine = (uint32 *)_TIFFmalloc(origwidth * rowPerStrip * sizeof (uint32));
            LOGII("Allocated ", (origwidth * rowPerStrip * sizeof (uint32) * 2));
            allocatedTotal += origwidth * rowPerStrip * sizeof (uint32);
            allocatedTotal += origwidth * rowPerStrip * sizeof (uint32);
    }
    if (rowPerStrip == -1) {
            rowPerStrip = origheight;
    }

    int writedLines = 0;
    int nextStripOffset = 0;
    int globalLineCounter = 0;

    unsigned int *matrixTopLine = (uint32 *) malloc(sizeof(jint) * origwidth);
    unsigned int *matrixBottomLine = (uint32 *) malloc(sizeof(jint) * origwidth);
    allocatedTotal += sizeof(jint) * origwidth;
    allocatedTotal += sizeof(jint) * origwidth;

    int isSecondRasterExist = 0;
    int ok = 1;
    for (int i = 0; i < origheight - rowPerStrip; i += rowPerStrip) {
            //LOGII("writedlines ", writedLines);
            //LOGII("line counter ", globalLineCounter);

        uint32 rows_to_write = 0;
            //if second raster is exist - copy it to work raster end decode next strip
            if (isSecondRasterExist) {
                _TIFFmemcpy(raster, rasterForBottomLine, origwidth * rowPerStrip * sizeof (uint32));

                //If next strip is exist - decode it, invert lines
                if (i + rowPerStrip < origheight) {
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
            } else {
                if (isSecondRasterExist) {
                    _TIFFmemcpy(matrixBottomLine, rasterForBottomLine /*+ lineAddrToCopyBottomLine * origwidth*/, sizeof(unsigned int) * origwidth);
                }
                 int workWritedLines = writedLines;
                 for (int resBmpY = workWritedLines, workY = 0; resBmpY < *bitmapheight && workY < rowPerStrip; /*wj++,*/ workY ++/*= inSampleSize*/) {
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

                            /*if (resBmpY >= 2500) {
                                unsigned int t1 = resBmpY * *bitmapwidth + resBmpX;
                                LOGII("write pixel", t1);
                            }*/
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
                LOGI("raster");
                if (rasterForBottomLine) {
                    _TIFFfree(rasterForBottomLine);
                    rasterForBottomLine = NULL;
                }
                LOGI("second raster");

        if (matrixTopLine) {
            _TIFFfree(matrixTopLine);
            matrixTopLine = NULL;
        }
        LOGI("matrixTopLine");
        if (matrixBottomLine) {
            _TIFFfree(matrixBottomLine);
            matrixBottomLine = NULL;
        }
        LOGI("matrixBottomLine");

        fixOrientation(pixels, pixelsBufferSize, *bitmapwidth, *bitmapheight);

        int mbUsed = allocatedTotal/1024/1024;
        LOGII("Max memmory use ", mbUsed);
        return pixels;
}

void NativeDecoder::rotateTileLinesVertical(uint32 tileHeight, uint32 tileWidth, uint32* whatRotate, uint32 *bufferLine) {
    for (int line = 0; line < tileHeight / 2; line++) {
        unsigned int  *top_line, *bottom_line;
        top_line = whatRotate + tileWidth * line;
        bottom_line = whatRotate + tileWidth * (tileHeight - line -1);
        _TIFFmemcpy(bufferLine, top_line, sizeof(unsigned int) * tileWidth);
        _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * tileWidth);
        _TIFFmemcpy(bottom_line, bufferLine, sizeof(unsigned int) * tileWidth);
    }
}

void NativeDecoder::rotateTileLinesHorizontal(uint32 tileHeight, uint32 tileWidth, uint32* whatRotate, uint32 *bufferLine) {
    uint32 buf;
    for (int y = 0; y < tileHeight; y++) {
        for (int x = 0; x < tileWidth / 2; x++) {
            buf = whatRotate[y * tileWidth + x];
            whatRotate[y * tileWidth + x] = whatRotate[y * tileWidth + tileWidth - x - 1];
            whatRotate[y * tileWidth + tileWidth - x - 1] = buf;
        }
    }
}

jint * NativeDecoder::getSampledRasterFromTile(int inSampleSize, int *bitmapwidth, int *bitmapheight) {

        unsigned long allocatedTotal = 0;

        LOGII("width", origwidth);
        LOGII("height", origheight);

        jint *pixels = NULL;
        *bitmapwidth = origwidth / inSampleSize;
        *bitmapheight = origheight / inSampleSize;
        uint32 pixelsBufferSize = *bitmapwidth * *bitmapheight;
        LOGII("new width", *bitmapwidth);
        LOGII("new height", *bitmapheight);

        uint32 tileWidth = 0, tileHeight = 0;
        TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileWidth);
        TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileHeight);
        LOGII("Tile width", tileWidth);
        LOGII("Tile height", tileHeight);
/*
        *bitmapwidth = tileWidth;
        *bitmapheight = tileHeight;
        pixelsBufferSize = *bitmapwidth * *bitmapheight;
*/
        unsigned long estimateMem = 0;
        estimateMem += (sizeof(jint) * pixelsBufferSize); //buffer for decoded pixels
        estimateMem += (tileWidth * tileHeight * sizeof(uint32)) * 3; //current, left and right tiles buffers
        estimateMem += (tileWidth * sizeof(uint32)); //work line for rotate tile
        LOGII("estimateMem", estimateMem);
        if (estimateMem > availableMemory) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
            return NULL;
        }

        pixels = (jint *) malloc(sizeof(jint) * pixelsBufferSize);
        if (pixels == NULL) {
            LOGE("Can\'t allocate memory for temp buffer");
            return NULL;
        }
        allocatedTotal += sizeof(jint) * pixelsBufferSize;

        uint32 row, column;

        //main worker tile
        uint32 *rasterTile = (uint32 *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32));
        allocatedTotal += tileWidth * tileHeight * sizeof(uint32);
        //left tile
        uint32 *rasterTileLeft = (uint32 *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32));
        allocatedTotal += tileWidth * tileHeight * sizeof(uint32);
        //right tile
        uint32 *rasterTileRight = (uint32 *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32));
        allocatedTotal += tileWidth * tileHeight * sizeof(uint32);

        uint32 *work_line_buf = (uint32*)_TIFFmalloc(tileWidth * sizeof (uint32));
        allocatedTotal += tileWidth * sizeof(uint32);

        //this variable calculate processed pixels for x and y direction to make right offsets at the begining of next tile
        //offset calculated from condition globalProcessed % inSampleSize should be 0
        uint32 globalProcessedX = 0;
        uint32 globalProcessedY = 0;

        for (row = 0; row < origheight; row += tileHeight) {
            short leftTileExists = 0;
            short rightTileExists = 0;
            for (column = 0; column < origwidth; column += tileWidth) {
                //If not first column - we should have previous tile - copy it to left tile buffer
                if (column != 0) {
                    _TIFFmemcpy(rasterTileLeft, rasterTile, tileWidth * tileHeight * sizeof(uint32));
                    leftTileExists = 1;
                } else {
                    leftTileExists = 0;
                }

                //if current column + tile width is less than origin width - we have right tile - copy it to current tile and read next tile to rasterTileRight buffer
                if (column + tileWidth < origwidth && rightTileExists) {
                    _TIFFmemcpy(rasterTile, rasterTileRight, tileWidth * tileHeight * sizeof(uint32));
                    TIFFReadRGBATile(image, column + tileWidth, row, rasterTileRight);
                    rightTileExists = 1;
                } else if (column + tileWidth < origwidth) {
                    //have right tile but this is first tile in row, so need to read raster and right raster
                    TIFFReadRGBATile(image, column + tileWidth, row, rasterTileRight);
                    TIFFReadRGBATile(image, column, row, rasterTile);
                    rightTileExists = 1;

                    //in that case we also need to invert lines in rasterTile
                    switch(origorientation) {
                        case 1:
                        case 5:
                            rotateTileLinesVertical(tileHeight, tileWidth, rasterTile, work_line_buf);
                            break;
                        case 2:
                        case 6:
                            rotateTileLinesVertical(tileHeight, tileWidth, rasterTile, work_line_buf);
                            rotateTileLinesHorizontal(tileHeight, tileWidth, rasterTile, work_line_buf);
                            break;
                        case 3:
                        case 7:
                            rotateTileLinesHorizontal(tileHeight, tileWidth, rasterTile, work_line_buf);
                            break;
                    }
                } else {
                    //otherwise we haven't right tile buffer, so we should read tile to current buffer
                    TIFFReadRGBATile(image, column, row, rasterTile);
                    rightTileExists = 0;
                }

                LOGII("left tile exist", leftTileExists);
                LOGII("right tile exist", rightTileExists);

                //if we have right tile - current tile already rotated and we need to rotate only right tile
                if (rightTileExists) {
                    switch(origorientation) {
                        case 1:
                        case 5:
                            rotateTileLinesVertical(tileHeight, tileWidth, rasterTileRight, work_line_buf);
                            break;
                        case 2:
                        case 6:
                            rotateTileLinesVertical(tileHeight, tileWidth, rasterTileRight, work_line_buf);
                            rotateTileLinesHorizontal(tileHeight, tileWidth, rasterTileRight, work_line_buf);
                            break;
                        case 3:
                        case 7:
                            rotateTileLinesHorizontal(tileHeight, tileWidth, rasterTileRight, work_line_buf);
                            break;
                    }
                } else {
                    //otherwise - current tile not rotated so rotate it
                    //tile orig is on bottom left - should change lines
                     switch(origorientation) {
                        case 1:
                        case 5:
                            rotateTileLinesVertical(tileHeight, tileWidth, rasterTile, work_line_buf);
                            break;
                        case 2:
                        case 6:
                            rotateTileLinesVertical(tileHeight, tileWidth, rasterTile, work_line_buf);
                            rotateTileLinesHorizontal(tileHeight, tileWidth, rasterTile, work_line_buf);
                            break;
                        case 3:
                        case 7:
                            rotateTileLinesHorizontal(tileHeight, tileWidth, rasterTile, work_line_buf);
                            break;
                    }
                }

                if (inSampleSize > 1 )
                {
                    //Tile could begin from not filled pixel(pixel[x,y] == 0). This variables allow to calculate begining of filled pixels
                    int tileStartDataX = -1;
                    int tileStartDataY = -1;

                    for (int origTileY = 0, pixY = row/inSampleSize; origTileY < tileHeight && pixY < *bitmapheight; origTileY++) {
                        if (tileStartDataY != -1 && globalProcessedY % inSampleSize != 0) {
                            if (tileStartDataY != -1) {
                                globalProcessedY++;
                            }
                        }
                        else
                        {
                            for (int origTileX = 0, pixX = column/inSampleSize; origTileX < tileWidth && pixX < *bitmapwidth; origTileX++) {
                                if (tileStartDataX != -1 && globalProcessedX % inSampleSize != 0)
                                {
                                    if (tileStartDataX != -1) {
                                        globalProcessedX++;
                                    }
                                }
                                else
                                {
                                    uint32 srcPosition = origTileY * tileWidth + origTileX;
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

                                        //using kernel 3x3
/*
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
*/
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
                                uint32 srcPosition = th * tileWidth + tw;
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

                     /*else {
                        //LOGII("bh", *bitmapheight);
                        //LOGII("bw", *bitmapwidth);
                        int rowHasPixels = 0;
                        for (int th = 0, bh = 0; th < tileHeight ; th++) {
                            for (int tw = 0, bw = 0; tw < tileWidth ; tw++) {
                                uint32 srcPosition = th * tileWidth + tw;
                                if (rasterTile[srcPosition] != 0) {
                                    int position = bw * *bitmapheight + bh;
                                    pixels[position] = rasterTile[srcPosition];
                                    rowHasPixels = 1;
                                    bw++;
                                }
                            }
                            if (rowHasPixels) {
                                bh++;
                                rowHasPixels = 0;
                            }
                        }*/
                        //fixTileOrientation(rasterTile, tileHeight * tileWidth, tileWidth, tileHeight);
                        /*for (int th = 0; th < tileHeight && th < origheight; th++) {
                            for (int tw = 0; tw < tileWidth && tw < origwidth; tw++) {
                                uint32 srcPosition = th * tileWidth + tw;
                                if (rasterTile[srcPosition] != 0) {
                                    int position = (row + th) * origwidth + column + tw;
                                    pixels[position] = rasterTile[srcPosition];
                                }
                            }
                        }*/
                    /*
                        for (int th = 0; th < tileHeight && th < origheight; th++) {
                            for (int tw = 0; tw < tileWidth && tw < origwidth; tw++) {
                                uint32 srcPosition = tw * tileHeight + th;
                                if (rasterTile[srcPosition] != 0) {
                                    int position = (row + th) * origwidth + column + tw;
                                    //int position =(column + tw) * origheight + row + th;
                                    pixels[position] = rasterTile[srcPosition];
                                }
                            }
                        }*/
                    //}
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

        //rotateRaster(pixels, 90, bitmapwidth, bitmapheight);
        //fixOrientation(pixels, pixelsBufferSize, *bitmapwidth, *bitmapheight);

        int mbUsed = allocatedTotal/1024/1024;
        LOGII("Max memmory use ", mbUsed);
        return pixels;
}



jint * NativeDecoder::getSampledRasterFromImage(int inSampleSize, int *bitmapwidth, int *bitmapheight)
{
    unsigned long allocatedTotal = 0;

    //buffer size for decoding tiff image in RGBA format
    int origBufferSize = origwidth * origheight * sizeof(unsigned int);
    LOGII("origbuffsize", origBufferSize);

    *bitmapwidth = origwidth / inSampleSize;
    *bitmapheight = origheight / inSampleSize;
    //buffer size for creating scaled image;
    uint32 pixelsBufferSize = *bitmapwidth * *bitmapheight * sizeof(jint);
    LOGII("pixelsBufferSize", pixelsBufferSize);

    /**Estimate usage of memory for decoding*/
    unsigned long estimateMem = origBufferSize;//origBufferSize - size of decoded RGBA image
    if (inSampleSize > 1) {
        estimateMem += pixelsBufferSize; //if inSmapleSize greater than 1 we need aditional vevory for scaled image
    }
    LOGII("estimateMem", estimateMem);

    if (estimateMem > availableMemory) {
        throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        return NULL;
    }

    unsigned int *origBuffer = NULL;

    origBuffer = (unsigned int *) _TIFFmalloc(origBufferSize);
    if (origBuffer == NULL) {
        LOGE("Can\'t allocate memory for origBuffer");
        return NULL;
    }
    allocatedTotal += origBufferSize;

	if (0 ==
        TIFFReadRGBAImageOriented(image, origwidth, origheight, origBuffer, ORIENTATION_TOPLEFT, 0)) {
	    free(origBuffer);
        LOGE("Error reading image.");
        throw_decode_file_exception(env, jPath);
        return NULL;
    }

    jint *pixels = NULL;

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
            allocatedTotal += sizeof(jint) * pixelsBufferSize;
            for (int i = 0, i1 = 0; i < *bitmapwidth; i++, i1 += inSampleSize) {
                for (int j = 0, j1 = 0; j < *bitmapheight; j++, j1 += inSampleSize) {

                    //Apply filter to pixel
                    jint crPix = origBuffer[j1 * origwidth + i1];
                    int sum = 1;

                    int alpha = colorMask & crPix >> 24;
                    int red = colorMask & crPix >> 16;
                    int green = colorMask & crPix >> 8;
                    int blue = colorMask & crPix;

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

    fixOrientation(pixels, pixelsBufferSize, *bitmapwidth, *bitmapheight);

    int mbUsed = allocatedTotal/1024/1024;
    LOGII("Max memmory use ", mbUsed);
    return pixels;
}

int NativeDecoder::getDecodeMethod()
{
	int method = -1;
	uint32 tileWidth, tileHeight;
	int readTW = 0, readTH = 0;
    readTW = TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileWidth);
    readTH = TIFFGetField(image, TIFFTAG_TILEWIDTH, &tileHeight);
    if (tileWidth > 0 && tileHeight > 0 && readTH > 0 && readTW > 0) {
        method = DECODE_METHOD_TILE;
    } else {
        int rowPerStrip = -1;
    	TIFFGetField(image, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);
    	uint32 stripSize = TIFFStripSize (image);
    	uint32 stripMax = TIFFNumberOfStrips (image);
    	int estimate = origwidth * 3;
    	LOGII("RPS", rowPerStrip);
    	LOGII("stripSize", stripSize);
    	LOGII("stripMax", stripMax);
    	if (rowPerStrip != -1 && stripSize > 0 && stripMax > 1) {
    	    method = DECODE_METHOD_STRIP;
    	} else {
    	method = DECODE_METHOD_IMAGE;
    	}
    }

	LOGII("Decode method", method);
	return method;
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
                    uint32 item = raster[h * *width + w];
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

            //return rotated;
        }

void NativeDecoder::fixOrientation(jint *pixels, uint32 pixelsBufferSize, int bitmapwidth, int bitmapheight)
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
    		for (int j = 0; j < bitmapheight; j++) {
    			uint32 crPix = raster[j * bitmapwidth + i];
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
    int dircount = 0;
    do {
        dircount++;
    } while (TIFFReadDirectory(image));
    return dircount;
}

void NativeDecoder::writeDataToOptions(int directoryNumber)
{
    TIFFSetDirectory(image, directoryNumber);
    jclass jOptionsClass = env->FindClass(
            "org/beyka/tiffbitmapfactory/TiffBitmapFactory$Options");

        jfieldID gOptions_outDirectoryCountFieldId = env->GetFieldID(jOptionsClass,
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
                "ORIENTATION_TOPLEFT",
                "Lorg/beyka/tiffbitmapfactory/Orientation;");
            break;
        case ORIENTATION_TOPRIGHT:
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                "ORIENTATION_TOPRIGHT",
                "Lorg/beyka/tiffbitmapfactory/Orientation;");
            break;
        case ORIENTATION_BOTRIGHT:
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                "ORIENTATION_BOTRIGHT",
                "Lorg/beyka/tiffbitmapfactory/Orientation;");
            break;
        case ORIENTATION_BOTLEFT:
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                "ORIENTATION_BOTLEFT",
                "Lorg/beyka/tiffbitmapfactory/Orientation;");
            break;
        case ORIENTATION_LEFTTOP:
            flipHW = true;
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                "ORIENTATION_LEFTTOP",
                "Lorg/beyka/tiffbitmapfactory/Orientation;");
            break;
        case ORIENTATION_RIGHTTOP:
            flipHW = true;
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                "ORIENTATION_RIGHTTOP",
                "Lorg/beyka/tiffbitmapfactory/Orientation;");
            break;
        case ORIENTATION_RIGHTBOT:
            flipHW = true;
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                "ORIENTATION_RIGHTBOT",
                "Lorg/beyka/tiffbitmapfactory/Orientation;");
            break;
        case ORIENTATION_LEFTBOT:
            flipHW = true;
            gOptions_ImageOrientationFieldId = env->GetStaticFieldID(gOptions_ImageOrientationClass,
                "ORIENTATION_LEFTBOT",
                "Lorg/beyka/tiffbitmapfactory/Orientation;");
            break;
        }
        if (gOptions_ImageOrientationFieldId != NULL) {
            jobject gOptions_ImageOrientationObj = env->GetStaticObjectField(
                gOptions_ImageOrientationClass,
                gOptions_ImageOrientationFieldId);

            //Set outImageOrientation field to options object
            jfieldID gOptions_outImageOrientationField = env->GetFieldID(jOptionsClass,
                "outImageOrientation",
                "Lorg/beyka/tiffbitmapfactory/Orientation;");
            env->SetObjectField(optionsObject, gOptions_outImageOrientationField,
                gOptions_ImageOrientationObj);
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
                "COMPRESSION_NONE",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_LZW:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "COMPRESSION_LZW",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_JPEG:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "COMPRESSION_JPEG",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_PACKBITS:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "COMPRESSION_PACKBITS",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_DEFLATE:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "COMPRESSION_DEFLATE",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            break;
        case COMPRESSION_ADOBE_DEFLATE:
            gOptions_ImageCompressionFieldId = env->GetStaticFieldID(gOptions_ImageCompressionClass,
                "COMPRESSION_ADOBE_DEFLATE",
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
            jfieldID gOptions_outCompressionSchemeField = env->GetFieldID(jOptionsClass,
                "outCompressionScheme",
                "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
            env->SetObjectField(optionsObject, gOptions_outCompressionSchemeField,
                gOptions_ImageCompressionObj);
        }

        jfieldID gOptions_OutCurDirNumberFieldID = env->GetFieldID(jOptionsClass,
            "outCurDirectoryNumber",
            "I");
        env->SetIntField(optionsObject, gOptions_OutCurDirNumberFieldID, directoryNumber);
        if (!flipHW) {
            jfieldID gOptions_outWidthFieldId = env->GetFieldID(jOptionsClass, "outWidth", "I");
            env->SetIntField(optionsObject, gOptions_outWidthFieldId, origwidth);

            jfieldID gOptions_outHeightFieldId = env->GetFieldID(jOptionsClass, "outHeight", "I");
            env->SetIntField(optionsObject, gOptions_outHeightFieldId, origheight);
        } else {
            jfieldID gOptions_outWidthFieldId = env->GetFieldID(jOptionsClass, "outWidth", "I");
            env->SetIntField(optionsObject, gOptions_outWidthFieldId, origheight);

            jfieldID gOptions_outHeightFieldId = env->GetFieldID(jOptionsClass, "outHeight", "I");
            env->SetIntField(optionsObject, gOptions_outHeightFieldId, origwidth);
        }

        int tagRead = 0;

        //Author
        char * artist;
        tagRead = TIFFGetField(image, TIFFTAG_ARTIST, & artist);
        if (tagRead == 1) {
            LOGI(artist);
            jstring jauthor = env->NewStringUTF(artist);
            jfieldID gOptions_outAuthorFieldId = env->GetFieldID(jOptionsClass, "outAuthor", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outAuthorFieldId, jauthor);
            env->DeleteLocalRef(jauthor);
            //free(artist);
        }

        //Copyright
        char * copyright;
        tagRead = TIFFGetField(image, TIFFTAG_COPYRIGHT, & copyright);
        if (tagRead == 1) {
            LOGI(copyright);
            jstring jcopyright = env->NewStringUTF(copyright);
            jfieldID gOptions_outCopyrightFieldId = env->GetFieldID(jOptionsClass, "outCopyright", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outCopyrightFieldId, jcopyright);
            env->DeleteLocalRef(jcopyright);
            //free(copyright);
        }

        //ImageDescription
        char * imgDescr;
        tagRead = TIFFGetField(image, TIFFTAG_IMAGEDESCRIPTION, & imgDescr);
        if (tagRead == 1) {
            LOGI(imgDescr);
            jstring jimgDescr = env->NewStringUTF(imgDescr);
            jfieldID gOptions_outimgDescrFieldId = env->GetFieldID(jOptionsClass, "outImageDescription", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outimgDescrFieldId, jimgDescr);
            env->DeleteLocalRef(jimgDescr);
            //free(imgDescr);
        }

        //Software
        char * software;
        tagRead = TIFFGetField(image, TIFFTAG_SOFTWARE, & software);
        if (tagRead == 1) {
            LOGI(software);
            jstring jsoftware = env->NewStringUTF(software);
            jfieldID gOptions_outsoftwareFieldId = env->GetFieldID(jOptionsClass, "outSoftware", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outsoftwareFieldId, jsoftware);
            env->DeleteLocalRef(jsoftware);
            //free(software);
        }

        //DateTime
        char * datetime;
        tagRead = TIFFGetField(image, TIFFTAG_DATETIME, & datetime);
        if (tagRead == 1) {
            LOGI(datetime);
            jstring jdatetime = env->NewStringUTF(datetime);
            jfieldID gOptions_outdatetimeFieldId = env->GetFieldID(jOptionsClass, "outDatetime", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outdatetimeFieldId, jdatetime);
            env->DeleteLocalRef(jdatetime);
            //free(datetime);
        }

        //Host Computer
        char * host;
        tagRead = TIFFGetField(image, TIFFTAG_HOSTCOMPUTER, & host);
        if (tagRead == 1) {
            LOGI(host);
            jstring jhost = env->NewStringUTF(host);
            jfieldID gOptions_outhostFieldId = env->GetFieldID(jOptionsClass, "outHostComputer", "Ljava/lang/String;");
            env->SetObjectField(optionsObject, gOptions_outhostFieldId, jhost);
            env->DeleteLocalRef(jhost);
            //free(host);
        }

        env->DeleteLocalRef(jOptionsClass);
}







