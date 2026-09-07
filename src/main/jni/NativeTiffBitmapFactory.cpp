using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

#include "NativeTiffBitmapFactory.h"

JNIEXPORT jobject
JNICALL Java_org_beyka_tiffbitmapfactory_TiffBitmapFactory_nativeDecodePath
        (JNIEnv *env, jclass clazz, jstring path, jobject options, jobject listener) {

    NativeDecoder *decoder = new NativeDecoder(env, clazz, path, options, listener);
    jobject java_bitmap = decoder->getBitmap();
    delete(decoder);

    return java_bitmap;
}

JNIEXPORT jobject
JNICALL Java_org_beyka_tiffbitmapfactory_TiffBitmapFactory_nativeDecodeFD
        (JNIEnv *env, jclass clazz, jint fd, jobject options, jobject listener) {

    NativeDecoder *decoder = new NativeDecoder(env, clazz, fd, options, listener);
    jobject java_bitmap = decoder->getBitmap();
    delete(decoder);

    return java_bitmap;
}

JNIEXPORT jobject
JNICALL Java_org_beyka_tiffbitmapfactory_TiffBitmapFactory_nativeCloseFd
        (JNIEnv *env, jclass clazz, jint fd) {
    close(fd);
}

#ifdef __cplusplus
}
#endif
