//
// Created by beyka on 5/10/17.
//

#include "TiffToPngConverter.h"

TiffToPngConverter::TiffToPngConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts)
{
    availableMemory = 8000 * 8000 * 4;
    env = e;
    inPath = in;
    outPath = out;
    optionsObj = opts;
    throwException = false;
    tiffDirectory = 0;
}

TiffToPngConverter::~TiffToPngConverter()
{
    if (tiffImage) {
        TIFFClose(tiffImage);
        tiffImage = NULL;
    }

    if (pngFile) {
        fclose(pngFile);
    }

    if (info_ptr) {
        png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    }

    if (png_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
    }
}

void TiffToPngConverter::readOptions()
{
    if (optionsObj == NULL) return;
    jclass optionsClass = env->FindClass("org/beyka/tiffbitmapfactory/TiffConverter$ConverterOptions");

    jfieldID tiffdirfield = env->GetFieldID(optionsClass, "tiffDirectoryRead", "I");
    tiffDirectory = env->GetIntField(optionsObj, tiffdirfield);

    jfieldID availablememfield = env->GetFieldID(optionsClass, "availableMemory", "J");
    availableMemory = env->GetLongField(optionsObj, availablememfield);

    jfieldID throwexceptionsfield = env->GetFieldID(optionsClass, "throwExceptions", "Z");
    throwException = env->GetBooleanField(optionsObj, throwexceptionsfield);

    env->DeleteLocalRef(optionsClass);
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

    png_write_end(png_ptr, info_ptr);

    return result;
}

jboolean TiffToPngConverter::convertFromImage() {
    int origBufferSize = width * height * sizeof(uint32);
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

    for (int y = 0; y < height; y++) {
        png_bytep row = (png_bytep)malloc(4 * width * sizeof(png_bytep));
        memcpy(row, origBuffer + (y * width), width * 4);
        png_write_row(png_ptr, row);
    }

    return JNI_TRUE;
}

jboolean TiffToPngConverter::convertFromTile() {
    uint32 tileWidth = 0, tileHeight = 0;
    TIFFGetField(tiffImage, TIFFTAG_TILEWIDTH, &tileWidth);
    TIFFGetField(tiffImage, TIFFTAG_TILEWIDTH, &tileHeight);


    uint32 *raster = (uint32 *)_TIFFmalloc(width * tileHeight * sizeof(uint32));
    uint32 *rasterTile = (uint32 *)_TIFFmalloc(tileWidth * tileHeight * sizeof(uint32));
    uint32 *work_line_buf = (uint32*)_TIFFmalloc(tileWidth * sizeof (uint32));

    uint32 row, column;

    for (row = 0; row < height; row += tileHeight) {
        LOGII("row", row);
        for (column = 0; column < width; column += tileWidth) {
        LOGII("column", column);

            TIFFReadRGBATile(tiffImage, column + tileWidth, row, rasterTile);
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

            LOGI("processing raster");
            int rowHasPixels = 0;
            for (int th = 0, bh = 0; th < tileHeight; th++) {
                for (int tw = 0, bw = 0; tw < tileWidth; tw++) {
                    uint32 srcPosition = th * tileWidth + tw;
                    if (rasterTile[srcPosition] != 0) {
                        int position = 0;
                        if (origorientation <= 4) {
                            position = (bh) * width + column + bw;
                        } else {
                            position = (column + bw) * tileHeight + bh;
                        }
                        //LOGII("pos", position);
                        raster[position] = rasterTile[srcPosition];
                        rowHasPixels = 1;
                        bw++;
                    }
                }
                if (rowHasPixels) {
                    bh++;
                    rowHasPixels = 0;
                }
                rowHasPixels = 0;
            }
        }

        LOGI("writing to png");
        for (int y = 0; y < tileHeight; y++) {
            png_bytep row = (png_bytep)malloc(4 * width * sizeof(png_bytep));
            memcpy(row, raster + (y * width), width * 4);
            png_write_row(png_ptr, row);
        }

    }

    if (raster) {
        _TIFFfree(raster);
        raster = NULL;
    }

    if (rasterTile) {
        _TIFFfree(rasterTile);
        rasterTile = NULL;
    }

    if (work_line_buf) {
        _TIFFfree(rasterTile);
        work_line_buf = NULL;
    }

    return JNI_TRUE;

}

jboolean TiffToPngConverter::convertFromStrip() {
    uint32 stripSize = TIFFStripSize (tiffImage);
    uint32 stripMax = TIFFNumberOfStrips (tiffImage);
    int rowPerStrip = -1;
    TIFFGetField(tiffImage, TIFFTAG_ROWSPERSTRIP, &rowPerStrip);

    uint32* work_line_buf = (uint32 *)_TIFFmalloc(width * sizeof(uint32));
    uint32* raster = (uint32 *)_TIFFmalloc(width * rowPerStrip * sizeof (uint32));

    uint32 rows_to_write = 0;

    for (int i = 0; i < stripMax*rowPerStrip; i += rowPerStrip) {
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

        for (int y = 0; y < rows_to_write; y++) {
            png_bytep row = (png_bytep)malloc(4 * width * sizeof(png_bytep));
            memcpy(row, raster + (y * width), width * 4);
            png_write_row(png_ptr, row);
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
