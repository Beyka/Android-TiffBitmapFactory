//
// Created by beyka on 9/20/17.
//
#include "BmpToTiffConverter.h"

BmpToTiffConverter::BmpToTiffConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts, jobject listener)
    : BaseTiffConverter(e, clazz, in, out, opts, listener)
{
}

BmpToTiffConverter::~BmpToTiffConverter()
{
    LOGI("Destructor");
    if (tiffImage) {
        TIFFClose(tiffImage);
        tiffImage = NULL;
    }
    LOGI("Tiff removed");

    if (inFile) {
        fclose(inFile);
    }
    LOGI("File closed");

    if (inf) {
        delete inf;
    }
    LOGI("Inf deleted");

    if (bmp) {
        delete bmp;
    }
    LOGI("header deleted");
}

jboolean BmpToTiffConverter::convert()
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

        //open bmp file fow reading
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

    readHeaders();

        //read file header
    //    size_t byte_count = 54;
    //    unsigned char *header = (unsigned char *)malloc(sizeof(unsigned char) * byte_count);
    //    fread(header, 1, byte_count, inFile);
    //    rewind(inFile);

        //Check is file is JPG image
        bool is_bmp = !strncmp( (const char*)&bmp->bfType, "BM", 2 );
        if (!is_bmp) {
            LOGE("Not bmp file");
            if (throwException) {
                throw_cant_open_file_exception(env, inPath);
            }
            return JNI_FALSE;
        } else {
            LOGI("IS BMP");
        }

        if (inf->biBitCount != 24 && inf->biBitCount != 0) {
            LOGE("Support only 24bpp bitmaps");
            if (throwException) {
                throw_cant_open_file_exception(env, inPath);
            }
            return JNI_FALSE;
        }
        LOGII("Bits per pixel", inf->biBitCount);



    int componentsPerPixel = 4;

    int width = inf->biWidth;
    int height = inf->biHeight;
    LOGII("width", width);
    LOGII("height", height);
    uint32 *pixels = getPixelsFromBmp();
    //uint32 *pixels = getPixelsFromBmp(&width, &height);

//TODO wrute tiff
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

    /*int biSizeImage = width * 3 + width % 4;
    LOGII("biSizeImage", biSizeImage);
    int rowSize = width * componentsPerPixel;
    LOGII("bmp samples", componentsPerPixel);*/

    int rowSize = width * componentsPerPixel;
    //Calculate row per strip
        //maximum size for strip should be less than 2Mb if memory available
        unsigned long MB2 = (availableMemory == -1 || availableMemory > 3 * 1024 * 1024) ? 2 * 1024 * 1024 : width * 4;
        int rowPerStrip = MB2/rowSize;
        if (rowPerStrip >= height) {
            rowPerStrip = height / 4;
        }
        if (rowPerStrip < 1) rowPerStrip = 1;
        LOGII("rowPerStrip", rowPerStrip);

        unsigned long estimateMem = rowPerStrip * width * 3;
        //estimateMem += sizeof(JSAMPLE) * rowSize;//jpg buffer
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

        //Buffer for read bmp image line by line
        //unsigned char *buffer = new unsigned char[biSizeImage];

        int ret;

        if (compressionInt == COMPRESSION_CCITTRLE || compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
            /*TIFFSetField(tiffImage, TIFFTAG_ROWSPERSTRIP, rowPerStrip);
            int compressedWidth = (width/8 + 0.5);
            for (int y = 0; y < height; y+=rowPerStrip) {
                        if (checkStop()) {
                            //for (int sy = 0; sy < rowPerStrip; sy++) {
                                free(pixels);
                            //}
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

                        fread(pixels, sizeof(unsigned char), rowPerStrip * rowSize, inFile);

                        unsigned char *bilevel = convertArgbToBilevel(pixels, componentsPerPixel, width, rowToRead);
                        ret = TIFFWriteEncodedStrip(tiffImage, y/rowPerStrip, bilevel, compressedWidth * sizeof(unsigned char) * rowToRead);
                        free(bilevel);
            }*/
        } else if (compressionInt == COMPRESSION_JPEG) {
            for (int ys = 0; ys < height; ys+=rowPerStrip) {
                        if (checkStop()) {
                            for (int sy = 0; sy < rowPerStrip; sy++) {
                                free(pixels);
                            }
                            if (fileDescriptor >= 0) {
                                close(fileDescriptor);
                            }

                            return JNI_FALSE;
                        }
                        int rowToRead = rowPerStrip;
                        if (rowToRead + ys >= height) {
                            rowToRead = height - ys;
                        }
                        LOGII("rowToRead", rowToRead);
                        sendProgress(ys * width, total);
                        //png_read_rows(png_ptr, &row_pointers[0], NULL, rowToRead);
                        uint32 *pixelsline = new uint32[width];
                        for (int k = 0; k < rowToRead; k++) {
                            memcpy(pixelsline, pixels, width * sizeof(uint32));
                            ret = TIFFWriteScanline(tiffImage, pixelsline, ys + k, 0);
                        }
                        //TIFFWriteEncodedStrip(tiffImage, y/rowPerStrip, pixels, width * sizeof(uint32) * rowToRead);
                        delete[] pixelsline;

                    }
                } else {

                    /*for (int y = 0; y < height; y++) {
                        uint32 *pixelsrow = new uint32[width * rowToRead];
                        memcpy(pixelsrow, pixels + y * width, rowToRead * width * sizeof(uint32));
                        ret = TIFFWriteScanline(tiffImage, pixelsline, ys + k, 0);
                    }*/

                      TIFFSetField(tiffImage, TIFFTAG_ROWSPERSTRIP, rowPerStrip);
                      for (int y = 0; y < height; y+=rowPerStrip) {
                          if (checkStop()) {
                              for (int sy = 0; sy < rowPerStrip; sy++) {
                                  free(pixels);
                              }
                              if (fileDescriptor >= 0) {
                                  close(fileDescriptor);
                              }
                              return JNI_FALSE;
                          }
                          int rowToRead = rowPerStrip;
                          if (rowToRead + y >= height) {
                              rowToRead = height - y;
                          }
                          LOGII("rowToRead", rowToRead);
                          sendProgress(y * width, total);
                          //png_read_rows(png_ptr, &row_pointers[0], NULL, rowToRead);
                          uint32 *pixelsrow = new uint32[width * rowToRead * sizeof(uint32)];
                          memcpy(pixelsrow, pixels + y * width, rowToRead * width * sizeof(uint32));
                          /*for (int k = 0; k < rowToRead; k++) {
                              memcpy(pixelsrow+k*width, pixels + (k + y) * width, width * sizeof(uint32));
                          }*/
                          TIFFWriteEncodedStrip(tiffImage, y/rowPerStrip, pixelsrow, width * sizeof(uint32) * rowToRead);
                          delete[] pixelsrow;
                      }
                  }






        ret = TIFFWriteDirectory(tiffImage);
        LOGII("ret = ", ret);

        free(pixels);

        sendProgress(total, total);
        conversion_result = JNI_TRUE;
        return conversion_result;
}

void BmpToTiffConverter::readHeaders()
{
    bmp = new BITMAPFILEHEADER;
    inf = new BITMAPINFOHEADER;
    fread(bmp, sizeof(BITMAPFILEHEADER), 1, inFile);
    LOGI("bmp read");
    fread(inf, sizeof(BITMAPINFOHEADER), 1, inFile);
    LOGI("inf read");
}

uint32 *BmpToTiffConverter::getPixelsFromBmp()
{

    unsigned char *buf;
    int size;
    int width = inf->biWidth;
    int height = inf->biHeight;
    LOGII("width = ", width);
    LOGII("height = ", height);
    if(inf->biSizeImage == 0)  {
        size = width * 3 + width % 4;
        size = size * height;
    } else {
        size = inf->biSizeImage;
    }

    buf = (unsigned char *)malloc(size);
    if (buf == NULL) {
        LOGE("Can\'t allocate buffer");
        return NULL;
    }

    fseek(inFile, bmp->bfOffBits, SEEK_SET);

    int n = fread(buf, sizeof(unsigned char), size, inFile);
    LOGII("Read bytes", n);

    int temp, line, i, j, numImgBytes, ind = 0;
    uint32 *pixels;

    temp = width * 3;
    line = temp + width % 4;
    numImgBytes = (4 * (width * height));
    pixels = (uint32*)malloc(numImgBytes);

    //memcpy(pixels, buf, numImgBytes);
    numImgBytes = line * height;
    for (i = 0; i < numImgBytes; i++) {
        unsigned char r, g, b, a = 0b11111111;
        if (i > temp && (i % line) >= temp) continue;

        b = buf[i];
        i++;
        g = buf[i];
        i++;
        r = buf[i];

        pixels[ind] = (r| g << 8 | b << 16 | a << 24) ;
        ind++;
    }

    uint32 *tmp = new uint32[width];
    for (i = 0; i < height/2 ;i++) {
        memcpy(tmp, pixels + i * width, width * sizeof(uint32));
        memcpy(pixels + i * width, pixels + (height - 1- i) * width , width * sizeof(uint32));
        memcpy(pixels + (height - 1- i) * width , tmp, width * sizeof(uint32));
    }
    free (tmp);

    free(buf);

    return pixels;
}

unsigned char * BmpToTiffConverter::convertArgbToBilevel(unsigned char *data, int components, uint32 width, uint32 height)
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



