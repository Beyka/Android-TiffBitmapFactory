using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

#include "NativeTiffBitmapFactory.h"
#include "readTiffIncremental.h"
#include "NativeExceptions.h"

JNIEXPORT jobject
JNICALL Java_org_beyka_tiffbitmapfactory_TiffBitmapFactory_nativeDecodePath
        (JNIEnv *env, jclass clazz, jstring path, jobject options) {

    NativeDecoder *decoder = new NativeDecoder(env, clazz, path, options);
    jobject java_bitmap = decoder->getBitmap();
    delete(decoder);

    return java_bitmap;
}

#ifdef __cplusplus
}
#endif
