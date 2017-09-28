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

#ifdef __cplusplus
}
#endif
