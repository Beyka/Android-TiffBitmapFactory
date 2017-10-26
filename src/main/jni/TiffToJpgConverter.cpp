//
// Created by beyka on 5/10/17.
//

#include "TiffToJpgConverter.h"

TiffToJpgConverter::TiffToJpgConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts, jobject listener)
    : BaseTiffConverter(e, clazz, in, out, opts, listener)
{
    jpeg_struct_init = 0;
}

TiffToJpgConverter::~TiffToJpgConverter()
{
    LOGI("destructor");
    if (tiffImage) {
        TIFFClose(tiffImage);
        tiffImage = NULL;
    }
        LOGI("tiff");

    if (jpegFile) {
        /*if (!conversion_result) {
            const char *strPngPath = NULL;
            strPngPath = env->GetStringUTFChars(outPath, 0);
            remove(strPngPath);
            env->ReleaseStringUTFChars(outPath, strPngPath);
        }*/
        fclose(jpegFile);
    }
    LOGI("file");
    if (jpeg_struct_init) {
        jpeg_destroy_compress(&cinfo);
    }
    LOGI("destructor finish");

}

jboolean TiffToJpgConverter::convert()
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
    jpegFile = fopen(strPngPath, "wb");
    if (!jpegFile) {
        if (throwException) {
            throw_cant_open_file_exception(env, outPath);
        }
        LOGES("Can\'t open out file", strPngPath);
        env->ReleaseStringUTFChars(outPath, strPngPath);
        return JNI_FALSE;
    } else {
        env->ReleaseStringUTFChars(outPath, strPngPath);
    }

    LOGI("initialize jpeg structure");
    //set error handling
    cinfo.err = jpeg_std_error(&jerr);
    LOGI("initialize error handling done");
    //initialize jpeg compression object
    jpeg_create_compress(&cinfo);
    jpeg_struct_init = 1;
    LOGI("initialize compress done");
    //set phisical file for jpeg
    jpeg_stdio_dest(&cinfo, jpegFile);
    LOGI("initialize dest file done");

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
        return JNI_FALSE;
    }

    LOGII("image width", width);
    LOGII("image height", height);
    LOGII("image bound width", boundWidth);
    LOGII("image bound height", boundHeight);
    LOGII("image out width", outWidth);
    LOGII("image out height", outHeight);

    cinfo.image_width = outWidth;
    cinfo.image_height = outHeight;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    LOGI("writing img parameters done");

    jpeg_set_defaults(&cinfo);
    LOGI("set defaults done");
    jpeg_set_quality(&cinfo, JPEG_QUALITY, TRUE /* limit to baseline-JPEG values */);
    LOGI("set quality done");

    //write file header
    jpeg_start_compress(&cinfo, TRUE);
    LOGI("start compressing");

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
        jpeg_finish_compress(&cinfo);
    }
    conversion_result = result;
    return conversion_result;
}

jboolean TiffToJpgConverter::convertFromImage() {
    int origBufferSize = width * height * sizeof(uint32);

    unsigned long estimateMem = origBufferSize;
    estimateMem += outWidth * sizeof(unsigned char) * 3;//working buf
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

    jlong total = width * height;
    sendProgress(0, total);

    int outY, outX;
    JSAMPROW row_pointer[1];
    for (int y = 0; y < height; y++) {
        if (checkStop()) {
            free(origBuffer);
            return JNI_FALSE;
        }
        sendProgress(y * width, total);
        if (y < outStartY || y >= outStartY + outHeight) continue;
        outY = y - outStartY;

        unsigned char *row = (unsigned char*)malloc(outWidth * sizeof(unsigned char) * 3);

        for (int x = 0; x < width * 3; x += 3) {
            if (x < outStartX * 3 || x >= (outStartX + outWidth) * 3) continue;
            outX = x - (outStartX*3);
            uint32 pix = origBuffer[y * width + x/3];
            unsigned char *vp = (unsigned char *)&pix;
            row[outX] = vp[0];
            row[outX+1] = vp[1];
            row[outX+2] = vp[2];
        }
        row_pointer[0] = row;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
        delete(row);
    }
    free(origBuffer);
    sendProgress(total, total);
    return JNI_TRUE;
}

jboolean TiffToJpgConverter::convertFromTile() {
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
    estimateMem += outWidth * sizeof(unsigned char) * 3; //bufer for writing to jpg
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
        JSAMPROW row_pointer[1];
        for (int y = starty; y < tileHeight; y++) {
            if (imageWritedLines == height) break;
            if (y + row < outStartY || y + row >= outStartY + outHeight) continue;
            outY = y + row - outStartY;
            unsigned char *jpgrow = (unsigned char*)malloc(outWidth * sizeof(unsigned char) * 3);

           // uint32 *rasterLine = (uint32 *)malloc(width * sizeof(uint32));

            //for (int x = startx, x2 = 0; x2 < width; x++, x2++) {
            //   rasterLine[x2] = raster[y * workingWidth + x];
            //}

            for (int x = 0; x < width * 3; x += 3) {
                if (x < outStartX * 3 || x >= (outStartX + outWidth) * 3) continue;
                outX = x - (outStartX*3);
                uint32 pix = raster[y * workingWidth + x/3];
                unsigned char *vp = (unsigned char *)&pix;
                jpgrow[outX] = vp[0];
                jpgrow[outX+1] = vp[1];
                jpgrow[outX+2] = vp[2];
            }
            row_pointer[0] = jpgrow;
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
            delete(jpgrow);
            //delete(rasterLine);

            imageWritedLines++;
        }
        //LOGII("imageWritedLines", imageWritedLines);
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

jboolean TiffToJpgConverter::convertFromStrip() {
    uint32 stripSize = TIFFStripSize (tiffImage);
    uint32 stripMax = TIFFNumberOfStrips (tiffImage);
    int rowPerStrip = -1;
    TIFFGetField(tiffImage, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);

    unsigned long estimateMem = width * sizeof(uint32);//working buf
    estimateMem += width * rowPerStrip * sizeof (uint32);//raster
    estimateMem += outWidth * sizeof(unsigned char) * 3; //buf for writing to jpg
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
        JSAMPROW row_pointer[1];
        for (int y = 0; y < rows_to_write; y++) {
            if (i + y < outStartY || i + y >= outStartY + outHeight) continue;
            outY = i + y - outStartY;

            unsigned char *jpgrow = (unsigned char*)malloc(outWidth * sizeof(unsigned char) * 3);

            for (int x = 0; x < width * 3; x += 3) {
                if (x < outStartX * 3 || x >= (outStartX + outWidth) * 3) continue;
                outX = x - (outStartX*3);
                uint32 pix = raster[y * width + x/3];
                unsigned char *vp = (unsigned char *)&pix;
                jpgrow[outX] = vp[0];
                jpgrow[outX+1] = vp[1];
                jpgrow[outX+2] = vp[2];
            }
            row_pointer[0] = jpgrow;
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
            delete(jpgrow);

            /*png_bytep row = (png_bytep)malloc(4 * width * sizeof(png_bytep));
            memcpy(row, raster + (y * width), width * 4);
            png_write_row(png_ptr, row);
            delete(row);*/
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

int TiffToJpgConverter::getDecodeMethod() {
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
