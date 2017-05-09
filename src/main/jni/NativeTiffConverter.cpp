//
// Created by beyka on 5/9/17.
//
using namespace std;
#ifdef __cplusplus
extern "C" {
#endif

#include "NativeTiffConverter.h"

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertTiffPng
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring pngPath)
  {
  return JNI_FALSE;
  }

JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffConverter_convertPngTiff
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring pngPath)
  {
  return JNI_FALSE;
  }

#ifdef __cplusplus
}
#endif