//
// Created by beyka on 5/12/17.
//

#include "PngToTiffConverter.h"

PngToTiffConverter::PngToTiffConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts, jobject listener)
    : BaseTiffConverter(e, clazz, in, out, opts, listener)
{
    png_ptr_init = 0;
    png_info_init = 0;

    compressionInt = 5;
}

PngToTiffConverter::~PngToTiffConverter() {
    if (tiffImage) {
        TIFFClose(tiffImage);
        tiffImage = NULL;
    }

    if (png_info_init) {
        png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    }

    if (png_ptr_init) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
    }

    if (inFile) {
        fclose(inFile);
    }
}

jboolean PngToTiffConverter::convert()
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

    //open png file fow reading
    const char *inCPath = NULL;
    inCPath = env->GetStringUTFChars(inPath, 0);
    LOGIS("IN path", inCPath);
    inFile = fopen(inCPath, "rb");
    if (!inFile) {
        if (throwException) {
            throw_cant_open_file_exception(env, inPath);
        }
        LOGES("Can\'t open out file", inCPath);
        env->ReleaseStringUTFChars(inPath, inCPath);
        return JNI_FALSE;
    } else {
        env->ReleaseStringUTFChars(inPath, inCPath);
    }

    //read file header
    size_t byte_count = 8;
    unsigned char *header = (unsigned char *)malloc(sizeof(unsigned char) * byte_count);
    fread(header, 1, byte_count, inFile);

    //check is file is PNG or not
    bool is_png = !png_sig_cmp(header, 0, byte_count);
    if (!is_png) {
        LOGE("Not png file");
        if (throwException) {
            throw_cant_open_file_exception(env, inPath);
        }
        return JNI_FALSE;
    } else {
        LOGI("Is png");
    }

    //init png struct
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        const char *message = "Can\'t create PNG structure";
        LOGE(*message);
        if (throwException) {
            jstring er = env->NewStringUTF(message);
            throw_decode_file_exception(env, inPath, er);
            env->DeleteLocalRef(er);
        }
        return JNI_FALSE;
    }
    png_ptr_init = 1;

    png_set_sig_bytes(png_ptr, byte_count);

    //create png info pointer
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        const char *message = "Can\'t create PNG info structure";
        LOGE(*message);
        if (throwException) {
            jstring er = env->NewStringUTF(message);
            throw_decode_file_exception(env, inPath, er);
            env->DeleteLocalRef(er);
        }
        return JNI_FALSE;
    }
    png_info_init = 1;

    //png error handler
    if (setjmp(png_jmpbuf(png_ptr))) {
        const char *message = "Error reading PNG";
        LOGE(message);
        if (throwException) {
            jstring er = env->NewStringUTF(message);
            throw_decode_file_exception(env, inPath, er);
            env->DeleteLocalRef(er);
        }
        return JNI_FALSE;
    }

    //Init PNG IO
    png_init_io(png_ptr, inFile);
    //seek file header
    png_set_sig_bytes(png_ptr, byte_count);

    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    LOGII("width", width);
    LOGII("height", height);
    LOGII("bit_depth", bit_depth);
    LOGII("color_type", color_type);



    // cast any pixel data to RGBA data for simplest reading
    if(bit_depth == 16)
        png_set_strip_16(png_ptr);

    if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

      // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

      // These color_type don't have an alpha channel then fill it with 0xff.
    if(color_type == PNG_COLOR_TYPE_RGB ||
         color_type == PNG_COLOR_TYPE_GRAY ||
         color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

    if(color_type == PNG_COLOR_TYPE_GRAY ||
         color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    int number_passes = png_set_interlace_handling(png_ptr);
    LOGII("number_passes", number_passes);

    png_read_update_info(png_ptr, info_ptr);


    LOGII("compression", compressionInt);
    //Set tiff parameters
    //Set various text parameters
    //Set image parameters
    TIFFSetField(tiffImage, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tiffImage, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tiffImage, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiffImage, TIFFTAG_COMPRESSION, compressionInt);
    TIFFSetField(tiffImage, TIFFTAG_ORIENTATION, orientationInt);
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
        TIFFSetField(tiffImage, TIFFTAG_SAMPLESPERPIXEL, 4);
        TIFFSetField(tiffImage, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
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

    //Calculate row per strip
    //maximum size for strip should be less than 2Mb if memory available
    unsigned long MB2 = (availableMemory == -1 || availableMemory > 3 * 1024 * 1024) ? 2 * 1024 * 1024 : width * 4;
    unsigned long rowSizeBytes = width * 4;
    int rowPerStrip = MB2/rowSizeBytes;
    if (rowPerStrip >= height) {
        rowPerStrip = height / 4;
    }
    if (rowPerStrip < 1) rowPerStrip = 1;
    LOGII("rowPerStrip", rowPerStrip);

    //check available memory and estimate memory
    unsigned long estimateMem = rowPerStrip * width * 4;
    estimateMem += (compressionInt == COMPRESSION_CCITTRLE || compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) ? (width/8 + 0.5) * rowPerStrip : 0;
    LOGII("estimateMem", estimateMem);
    if (estimateMem > availableMemory && availableMemory != -1) {
        LOGEI("Not enough memory", availableMemory);
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return JNI_FALSE;
    }

    if (checkStop()) {
        conversion_result = JNI_FALSE;
        return conversion_result;
    }

    //progress reporter
    jlong total = width * height;
    sendProgress(0, total);


    int rowbytes = png_get_rowbytes(png_ptr,info_ptr);
    png_bytep row_pointers[rowPerStrip];
    for (int sy = 0; sy < rowPerStrip; sy++) {
        row_pointers[sy] = (png_byte*)malloc(rowbytes);
    }

    int ret;

    // Write the information to the file
    if (compressionInt == COMPRESSION_CCITTRLE || compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
        TIFFSetField(tiffImage, TIFFTAG_ROWSPERSTRIP, rowPerStrip);
        int compressedWidth = (width/8 + 0.5);
        for (int y = 0; y < height; y+=rowPerStrip) {
            if (checkStop()) {
                for (int sy = 0; sy < rowPerStrip; sy++) {
                    free(row_pointers[sy]);
                }
                if (fileDescriptor >= 0) {
                    close(fileDescriptor);
                }
                conversion_result = JNI_FALSE;
                return conversion_result;
            }
            int rowToRead = rowPerStrip;
            if (rowToRead + y >= height) {
                rowToRead = height - y;
            }
            sendProgress(y * width, total);
            png_read_rows(png_ptr, &row_pointers[0], NULL, rowToRead);
            unsigned char *bilevel = convertArgbToBilevel(&row_pointers[0], 4, width, rowToRead);
            TIFFWriteEncodedStrip(tiffImage, y/rowPerStrip, bilevel, compressedWidth * sizeof(unsigned char) * rowToRead);
            free(bilevel);
        }
    } else if (compressionInt == COMPRESSION_JPEG) {
        for (int ys = 0; ys < height; ys+=rowPerStrip) {
            if (checkStop()) {
                for (int sy = 0; sy < rowPerStrip; sy++) {
                    free(row_pointers[sy]);
                }
                if (fileDescriptor >= 0) {
                    close(fileDescriptor);
                }
                conversion_result = JNI_FALSE;
                return conversion_result;
            }
            int rowToRead = rowPerStrip;
            if (rowToRead + ys >= height) {
                rowToRead = height - ys;
            }
            LOGII("rowToRead", rowToRead);
            sendProgress(ys * width, total);
            png_read_rows(png_ptr, &row_pointers[0], NULL, rowToRead);
            uint32 *pixels = new uint32[width];
            for (int k = 0; k < rowToRead; k++) {
                memcpy(pixels, row_pointers[k], width * sizeof(uint32));
                ret = TIFFWriteScanline(tiffImage, pixels, ys + k, 0);
            }
            //TIFFWriteEncodedStrip(tiffImage, y/rowPerStrip, pixels, width * sizeof(uint32) * rowToRead);
            delete[] pixels;

        }
    } else {
        TIFFSetField(tiffImage, TIFFTAG_ROWSPERSTRIP, rowPerStrip);
        for (int y = 0; y < height; y+=rowPerStrip) {
            if (checkStop()) {
                for (int sy = 0; sy < rowPerStrip; sy++) {
                    free(row_pointers[sy]);
                }
                if (fileDescriptor >= 0) {
                    close(fileDescriptor);
                }
                conversion_result = JNI_FALSE;
                return conversion_result;
            }
            int rowToRead = rowPerStrip;
            if (rowToRead + y >= height) {
                rowToRead = height - y;
            }
            LOGII("rowToRead", rowToRead);
            sendProgress(y * width, total);
            png_read_rows(png_ptr, &row_pointers[0], NULL, rowToRead);
            uint32 *pixels = new uint32[width * rowPerStrip];
            for (int k = 0; k < rowToRead; k++) {
                memcpy(pixels+k*width, row_pointers[k], width * sizeof(uint32));
            }
            TIFFWriteEncodedStrip(tiffImage, y/rowPerStrip, pixels, width * sizeof(uint32) * rowToRead);
            delete[] pixels;
        }
    }
    //free memory allocated for png rows
    for (int sy = 0; sy < rowPerStrip; sy++) {
        free(row_pointers[sy]);
    }
    ret = TIFFWriteDirectory(tiffImage);
    LOGII("ret = ", ret);


    //if file descriptor was openned then close it
    if (fileDescriptor >= 0) {
        close(fileDescriptor);
    }

    sendProgress(total, total);
    conversion_result = JNI_TRUE;
    return conversion_result;
}

/*unsigned char * PngToTiffConverter::convertArgbToBilevel(png_byte *row, int samplePerPixel, uint32 width, uint32 height) {

        unsigned char red;
        unsigned char green;
        unsigned char blue;
        unsigned char alpha;


        uint32 crPix;
        uint32 grayPix;
        int bilevelWidth = (width / 8 + 0.5);

        unsigned char *dest = (unsigned char *) malloc(sizeof(unsigned char) * bilevelWidth);

        uint32 maxGrey = (0.2125 * 255 + 0.7154 * 255 + 0.0721 * 255);
        uint32 halfGrey = maxGrey/2;

        uint32 shift = 0;
        unsigned char charsum = 0;
        int k = 7;

        //for (int y = 0; y < height; y++) {
            shift = 0;
            charsum = 0;
            k = 7;
            //png_bytep row = data[y];
            for (int i = 0; i < width; i++) {
                            png_byte *px = &(row[i * 4]);
                            red = px[0];
                            green = px[1];
                            blue = px[2];
                                                        //crPix = &(row[x * 4]);
                            grayPix = (0.2125 * red + 0.7154 * green + 0.0721 * blue);

                            if (grayPix < halfGrey) charsum &= ~(1 << k);
                            else charsum |= 1 << k;

                            if (k == 0) {
                                dest[shift] = charsum;
                                shift++;
                                k = 7;
                                charsum = 0;
                            } else {
                                k--;
                            }
                        }
        //}
        return dest;
}*/

unsigned char * PngToTiffConverter::convertArgbToBilevel(png_bytep *data, int samplePerPixel, uint32 width, uint32 height) {
        unsigned char red;
        unsigned char green;
        unsigned char blue;
        unsigned char alpha;


        uint32 crPix;
        uint32 grayPix;
        int bilevelWidth = (width / 8 + 0.5);

        unsigned char *dest = (unsigned char *) malloc(sizeof(unsigned char) * bilevelWidth * height);

        uint32 maxGrey = (0.2125 * 255 + 0.7154 * 255 + 0.0721 * 255);
        uint32 halfGrey = maxGrey/2;

        uint32 shift = 0;
        unsigned char charsum = 0;
        int k = 7;
        for (int y = 0; y < height; y++) {
            shift = 0;
            charsum = 0;
            k = 7;
            png_bytep row = data[y];
            for (int i = 0; i < width; i++) {
                png_bytep px = &(row[i * samplePerPixel]);
                red = px[0];
                green = px[1];
                blue = px[2];
                grayPix = (0.2125 * red + 0.7154 * green + 0.0721 * blue);

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