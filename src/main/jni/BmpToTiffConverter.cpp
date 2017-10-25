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
    LOGI("CONVERT");
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
    //Read header of bitmap file
    readHeaders();

    //Check is file is BMP image
    bool is_bmp = !strncmp( (const char*)&bmp->bfType, "BM", 2 );
    if (!is_bmp) {
        LOGE("Not bmp file");
        if (throwException) {
            throw_cant_open_file_exception(env, inPath);
        }
        return JNI_FALSE;
    }

    //check is bitmap has 24  bit per pixel. other format not supported
    if (inf->biBitCount != 16 && inf->biBitCount != 24 && inf->biBitCount != 32 && inf->biBitCount != 0) {
        LOGE("Support only 24bpp bitmaps");
        if (throwException) {
            throw_cant_open_file_exception(env, inPath);
        }
        return JNI_FALSE;
    }
    LOGII("Bits per pixel", inf->biBitCount);

         int compression = inf->biCompression;
                  LOGII("compression", inf->biCompression);
                  for (int i = 0; i < 3; i++) {
                      LOGII("mask", inf->biPalete[i]);
                  }

    //Component per pixel will be always 4. Alpha will be always 0xff
    int componentsPerPixel = 4;//inf->biBitCount / 8;

    int width = inf->biWidth;
    int height = inf->biHeight;
    LOGII("width", width);
    LOGII("height", height);

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

    //progress reporter
    jlong total = width * height;
    sendProgress(0, total);

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

    unsigned long estimateMem = rowPerStrip * width * 4 * 2;//need 2 buffers for read data from bitmap. Check getPixelsFromBmp method

    if (compressionInt == COMPRESSION_JPEG) {
        estimateMem += width * sizeof(uint32);//temp array for writing JPEG lines
        estimateMem += width * sizeof(uint32);//temp array for fliping bitmap data in getPixelsFromBmp method
    } else if (compressionInt == COMPRESSION_CCITTRLE || compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
        estimateMem += (width/8 + 0.5) * rowPerStrip; // bilevel array
    } else {
        estimateMem += 0;
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
        conversion_result = JNI_FALSE;
        return conversion_result;
    }

    int ret;

    if (compressionInt == COMPRESSION_CCITTRLE || compressionInt == COMPRESSION_CCITTFAX3 || compressionInt == COMPRESSION_CCITTFAX4) {
        TIFFSetField(tiffImage, TIFFTAG_ROWSPERSTRIP, rowPerStrip);
        int compressedWidth = (width/8 + 0.5);
        for (int y = 0; y < height; y+=rowPerStrip) {
            if (checkStop()) {
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
            uint32 *pixels = getPixelsFromBmp(y, rowToRead);
            unsigned char *bilevel = convertArgbToBilevel(pixels, width, rowToRead);
            free(pixels);
            ret = TIFFWriteEncodedStrip(tiffImage, y/rowToRead, bilevel, compressedWidth * sizeof(unsigned char) * rowToRead);
            free(bilevel);
        }
    } else if (compressionInt == COMPRESSION_JPEG) {
        for (int ys = 0; ys < height; ys+=rowPerStrip) {
        if (checkStop()) {
            if (fileDescriptor >= 0) {
                close(fileDescriptor);
            }
            return JNI_FALSE;
        }
        int rowToRead = rowPerStrip;
        if (rowToRead + ys >= height) {
            rowToRead = height - ys;
        }
        sendProgress(ys * width, total);
        uint32 *pixels = getPixelsFromBmp(ys, rowToRead);
        uint32 *pixelsline = new uint32[width];
        for (int k = 0; k < rowToRead; k++) {
            memcpy(pixelsline, &pixels [k * width], width * sizeof(uint32));
            ret = TIFFWriteScanline(tiffImage, pixelsline, ys + k, 0);
        }
        delete[] pixelsline;
        free(pixels);
        }
    } else {
        TIFFSetField(tiffImage, TIFFTAG_ROWSPERSTRIP, rowPerStrip);
        for (int y = 0; y < height; y+=rowPerStrip) {
            if (checkStop()) {
                if (fileDescriptor >= 0) {
                    close(fileDescriptor);
                }
                return JNI_FALSE;
            }
            int rowToRead = rowPerStrip;
            if (rowToRead + y >= height) {
                rowToRead = height - y;
            }
            //LOGII("rowToRead", rowToRead);
            sendProgress(y * width, total);
            uint32 *pixels = getPixelsFromBmp(y, rowToRead);
            TIFFWriteEncodedStrip(tiffImage, y/rowPerStrip, pixels, width * sizeof(uint32) * rowToRead);
            free(pixels);
        }
    }

    ret = TIFFWriteDirectory(tiffImage);
    LOGII("ret = ", ret);

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

uint32 *BmpToTiffConverter::getPixelsFromBmp(int offset, int limit)
{
    int componentPerPixel = inf->biBitCount/8;
    LOGII("componentPerPixel", inf->biBitCount);
    LOGII("componentPerPixel", componentPerPixel);

    if (componentPerPixel == 2) {
        return getPixelsFrom16Bmp(offset, limit);
    } else if (componentPerPixel == 3) {
        return getPixelsFrom24Bmp(offset, limit);
    } else if (componentPerPixel == 4) {
        return getPixelsFrom32Bmp(offset, limit);
    }

    return NULL;
}

uint32 *BmpToTiffConverter::getPixelsFrom16Bmp(int offset, int limit)
{
    unsigned char *buf;
    int size;
    int width = inf->biWidth;
    int height = inf->biHeight;

    LOGII("compr", inf->biCompression);

    if (offset + limit >= height) {
        limit = height - offset;
    }

    //width of bitmap should be multiple 4
    size = width * 2 + (width * 2) % 4;
    size = size * limit;

    buf = (unsigned char *)malloc(size);
    if (buf == NULL) {
        LOGE("Can\'t allocate buffer");
        return NULL;
    }

    //in bitmap picture stored fliped from up to down.
    //when need read 0 row - should read height-1 row
    //seek file to position from the end of file to read
    fseek(inFile, bmp->bfOffBits + (height - offset - limit) * (width * 2 + (width * 2) % 4) , SEEK_SET);

    int n = fread(buf, sizeof(unsigned char), size, inFile);
    LOGII("Read bytes", n);

    int temp, line, i, j, numImgBytes, ind = 0;
    uint32 *pixels;

    temp = width * 2;
    line = temp + (width * 2) % 4;//width % 4;
    numImgBytes = (4 * (width * limit));
    pixels = (uint32*)malloc(numImgBytes);

    int pixelss = 0;

    numImgBytes = line * limit;
    for (i = 0; i < numImgBytes; i+=2) {

        if (i > temp && (i % line) >= temp) continue;

        uint16_t r, g, b, a = 0b11111111;

        uint16_t* pix = (uint16_t*) (buf + i);

        r = inf->biPalete[0] & *pix;
        g = inf->biPalete[1] & *pix;
        b = inf->biPalete[2] & *pix;

        //Use one of palete mask to determine which type of 1 bpp used 555 or 565
        char greenShiftBits;
        g = g >> 5;
        if (inf->biPalete[1] == 0x03E0) {
            greenShiftBits = 3;
            r = r >> 10;
        } else {
            greenShiftBits = 2;
            r = r >> 11;
        }

        pixels[ind] = (r<<3) | (g << greenShiftBits << 8) | (b << 3 << 16)  | a << 24 ;//(r| g << 8 | b << 16 | a << 24) ;

        ind++;
    }

    uint32 *tmp = new uint32[width];
    for (i = 0; i < limit/2 ;i++) {
        memcpy(tmp, pixels + i * width, width * sizeof(uint32));
        memcpy(pixels + i * width, pixels + (limit - 1- i) * width , width * sizeof(uint32));
        memcpy(pixels + (limit - 1- i) * width , tmp, width * sizeof(uint32));
    }
    free (tmp);

    free(buf);

    return pixels;
}

uint32 *BmpToTiffConverter::getPixelsFrom24Bmp(int offset, int limit)
{
    unsigned char *buf;
    int size;
    int width = inf->biWidth;
    int height = inf->biHeight;

    if (offset + limit >= height) {
        limit = height - offset;
    }

    //width of bitmap should be multiple 4
    size = width * 3 + width % 4;
    size = size * limit;

    buf = (unsigned char *)malloc(size);
    if (buf == NULL) {
        LOGE("Can\'t allocate buffer");
        return NULL;
    }

    //in bitmap picture stored fliped from up to down.
    //when need read 0 row - should read height-1 row
    //seek file to position from the end of file to read
    fseek(inFile, bmp->bfOffBits + (height - offset - limit) * (width * 3 + width % 4) , SEEK_SET);

    int n = fread(buf, sizeof(unsigned char), size, inFile);
    LOGII("Read bytes", n);

    int temp, line, i, j, numImgBytes, ind = 0;
    uint32 *pixels;

    temp = width * 3;
    line = temp + width % 4;//width % 4;
    numImgBytes = (4 * (width * limit));
    pixels = (uint32*)malloc(numImgBytes);

    numImgBytes = line * limit;
    for (i = 0; i < numImgBytes; i++) {
        unsigned char r, g, b, a = 0b11111111;
        //width of bitmap should be multiple 4
        //here skip pixels that haven't data
        //but if bitmap is 32 bpp - it is laready multiple to 4
        if (i > temp && (i % line) >= temp) continue;

        //read colors. alpha always 255lf because 24bpp bitmap hasn't alpha chanel

        b = buf[i];
        i++;
        g = buf[i];
        i++;
        r = buf[i];


        pixels[ind] = (r| g << 8 | b << 16 | a << 24) ;
        ind++;
    }

    uint32 *tmp = new uint32[width];
    for (i = 0; i < limit/2 ;i++) {
        memcpy(tmp, pixels + i * width, width * sizeof(uint32));
        memcpy(pixels + i * width, pixels + (limit - 1- i) * width , width * sizeof(uint32));
        memcpy(pixels + (limit - 1- i) * width , tmp, width * sizeof(uint32));
    }
    free (tmp);

    free(buf);

    return pixels;
}

uint32 *BmpToTiffConverter::getPixelsFrom32Bmp(int offset, int limit)
{
    unsigned char *buf;
    int size;
    int width = inf->biWidth;
    int height = inf->biHeight;

    if (offset + limit >= height) {
        limit = height - offset;
    }

    //width of bitmap should be multiple 4
    size = width * 4;
    size = size * limit;

    buf = (unsigned char *)malloc(size);
    if (buf == NULL) {
        LOGE("Can\'t allocate buffer");
        return NULL;
    }


    //in bitmap picture stored fliped from up to down.
    //when need read 0 row - should read height-1 row
    //seek file to position from the end of file to read
    fseek(inFile, bmp->bfOffBits + (height - offset - limit) * width * 4, SEEK_SET);

    int n = fread(buf, sizeof(unsigned char), size, inFile);
    LOGII("Read bytes", n);

    int temp, line, i, j, numImgBytes, ind = 0;
    uint32 *pixels;

    line = width * 4;//width % 4;
    numImgBytes = (4 * (width * limit));
    pixels = (uint32*)malloc(numImgBytes);

    for (i = 0; i < numImgBytes; i++) {
        unsigned char r, g, b, a = 0b11111111;

        //read colors. alpha always 255lf because 24bpp bitmap hasn't alpha chanel
        //a = buf[i];
        i++;

        b = buf[i];
        i++;
        g = buf[i];
        i++;
        r = buf[i];

        pixels[ind] = (r| g << 8 | b << 16 | a << 24) ;
        ind++;
    }

    uint32 *tmp = new uint32[width];
    for (i = 0; i < limit/2 ;i++) {
        memcpy(tmp, pixels + i * width, width * sizeof(uint32));
        memcpy(pixels + i * width, pixels + (limit - 1- i) * width , width * sizeof(uint32));
        memcpy(pixels + (limit - 1- i) * width , tmp, width * sizeof(uint32));
    }
    free (tmp);

    free(buf);

    return pixels;
}

unsigned char * BmpToTiffConverter::convertArgbToBilevel(uint32 *data, uint32 width, uint32 height)
{
    unsigned char red;
    unsigned char green;
    unsigned char blue;

    uint32 crPix;
    uint32 grayPix;
    int bilevelWidth = (width / 8 + 0.5);

    unsigned char *dest = (unsigned char *) malloc(sizeof(unsigned char) * bilevelWidth * height);

    uint32 maxGrey = 0.2125 * 255 + 0.7154 * 255 + 0.0721 * 255;
    uint32 halfGrey = maxGrey/2;

    uint32 shift = 0;
    unsigned char charsum = 0;
    int k = 7;
    for (int y = 0; y < height; y++) {
        shift = 0;
        charsum = 0;
        k = 7;
        for (int i = 0; i < width; i++) {
            uint32 *px = &data[y * width + i];
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



