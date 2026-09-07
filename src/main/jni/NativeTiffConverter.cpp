//
// Created by beyka on 5/9/17.
//
using namespace std;
#ifdef __cplusplus
extern "C" {
#endif

#include "NativeTiffConverter.h"
#include "png.h"

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffPng
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring pngPath, jobject options, jobject listener)
  {

    TiffToPngConverter *converter = new TiffToPngConverter(env, clazz, tiffPath, pngPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffPngFd
  (JNIEnv *env, jclass clazz, jint tiffFd, jint pngFd, jobject options, jobject listener)
  {

    TiffToPngConverter *converter = new TiffToPngConverter(env, clazz, tiffFd, pngFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffJpg
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring jpgPath, jobject options, jobject listener)
  {

    TiffToJpgConverter *converter = new TiffToJpgConverter(env, clazz, tiffPath, jpgPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffJpgFd
  (JNIEnv *env, jclass clazz, jint tiffFd, jint jpgFd, jobject options, jobject listener)
  {

    TiffToJpgConverter *converter = new TiffToJpgConverter(env, clazz, tiffFd, jpgFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffBmp
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring outPath, jobject options, jobject listener)
  {

    TiffToBmpConverter *converter = new TiffToBmpConverter(env, clazz, tiffPath, outPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffBmpFd
  (JNIEnv *env, jclass clazz, jint tiffFd, jint bmpFd, jobject options, jobject listener)
  {

    TiffToBmpConverter *converter = new TiffToBmpConverter(env, clazz, tiffFd, bmpFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertPngTiff
  (JNIEnv *env, jclass clazz, jstring pngPath, jstring tiffPath, jobject options, jobject listener)
  {
    PngToTiffConverter *converter = new PngToTiffConverter(env, clazz, pngPath, tiffPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertPngTiffFd
  (JNIEnv *env, jclass clazz, jint pngFd, jint tiffFd, jobject options, jobject listener)
  {
    PngToTiffConverter *converter = new PngToTiffConverter(env, clazz, pngFd, tiffFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertJpgTiff
  (JNIEnv *env, jclass clazz, jstring pngPath, jstring tiffPath, jobject options, jobject listener)
  {
    JpgToTiffConverter *converter = new JpgToTiffConverter(env, clazz, pngPath, tiffPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertJpgTiffFd
  (JNIEnv *env, jclass clazz, jint jpgFd, jint tiffFd, jobject options, jobject listener)
  {
    JpgToTiffConverter *converter = new JpgToTiffConverter(env, clazz, jpgFd, tiffFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertBmpTiff
  (JNIEnv *env, jclass clazz, jstring bmpPath, jstring tiffPath, jobject options, jobject listener)
  {
    BmpToTiffConverter *converter = new BmpToTiffConverter(env, clazz, bmpPath, tiffPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertBmpTiffFd
  (JNIEnv *env, jclass clazz, jint bmpFd, jint tiffFd, jobject options, jobject listener)
  {
    BmpToTiffConverter *converter = new BmpToTiffConverter(env, clazz, bmpFd, tiffFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

  JNIEXPORT jobject JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_readBmp
    (JNIEnv *env, jclass clazz, jstring tiffPath, jstring bmpPath, jobject options, jobject listener)
    {
      return readBmp(env, clazz, tiffPath, bmpPath, options, listener);
    }

JNIEXPORT jobject JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_getImageType
  (JNIEnv *env, jclass clazz, jstring path)
  {


    const char *strPath = NULL;
    strPath = env->GetStringUTFChars(path, 0);
    LOGIS("path", strPath);

    int imageformat;

    FILE *inFile = fopen(strPath, "rb");
    if (inFile) {
        //read file header
        size_t byte_count = 8;
        unsigned char *data = (unsigned char *)malloc(sizeof(unsigned char) * byte_count);
        fread(data, 1, byte_count, inFile);

        LOGIS("header", data);

        switch(data[0]) {
            case (unsigned char)'\xFF':
                 imageformat =  ( !strncmp( (const char*)data, "\xFF\xD8\xFF", 3 )) ?
                    IMAGE_FILE_JPG : IMAGE_FILE_INVALID;
                 break;

              case (unsigned char)'\x89':
                 imageformat = ( !strncmp( (const char*)data,
                                    "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8 )) ?
                    IMAGE_FILE_PNG : IMAGE_FILE_INVALID;
                 break;

              case 'G':
                 imageformat = ( !strncmp( (const char*)data, "GIF87a", 6 ) ||
                          !strncmp( (const char*)data, "GIF89a", 6 ) ) ?
                    IMAGE_FILE_GIF : IMAGE_FILE_INVALID;
                 break;

              case 'I':
                 imageformat = ( !strncmp( (const char*)data, "\x49\x49\x2A\x00", 4 )) ?
                    IMAGE_FILE_TIFF : IMAGE_FILE_INVALID;
                 break;

              case 'M':
                 imageformat = ( !strncmp( (const char*)data, "\x4D\x4D\x00\x2A", 4 )) ?
                     IMAGE_FILE_TIFF : IMAGE_FILE_INVALID;
                     break;

              case 'B':
                 imageformat = (( data[1] == 'M' )) ?
                     IMAGE_FILE_BMP : IMAGE_FILE_INVALID;
                 break;

              case 'R':
                 if ( strncmp( (const char*)data,     "RIFF", 4 )) {
                        imageformat = IMAGE_FILE_INVALID;
                        break;
                    }
                 if ( strncmp( (const char*)(data+8), "WEBP", 4 )) {
                        imageformat = IMAGE_FILE_INVALID;
                        break;
                    }
                 imageformat = IMAGE_FILE_WEBP;
                 break;

              case '\0':
                 if ( !strncmp( (const char*)data, "\x00\x00\x01\x00", 4 )) {
                        imageformat = IMAGE_FILE_ICO;
                        break;
                    }
                 if ( !strncmp( (const char*)data, "\x00\x00\x02\x00", 4 )) {
                        imageformat = IMAGE_FILE_ICO;
                        break;
                    }
                 imageformat =  IMAGE_FILE_INVALID;
                    break;
              default:
                 imageformat = IMAGE_FILE_INVALID;
        }

        fclose(inFile);
    } else {
        imageformat = IMAGE_FILE_INVALID;
    }
    
    jclass imageFormatClass = env->FindClass(
                "org/beyka/tiffbitmapfactory/ImageFormat");
    jfieldID imageFormatFieldId = NULL;
    switch (imageformat) {
        case IMAGE_FILE_JPG:
            imageFormatFieldId = env->GetStaticFieldID(imageFormatClass,
                                           "JPEG",
                                           "Lorg/beyka/tiffbitmapfactory/ImageFormat;");
            break;
        case IMAGE_FILE_PNG:
            imageFormatFieldId = env->GetStaticFieldID(imageFormatClass,
                                           "PNG",
                                           "Lorg/beyka/tiffbitmapfactory/ImageFormat;");
            break;
        case IMAGE_FILE_TIFF:
            imageFormatFieldId = env->GetStaticFieldID(imageFormatClass,
                                           "TIFF",
                                           "Lorg/beyka/tiffbitmapfactory/ImageFormat;");
            break;
         case IMAGE_FILE_BMP:
            imageFormatFieldId = env->GetStaticFieldID(imageFormatClass,
                                           "BMP",
                                           "Lorg/beyka/tiffbitmapfactory/ImageFormat;");
            break;
        default:
            imageFormatFieldId = env->GetStaticFieldID(imageFormatClass,
                                           "UNKNOWN",
                                           "Lorg/beyka/tiffbitmapfactory/ImageFormat;");
    }
    
    jobject imageFormatObj = env->GetStaticObjectField(
                    imageFormatClass,
                    imageFormatFieldId);

    return imageFormatObj;

  }

JNIEXPORT jobject JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_getImageTypeFd
  (JNIEnv *env, jclass clazz, jint fd)
  {

    int imageformat;

    LOGII("fd ", fd);

    if (fd != -1) {
        LOGI("Start check");
        //read file header
        int i= 0;
        size_t byte_count = 8;
        /*while( i < byte_count) {
            char c;
            read(fd, &c, 1);
            LOGII("Read ", c);
            i++;
        }*/
        unsigned char *data = (unsigned char *)malloc(sizeof(unsigned char) * byte_count);
        int b = read(fd, data, byte_count);
        LOGII("Read bytes: ", b);

        LOGIS("header", data);

        lseek(fd, 0, SEEK_SET);

        switch(data[0]) {
            case (unsigned char)'\xFF':
                 imageformat =  ( !strncmp( (const char*)data, "\xFF\xD8\xFF", 3 )) ?
                    IMAGE_FILE_JPG : IMAGE_FILE_INVALID;
                 break;

              case (unsigned char)'\x89':
                 imageformat = ( !strncmp( (const char*)data,
                                    "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8 )) ?
                    IMAGE_FILE_PNG : IMAGE_FILE_INVALID;
                 break;

              case 'G':
                 imageformat = ( !strncmp( (const char*)data, "GIF87a", 6 ) ||
                          !strncmp( (const char*)data, "GIF89a", 6 ) ) ?
                    IMAGE_FILE_GIF : IMAGE_FILE_INVALID;
                 break;

              case 'I':
                 imageformat = ( !strncmp( (const char*)data, "\x49\x49\x2A\x00", 4 )) ?
                    IMAGE_FILE_TIFF : IMAGE_FILE_INVALID;
                 break;

              case 'M':
                 imageformat = ( !strncmp( (const char*)data, "\x4D\x4D\x00\x2A", 4 )) ?
                     IMAGE_FILE_TIFF : IMAGE_FILE_INVALID;
                     break;

              case 'B':
                 imageformat = (( data[1] == 'M' )) ?
                     IMAGE_FILE_BMP : IMAGE_FILE_INVALID;
                 break;

              case 'R':
                 if ( strncmp( (const char*)data,     "RIFF", 4 )) {
                        imageformat = IMAGE_FILE_INVALID;
                        break;
                    }
                 if ( strncmp( (const char*)(data+8), "WEBP", 4 )) {
                        imageformat = IMAGE_FILE_INVALID;
                        break;
                    }
                 imageformat = IMAGE_FILE_WEBP;
                 break;

              case '\0':
                 if ( !strncmp( (const char*)data, "\x00\x00\x01\x00", 4 )) {
                        imageformat = IMAGE_FILE_ICO;
                        break;
                    }
                 if ( !strncmp( (const char*)data, "\x00\x00\x02\x00", 4 )) {
                        imageformat = IMAGE_FILE_ICO;
                        break;
                    }
                 imageformat =  IMAGE_FILE_INVALID;
                    break;
              default:
                 imageformat = IMAGE_FILE_INVALID;
        }

    } else {
        imageformat = IMAGE_FILE_INVALID;
    }

    jclass imageFormatClass = env->FindClass(
                "org/beyka/tiffbitmapfactory/ImageFormat");
    jfieldID imageFormatFieldId = NULL;
    switch (imageformat) {
        case IMAGE_FILE_JPG:
            imageFormatFieldId = env->GetStaticFieldID(imageFormatClass,
                                           "JPEG",
                                           "Lorg/beyka/tiffbitmapfactory/ImageFormat;");
            break;
        case IMAGE_FILE_PNG:
            imageFormatFieldId = env->GetStaticFieldID(imageFormatClass,
                                           "PNG",
                                           "Lorg/beyka/tiffbitmapfactory/ImageFormat;");
            break;
        case IMAGE_FILE_TIFF:
            imageFormatFieldId = env->GetStaticFieldID(imageFormatClass,
                                           "TIFF",
                                           "Lorg/beyka/tiffbitmapfactory/ImageFormat;");
            break;
         case IMAGE_FILE_BMP:
            imageFormatFieldId = env->GetStaticFieldID(imageFormatClass,
                                           "BMP",
                                           "Lorg/beyka/tiffbitmapfactory/ImageFormat;");
            break;
        default:
            imageFormatFieldId = env->GetStaticFieldID(imageFormatClass,
                                           "UNKNOWN",
                                           "Lorg/beyka/tiffbitmapfactory/ImageFormat;");
    }

    jobject imageFormatObj = env->GetStaticObjectField(
                    imageFormatClass,
                    imageFormatFieldId);

    return imageFormatObj;

  }

JNIEXPORT jobject
JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeCloseFd
        (JNIEnv *env, jclass clazz, jint fd) {
    close(fd);
}

#ifdef __cplusplus
}
#endif