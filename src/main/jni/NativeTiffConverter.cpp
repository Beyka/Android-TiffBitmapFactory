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
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring pngPath, jobject options)
  {

    TiffToPngConverter *converter = new TiffToPngConverter(env, clazz, tiffPath, pngPath, options);
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

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffJpg
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring pngPath, jobject options)
  {

    TiffToJpgConverter *converter = new TiffToJpgConverter(env, clazz, tiffPath, pngPath, options);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertPngTiff
  (JNIEnv *env, jclass clazz, jstring pngPath, jstring tiffPath, jobject options)
  {
    PngToTiffConverter *converter = new PngToTiffConverter(env, clazz, pngPath, tiffPath, options);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertJpgTiff
  (JNIEnv *env, jclass clazz, jstring pngPath, jstring tiffPath, jobject options)
  {
    JpgToTiffConverter *converter = new JpgToTiffConverter(env, clazz, pngPath, tiffPath, options);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jint JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_getImageType
  (JNIEnv *env, jclass clazz, jstring path)
  {


    const char *strPath = NULL;
    strPath = env->GetStringUTFChars(path, 0);
    LOGIS("path", strPath);

    FILE *inFile = fopen(strPath, "rb");
    if (inFile) {
        //read file header
        size_t byte_count = 8;
        unsigned char *data = (unsigned char *)malloc(sizeof(unsigned char) * byte_count);
        fread(data, 1, byte_count, inFile);

        switch(data[0]) {
            case (unsigned char)'\xFF':
                 return ( !strncmp( (const char*)data, "\xFF\xD8\xFF", 3 )) ?
                    IMAGE_FILE_JPG : IMAGE_FILE_INVALID;

              case (unsigned char)'\x89':
                 return ( !strncmp( (const char*)data,
                                    "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8 )) ?
                    IMAGE_FILE_PNG : IMAGE_FILE_INVALID;

              case 'G':
                 return ( !strncmp( (const char*)data, "GIF87a", 6 ) ||
                          !strncmp( (const char*)data, "GIF89a", 6 ) ) ?
                    IMAGE_FILE_GIF : IMAGE_FILE_INVALID;

              case 'I':
                 return ( !strncmp( (const char*)data, "\x49\x49\x2A\x00", 4 )) ?
                    IMAGE_FILE_TIFF : IMAGE_FILE_INVALID;

              case 'M':
                 return ( !strncmp( (const char*)data, "\x4D\x4D\x00\x2A", 4 )) ?
                     IMAGE_FILE_TIFF : IMAGE_FILE_INVALID;

              case 'B':
                 return (( data[1] == 'M' )) ?
                     IMAGE_FILE_BMP : IMAGE_FILE_INVALID;

              case 'R':
                 if ( strncmp( (const char*)data,     "RIFF", 4 ))
                    return IMAGE_FILE_INVALID;
                 if ( strncmp( (const char*)(data+8), "WEBP", 4 ))
                    return IMAGE_FILE_INVALID;
                 return IMAGE_FILE_WEBP;

              case '\0':
                 if ( !strncmp( (const char*)data, "\x00\x00\x01\x00", 4 ))
                    return IMAGE_FILE_ICO;
                 if ( !strncmp( (const char*)data, "\x00\x00\x02\x00", 4 ))
                    return IMAGE_FILE_ICO;
                 return IMAGE_FILE_INVALID;

              default:
                 return IMAGE_FILE_INVALID;
        }

        fclose(inFile);
    } else {
        return IMAGE_FILE_INVALID;
    }

  }

#ifdef __cplusplus
}
#endif