//
// Created by beyka on 5/15/17.
//

#include "JpgToTiffConverter.h"

JpgToTiffConverter::JpgToTiffConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts, jobject listener)
    : BaseTiffConverter(e, clazz, in, out, opts, listener)
{
    jpeg_struct_init = 0;

    compressionInt = 5;
}

JpgToTiffConverter::~JpgToTiffConverter()
{
    LOGI("Destructor");
    if (tiffImage) {
        TIFFClose(tiffImage);
        tiffImage = NULL;
    }
    LOGI("Tiff removed");

    if (jpeg_struct_init) {
        jpeg_destroy_decompress(&cinfo);
    }
    LOGI("JPEG destroyed");

    if (inFile) {
        fclose(inFile);
    }
    LOGI("File closed");
}

jboolean JpgToTiffConverter::convert()
{
    readOptions();

    //open tiff file for writing or appending
    const char *outCPath = NULL;
    outCPath = env->GetStringUTFChars(outPath, 0);
    LOGIS("OUT path", outCPath);

    int fileDescriptor = -1;
    if (!appendTiff) {
        if((tiffImage = TIFFOpen(outCPath, "w")) == NULL) {
            LOGE("can not open file. Trying file descriptor");
            //if TIFFOpen returns null then try to open file from descriptor
            int mode = O_RDWR | O_CREAT | O_TRUNC | 0;
            fileDescriptor = open(outCPath, mode, 0666);
            if (fileDescriptor < 0) {
                LOGE("Unable to create tif file descriptor");
                if (throwException) {
                    throw_cant_open_file_exception(env, outPath);
                }
                env->ReleaseStringUTFChars(outPath, outCPath);
                return JNI_FALSE;
            } else {
                if ((tiffImage = TIFFFdOpen(fileDescriptor, outCPath, "w")) == NULL) {
                    close(fileDescriptor);
                    LOGE("Unable to write tif file");
                    if (throwException) {
                        throw_cant_open_file_exception(env, outPath);
                    }
                    env->ReleaseStringUTFChars(outPath, outCPath);
                    return JNI_FALSE;
                }
            }
        }
    } else {
        if((tiffImage = TIFFOpen(outCPath, "a")) == NULL){
            LOGE("can not open file. Trying file descriptor");
            //if TIFFOpen returns null then try to open file from descriptor
            int mode = O_RDWR|O_CREAT;
            fileDescriptor = open(outCPath, mode, 0666);
            if (fileDescriptor < 0) {
                LOGE("Unable to create tif file descriptor");
                if (throwException) {
                    throw_cant_open_file_exception(env, outPath);
                }
                env->ReleaseStringUTFChars(outPath, outCPath);
                return JNI_FALSE;
            } else {
                if ((tiffImage = TIFFFdOpen(fileDescriptor, outCPath, "a")) == NULL) {
                close(fileDescriptor);
                LOGE("Unable to write tif file");
                if (throwException) {
                    throw_cant_open_file_exception(env, outPath);
                }
                env->ReleaseStringUTFChars(outPath, outCPath);
                return JNI_FALSE;
                }
            }
        }
    }
    env->ReleaseStringUTFChars(outPath, outCPath);

    //open jpg file fow reading
    const char *inCPath = NULL;
    inCPath = env->GetStringUTFChars(inPath, 0);
    LOGIS("IN path", inCPath);
    inFile = fopen(inCPath, "rb");
    if (!inFile) {
        if (throwException) {
            throw_cant_open_file_exception(env, inPath);
        }
        LOGES("Can\'t open in file", inCPath);
        env->ReleaseStringUTFChars(inPath, inCPath);
        return JNI_FALSE;
    } else {
        env->ReleaseStringUTFChars(inPath, inCPath);
    }

    //read file header
    size_t byte_count = 8;
    unsigned char *header = (unsigned char *)malloc(sizeof(unsigned char) * byte_count);
    fread(header, 1, byte_count, inFile);

    //Check is file is JPG image
    bool is_jpg = !strncmp( (const char*)header, "\xFF\xD8\xFF", 3 );
    //seek file to begin
    rewind(inFile);
    if (!is_jpg) {
        LOGE("Not jpeg file");
        if (throwException) {
            throw_cant_open_file_exception(env, inPath);
        }
        return JNI_FALSE;
    } else {
        LOGI("IS JPEG");
    }

    /* We set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg_std_error(&jerr);
    //cinfo.err = jpeg_std_error(&jerr.pub);
    //jerr.pub.error_exit = my_error_exit;
    /* Establish the setjmp return context for my_error_exit to use. */
    /*if (setjmp(jerr.setjmp_buffer)) {
       char *message = "Error reading JPEG";
       LOGE(message);
       if (throwException) {
        jstring er = env->NewStringUTF(message);
        throw_decode_file_exception(env, inPath, er);
        env->DeleteLocalRef(er);
       }
       return JNI_FALSE;
    }*/

    jpeg_create_decompress(&cinfo);
    jpeg_struct_init = 1;
    LOGI("decompress created");

    jpeg_stdio_src(&cinfo, inFile);
    LOGI("set file");

    //read file parameters
    int readHeader = jpeg_read_header(&cinfo, TRUE);
    LOGII("read jpeg header", readHeader);

    //start decompress
    int startDecompress = jpeg_start_decompress(&cinfo);
    LOGII("start decompress", startDecompress);

    width = cinfo.image_width;
    height = cinfo.image_height;
    LOGII("width", width);
    LOGII("height", height);

    componentsPerPixel = cinfo.output_components;

    //Create tiff structure
    TIFFSetField(tiffImage, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tiffImage, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tiffImage, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiffImage, TIFFTAG_COMPRESSION, compressionInt);
    TIFFSetField(tiffImage, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tiffImage, TIFFTAG_XRESOLUTION, xRes);
    TIFFSetField(tiffImage, TIFFTAG_YRESOLUTION, yRes);
    TIFFSetField(tiffImage, TIFFTAG_RESOLUTIONUNIT, resUnit);

    if (compressionInt == COMPRESSION_CCITTRLE || compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
        TIFFSetField(tiffImage, TIFFTAG_BITSPERSAMPLE,	1);
        TIFFSetField(tiffImage, TIFFTAG_SAMPLESPERPIXEL,	1);
        TIFFSetField(tiffImage, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tiffImage, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    } else {
        TIFFSetField(tiffImage, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(tiffImage, TIFFTAG_SAMPLESPERPIXEL, componentsPerPixel);
        if (componentsPerPixel == 1) {
            TIFFSetField(tiffImage, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        } else {
            TIFFSetField(tiffImage, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        }
    }
    //creation date
    char *date = getCreationDate();
    TIFFSetField(tiffImage, TIFFTAG_DATETIME, date);
    free(date);
    //image description
    if (cdescription) {
        TIFFSetField(tiffImage, TIFFTAG_IMAGEDESCRIPTION, cdescription);
    }
    //software tag
    if (csoftware) {
        TIFFSetField(tiffImage, TIFFTAG_SOFTWARE, csoftware);
    }

    //progress reporter
    jlong total = width * height;
    sendProgress(0, total);

    int rowSize = width * componentsPerPixel;
    LOGII("jpg samples", componentsPerPixel);

    //Calculate row per strip
    //maximum size for strip should be less than 2Mb if memory available
    unsigned long MB2 = (availableMemory == -1 || availableMemory > 3 * 1024 * 1024) ? 2 * 1024 * 1024 : width * 4;
    int rowPerStrip = MB2/rowSize;
    //TODO This is workaround. Need to understand why FAX compression schemes have shift in strips.
    if (compressionInt == COMPRESSION_CCITTRLE || compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
        rowPerStrip = 1;
    }

    if (rowPerStrip >= height) {
        rowPerStrip = height / 4;
    }
    if (rowPerStrip < 1) rowPerStrip = 1;
    LOGII("rowPerStrip", rowPerStrip);

    unsigned long estimateMem = rowPerStrip * width * 4;
    estimateMem += sizeof(JSAMPLE) * rowSize;//jpg buffer
    if (compressionInt == COMPRESSION_JPEG) {
        estimateMem += 0;
    } else if (compressionInt == COMPRESSION_CCITTRLE || compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
        estimateMem += (width/8 + 0.5) * rowPerStrip;
    } else {
        estimateMem += rowPerStrip * rowSize;
    }
    LOGII("estimateMem", estimateMem);
    if (estimateMem > availableMemory && availableMemory != -1) {
        LOGEI("Not enough memory", availableMemory);
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return JNI_FALSE;
    }

    if (checkStop()) {
        //jpeg_finish_decompress(&cinfo);
        conversion_result = JNI_FALSE;
        return conversion_result;
    }

    //Buffer for read jpeg image line by line
    int buffer_height = 1;
    JSAMPARRAY buffer = (JSAMPARRAY)malloc(sizeof(JSAMPROW) * buffer_height);
    buffer[0] = (JSAMPROW)malloc(sizeof(JSAMPLE) * rowSize);

    //buffer for format strips for tiff


    int ret;
    if (compressionInt == COMPRESSION_JPEG) {
        int line = 0;
        while (cinfo.output_scanline < height) {
            if (checkStop()) {
                free(buffer[0]);
                free(buffer);
                conversion_result = JNI_FALSE;
                return conversion_result;
            }
            if (line % rowPerStrip == 0) {
                sendProgress(line * width, total);
            }
            jpeg_read_scanlines(&cinfo, buffer, 1);
            ret = TIFFWriteScanline(tiffImage, buffer[0], line, 0);
            line++;
        }
    } else {
        TIFFSetField(tiffImage, TIFFTAG_ROWSPERSTRIP, rowPerStrip);
        unsigned char *data = new unsigned char[rowSize * rowPerStrip];
        int totalRowCounter = 0;
        int rowCounter = 0;
        bool shouldWrite = false;
        while (cinfo.output_scanline < height) {
            shouldWrite = true;
            jpeg_read_scanlines(&cinfo, buffer, 1);
            memcpy(data + rowCounter * rowSize, buffer[0], rowSize);
            rowCounter++;
            totalRowCounter++;
            if (rowCounter == rowPerStrip) {
                if (checkStop()) {
                    //jpeg_finish_decompress(&cinfo);
                    delete[] data;
                    free(buffer[0]);
                    free(buffer);
                    conversion_result = JNI_FALSE;
                    return conversion_result;
                }
                if (compressionInt == COMPRESSION_CCITTRLE || compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
                    int compressedWidth = (width/8 + 0.5);
                    unsigned char *bilevel = convertArgbToBilevel(data, componentsPerPixel, width, rowPerStrip);
                    ret = TIFFWriteEncodedStrip(tiffImage, totalRowCounter/rowPerStrip - 1, bilevel, compressedWidth * sizeof(unsigned char) * rowPerStrip);
                    free(bilevel);
                } else {
                    ret = TIFFWriteEncodedStrip(tiffImage, totalRowCounter/rowPerStrip - 1, data, rowPerStrip * rowSize);
                }
                rowCounter = 0;
                shouldWrite = false;
                sendProgress(totalRowCounter * width, total);
            }
        }
        if (shouldWrite) {  LOGI("last stage");
            if (checkStop()) {
                //jpeg_finish_decompress(&cinfo);
                delete[] data;
                free(buffer[0]);
                free(buffer);
                conversion_result = JNI_FALSE;
                return conversion_result;
            }
            if (compressionInt == COMPRESSION_CCITTRLE || compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
                int compressedWidth = (width/8 + 0.5);
                unsigned char *bilevel = convertArgbToBilevel(data, componentsPerPixel, width, rowPerStrip);
                ret = TIFFWriteEncodedStrip(tiffImage, totalRowCounter/rowPerStrip, bilevel, compressedWidth * sizeof(unsigned char) * rowPerStrip);
                free(bilevel);
            } else {
                ret = TIFFWriteEncodedStrip(tiffImage, totalRowCounter/rowPerStrip, data, rowPerStrip * rowSize);
            }
        }
        delete[] data;
    }

    ret = TIFFWriteDirectory(tiffImage);
    LOGII("ret = ", ret);

    jpeg_finish_decompress(&cinfo);


    free(buffer[0]);
    free(buffer);

    sendProgress(total, total);
    conversion_result = JNI_TRUE;
    return conversion_result;
}

unsigned char * JpgToTiffConverter::convertArgbToBilevel(unsigned char *data, int components, uint32 width, uint32 height)
{
    unsigned char red;
    unsigned char green;
    unsigned char blue;

    uint32 crPix;
    uint32 grayPix;
    int bilevelWidth = (width / 8 + 0.5);

    unsigned char *dest = (unsigned char *) malloc(sizeof(unsigned char) * bilevelWidth * height);

    uint32 maxGrey = (components > 1) ? (0.2125 * 255 + 0.7154 * 255 + 0.0721 * 255) : 255;
    uint32 halfGrey = maxGrey/2;

    uint32 shift = 0;
    unsigned char charsum = 0;
    int k = 7;
    for (int y = 0; y < height; y++) {
        shift = 0;
        charsum = 0;
        k = 7;
        for (int i = 0; i < width * components; i+=components) {
            unsigned char *px = &data[y * width * components + i];
            if (components == 1) {
                grayPix = (*px);
            } else {
                red = px[0];
                green = px[1];
                blue = px[2];
                grayPix = (0.2125 * red + 0.7154 * green + 0.0721 * blue);
            }

            if (grayPix < halfGrey) charsum &= ~(1 << k);
            else charsum |= 1 << k;

            if (k == 0) {
                dest[y * bilevelWidth + shift] = charsum;
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

