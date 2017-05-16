//
// Created by beyka on 5/10/17.
//

#include "TiffToPngConverter.h"

TiffToPngConverter::TiffToPngConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts, jobject listener)
    : BaseTiffConverter(e, clazz, in, out, opts, listener)
{
    png_ptr_init = 0;
    png_info_init = 0;
}

TiffToPngConverter::~TiffToPngConverter()
{
    LOGI("destructor start");
    if (tiffImage) {
        TIFFClose(tiffImage);
        tiffImage = NULL;
    }
    LOGI("tiff free");

    if (png_info_init) {
        png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    }
    LOGI("info_ptr free");

    if (png_ptr_init) {
        png_destroy_write_struct(&png_ptr, NULL);
    }
    LOGI("png_ptr free");

    if (pngFile) {
        LOGI("pngFile != NULL");
        /*if (!conversion_result) {
            const char *strPngPath = NULL;
            strPngPath = env->GetStringUTFChars(outPath, 0);

            remove(strPngPath);

            env->ReleaseStringUTFChars(outPath, strPngPath);
        }*/
        fclose(pngFile);
    }
    LOGI("png file free");
}

jboolean TiffToPngConverter::convert()
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

    //open png file for writing
    const char *strPngPath = NULL;
    strPngPath = env->GetStringUTFChars(outPath, 0);
    LOGIS("OUT path", strPngPath);
    pngFile = fopen(strPngPath, "wb");
    if (!pngFile) {
        if (throwException) {
            throw_cant_open_file_exception(env, outPath);
        }
        LOGES("Can\'t open out file", strPngPath);
        env->ReleaseStringUTFChars(outPath, strPngPath);
        return JNI_FALSE;
    } else {
        env->ReleaseStringUTFChars(outPath, strPngPath);
    }

    //create png structure pointer
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        char *message = "Can\'t create PNG structure";
        LOGE(*message);
        if (throwException) {
            jstring er = env->NewStringUTF(message);
            throw_decode_file_exception(env, outPath, er);
            env->DeleteLocalRef(er);
        }
        return JNI_FALSE;
    }
    png_ptr_init = 1;

    //create png info pointer
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        char *message = "Can\'t create PNG info structure";
        LOGE(*message);
        if (throwException) {
            jstring er = env->NewStringUTF(message);
            throw_decode_file_exception(env, outPath, er);
            env->DeleteLocalRef(er);
        }
        return JNI_FALSE;
    }
    png_info_init = 1;

    //png error handler
    if (setjmp(png_jmpbuf(png_ptr))) {
        char *message = "Error creating PNG";
        LOGE(message);
        if (throwException) {
            jstring er = env->NewStringUTF(message);
            throw_decode_file_exception(env, outPath, er);
            env->DeleteLocalRef(er);
        }
        return JNI_FALSE;
    }

    //Init PNG IO
    png_init_io(png_ptr, pngFile);

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
    LOGII("image width", width);
    LOGII("image height", height);

    //set png data
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA,
                PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);

                LOGI("png_set_IHDR done");

    //write file header
    png_write_info(png_ptr, info_ptr);
    LOGI("png_write_info done");

    png_set_flush(png_ptr, 32);

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

    if (result) {
        png_write_end(png_ptr, info_ptr);
    }
    conversion_result = result;
    return conversion_result;
}

jboolean TiffToPngConverter::convertFromImage() {
    int origBufferSize = width * height * sizeof(uint32);

    unsigned long estimateMem = origBufferSize;
    estimateMem += 4 * width * sizeof(png_bytep);//working buf
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
        char *message = "Can\'t allocate buffer";
        LOGE(*message);
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, origBufferSize);
        }
        return JNI_FALSE;
    }

    if (0 == TIFFReadRGBAImageOriented(tiffImage, width, height, origBuffer, ORIENTATION_TOPLEFT, 0)) {
        free(origBuffer);
        char *message = "Can\'t read tiff";
        if (throwException) {
            jstring er = env->NewStringUTF(message);
            throw_decode_file_exception(env, outPath, er);
            env->DeleteLocalRef(er);
        }
        LOGE(message);
        return JNI_FALSE;
    }

    jlong total = width * height;
    sendProgress(0, total);

    for (int y = 0; y < height; y++) {
        if (checkStop()) {
            free(origBuffer);
            return JNI_FALSE;
        }
        sendProgress(y * width, total);
        png_bytep row = (png_bytep)malloc(4 * width * sizeof(png_bytep));
        memcpy(row, origBuffer + (y * width), width * 4);
        png_write_row(png_ptr, row);
        delete(row);
    }
    free(origBuffer);
    sendProgress(total, total);
    return JNI_TRUE;
}

jboolean TiffToPngConverter::convertFromTile() {
    uint32 tileWidth = 0, tileHeight = 0;
    TIFFGetField(tiffImage, TIFFTAG_TILEWIDTH, &tileWidth);
    TIFFGetField(tiffImage, TIFFTAG_TILEWIDTH, &tileHeight);
    LOGII("Tile width", tileWidth);
    LOGII("Tile height", tileHeight);

    //uint32 *raster = (uint32 *)_TIFFmalloc(width * tileHeight * sizeof(uint32));
    uint32 workingWidth = (width/tileWidth + (width%tileWidth == 0 ? 0 : 1)) * tileWidth;
    LOGII("workingWidth ", workingWidth );
    uint32 rasterSize =  workingWidth  * tileHeight ;
    LOGII("rasterSize ", rasterSize );
    //uint32 *raster = (uint32 *)_TIFFmalloc(rasterSize * sizeof(uint32));


    unsigned long estimateMem = rasterSize * sizeof(uint32); //raster
    estimateMem += tileWidth * tileHeight * sizeof(uint32); //tile raster
    estimateMem += tileWidth * sizeof (uint32); //working buf
    estimateMem += 4 * width * sizeof(png_bytep); //bufer for writing to png
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
                {
                    rotateTileLinesHorizontal(tileHeight, tileWidth, rasterTile, work_line_buf);

                    break;
                    }
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

        LOGI("writing to png");
        LOGII("endy", endy);
        LOGII("endx", endx);
        for (int y = starty; y < tileHeight; y++) {
            if (imageWritedLines == height) break;
            //create temp raster and write there pixels than not null
            //uint32 *rasterLine = (uint32 *)malloc(width * sizeof(uint32));
            //for (int x = startx, x2 = 0; x2 < width; x++, x2++) {
            //    rasterLine[x2] = raster[y * workingWidth + x];
            //}
            png_bytep pngrow = (png_bytep)malloc(4 * width * sizeof(png_bytep));
            memcpy(pngrow, raster + y * workingWidth, width * 4);
            //memcpy(pngrow, rasterLine, width * 4);
            png_write_row(png_ptr, pngrow);
            free(pngrow);
            //delete(rasterLine);
            imageWritedLines++;
        }
        LOGII("imageWritedLines", imageWritedLines);

        free(raster);

    }

    /*if (raster) {
        _TIFFfree(raster);
        raster = NULL;
    }*/

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

jboolean TiffToPngConverter::convertFromStrip() {
    uint32 stripSize = TIFFStripSize (tiffImage);
    uint32 stripMax = TIFFNumberOfStrips (tiffImage);
    int rowPerStrip = -1;
    TIFFGetField(tiffImage, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);

    unsigned long estimateMem = width * sizeof(uint32);//working buf
    estimateMem += width * rowPerStrip * sizeof (uint32);//raster
    estimateMem += 4 * width * sizeof(png_bytep); //buf for writing to png
    LOGII("estimateMem", estimateMem);
    if (estimateMem > availableMemory && availableMemory != -1) {
        LOGEI("Not enought memory", availableMemory);
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, estimateMem);
        }
        return JNI_FALSE;
    }

    jlong total = stripMax * rowPerStrip * width;
    sendProgress(0, total);

    uint32* work_line_buf = (uint32 *)_TIFFmalloc(width * sizeof(uint32));
    uint32* raster = (uint32 *)_TIFFmalloc(width * rowPerStrip * sizeof (uint32));

    uint32 rows_to_write = 0;

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
        sendProgress(i * width, total);
        TIFFReadRGBAStrip(tiffImage, i, raster);

        rows_to_write = 0;
        if( i + rowPerStrip > height )
            rows_to_write = height - i;
        else
            rows_to_write = rowPerStrip;

        if (origorientation <= 4) {
            for (int line = 0; line < rowPerStrip / 2; line++) {
                unsigned int  *top_line, *bottom_line;
                top_line = raster + width * line;
                bottom_line = raster + width * (rowPerStrip - line - 1);

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

        for (int y = 0; y < rows_to_write; y++) {
            png_bytep row = (png_bytep)malloc(4 * width * sizeof(png_bytep));
            memcpy(row, raster + (y * width), width * 4);
            png_write_row(png_ptr, row);
            delete(row);
        }

    }

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

int TiffToPngConverter::getDecodeMethod() {
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
    /*int method = -1;
	uint32 tileWidth, tileHeight;
	int readTW = 0, readTH = 0;
    readTW = TIFFGetField(tiffImage, TIFFTAG_TILEWIDTH, &tileWidth);
    readTH = TIFFGetField(tiffImage, TIFFTAG_TILELENGTH, &tileHeight);
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
    } else if (tileWidth > 0 && tileHeight > 0 && readTH > 0 && readTW > 0) {
        method = DECODE_METHOD_TILE;
    } else {
        method = DECODE_METHOD_IMAGE;
    }
	LOGII("Decode method", method);
	return method;*/
}

void TiffToPngConverter::rotateTileLinesVertical(uint32 tileHeight, uint32 tileWidth, uint32* whatRotate, uint32 *bufferLine) {
    for (int line = 0; line < tileHeight / 2; line++) {
        unsigned int  *top_line, *bottom_line;
        top_line = whatRotate + tileWidth * line;
        bottom_line = whatRotate + tileWidth * (tileHeight - line -1);
        _TIFFmemcpy(bufferLine, top_line, sizeof(unsigned int) * tileWidth);
        _TIFFmemcpy(top_line, bottom_line, sizeof(unsigned int) * tileWidth);
        _TIFFmemcpy(bottom_line, bufferLine, sizeof(unsigned int) * tileWidth);
    }
}

void TiffToPngConverter::rotateTileLinesHorizontal(uint32 tileHeight, uint32 tileWidth, uint32* whatRotate, uint32 *bufferLine) {
    uint32 buf;
    for (int y = 0; y < tileHeight; y++) {
        for (int x = 0; x < tileWidth / 2; x++) {
            buf = whatRotate[y * tileWidth + x];
            whatRotate[y * tileWidth + x] = whatRotate[y * tileWidth + tileWidth - x - 1];
            whatRotate[y * tileWidth + tileWidth - x - 1] = buf;
        }
    }
}

void TiffToPngConverter::normalizeTile(uint32 tileHeight, uint32 tileWidth, uint32* rasterTile) {
    //normalize tile
    //find start and end pixels
    int sx = -1, ex= -1, sy= -1, ey= -1;
    for (int y = 0; y < tileHeight; y++) {
        for (int x = 0; x < tileWidth; x++) {
            if (rasterTile[y * tileWidth + x] != 0) {
                sx = x;
                sy = y;
                break;
            }
        }
        if (sx != -1) break;
    }
    for (int y = tileHeight -1; y >= 0; y--) {
        for (int x = tileWidth -1; x >= 0; x--) {
            if (rasterTile[y * tileWidth + x] != 0) {
                ex = x;
                ey = y;
                break;
            }
        }
        if (ex != -1) break;
    }
    if (sy != 0) {
        for (int y = 0; y < tileHeight - sy -1; y++) {
            memcpy(rasterTile + (y * tileWidth), rasterTile + ((y+sy)*tileWidth), tileWidth * sizeof(uint32));
        }
    }
    if (sx != 0) {
        for (int y = 0; y < tileHeight; y++) {
           for (int x = 0; x < tileWidth - sx -1; x++) {
               rasterTile[y * tileWidth + x] = rasterTile[y * tileWidth + x + sx];
           }
        }
    }
}
