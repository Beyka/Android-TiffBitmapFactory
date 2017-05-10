//
// Created by beyka on 5/9/17.
//
using namespace std;
#ifdef __cplusplus
extern "C" {
#endif

#include "NativeTiffConverter.h"
#include "png.h"

struct png_image {
	png_uint_32 imWidth, imHeight; //реальный размер картинки
	png_uint_32 glWidth, glHeight; //размер который подойдет для OpenGL
	int bit_depth, color_type;
	char* data; //данные RGB/RGBA
};

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffPng
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring pngPath)
  {

    TiffToPngConverter *converter = new TiffToPngConverter(env, clazz, tiffPath, pngPath, NULL);
    jboolean result = converter->convert();
    delete(converter);
    return result;

/*
    //local variables to convert
    TIFF *tiffImage;
    png_image pngImage;
    //png_structp pngImage;

    uint32 width;
    uint32 height;


    //open tiff file for reading
    const char *strTiffPath = NULL;
    strTiffPath = env->GetStringUTFChars(tiffPath, 0);
    LOGIS("nativeTiffOpen", strTiffPath);

    tiffImage = TIFFOpen(strTiffPath, "r");

    if (tiffImage == NULL) {

        LOGES("Can\'t open bitmap", strTiffPath);
        env->ReleaseStringUTFChars(tiffPath, strTiffPath);
        return JNI_FALSE;
    } else {
        LOGIS("Tiff openned", strTiffPath);
        env->ReleaseStringUTFChars(tiffPath, strTiffPath);
    }
    LOGI("Tiff is open");



    //open png file for writing
        const char *strPngPath = NULL;
        strPngPath = env->GetStringUTFChars(pngPath, 0);
        LOGIS("nativePngOpen", strPngPath);

        FILE* pngfile = fopen(strPngPath, "wb");
        if (!pngfile) {
            LOGES("Can\'t open png file", strPngPath);
            return JNI_FALSE;
        }
        //fseek(pngfile, 8, SEEK_CUR);
        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr) {
            LOGES("Can\'t create png struct", strPngPath);
            return JNI_FALSE;
        }
        png_infop info_ptr;// = png_create_info_struct(png_ptr);
        if (!(info_ptr = png_create_info_struct(png_ptr))) {
            LOGES("Can\'t create png info struct", strPngPath);
            return JNI_FALSE;
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            LOGE("set jump?");
            return JNI_FALSE;
        }

        png_init_io(png_ptr, pngfile);

        //pngImage = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        LOGI("PNG is open");



    //Set tiff directory(should read fro options)
    TIFFSetDirectory(tiffImage, 1);

    //Read dimensions of image
    TIFFGetField(tiffImage, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiffImage, TIFFTAG_IMAGELENGTH, &height);
    LOGII("width", width);
    LOGII("height", height);

    //TODO read argb pixels
    int origBufferSize = width * height * sizeof(uint32);
    uint32 *origBuffer = NULL;
    origBuffer = (uint32 *) _TIFFmalloc(origBufferSize);
    if (origBuffer == NULL) {
        LOGE("Can\'t allocate memory for origBuffer");
        return JNI_FALSE;
    }
    LOGI("orig buffer allocated");

    if (0 == TIFFReadRGBAImageOriented(tiffImage, width, height, origBuffer, ORIENTATION_TOPLEFT, 0)) {
        free(origBuffer);
        LOGE("Can\'t read tiff");
        return JNI_FALSE;
    }
    LOGI("tiff img read");

//set data to png
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA,
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);
    LOGI("png data setted");


    png_text title;
    title.compression = PNG_TEXT_COMPRESSION_NONE;
    title.key = "Title";
    title.text = "test title text";
    png_set_text(png_ptr, info_ptr, &title, 1);

    png_write_info(png_ptr, info_ptr);

//    unsigned char *rows[height]; //= new png_bytepp[height * 4];


    for (int y = 0; y < height; y++) {
        LOGII("row", y);
        png_bytep row = (png_bytep)malloc(4 * width * sizeof(png_bytep));
        memcpy(row, origBuffer + (y * width), width * 4);
        png_write_row(png_ptr, row);

        /*unsigned char * column = (unsigned char *)malloc(width * 4);
        memcpy(column, origBuffer + (y * width), width * 4);
        rows[y] = column;

    }

    //write rows to png

    //png_write_image(png_ptr, rows);
    //png_set_rows(png_ptr, info_ptr, rows);
    //LOGI("png rows setted");
    //png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
    //LOGI("png wrote setted");
    png_write_end(png_ptr, info_ptr);
    LOGI("png wrote end setted");





    //Close all openned resources
    png_destroy_write_struct(&png_ptr, NULL);
    fclose(pngfile);

    if (tiffImage) {
        TIFFClose(tiffImage);
        tiffImage = NULL;
    }

      return JNI_TRUE;*/
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertPngTiff
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring pngPath)
  {
  return JNI_FALSE;
  }

#ifdef __cplusplus
}
#endif