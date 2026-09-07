//
// Created by beyka on 5/9/17.
//
#ifdef __cplusplus
extern "C" {
#endif

#include "NativeTiffConverter.h"
#include "png.h"

static int detectImageFormat(const unsigned char *data, size_t size)
{
    if (size >= 3 && memcmp(data, "\xFF\xD8\xFF", 3) == 0) {
        return IMAGE_FILE_JPG;
    }
    if (size >= 8 && memcmp(data, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) == 0) {
        return IMAGE_FILE_PNG;
    }
    if (size >= 6 && (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0)) {
        return IMAGE_FILE_GIF;
    }
    if (size >= 4 && (memcmp(data, "\x49\x49\x2A\x00", 4) == 0
            || memcmp(data, "\x4D\x4D\x00\x2A", 4) == 0)) {
        return IMAGE_FILE_TIFF;
    }
    if (size >= 2 && data[0] == 'B' && data[1] == 'M') {
        return IMAGE_FILE_BMP;
    }
    if (size >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0) {
        return IMAGE_FILE_WEBP;
    }
    if (size >= 4 && (memcmp(data, "\x00\x00\x01\x00", 4) == 0
            || memcmp(data, "\x00\x00\x02\x00", 4) == 0)) {
        return IMAGE_FILE_ICO;
    }
    return IMAGE_FILE_INVALID;
}

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertTiffPng
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring pngPath, jobject options, jobject listener)
  {

    TiffToPngConverter *converter = new TiffToPngConverter(env, clazz, tiffPath, pngPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertTiffPngFd
  (JNIEnv *env, jclass clazz, jint tiffFd, jint pngFd, jobject options, jobject listener)
  {

    TiffToPngConverter *converter = new TiffToPngConverter(env, clazz, tiffFd, pngFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertTiffJpg
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring jpgPath, jobject options, jobject listener)
  {

    TiffToJpgConverter *converter = new TiffToJpgConverter(env, clazz, tiffPath, jpgPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertTiffJpgFd
  (JNIEnv *env, jclass clazz, jint tiffFd, jint jpgFd, jobject options, jobject listener)
  {

    TiffToJpgConverter *converter = new TiffToJpgConverter(env, clazz, tiffFd, jpgFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertTiffBmp
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring outPath, jobject options, jobject listener)
  {

    TiffToBmpConverter *converter = new TiffToBmpConverter(env, clazz, tiffPath, outPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertTiffBmpFd
  (JNIEnv *env, jclass clazz, jint tiffFd, jint bmpFd, jobject options, jobject listener)
  {

    TiffToBmpConverter *converter = new TiffToBmpConverter(env, clazz, tiffFd, bmpFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertPngTiff
  (JNIEnv *env, jclass clazz, jstring pngPath, jstring tiffPath, jobject options, jobject listener)
  {
    PngToTiffConverter *converter = new PngToTiffConverter(env, clazz, pngPath, tiffPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertPngTiffFd
  (JNIEnv *env, jclass clazz, jint pngFd, jint tiffFd, jobject options, jobject listener)
  {
    PngToTiffConverter *converter = new PngToTiffConverter(env, clazz, pngFd, tiffFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertJpgTiff
  (JNIEnv *env, jclass clazz, jstring pngPath, jstring tiffPath, jobject options, jobject listener)
  {
    JpgToTiffConverter *converter = new JpgToTiffConverter(env, clazz, pngPath, tiffPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertJpgTiffFd
  (JNIEnv *env, jclass clazz, jint jpgFd, jint tiffFd, jobject options, jobject listener)
  {
    JpgToTiffConverter *converter = new JpgToTiffConverter(env, clazz, jpgFd, tiffFd, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertBmpTiff
  (JNIEnv *env, jclass clazz, jstring bmpPath, jstring tiffPath, jobject options, jobject listener)
  {
    BmpToTiffConverter *converter = new BmpToTiffConverter(env, clazz, bmpPath, tiffPath, options, listener);
    jboolean result = converter->convert();
    delete(converter);
    return result;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeConvertBmpTiffFd
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

JNIEXPORT jobject JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeGetImageType
  (JNIEnv *env, jclass clazz, jstring path)
  {


    const char *strPath = NULL;
    strPath = env->GetStringUTFChars(path, 0);
    LOGIS("path", strPath);

    int imageformat = IMAGE_FILE_INVALID;

    FILE *inFile = fopen(strPath, "rb");
    if (inFile) {
        unsigned char data[12] = {0};
        size_t bytesRead = fread(data, 1, sizeof(data), inFile);
        imageformat = detectImageFormat(data, bytesRead);
        fclose(inFile);
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

JNIEXPORT jobject JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeGetImageTypeFd
  (JNIEnv *env, jclass clazz, jint fd)
  {

    int imageformat = IMAGE_FILE_INVALID;

    LOGII("fd ", fd);

    if (fd != -1) {
        LOGI("Start check");
        unsigned char data[12] = {0};
        ssize_t bytesRead = pread(fd, data, sizeof(data), 0);
        if (bytesRead > 0) {
            imageformat = detectImageFormat(data, static_cast<size_t>(bytesRead));
        }
        // Keep the historical contract: callers commonly detect the format
        // immediately before passing the same descriptor to a decoder or
        // converter, which expects to start at the beginning of the file.
        lseek(fd, 0, SEEK_SET);
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

JNIEXPORT void
JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_nativeCloseFd
        (JNIEnv *env, jclass clazz, jint fd) {
    close(fd);
}

#ifdef __cplusplus
}
#endif
