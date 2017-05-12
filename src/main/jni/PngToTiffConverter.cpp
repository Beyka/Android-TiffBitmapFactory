//
// Created by beyka on 5/12/17.
//

#include "PngToTiffConverter.h"

PngToTiffConverter::PngToTiffConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts)
    : BaseTiffConverter(e, clazz, in, out, opts)
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

    if (pngFile) {
        fclose(pngFile);
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
    pngFile = fopen(inCPath, "rb");
    if (!pngFile) {
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
    fread(header, 1, byte_count, pngFile);

    //check is file is PNG or not
    bool is_png = !png_sig_cmp(header, 0, byte_count);
    if (!is_png) {
        LOGE("Not png");
        return JNI_FALSE;
    } else {
        LOGI("Is png");
    }

    //init png struct
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        char *message = "Can\'t create PNG structure";
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
        char *message = "Can\'t create PNG info structure";
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
        char *message = "Error reading PNG";
        LOGE(message);
        if (throwException) {
            jstring er = env->NewStringUTF(message);
            throw_decode_file_exception(env, inPath, er);
            env->DeleteLocalRef(er);
        }
        return JNI_FALSE;
    }

    //Init PNG IO
    png_init_io(png_ptr, pngFile);
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

    png_read_update_info(png_ptr, info_ptr);

    //allocate memory for png image
    png_bytep *row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for(int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png_ptr,info_ptr));
    }
    //read image
    png_read_image(png_ptr, row_pointers);


    LOGII("compression", compressionInt);
    //Set tiff parameters
    TIFFSetField(tiffImage, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tiffImage, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tiffImage, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiffImage, TIFFTAG_COMPRESSION, compressionInt);
    TIFFSetField(tiffImage, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
/*    TIFFSetField(tiffImage, TIFFTAG_XRESOLUTION, xRes);
    TIFFSetField(tiffImage, TIFFTAG_YRESOLUTION, yRes);
    TIFFSetField(tiffImage, TIFFTAG_RESOLUTIONUNIT, resUnit); */

    if (compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
        TIFFSetField(tiffImage, TIFFTAG_BITSPERSAMPLE,	1);
        TIFFSetField(tiffImage, TIFFTAG_SAMPLESPERPIXEL,	1);
        TIFFSetField(tiffImage, TIFFTAG_ROWSPERSTRIP, 1);
        TIFFSetField(tiffImage, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        TIFFSetField(tiffImage, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    } else {
        TIFFSetField(tiffImage, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(tiffImage, TIFFTAG_SAMPLESPERPIXEL, 4);
        TIFFSetField(tiffImage, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    }


    //

    // Write the information to the file
    if (compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
        unsigned char *bilevel = convertArgbToBilevel(row_pointers, 4, width, height);
        int compressedWidth = (width/8 + 0.5);
        for (int i = 0; i < height; i++) {
            //unsigned char *buf = (unsigned char *)malloc(sizeof(unsigned char) * compressedWidth);
            //memcpy(buf, bilevel + (i * compressedWidth), sizeof(unsigned char) * compressedWidth);
            TIFFWriteEncodedStrip(tiffImage, i, &bilevel[i * compressedWidth], (compressedWidth));
            //delete buf;

            //TIFFWriteEncodedStrip(tiffImage, i, &bilevel[i * compressedWidth], (compressedWidth));
        }
        free(bilevel);
    } else {
        LOGI("compression LZW");

        for (int y = 0; y < height; y++) {
            TIFFWriteScanline(tiffImage, row_pointers[y], y, 0);
        }
    }
    int ret = TIFFWriteDirectory(tiffImage);
    LOGII("ret = ", ret);


    //if file descriptor was openned then close it
    if (fileDescriptor >= 0) {
        close(fileDescriptor);
    }




    for(int y = 0; y < height; y++) {
        delete(row_pointers[y]);
    }
    delete(row_pointers);

    return JNI_TRUE;
}

unsigned char * PngToTiffConverter::convertArgbToBilevel(png_bytep *data, int samplePerPixel, uint32 width, uint32 height) {
        long long threshold = 0;

        unsigned char red;
        unsigned char green;
        unsigned char blue;
        unsigned char alpha;


        uint32 crPix;
        uint32 grayPix;
        int bilevelWidth = (width / 8 + 0.5);

        unsigned char *dest = (unsigned char *) malloc(sizeof(unsigned char) * bilevelWidth * height);

        for (int y = 0; y < height; y++) {
            png_bytep row = data[y];
            for (int x = 0; x < width; x++) {
                            png_bytep px = &(row[x * 4]);
                            red = px[0];
                            green = px[1];
                            blue = px[2];
                            //crPix = &(row[x * 4]);
                            grayPix = (0.2125 * red + 0.7154 * green + 0.0721 * blue);
                            //LOGII("grayPix", grayPix);
                            threshold += grayPix;
                        }
            /*unsigned char *ar = &data[y];
            for (int x = 0; x < width * samplePerPixel; x+= samplePerPixel) {
                red = ar[width * samplePerPixel + x];
                green = ar[width * samplePerPixel + x + 1];
                blue = ar[width * samplePerPixel + x + 2];

                grayPix = (0.2125 * (red) + 0.7154 * (green) + 0.0721 * (blue));
LOGII("grayPix", grayPix);
                threshold += grayPix;
            }*/
        }
        LOGII("threshold", threshold);
/*
        for (int i = 0; i < width; i++) {
            for (int j = 0; j < height; j++) {
                crPix = source[j * width + i];
                grayPix = (0.2125 * (colorMask & crPix >> 16) + 0.7154 * (colorMask & crPix >> 8) + 0.0721 * (colorMask & crPix));
                threshold += grayPix;
            }
        }
*/
        uint32 shift = 0;
        unsigned char charsum = 0;
        int k = 7;
        long long barier = threshold / (width * height);
        for (int y = 0; y < height; y++) {
            shift = 0;
            charsum = 0;
            k = 7;
            png_bytep row = data[y];
            for (int i = 0; i < width; i++) {
                            png_bytep px = &(row[i * 4]);
                                                        red = px[0];
                                                        green = px[1];
                                                        blue = px[2];
                                                        //crPix = &(row[x * 4]);
                                                        grayPix = (0.2125 * red + 0.7154 * green + 0.0721 * blue);

                            if (grayPix < barier) charsum &= ~(1 << k);
                            else charsum |= 1 << k;

                            if (k == 0) {
                                dest[y * bilevelWidth + shift] = charsum;
                                shift++;
                                k = 7;
                                //LOGII("charsum", charsum);
                                charsum = 0;
                            } else {
                                k--;
                            }
                        }

            /*for (int x = 0; x < width * samplePerPixel; x+=samplePerPixel) {

                red = ar[width * samplePerPixel + x];
                                green = ar[width * samplePerPixel + x + 1];
                                blue = ar[width * samplePerPixel + x + 2];

                grayPix = (0.2125 * (red) + 0.7154 * (green) + 0.0721 * (blue));

                if (grayPix < barier) charsum &= ~(1 << k);
                else charsum |= 1 << k;

                if (k == 0) {
                    dest[y * bilevelWidth + shift] = charsum;
                    shift++;
                    k = 7;
                    //LOGII("charsum", charsum);
                    charsum = 0;
                } else {
                    k--;
                }
            }*/
        }
        return dest;
}