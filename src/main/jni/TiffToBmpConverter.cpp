//
// Created by beyka on 9/22/17.
//
#include "TiffToBmpConverter.h"
#include "BMP.h"

TiffToBmpConverter::TiffToBmpConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts, jobject listener)
    : BaseTiffConverter(e, clazz, in, out, opts, listener)
{
    bmpHeader = new BITMAPFILEHEADER;
    bmpInfo = new BITMAPINFOHEADER;
}

TiffToBmpConverter::~TiffToBmpConverter()
{
    if (bmpHeader)
        free(bmpHeader);
    if (bmpInfo)
        free(bmpInfo);
}

jboolean TiffToBmpConverter::convert()
{
    readOptions();

    //in c++ path
    const char *strTiffPath = NULL;
    strTiffPath = env->GetStringUTFChars(inPath, 0);
    LOGIS("IN path", strTiffPath);

    //open tiff image for reading
    tiffImage = TIFFOpen(strTiffPath, "r");
    if (tiffImage == NULL) {
        if (throwException) {
            throw_cant_open_file_exception(env, inPath);
        }
        LOGES("Can\'t open in file", strTiffPath);
        env->ReleaseStringUTFChars(inPath, strTiffPath);
        return JNI_FALSE;
    } else {
        env->ReleaseStringUTFChars(inPath, strTiffPath);
    }
    LOGI("Tiff file opened for reading");

    //open bmp file for writing
    const char *strPngPath = NULL;
    strPngPath = env->GetStringUTFChars(outPath, 0);
    LOGIS("OUT path", strPngPath);
    outFIle = fopen(strPngPath, "wb");
    if (!outFIle) {
        if (throwException) {
            throw_cant_open_file_exception(env, outPath);
        }
        LOGES("Can\'t open out file", strPngPath);
        env->ReleaseStringUTFChars(outPath, strPngPath);
        return JNI_FALSE;
    } else {
        env->ReleaseStringUTFChars(outPath, strPngPath);
    }

    //set tiff directory and read image dimensions
    TIFFSetDirectory(tiffImage, tiffDirectory);
    TIFFGetField(tiffImage, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiffImage, TIFFTAG_IMAGELENGTH, &height);
    //Getting image orientation and createing ImageOrientation enum
    TIFFGetField(tiffImage, TIFFTAG_ORIENTATION, &origorientation);
    //If orientation field is empty - use ORIENTATION_TOPLEFT
    if (origorientation == 0) {
        origorientation = ORIENTATION_TOPLEFT;
    }

    if (!normalizeDecodeArea()) {
        fclose(outFIle);
        return JNI_FALSE;
    }

    LOGII("image width", width);
    LOGII("image height", height);
    LOGII("image bound width", boundWidth);
    LOGII("image bound height", boundHeight);

    unsigned long long imageDataSize = (outWidth * 3 + outWidth % 4) * outHeight;

    LOGII("image out width", outWidth);
    LOGII("image out height", outHeight);


    LOGII("image data size", imageDataSize);

    LOGII("size", sizeof(BITMAPINFOHEADER));

    //char *bm = "BM";
     //memcpy(&bmpHeader->bfType, bm, sizeof(short));
    bmpHeader->bfType[0] = 0x42;
    bmpHeader->bfType[1] = 0x4d;
    bmpHeader->bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageDataSize;//sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 3 * width * height;
    bmpHeader->bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);//sizeof(BITMAPFILEHEADER) + 108;

    bmpInfo->biSize = 108;
    bmpInfo->biWidth = outWidth;
    bmpInfo->biHeight = outHeight;
    bmpInfo->biBitCount = 24;
    bmpInfo->biPlanes = 1;
    bmpInfo->biCompression = 0;

    bmpInfo->biSizeImage = 0;//imageDataSize;

    bmpInfo->biClrUsed = 0;
    bmpInfo->biClrImportant = 0;

    bmpInfo->biPalete[0] = 0;
    bmpInfo->biPalete[1] = 0;
    bmpInfo->biPalete[2] = 0;

    for (int i = 0; i < 55; i++) {
        bmpInfo->reserved[i] = 0;
    }

    fwrite(bmpHeader,sizeof(BITMAPFILEHEADER),1,outFIle);
    fseek(outFIle, sizeof(BITMAPFILEHEADER) , SEEK_SET);
    fwrite(bmpInfo,sizeof(BITMAPINFOHEADER),1,outFIle);
    fseek(outFIle, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) , SEEK_SET);

    jboolean result = JNI_FALSE;

    switch(getDecodeMethod()) {
        case DECODE_METHOD_IMAGE:
            result = convertFromImage();
            break;
        case DECODE_METHOD_TILE:
           result = convertFromTile();
           break;
       case DECODE_METHOD_STRIP:
           result = convertFromStrip();
           break;
    }

    fclose(outFIle);
    if (!result) {
        //maybe shold delete file?
    }

    LOGI("fille closed");
    conversion_result = result;
    return conversion_result;
    //return JNI_FALSE;
}

jboolean TiffToBmpConverter::convertFromImage() {
    int origBufferSize = width * height * sizeof(uint32);

    unsigned long estimateMem = origBufferSize;
    estimateMem += outWidth * 3 + outWidth % 4; //working buf to write to file
    LOGII("estimateMem", estimateMem);
    if (estimateMem > availableMemory && availableMemory != -1) {
        LOGEI("Not enough memory", availableMemory);
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return JNI_FALSE;
    }

    uint32 *origBuffer = NULL;
    origBuffer = (uint32 *) _TIFFmalloc(origBufferSize);
    if (origBuffer == NULL) {
        const char *message = "Can\'t allocate buffer";
        LOGE(*message);
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, origBufferSize);
        }
        return JNI_FALSE;
    }

    if (0 == TIFFReadRGBAImageOriented(tiffImage, width, height, origBuffer, ORIENTATION_TOPLEFT, 0)) {
        free(origBuffer);
        const char *message = "Can\'t read tiff";
        if (throwException) {
            jstring er = env->NewStringUTF(message);
            throw_decode_file_exception(env, outPath, er);
            env->DeleteLocalRef(er);
        }
        LOGE(message);
        return JNI_FALSE;
    }
    LOGI("tiff read");

    jlong total = outWidth * outHeight;
    int outY, outX;
    sendProgress(0, total);

    //24 bpp bmp should has with multiple 4
    int rowSize = outWidth * 3 + outWidth % 4;
    unsigned char *row = new unsigned char[rowSize];

    for (int y = 0; y < height ; y++) {
        if (y < outStartY || y >= outStartY + outHeight) continue;
        if (checkStop()) {
            free(origBuffer);
            return JNI_FALSE;
        }
        outY = y - outStartY;
        sendProgress(outY * outWidth, total);

        for (int x = 0; x < width * 3; x += 3) {
            if (x < outStartX * 3 || x >= (outStartX + outWidth) * 3) continue;
            outX = x - (outStartX*3);
            uint32 pix = origBuffer[y * width + x/3];
            unsigned char *vp = (unsigned char *)&pix;
            //in bmp colors stores as bgr
            row[outX] = vp[2]; //red
            row[outX+1] = vp[1]; //green
            row[outX+2] = vp[0];   //blue
        }

        //in bmp lines stored fliped verticaly. Write lines from bottom to top
        fseek(outFIle, 122 + (outHeight - outY - 1) * rowSize , SEEK_SET);
        fwrite(row,rowSize,1,outFIle);
    }
    free(row);
    LOGI("exit for");
    if (origBuffer) {
        _TIFFfree(origBuffer);
    }
    LOGI("free orig");
    sendProgress(total, total);
    LOGI("dend progress");
    return JNI_TRUE;
}

jboolean TiffToBmpConverter::convertFromTile() {
    uint32 tileWidth = 0, tileHeight = 0;
    TIFFGetField(tiffImage, TIFFTAG_TILEWIDTH, &tileWidth);
    TIFFGetField(tiffImage, TIFFTAG_TILEWIDTH, &tileHeight);
    LOGII("Tile width", tileWidth);
    LOGII("Tile height", tileHeight);

    uint32 workingWidth = (width/tileWidth + (width%tileWidth == 0 ? 0 : 1)) * tileWidth;
    LOGII("workingWidth ", workingWidth );
    uint32 rasterSize =  workingWidth  * tileHeight ;
    LOGII("rasterSize ", rasterSize );


    unsigned long estimateMem = rasterSize * sizeof(uint32); //raster
    estimateMem += tileWidth * tileHeight * sizeof(uint32); //tile raster
    estimateMem += tileWidth * sizeof (uint32); //working buf
    estimateMem += width * 3 + width % 4; //bufer for writing scanline to bmp
    LOGII("estimateMem", estimateMem);
    if (estimateMem > availableMemory && availableMemory != -1) {
        LOGEI("Not enought memory", availableMemory);
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return JNI_FALSE;
    }

    uint32 *rasterTile = (uint32 *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32));
    uint32 *work_line_buf = (uint32*)_TIFFmalloc(tileWidth * sizeof (uint32));

     jlong total = ((width/tileWidth + (width%tileWidth == 0 ? 0 : 1)) * tileWidth)
                * ((height/tileHeight + (height%tileHeight == 0 ? 0 : 1)) * tileHeight);
        sendProgress(0, total);

    uint32 row, column;

    int startx = -1, starty = -1, endx = -1, endy = -1;
    uint32 imageWritedLines = 0;

    //24 bpp bmp should has with multiple 4
    int scanlineSize = outWidth * 3 + outWidth % 4;
    unsigned char *scanline = new unsigned char[scanlineSize];

    for (row = 0; row < height; row += tileHeight) {
        sendProgress(row * width, total);
        endy = -1;
        starty = -1;
        uint32 *raster = (uint32 *)_TIFFmalloc(rasterSize * sizeof(uint32));

        for (column = 0; column < width; column += tileWidth) {
            if (checkStop()) {
                free(raster);
                raster = NULL;
                if (rasterTile) {
                    _TIFFfree(rasterTile);
                    rasterTile = NULL;
                }
                if (work_line_buf) {
                    _TIFFfree(rasterTile);
                    work_line_buf = NULL;
                }
                return JNI_FALSE;
            }
            endx = -1;
            startx = -1;
            TIFFReadRGBATile(tiffImage, column , row, rasterTile);
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

            normalizeTile(tileHeight, tileWidth, rasterTile);

            //find start and end position
            for (int ty = 0; ty < tileHeight; ty++) {

                for (int tx = 0; tx < tileWidth; tx++) {
                    if (rasterTile[ty * tileWidth + tx] != 0) {
                        if (startx == -1) {
                            startx = tx;
                        }
                        if (starty == -1) {
                            starty = ty;
                        }

                        if (tx > endx)
                            endx = tx;
                        if (ty > endy)
                            endy = ty;

                        uint32 rasterPos = (ty ) * workingWidth + (tx + column);
                        //LOGII("rp", rasterPos);
                        raster[rasterPos] = rasterTile[ty * tileWidth + tx];
                    }

                }
            }
        }

        int outY, outX;
        int tileShift = 0;
        for (int y = starty; y < tileHeight; y++) {
            if (imageWritedLines == height) break;
            if (y + row < outStartY || y + row >= outStartY + outHeight) continue;
            outY = y + row - outStartY;
            for (int x = 0; x < width * 3; x += 3) {
                if (x < outStartX * 3 || x >= (outStartX + outWidth) * 3) continue;
                outX = x - (outStartX*3);
                uint32 pix = raster[y * workingWidth + x/3];
                unsigned char *vp = (unsigned char *)&pix;
                scanline[outX] = vp[2];
                scanline[outX+1] = vp[1];
                scanline[outX+2] = vp[0];
            }

            //in bmp lines stored fliped verticaly. Write lines from bottom to top
            fseek(outFIle, 122 + (/*height - tileShift - row*/outHeight - outY - 1) * scanlineSize , SEEK_SET);
            fwrite(scanline,scanlineSize,1,outFIle);
            tileShift++;
            imageWritedLines++;
        }
        //LOGII("imageWritedLines", imageWritedLines);
        free(raster);
    }
    free(scanline);

    if (rasterTile) {
        _TIFFfree(rasterTile);
        rasterTile = NULL;
    }

    if (work_line_buf) {
        _TIFFfree(rasterTile);
        work_line_buf = NULL;
    }

    sendProgress(total, total);
    return JNI_TRUE;

}

jboolean TiffToBmpConverter::convertFromStrip() {
    uint32 stripSize = TIFFStripSize (tiffImage);
    uint32 stripMax = TIFFNumberOfStrips (tiffImage);
    int rowPerStrip = -1;
    TIFFGetField(tiffImage, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);

    unsigned long estimateMem = width * sizeof(uint32);//working buf
    estimateMem += width * rowPerStrip * sizeof (uint32);//raster
    estimateMem += outWidth * 3 + outWidth % 4;
    //estimateMem += 4 * width * sizeof(png_bytep); //buf for writing to png
    LOGII("estimateMem", estimateMem);
    if (estimateMem > availableMemory && availableMemory != -1) {
        LOGEI("Not enought memory", availableMemory);
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return JNI_FALSE;
    }

    LOGII("rps", rowPerStrip);
    LOGII("sm", stripMax);

    jlong total = stripMax * rowPerStrip * width;
    sendProgress(0, total);

    uint32* work_line_buf = (uint32 *)_TIFFmalloc(width * sizeof(uint32));
    uint32* raster = (uint32 *)_TIFFmalloc(width * rowPerStrip * sizeof (uint32));

    uint32 rows_to_write = 0;

    //24 bpp bmp should has with multiple 4
    int rowSize = outWidth * 3 + outWidth % 4;
    unsigned char *row = new unsigned char[rowSize];
    for (int i = 0; i < stripMax*rowPerStrip; i += rowPerStrip) {
        if (checkStop()) {
            if (raster) {
                _TIFFfree(raster);
                raster = NULL;
            }
            if (work_line_buf) {
                _TIFFfree(work_line_buf);
                work_line_buf = NULL;
            }
            return JNI_FALSE;
        }

        if ((i < outStartY && i + rowPerStrip < outStartY) || (i >= outStartY + outHeight && i + rowPerStrip >= outStartY + outHeight)) continue;
        LOGII("i", i);

        sendProgress(i * width, total);
        TIFFReadRGBAStrip(tiffImage, i, raster);

        rows_to_write = 0;
        if( i + rowPerStrip > height )
            rows_to_write = height - i;
        else
            rows_to_write = rowPerStrip;

        if (origorientation <= 4) {
            for (int line = 0; line < rows_to_write / 2; line++) {
                unsigned int  *top_line, *bottom_line;
                top_line = raster + width * line;
                bottom_line = raster + width * (rows_to_write - line - 1);

                _TIFFmemcpy(work_line_buf, top_line, sizeof(unsigned int) * width);
                _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * width);
                _TIFFmemcpy(bottom_line, work_line_buf, sizeof(unsigned int) * width);
            }
        }

        if (origorientation == ORIENTATION_TOPRIGHT || origorientation == ORIENTATION_BOTRIGHT
                || origorientation == ORIENTATION_RIGHTTOP || origorientation == ORIENTATION_RIGHTBOT) {

                for (int y = 0; y < rows_to_write; y++) {
                    for (int x = 0; x < width/2; x++) {
                        uint32 buf = raster[y * width + x];
                        raster[y * width + x] = raster[y * width + width - 1 - x];
                        raster[y * width + width - 1 - x] = buf;
                    }
                }
        }

        int outY, outX;
        for (int y = 0; y < rows_to_write; y++) {
            if (i + y < outStartY || i + y >= outStartY + outHeight) continue;
            outY = i + y - outStartY;
            for (int x = 0; x < width * 3; x += 3) {
                if (x < outStartX * 3 || x >= (outStartX + outWidth) * 3) continue;
                outX = x - (outStartX*3);
                uint32 pix = raster[y * width + x/3];
                unsigned char *vp = (unsigned char *)&pix;
                //in bmp colors stores as bgr
                row[outX] = vp[2]; //red
                row[outX+1] = vp[1]; //green
                row[outX+2] = vp[0];   //blue
            }

            //in bmp lines stored fliped verticaly. Write lines from bottom to top
            fseek(outFIle, 122 + (outHeight - outY - 1) * rowSize , SEEK_SET);
            fwrite(row,rowSize,1,outFIle);
        }
    }
    free(row);

    if (raster) {
        _TIFFfree(raster);
        raster = NULL;
    }

    if (work_line_buf) {
        _TIFFfree(work_line_buf);
        work_line_buf = NULL;
    }

    sendProgress(total, total);

    return JNI_TRUE;
}

int TiffToBmpConverter::getDecodeMethod() {
    int method = -1;
	uint32 tileWidth, tileHeight;
	int readTW = 0, readTH = 0;
    readTW = TIFFGetField(tiffImage, TIFFTAG_TILEWIDTH, &tileWidth);
    readTH = TIFFGetField(tiffImage, TIFFTAG_TILELENGTH, &tileHeight);
    if (tileWidth > 0 && tileHeight > 0 && readTH > 0 && readTW > 0) {
        method = DECODE_METHOD_TILE;
    } else {
        int rowPerStrip = -1;
    	TIFFGetField(tiffImage, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);
    	uint32 stripSize = TIFFStripSize (tiffImage);
    	uint32 stripMax = TIFFNumberOfStrips (tiffImage);
    	int estimate = width * 3;
    	LOGII("RPS", rowPerStrip);
    	LOGII("stripSize", stripSize);
    	LOGII("stripMax", stripMax);
    	if (rowPerStrip != -1 && stripSize > 0 && stripMax > 1 && rowPerStrip < height) {
    	    method = DECODE_METHOD_STRIP;
    	} else {
    	method = DECODE_METHOD_IMAGE;
    	}
    }

	LOGII("Decode method", method);
	return method;

}
